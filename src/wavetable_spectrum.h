#pragma once
#include "wavetable_warp.h"
#include "wavetable_bank.h"

namespace groove {
struct WavetableQuadrature {
 float re,im;
 WavetableQuadrature mix(WavetableQuadrature b,float x)const{return {re+x*(b.re-re),im+x*(b.im-im)};}
 WavetableQuadrature times(WavetableQuadrature b)const{return {re*b.re-im*b.im,re*b.im+im*b.re};}
};
constexpr int kWarpHarmonics=4,kPmSidebands=3,kWarpDepthSteps=32,kUpperMip=2,kUpperHarmonics=64>>kUpperMip;
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
struct WavetableWarpSpectrum {WavetableQuadrature harmonic[2*kWarpHarmonics+1];};
extern WavetableWarpSpectrum kWarpSpectra[4][kWarpDepthSteps+1];
extern WavetableQuadrature kUpperSource[kWavetableWaves][kUpperHarmonics-1]; // harmonics 2..16
// At high pitches the carrier and PM sidebands can cross Nyquist separately.
// Sum the admitted components at their physical frequencies. This keeps the
// Fourier coefficients (table scan and moving controls) outside the phase map.
inline float wavetable_spectral_sample(WavetablePhaseMap map,float step,float carrier,float modulator,
                                       float position,unsigned mip,float mip_blend){
 const float depth=(map.warp+1)*(.5f*kWarpDepthSteps);
 unsigned bin=unsigned(depth);if(bin>=kWarpDepthSteps)bin=kWarpDepthSteps-1;
 const auto* spectra=kWarpSpectra[map.mode]+bin;
 auto rotation=[](float p)->WavetableQuadrature{return {wavetable_sine(p+.25f),wavetable_sine(p)};};
 WavetableQuadrature c[2*kWarpHarmonics+1],m[2*kPmSidebands+1];
 auto powers=[](WavetableQuadrature* center,int length,WavetableQuadrature unit){
  center[0]={1,0};
  for(int i=1;i<=length;++i){center[i]=center[i-1].times(unit);center[-i]={center[i].re,-center[i].im};}
 };
 powers(c+kWarpHarmonics,kWarpHarmonics,rotation(carrier));
 powers(m+kPmSidebands,kPmSidebands,rotation(modulator));
 // Jacobi-Anger expansion. The modulation index is at most 0.35*2*pi.
 // Four terms of each Bessel series suffice over that bounded interval.
 const float x=1.09955743f*map.pm,q=x*x;
 const float j[]={1-q*(1-q*(.25f-q*(1.f/36-q/576))),
  x*(1-q*(.5f-q*(1.f/12-q*(1.f/144-q/2880)))),
  q*(.5f-q*(1.f/6-q*(1.f/48-q/720))),
  x*q*(1.f/6-q*(1.f/24-q*(1.f/240-q/4320)))};
 const float side[]={-j[3],j[2],-j[1],j[0],j[1],j[2],j[3]};
 WavetableQuadrature result{};
 for(int h=-kWarpHarmonics;h<=kWarpHarmonics;++h){
  const unsigned k=unsigned(h+kWarpHarmonics);
  const auto a=spectra[0].harmonic[k].mix(spectra[1].harmonic[k],depth-bin);
  WavetableQuadrature pm{};
  const int extent=map.pm>0?kPmSidebands:0;
  for(int n=-extent;n<=extent;++n){
   const float frequency=fabsf((h+n*map.ratio)*step);
   if(frequency>=.48f)continue;
   const float gain=side[n+kPmSidebands]*fminf(1,(.48f-frequency)*20);
   pm.re+=gain*m[n+kPmSidebands].re;pm.im+=gain*m[n+kPmSidebands].im;
  }
  const auto term=a.times(c[k]).times(pm);result.re+=term.re;result.im+=term.im;
 }
 const unsigned a=unsigned(position),b=a+1<kWavetableWaves?a+1:a;
 const float mix=position-a;
 const auto* first=kWavetableData[a][6];const auto* second=kWavetableData[b][6];
 const float cosine=first[0]+mix*(second[0]-first[0]),sine=first[64]+mix*(second[64]-first[64]);
 float sample=cosine*result.re+sine*result.im;
 // Preserve the source's remaining in-band partials at low warp/PM depths.
 // The same slope, brightness and mip blend as the oversampled path remove
 // them progressively as the warped fundamental needs more bandwidth.
 const unsigned limit=64u>>mip;
 auto osc=c[kWarpHarmonics+1];
 for(unsigned h=2;h<=kUpperHarmonics && h<=limit;++h){
  osc=h<=kWarpHarmonics?c[kWarpHarmonics+h]:osc.times(c[kWarpHarmonics+1]);
  const auto part=kUpperSource[a][h-2].mix(kUpperSource[b][h-2],mix);
  sample+=(part.re*osc.re+part.im*osc.im)*(h>limit/2?1-mip_blend:1);
 }
 return sample/32767.f;
}
}
