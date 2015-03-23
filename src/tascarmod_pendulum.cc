#include "session.h"

class pendulum_t : public TASCAR::actor_module_t {
public:
  pendulum_t(xmlpp::Element* xmlsrc,TASCAR::session_t* session);
  ~pendulum_t();
  void write_xml();
  void update(double t, bool running);
private:
  double amplitude;
  double frequency;
  double decaytime;
  double starttime;
  TASCAR::pos_t distance;
};

pendulum_t::pendulum_t(xmlpp::Element* xmlsrc,TASCAR::session_t* session)
  : actor_module_t(xmlsrc,session),
    amplitude(45),
    frequency(0.5),
    decaytime(40),
    starttime(10),
    distance(0,0,-2)
{
  GET_ATTRIBUTE(amplitude);
  GET_ATTRIBUTE(frequency);
  GET_ATTRIBUTE(decaytime);
  GET_ATTRIBUTE(starttime);
  GET_ATTRIBUTE(distance);
}

void pendulum_t::write_xml()
{
  SET_ATTRIBUTE(amplitude);
  SET_ATTRIBUTE(frequency);
  SET_ATTRIBUTE(decaytime);
  SET_ATTRIBUTE(starttime);
  SET_ATTRIBUTE(distance);
}

pendulum_t::~pendulum_t()
{
}

void pendulum_t::update(double time,bool running)
{
  double rx(amplitude*DEG2RAD);
  time -= starttime;
  if( time>0 )
    rx = amplitude*DEG2RAD*cos(PI2*frequency*(time))*exp(-0.70711*time/decaytime);
  for(std::vector<TASCAR::named_object_t>::iterator iobj=obj.begin();iobj!=obj.end();++iobj){
    TASCAR::zyx_euler_t dr(0,0,rx);
    TASCAR::pos_t dp(distance);
    dp *= TASCAR::zyx_euler_t(0,0,rx);
    dp *= TASCAR::zyx_euler_t(iobj->obj->c6dof.o.z,iobj->obj->c6dof.o.y,0);
    iobj->obj->dorientation = dr;
    iobj->obj->dlocation = dp;
  }
}

REGISTER_MODULE(pendulum_t);
REGISTER_MODULE_UPDATE(pendulum_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
