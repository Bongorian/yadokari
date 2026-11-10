#include "virtual_analog_voice.h"
#include "virtual_analog_state.h"
#include "wavetable_voice.h"
#include "wavetable_state.h"
#include "spectral_voices.h"
#include "spectral_states.h"
#include <cassert>
#include <initializer_list>
#include <cmath>
#include <cstdio>
using namespace groove;
template<class Voice,class Trigger,unsigned N>
static unsigned check(const char* name,Voice& voice,const ParameterDescriptor (&descriptors)[N],unsigned attack,unsigned decay,unsigned sustain,unsigned release,Trigger trigger){
 unsigned count=0;
 auto run=[&](unsigned mode,unsigned note,unsigned velocity,bool maximum_envelope){
  float values[N];for(unsigned i=0;i<N;++i)values[i]=mode==0?descriptors[i].initial:mode==1?descriptors[i].minimum:descriptors[i].maximum;
  values[attack]=descriptors[attack].minimum;values[decay]=descriptors[decay].minimum;values[release]=descriptors[release].minimum;
  if(maximum_envelope){values[attack]=descriptors[attack].maximum;values[decay]=descriptors[decay].maximum;values[sustain]=1;values[release]=descriptors[release].maximum;}
  voice.init(48000);float hz=440*std::pow(2.f,(float(note)-69)/12);trigger(hz,float(velocity)/127,values);
  unsigned held=maximum_envelope?unsigned(values[attack]*48000*1.1f)+480:4096;
  unsigned released=unsigned(values[release]*48000*5)+4800;double peak=0,sum=0,energy=0;float tail[256]{};
  for(unsigned i=0;i<held+released;++i){float x=voice.next(hz,i<held);assert(std::isfinite(x));peak=std::fmax(peak,std::fabs(x));assert(peak<=8);if(i<held){sum+=x;energy+=double(x)*x;tail[i%256]=x;}}
  assert(!voice.active());assert(voice.next(hz,false)==0);
  if(!maximum_envelope && mode==0 && note==60 && velocity==127)assert(energy>1.e-6);
  double total=0,high=0;for(unsigned k=1;k<128;++k){double re=0,im=0;for(unsigned n=0;n<256;++n){double x=tail[n]*(.5-.5*std::cos(6.283185307179586*n/256));double phase=6.283185307179586*k*n/256;re+=x*std::cos(phase);im+=x*std::sin(phase);}double power=re*re+im*im;total+=power;if(k>=103)high+=power;}
  std::printf("{\"voice\":\"%s\",\"mode\":%u,\"note\":%u,\"velocity\":%u,\"max_envelope\":%u,\"held_frames\":%u,\"release_budget_frames\":%u,\"peak\":%.8f,\"mean\":%.8f,\"rms\":%.8f,\"high_band_fraction\":%.8f}\n",name,mode,note,velocity,unsigned(maximum_envelope),held,released,peak,sum/held,std::sqrt(energy/held),total?high/total:0);
  ++count;
 };
 for(unsigned mode=0;mode<3;++mode)for(unsigned note:{21u,60u,108u})for(unsigned velocity:{1u,64u,127u})run(mode,note,velocity,false);
 run(0,60,100,true);return count;
}
int main(){assert(prepare_wavetable_bank());VirtualAnalogVoice va;WavetableVoice wavetable;AdditiveVoice additive;PhaseDistVoice phase;
 unsigned cases=0;
 cases+=check("VA",va,kVirtualAnalogParameters,11,12,13,14,[&](float,float v,const float* p){va.trigger(v,p,0x52454c31);});
 cases+=check("WAVETABLE",wavetable,kWavetableParameters,3,5,6,4,[&](float hz,float v,const float* p){wavetable.trigger(hz,v,p);});
 cases+=check("ADDITIVE",additive,kAdditiveParameters,5,6,7,8,[&](float,float v,const float* p){additive.trigger(v,p);});
 cases+=check("PHASE DIST",phase,kPhaseDistParameters,5,6,7,8,[&](float,float v,const float* p){phase.trigger(v,p);});
 std::printf("Voice boundaries: %u fixed cases, finite/bounded output, default middle-note energy and timed release passed; mean/high-band are diagnostics, not a listening or alias-free claim\n",cases);
}
