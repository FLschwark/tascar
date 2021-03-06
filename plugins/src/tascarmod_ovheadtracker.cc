#include "serviceclass.h"
#include "session.h"
#include <atomic>
#include <chrono>
#include <thread>

#include "serialport.h"

class ovheadtracker_t : public TASCAR::actor_module_t,
                        protected TASCAR::service_t {
public:
  ovheadtracker_t(const TASCAR::module_cfg_t& cfg);
  virtual ~ovheadtracker_t();
  void add_variables(TASCAR::osc_server_t* srv);
  void update(uint32_t frame, bool running);
  void configure();
  void release();

protected:
  void service();
  void service_level();

private:
  // configuration variables:
  std::string name;
  std::vector<std::string> devices;
  // data logging OSC url:
  std::string url;
  // rotation OSC url:
  std::string roturl;
  // rotation OSC path:
  std::string rotpath;
  uint32_t ttl;
  std::string calib0path;
  std::string calib1path;
  std::vector<int32_t> axes;
  double accscale;
  double gyrscale;
  bool apply_loc;
  bool apply_rot;
  bool send_only_quaternion;
  double autoref;
  // run-time variables:
  lo_address target;
  lo_address rottarget;
  TASCAR::pos_t p0;
  TASCAR::zyx_euler_t o0;
  bool bcalib;
  TASCAR::quaternion_t qref;
  bool first;
  // level logging:
  std::thread srv_level;
  std::atomic_bool run_service_level;
  std::vector<std::string> levelpattern;
  std::vector<TASCAR::Scene::audio_port_t*> ports;
  std::vector<TASCAR::Scene::route_t*> routes;
  std::vector<lo_message> vmsg;
  std::vector<lo_arg**> vargv;
  // level meter paths:
  std::vector<std::string> vpath;
};

void ovheadtracker_t::configure()
{
  TASCAR::actor_module_t::configure();
  ports.clear();
  routes.clear();
  for(auto msg : vmsg)
    lo_message_free(msg);
  vmsg.clear();
  vargv.clear();
  vpath.clear();
  if(session)
    ports = session->find_audio_ports(levelpattern);
  for(auto port : ports) {
    TASCAR::Scene::route_t* r(dynamic_cast<TASCAR::Scene::route_t*>(port));
    if(!r) {
      TASCAR::Scene::sound_t* s(dynamic_cast<TASCAR::Scene::sound_t*>(port));
      if(s)
        r = dynamic_cast<TASCAR::Scene::route_t*>(s->parent);
    }
    routes.push_back(r);
  }
  for(auto route : routes) {
    vmsg.push_back(lo_message_new());
    for(uint32_t k = 0; k < route->metercnt(); ++k)
      lo_message_add_float(vmsg.back(), 0);
    vargv.push_back(lo_message_get_argv(vmsg.back()));
    vpath.push_back(std::string("/") + name + std::string("/") +
                    route->get_name());
  }
  first = true;
  srv_level = std::thread(&ovheadtracker_t::service_level, this);
}

void ovheadtracker_t::release()
{
  run_service_level = false;
  srv_level.join();
  TASCAR::actor_module_t::release();
}

ovheadtracker_t::ovheadtracker_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg), name("ovheadtracker"),
      devices({"/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2"}), ttl(1),
      calib0path("/calib0"), calib1path("/calib1"), axes({0, 1, 2}),
      accscale(16384 / 9.81), gyrscale(16.4), apply_loc(false), apply_rot(true),
      send_only_quaternion(false), autoref(0), target(NULL), rottarget(NULL),
      bcalib(false), qref(1, 0, 0, 0), first(true), run_service_level(true)
{
  GET_ATTRIBUTE(name);
  GET_ATTRIBUTE(devices);
  GET_ATTRIBUTE(url);
  GET_ATTRIBUTE(roturl);
  GET_ATTRIBUTE(rotpath);
  GET_ATTRIBUTE(ttl);
  GET_ATTRIBUTE(calib0path);
  GET_ATTRIBUTE(calib1path);
  GET_ATTRIBUTE(autoref);
  GET_ATTRIBUTE(axes);
  GET_ATTRIBUTE(accscale);
  GET_ATTRIBUTE(gyrscale);
  GET_ATTRIBUTE_BOOL(apply_loc);
  GET_ATTRIBUTE_BOOL(apply_rot);
  GET_ATTRIBUTE_BOOL(send_only_quaternion);
  GET_ATTRIBUTE(levelpattern);
  if(url.size()) {
    target = lo_address_new_from_url(url.c_str());
    if(!target)
      throw TASCAR::ErrMsg("Unable to create target adress \"" + url + "\".");
    lo_address_set_ttl(target, ttl);
  }
  if((roturl.size() > 0) && (rotpath.size() > 0)) {
    rottarget = lo_address_new_from_url(roturl.c_str());
    if(!rottarget)
      throw TASCAR::ErrMsg("Unable to create target adress \"" + roturl +
                           "\".");
    lo_address_set_ttl(rottarget, ttl);
  }
  add_variables(session);
  start_service();
}

void ovheadtracker_t::add_variables(TASCAR::osc_server_t* srv)
{
  std::string p;
  if(name.size())
    p = "/" + name;
  srv->add_double(p + "/autoref", &autoref);
  srv->add_bool(p + "/apply_loc", &apply_loc);
  srv->add_bool(p + "/apply_rot", &apply_rot);
}

void ovheadtracker_t::service()
{
  uint32_t devidx(0);
  for(size_t k = axes.size(); k < 8; ++k)
    axes.push_back(-1);
  for(auto ax : axes) {
    ax = std::min(ax, 3);
    if(ax < 0)
      ax = 3;
  }
  std::vector<double> data(4, 0.0);
  std::vector<double> data2(7, 0.0);
  std::string p;
  if(name.size())
    p = "/" + name;
  while(run_service) {
    try {
      TASCAR::serialport_t dev;
      dev.open(devices[devidx].c_str(), B115200, 0, 1);
      while(run_service) {
        std::string l(dev.readline(1024, 10));
        if(l.size()) {
          switch(l[0]) {
          case 'A': {
            l = l.substr(1);
            std::string::size_type sz;
            data[0] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[1] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[2] = std::stod(l);
            data[3] = 0.0;
            if(target && (!send_only_quaternion))
              lo_send(target, (p + "/acc").c_str(), "fff",
                      data[axes[0]] / accscale, data[axes[1]] / accscale,
                      data[axes[2]] / accscale);
          } break;
          case 'W': {
            l = l.substr(1);
            std::string::size_type sz;
            data[0] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[1] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[2] = std::stod(l);
            data[3] = 0.0;
            if(target && (!send_only_quaternion)) {
              TASCAR::pos_t wacc(data[axes[0]], data[axes[1]], data[axes[2]]);
              wacc *= 1.0 / accscale;
              lo_send(target, (p + "/wacc").c_str(), "fff", wacc.x, wacc.y,
                      wacc.z);
              qref.rotate(wacc);
              lo_send(target, (p + "/waccref").c_str(), "fff", wacc.x, wacc.y,
                      wacc.z);
            }
          } break;
          case 'R': {
            l = l.substr(1);
            std::string::size_type sz;
            data2[0] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[1] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[2] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[3] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[4] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[5] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[6] = std::stod(l);
            if(target && (!send_only_quaternion)) {
              lo_send(target, (p + "/raw").c_str(), "fffffff", data2[0],
                      data2[1], data2[2], data2[3], data2[4], data2[5],
                      data2[6]);
              lo_send(target, (p + "/acc").c_str(), "fff", data2[1] / accscale,
                      data2[2] / accscale, data2[3] / accscale);
              lo_send(target, (p + "/gyr").c_str(), "fff", data2[4] / gyrscale,
                      data2[5] / gyrscale, data2[6] / gyrscale);
            }
          } break;
          case 'O': {
            l = l.substr(1);
            std::string::size_type sz;
            data2[0] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[1] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[2] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[3] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[4] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data2[5] = std::stod(l, &sz);
            if(target && (!send_only_quaternion))
              lo_send(target, (p + "/offs").c_str(), "ffffff", data2[0],
                      data2[1], data2[2], data2[3], data2[4], data2[5]);
          } break;
          case 'Q': {
            l = l.substr(1);
            std::string::size_type sz;
            TASCAR::quaternion_t q;
            q.w = std::stod(l, &sz);
            l = l.substr(sz + 1);
            q.x = std::stod(l, &sz);
            l = l.substr(sz + 1);
            q.y = std::stod(l, &sz);
            l = l.substr(sz + 1);
            q.z = std::stod(l, &sz);
            if(bcalib)
              qref = q.inverse();
            if(autoref > 0) {
              if(first) {
                qref = q.inverse();
                first = false;
              } else {
                qref = qref.scale(1.0 - autoref);
                qref += q.inverse().scale(autoref);
                qref = qref.scale(1.0 / qref.norm());
              }
            }
            q *= qref;
            o0 = q.to_euler();
            if(target)
              lo_send(target, (p + "/quaternion").c_str(), "ffff", q.w, q.x,
                      q.y, q.z);
            if(rottarget)
              lo_send(rottarget, rotpath.c_str(), "fff", RAD2DEG * o0.z,
                      RAD2DEG * o0.y, RAD2DEG * o0.x);
          } break;
          case 'G': {
            l = l.substr(1);
            std::string::size_type sz;
            data[0] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[1] = std::stod(l, &sz);
            l = l.substr(sz + 1);
            data[2] = std::stod(l);
            data[3] = 0.0;
            if(target && (!send_only_quaternion))
              lo_send(target, (p + "/axrot").c_str(), "fff", data[axes[0]],
                      data[axes[1]], data[axes[2]]);
          } break;
          case 'C': {
            if((l.size() > 1)) {
              if((l[1] == '1') && calib1path.size()) {
                bcalib = true;
                if(target)
                  lo_send(target, calib1path.c_str(), "i", 1);
              }
              if((l[1] == '0') && calib0path.size()) {
                bcalib = false;
                if(target)
                  lo_send(target, calib0path.c_str(), "i", 1);
              }
            }
          } break;
            // default: {
            //  std::cout << l << std::endl;
            //} break;
          }
        }
      }
    }
    catch(const std::exception& e) {
      std::cerr << e.what() << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      ++devidx;
      if(devidx >= devices.size())
        devidx = 0;
    }
  }
}

ovheadtracker_t::~ovheadtracker_t()
{
  stop_service();
  if(target)
    lo_address_free(target);
}

void ovheadtracker_t::update(uint32_t tp_frame, bool tp_rolling)
{
  if(apply_loc)
    set_location(p0);
  if(apply_rot)
    set_orientation(o0);
}

void ovheadtracker_t::service_level()
{
  while(run_service_level) {
    uint32_t k = 0;
    for(auto it = routes.begin(); it != routes.end(); ++it) {
      const std::vector<float>& leveldata((*it)->readmeter());
      uint32_t n(leveldata.size());
      for(uint32_t km = 0; km < n; ++km)
        vargv[k][km]->f = leveldata[km];
      if(target)
        lo_send_message(target, vpath[k].c_str(), vmsg[k]);
      ++k;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

REGISTER_MODULE(ovheadtracker_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
