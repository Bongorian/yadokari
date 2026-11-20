#include "daisy_drum_bank.h"
#include <cmath>
#include <cstdio>
#include <limits>
using namespace groove;
int main(){
 for(unsigned kit=0;kit<2;++kit)for(unsigned voice=0;voice<3;++voice){
  DaisyDrumBank drums;drums.init(48000);float p[]={float(voice)/3+0.05f,0.5f,0.5f,0.6f,0.7f};
  drums.trigger(kit,p,0.8f,voice==0?110.0f:220.0f);
  float peak=0;double energy=0;
  for(unsigned i=0;i<24000;++i){float x=drums.next();if(!std::isfinite(x))return 1;peak=std::fmax(peak,std::fabs(x));energy+=double(x)*x;}
  if(peak<0.001f||energy<0.0001)return 2;
  std::printf("kit=%u voice=%u peak=%.3f energy=%.3f\n",kit,voice,peak,energy);
 }
 // Non-finite controls never reach DaisySP: every voice keeps sounding and stays finite.
 const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 for(unsigned kit=0;kit<2;++kit)for(unsigned voice=0;voice<3;++voice){
  DaisyDrumBank drums;drums.init(48000);float good[]={float(voice)/3+0.05f,0.5f,0.65f,0.68f,1.0f};
  float bad[]={good[0],nan,inf,nan,-inf};drums.trigger(kit,bad,nan,nan);
  for(unsigned i=0;i<12000;++i)if(!std::isfinite(drums.next()))return 3;
  drums.trigger(kit,good,0.9f,65.4f);double energy=0;
  for(unsigned i=0;i<12000;++i){float x=drums.next();if(!std::isfinite(x))return 4;energy+=double(x)*x;}
  if(energy<0.0001)return 5;
  if(drums.recoveries())return 6;
 }
 // Shared by all voices of a track: the generator advances once per output sample,
 // however many overlapping voices read it. The analog kick has no noise source, so a
 // second bank stepped once per sample must match it exactly.
 {DaisyDrumBank single,shared;single.init(48000);shared.init(48000);float p[]={0.05f,0.5f,0.65f,0.68f,1.0f};
  single.trigger(0,p,0.9f,65.4f);shared.trigger(0,p,0.9f,65.4f);
  for(uint64_t n=0;n<12000;++n){float a=single.next(n);float b=shared.next(n),c=shared.next(n),d=shared.next(n);if(a!=b || b!=c || c!=d)return 7;}}
 for(unsigned kit=0;kit<2;++kit)for(unsigned voice=0;voice<3;++voice){
  DaisyDrumBank drums;drums.init(48000);float p[]={float(voice)/3+0.05f,0.5f,0.65f,0.68f,1.0f};drums.trigger(kit,p,0.9f,65.4f);
  for(uint64_t n=0;n<4800;++n){float b=drums.next(n);if(drums.next(n)!=b || drums.next(n)!=b)return 8;}
 }
 std::printf("drum bank: non-finite controls sanitized; shared voices advance once per sample\n");
}
