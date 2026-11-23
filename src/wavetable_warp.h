#pragma once
#include <math.h>

namespace groove {
struct WavetableReadPhases { float first, second, blend; };

inline float wavetable_wrap(float phase){ return phase-floorf(phase); }

// step is carrier cycles per output sample. Above .1533 Fs, progressively
// remove warp bandwidth; at .32 Fs even a second harmonic is ultrasonic.
inline float wavetable_limited_warp(unsigned mode,float amount,float step){
 amount*=fminf(1,fmaxf(0,(.32f-step)*6));
 if(mode==1 && step>0)
  amount=copysignf(fminf(fabsf(amount),fmaxf(0,(.45f/step-1)/3)),amount);
 return amount;
}
inline float wavetable_limited_pm(float amount,float mod_step){
 return amount*fmaxf(0,fminf(1,(.49f-mod_step)*3.4f));
}

struct WavetablePhaseMap {
 unsigned mode;
 float warp,ratio,pm;
};

inline WavetablePhaseMap wavetable_phase_map(unsigned mode,float warp,float ratio,float pm,float step){
 return {mode,wavetable_limited_warp(mode,warp,step),ratio,wavetable_limited_pm(pm,ratio*step)};
}

// Odd quarter-wave polynomial. Reducing to [0, pi/2] keeps the error below
// 4e-6; the same sine is used by the audio voice and its phase preview.
inline float wavetable_sine(float phase){
 phase-=floorf(phase+.5f);
 float distance=fabsf(phase);
 float x=6.283185307f*fminf(distance,.5f-distance),x2=x*x;
 float y=x*(1+x2*(-1.f/6+x2*(1.f/120+x2*(-1.f/5040+x2/362880))));
 return copysignf(y,phase);
}

// A periodic carrier phase map. Signed sync chooses the reset anchor; blend
// adjacent whole cycles so fractional ratios never tear the waveform at wrap.
inline WavetableReadPhases wavetable_read_phases(float phase,unsigned mode,float amount,
                                                 float mod_phase,float pm_amount){
 float modulation=pm_amount==0?0:.35f*pm_amount*wavetable_sine(mod_phase);
 phase=wavetable_wrap(phase);
 if(amount==0){phase=wavetable_wrap(phase+modulation);return {phase,phase,0};}
 switch(mode){
 case 0: phase+=.15f*amount*wavetable_sine(phase);break;
 case 1:{
  float ratio=1+3*fabsf(amount),lo=floorf(ratio),anchor=amount<0?.5f:0;
  float first=wavetable_wrap((phase-anchor)*lo+anchor+modulation);
  float second=wavetable_wrap((phase-anchor)*(lo+1)+anchor+modulation);
  return {first,second,ratio-lo};
 }
 case 2:{
  // Shift the half-cycle crossing continuously; both slopes meet at wrap.
  float edge=phase*(1-phase);
  phase+=3.7f*amount*edge*edge;
  break;
 }
 case 3:{
  // Bend just one lobe; the opposite lobe keeps its original phase.
  float local=2*phase-(amount<0?1.f:0.f);
  if(local>0 && local<1){
   float edge=local*(1-local);
   phase+=9.6f*amount*edge*edge*edge;
  }
  break;
 }
 }
 phase=wavetable_wrap(phase+modulation);
 return {phase,phase,0};
}
}
