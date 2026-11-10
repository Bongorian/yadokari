#include "creative_voice.h"
#include "plaits_voice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
struct Reader:ISampleReader {WavView resident;unsigned calls=0;explicit Reader(WavView s):resident(s){}bool read_frame(uint32_t frame,unsigned ch,float& value)override{++calls;value=wav_sample(resident,frame,ch);return frame<resident.frames && ch<resident.channels;}uint32_t underruns()const override{return 0;}};
static void stretch(){
 std::vector<float> pcm(65536);for(unsigned i=4096;i<12288;++i)pcm[i]=.5f;
 WavView view{reinterpret_cast<const uint8_t*>(pcm.data()),unsigned(pcm.size()),48000,1,32,3};
 const float ratios[]={1.f,.5f};const unsigned rates[]={48000,44100};for(unsigned test=0;test<2;++test){float ratio=ratios[test];unsigned rate=rates[test];
  WavVoice voice;SampleEdit edit;edit.mode=SampleMode::Stretch;assert(voice.start_edit(view,rate,1,false,false,edit,ratio));
  unsigned end=unsigned(49152*ratio*rate/48000),tail=unsigned((12288*ratio+8192)*rate/48000);float peak=0;
  for(unsigned i=0;i<end;++i){float l,r;voice.next(l,r);peak=fmaxf(peak,fabsf(l));if(i>tail)assert(l==0 && r==0);}
  assert(peak>.4f);
 }
}
static void creative(){
 // Real >8MiB PCM extent, represented alternately by resident memory and a
 // nonblocking reader. Every sample, including spectral frames, must agree.
 std::vector<float> pcm(2200000);for(unsigned i=0;i<pcm.size();++i)pcm[i]=.3f*sinf(float(i)*.017f);
 WavView resident{reinterpret_cast<const uint8_t*>(pcm.data()),unsigned(pcm.size()),32000,1,32,3};Reader reader(resident),other(resident);other.resident.rate=44100;
 WavView a[64]{},b[64]{};for(unsigned i=0;i<64;++i){a[i]=resident;b[i]=resident;b[i].data=nullptr;b[i].reader=&reader;}
 CreativeState state;for(auto& value:state.value[2])value=0;float parameters[]={.4f,.5f,.7f,.3f,.6f};
 for(Instrument kind:{Instrument::WaveSequence,Instrument::TapeMicrosound,Instrument::SpectralFreeze}){
  auto left=std::make_unique<CreativeVoice>(),right=std::make_unique<CreativeVoice>();left->reset();right->reset();left->trigger(state,unsigned(kind));right->trigger(state,unsigned(kind));double energy=0;
  for(unsigned i=0;i<5000;++i){float l=left->next(unsigned(kind),220,48000,120,parameters,state,a,0,false,0),r=right->next(unsigned(kind),220,48000,120,parameters,state,b,0,false,0);assert(l==r && std::isfinite(r));energy+=r*r;}
  assert(energy>0);
 }
 assert(reader.calls>0);
 // Same data=nullptr but a different reader and rate must refresh Tape's reel.
 auto switched=std::make_unique<CreativeVoice>(),expected=std::make_unique<CreativeVoice>();switched->reset();expected->reset();
 switched->trigger(state,unsigned(Instrument::TapeMicrosound));expected->trigger(state,unsigned(Instrument::TapeMicrosound));for(unsigned i=0;i<8;++i)assert(switched->next(unsigned(Instrument::TapeMicrosound),220,48000,120,parameters,state,b,0,false,0)==expected->next(unsigned(Instrument::TapeMicrosound),220,48000,120,parameters,state,a,0,false,0));
 for(unsigned i=0;i<64;++i){a[i]=other.resident;b[i]=other.resident;b[i].data=nullptr;b[i].reader=&other;}
 switched->trigger(state,unsigned(Instrument::TapeMicrosound));expected->trigger(state,unsigned(Instrument::TapeMicrosound));
 for(unsigned i=0;i<2000;++i)assert(switched->next(unsigned(Instrument::TapeMicrosound),220,48000,120,parameters,state,b,0,false,0)==expected->next(unsigned(Instrument::TapeMicrosound),220,48000,120,parameters,state,a,0,false,0));
 assert(other.calls>=4096);
}
static void plaits_quantum(){
 auto a=std::make_unique<PlaitsVoice>(),b=std::make_unique<PlaitsVoice>();a->init();b->init();a->seed(42);b->seed(42);a->trigger(100,true);b->trigger(100,true);
 for(unsigned i=0;i<1205;++i)assert(a->next()==b->next());
 assert(a->set_parameter(0,.9f));for(unsigned i=0;i<7;++i)assert(a->next()==b->next());
 bool changed=false;for(unsigned i=0;i<120;++i)changed|=a->next()!=b->next();assert(changed);
}
int main(){stretch();creative();plaits_quantum();std::puts("R1/R2/R15: stretch consumed silence, resident/reader Creative equivalence/cache refresh and Plaits residual quantum passed");}
