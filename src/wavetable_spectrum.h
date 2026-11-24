#pragma once
#include "wavetable_warp.h"

namespace groove {
constexpr unsigned kUpperMip=2,kUpperHarmonics=64>>kUpperMip;
struct WavetableLevels {unsigned mip,next;float blend;};
inline WavetableLevels wavetable_levels(float step,float brightness){
  float allowed=step>0?.45f/step:128;allowed*=.015625f+.984375f*brightness;
  unsigned mip=0,harmonics=64;
  while(mip<6 && float(harmonics)>allowed){++mip;harmonics>>=1;}
  float blend=2-allowed/float(harmonics);if(blend<0)blend=0;if(blend>1)blend=1;
  return {mip,mip<6?mip+1:mip,blend};
 }
inline WavetableLevels wavetable_extended_levels(WavetablePhaseMap map){
 const float slope=1+(map.mode==1?4:1)*fabsf(map.warp);
 return wavetable_levels(map.step*slope*(1+2.2f*map.pm*map.ratio),map.brightness);
}
inline float wavetable_spectral_mix(float step){
 constexpr float crossover=.45f/(2*kUpperHarmonics);
 return fmaxf(0,fminf(1,step/crossover-1));
}
float wavetable_spectral_sample(WavetablePhaseMap map,float step,float carrier,float modulator,
                                       float position,unsigned mip,float mip_blend);
}
