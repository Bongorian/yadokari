#include "rings/dsp/string.h"
#undef RINGS_DSP_STRING_H_
#define String ReferenceString
#define DampingFilter ReferenceDampingFilter
#define kDelayLineSize ReferenceDelayLineSize
#include "../third_party/mutable/rings/dsp/string.h"
#undef kDelayLineSize
#undef DampingFilter
#undef String
#include "stmlib/utils/random.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
int main(){
 for(bool dispersion:{false,true}){
  auto cached=std::make_unique<rings::String>();auto reference=std::make_unique<rings::ReferenceString>();std::memset(cached.get(),0,sizeof *cached);std::memset(reference.get(),0,sizeof *reference);cached->Init(dispersion);reference->Init(dispersion);
  uint32_t random=77;
  for(unsigned block=0;block<10000;++block){
   if(block%97==0){float frequency=(block%7==0?5.f:55.f+block%1200)/48000.f,brightness=(block%99)/100.f,damping=(block%101)/100.f,position=(block%79)/80.f;cached->set_frequency(frequency);reference->set_frequency(frequency);cached->set_brightness(brightness);reference->set_brightness(brightness);cached->set_damping(damping);reference->set_damping(damping);cached->set_position(position);reference->set_position(position);}
   float in[24]{},a[24]{},b[24]{},ar[24]{},br[24]{};if(block%31==0)in[0]=.2f;
   stmlib::Random::Seed(random);cached->Process(in,a,b,24);uint32_t next=stmlib::Random::state();stmlib::Random::Seed(random);reference->Process(in,ar,br,24);assert(stmlib::Random::state()==next);random=next;
   for(unsigned i=0;i<24;++i){assert(std::isfinite(a[i]) && std::isfinite(b[i]));}assert(!std::memcmp(a,ar,sizeof a) && !std::memcmp(b,br,sizeof b));
  }
 }
}
