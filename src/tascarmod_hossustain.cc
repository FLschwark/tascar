#include "session.h"
#include "ola.h"
#include <stdlib.h>

double drand()
{
  return (double)random()/(double)(RAND_MAX+1.0);
}

class sustain_vars_t : public TASCAR::module_base_t {
public:
  sustain_vars_t( const TASCAR::module_cfg_t& cfg );
  ~sustain_vars_t(){};
protected:
  std::string id;
  float tau_sustain;
  float tau_envelope;
  float bass;
  float bassratio;
  float wet;
  uint32_t wlen;
  float fcut;
  double gain;
};

sustain_vars_t::sustain_vars_t( const TASCAR::module_cfg_t& cfg )
  : TASCAR::module_base_t(cfg),
    tau_sustain(20),
    tau_envelope(1),
    bass(0),
    bassratio(2),
    wet(1.0),
    wlen(8192),
    fcut(40),
    gain(1.0)
{
  GET_ATTRIBUTE(id);
  if( id.empty() )
    id = "sustain";
  GET_ATTRIBUTE(tau_sustain);
  GET_ATTRIBUTE(tau_envelope);
  GET_ATTRIBUTE(wet);
  GET_ATTRIBUTE(wlen);
  GET_ATTRIBUTE(bass);
  GET_ATTRIBUTE(bassratio);
  GET_ATTRIBUTE(fcut);
  GET_ATTRIBUTE_DB(gain);
}

class sustain_t : public sustain_vars_t, public jackc_db_t {
public:
  sustain_t( const TASCAR::module_cfg_t& cfg );
  ~sustain_t();
  virtual int inner_process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
  virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
  static int osc_apply(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_apply(float t);
  void cleanup();
protected:
  TASCAR::ola_t ola;
  TASCAR::wave_t absspec;
  double Lin;
  double Lout;
  uint32_t t_apply;
  float deltaw;
  float currentw;
  uint32_t fcut_int;
}; 

int sustain_t::osc_apply(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  ((sustain_t*)user_data)->set_apply(argv[0]->f);
  return 0;
}

void sustain_t::set_apply(float t)
{
  deltaw = 0;
  t_apply = 0;
  if( t >= 0 ){
    uint32_t tau(std::max(1,(int32_t)(srate*t)));
    deltaw = (wet-currentw)/(float)tau;
    t_apply = tau;
  }
}

int sustain_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  jackc_db_t::process(n,vIn,vOut);
  TASCAR::wave_t w_in(n,vIn[0]);
  TASCAR::wave_t w_out(n,vOut[0]);
  float env_c1(0);
  if( tau_envelope > 0 )
    env_c1 = exp( -1.0/(tau_envelope*(double)srate));
  float env_c2(1.0f-env_c1);
  // envelope reconstruction:
  for(uint32_t k=0;k<w_in.size();++k){
    Lin *= env_c1;
    Lin += env_c2*w_in[k]*w_in[k];
    Lout *= env_c1;
    Lout += env_c2*w_out[k]*w_out[k];
    if( Lout > 0 )
      w_out[k] *= sqrt(Lin/Lout);
    if( t_apply ){
      t_apply--;
      currentw += deltaw;
    }
    w_out[k] *= gain*currentw;
    w_out[k] += (1.0f-std::max(0.0f,currentw))*w_in[k];
  }
  return 0;
}

int sustain_t::inner_process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  fcut_int = (fcut/(0.5*f_sample))*ola.s.size();
  TASCAR::wave_t w_in(n,vIn[0]);
  TASCAR::wave_t w_out(n,vOut[0]);
  ola.process(w_in);
  float sus_c1(0);
  if( tau_sustain > 0 )
    sus_c1 = exp( -1.0/(tau_sustain*(double)srate/(double)(w_in.size())));
  float sus_c2(1.0f-sus_c1);
  ola.s *= sus_c2;
  absspec *= sus_c1;
  float br(1.0f/std::max(1.0f,bassratio));
  for(uint32_t k=ola.s.size();k--;){
    absspec[k] += cabsf(ola.s[k]);
    if( (bass > 0) )
      absspec[k*br] += bass*cabsf(ola.s[k]);
    ola.s[k] = absspec[k]*cexpf(I*drand()*PI2);
    if( k<fcut_int )
      ola.s[k] = 0;
  }
  ola.ifft(w_out);
  return 0;
}

sustain_t::sustain_t( const TASCAR::module_cfg_t& cfg )
  : sustain_vars_t( cfg ),
    jackc_db_t( id, wlen ),
    ola(2*wlen,2*wlen,wlen,TASCAR::stft_t::WND_HANNING,TASCAR::stft_t::WND_RECT,0.5,TASCAR::stft_t::WND_SQRTHANN),
    absspec(ola.s.size()),
    Lin(0),
    Lout(0),
    t_apply(0),
    deltaw(0),
    currentw(0),
    fcut_int(0)
{
  add_input_port("in");
  add_output_port("out");
  session->add_float("/c/"+id+"/sustain/tau_sus",&tau_sustain);
  session->add_float("/c/"+id+"/sustain/tau_env",&tau_envelope);
  session->add_float("/c/"+id+"/sustain/bass",&bass);
  session->add_float("/c/"+id+"/sustain/bassratio",&bassratio);
  session->add_float("/c/"+id+"/sustain/fcut",&fcut);
  session->add_double_db("/c/"+id+"/sustain/gain",&gain);
  session->add_float("/c/"+id+"/sustain/wet",&wet);
  session->add_method("/c/"+id+"/sustain/wetapply","f",&sustain_t::osc_apply,this);
  activate();
}

void sustain_t::cleanup()
{
  deactivate();
}

sustain_t::~sustain_t()
{
}

REGISTER_MODULE(sustain_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * compile-command: "make -C .."
 * End:
 */
