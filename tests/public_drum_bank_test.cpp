#include "daisy_drum_bank.h"
#include <cmath>
#include <cstdio>
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
}
