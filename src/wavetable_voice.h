#pragma once
#include "wavetable_bank.h"
#include "wavetable_warp.h"
#include "wavetable_state.h"
#include "Control/adsr.h"
#include <math.h>

namespace groove {
class WavetableVoice {
 float rate_=48000,phase_=0,scan_=0,mod_phase_=0,velocity_=0,level_=0,wet_=0;
 float values_[kWavetableParameterCount]{},target_[kWavetableParameterCount]{};
 daisysp::Adsr envelope_;
 bool started_=false;

 // First decimation (4x to 2x): polyphase halfband with unity phase
 // alignment through .24 of 4x Nyquist (~113 dB mirrored rejection).
 // The earlier sample is the odd, delayed branch.
 float first_stage_[4]{},second_stage_[12]{};
 float halfband(float earlier,float later){
  constexpr float coeff[]={.04070874f,.16504998f,.38481404f,.74011707f};
  auto& state=first_stage_;
  for(unsigned i=0;i<4;i+=2){
   float y=coeff[i]*later+state[i];
   state[i]=later-coeff[i]*y;later=y;
  }
  for(unsigned i=1;i<4;i+=2){
   float y=coeff[i]*earlier+state[i];
   state[i]=earlier-coeff[i]*y;earlier=y;
  }
  return .5f*(earlier+later);
 }

 // Final decimation (2x to 1x): elliptic reconstruction filter at 2x,
 // pass edge .425, stop edge .501 of 2x Nyquist (.025 dB / 110 dB).
 // A halfband here would pass substantial foldback near output Nyquist.
 float lowpass(float x){
  static constexpr float sections[][5]={
   {8.284775058153e-04f, 1.567862198144e-03f, 8.284775058153e-04f, -1.099592427363e+00f, 3.324419837930e-01f},
   {1.000000000000e+00f, 1.273811012039e+00f, 1.000000000000e+00f, -9.485817793894e-01f, 4.542936013641e-01f},
   {1.000000000000e+00f, 6.636926444652e-01f, 1.000000000000e+00f, -7.457776514240e-01f, 6.211021772141e-01f},
   {1.000000000000e+00f, 2.797571499348e-01f, 1.000000000000e+00f, -5.767564222557e-01f, 7.682698098691e-01f},
   {1.000000000000e+00f, 7.581366921343e-02f, 1.000000000000e+00f, -4.704675940242e-01f, 8.783628400174e-01f},
   {1.000000000000e+00f, -1.065765306933e-02f, 1.000000000000e+00f, -4.264005281304e-01f, 9.619450526552e-01f}
  };
  for(unsigned i=0;i<sizeof(sections)/sizeof(*sections);++i){
   const auto& c=sections[i];float* const state=second_stage_+2*i;
   float y=c[0]*x+state[0];
   state[0]=c[1]*x-c[3]*y+state[1];state[1]=c[2]*x-c[4]*y;x=y;
  }
  return x;
 }
 static float table(unsigned wave,unsigned mip,float phase){
  float x=phase*256;unsigned i=unsigned(x)&255;
  float a=kWavetableData[wave][mip][i],b=kWavetableData[wave][mip][(i+1)&255];
  return (a+(b-a)*(x-floorf(x)))/32767;
 }
 struct Levels {unsigned mip,next;float blend;};
 static Levels select_levels(float step,float brightness){
  float allowed=step>0?.45f/step:128;allowed*=.015625f+.984375f*brightness;
  unsigned mip=0,harmonics=64;
  while(mip<6 && float(harmonics)>allowed){++mip;harmonics>>=1;}
  float blend=2-allowed/float(harmonics);if(blend<0)blend=0;if(blend>1)blend=1;
  return {mip,mip<6?mip+1:mip,blend};
 }
 float legacy_sample(float position,float step)const{
  auto l=select_levels(step,values_[7]);unsigned a=unsigned(position),b=a<127?a+1:a;
  float x=table(a,l.mip,phase_),y=table(b,l.mip,phase_);x+=(y-x)*(position-a);
  if(l.blend>0){float lo=table(a,l.next,phase_),hi=table(b,l.next,phase_);lo+=(hi-lo)*(position-a);x+=(lo-x)*l.blend;}
  return x;
 }
 struct Frame {
  const int16_t *a,*b,*low_a,*low_b;
  float wave_mix,mip_mix;
  bool fundamental;float cosine,sine;
  static float cubic(const int16_t* data,unsigned i,float t){
   float a=data[(i-1)&255],b=data[i],c=data[(i+1)&255],d=data[(i+2)&255];
   return b+.5f*t*((c-a)+t*((2*a-5*b+4*c-d)+t*(3*(b-c)+d-a)));
  }
  float read(float phase)const{
   if(fundamental)return cosine*wavetable_sine(phase+.25f)+sine*wavetable_sine(phase);
   float x=phase*256;unsigned i=unsigned(x)&255;float t=x-floorf(x);
   float sample=cubic(a,i,t);
   if(wave_mix>0)sample+=(cubic(b,i,t)-sample)*wave_mix;
   if(mip_mix>0){
    float lo=cubic(low_a,i,t);
    if(wave_mix>0)lo+=(cubic(low_b,i,t)-lo)*wave_mix;
    sample+=(lo-sample)*mip_mix;
   }
   return sample;
  }
 };
 float carrier_step(float hz)const{return fminf(.45f,fmaxf(0,hz/rate_));}
 WavetablePhaseMap phase_map(float step)const{
  return wavetable_phase_map(unsigned(target_[WavetableWarpMode]),values_[WavetableWarpAmount],
                             values_[WavetablePmRatio],values_[WavetablePmAmount],step);
 }
 float extended_sample(float position,float step){
  const auto map=phase_map(step);
  unsigned mode=map.mode,a=unsigned(position),b=a<127?a+1:a;
  float ratio=map.ratio,mod_step=ratio*step,pm=map.pm,warp=map.warp;
  // Reserve source bandwidth for both the phase warp and the PM sidebands.
  float slope=mode==1?1+4*fabsf(warp):1+fabsf(warp);
  auto l=select_levels(step*slope*(1+2.2f*pm*ratio),values_[7]);
  Frame frame{kWavetableData[a][l.mip],kWavetableData[b][l.mip],
              kWavetableData[a][l.next],kWavetableData[b][l.next],position-a,l.next==l.mip?0:l.blend,l.mip==6,0,0};
  // Level six contains only the fundamental. Recover its two quadratures
  // directly, avoiding re-modulation of the int16 table's quantisation noise.
  if(frame.fundamental){
   frame.cosine=frame.a[0]+frame.wave_mix*(frame.b[0]-frame.a[0]);
   frame.sine=frame.a[64]+frame.wave_mix*(frame.b[64]-frame.a[64]);
  }
  auto sub=[&](float offset){
   auto phases=wavetable_read_phases(phase_+offset*step,mode,warp,mod_phase_+offset*mod_step,pm);
   float sample=frame.read(phases.first);
   if(phases.blend>0)sample+=(frame.read(phases.second)-sample)*phases.blend;
   return sample;
  };
  float s0=sub(0),s1=sub(.25f),s2=sub(.5f),s3=sub(.75f);
  float lo=halfband(s0,s1);
  float hi=halfband(s2,s3);
  mod_phase_=wavetable_wrap(mod_phase_+mod_step);
  lowpass(lo);return lowpass(hi)/32767;
 }
 public:
 void init(unsigned rate){rate_=float(rate);envelope_.Init(rate_);started_=false;}
 void parameters(const float* values){
  for(unsigned i=0;i<kWavetableParameterCount;++i)target_[i]=values[i];
  envelope_.SetTime(daisysp::ADSR_SEG_ATTACK,values[3]);envelope_.SetTime(daisysp::ADSR_SEG_DECAY,values[5]);envelope_.SetSustainLevel(values[6]);envelope_.SetTime(daisysp::ADSR_SEG_RELEASE,values[4]);
 }
 void trigger(float,float velocity,const float* values){
  if(!wavetable_bank_ready()){started_=false;return;}
  velocity_=velocity;phase_=scan_=mod_phase_=0;envelope_.Init(rate_);parameters(values);
  for(unsigned i=0;i<kWavetableParameterCount;++i)values_[i]=values[i];
  wet_=(values[WavetableWarpAmount]!=0 || values[WavetablePmAmount]!=0)?1.f:0.f;
  for(float& v:first_stage_)v=0;
  for(float& v:second_stage_)v=0;
  started_=true;
 }
 bool active()const{return started_;}
 float level()const{return level_;}
 WavetablePhaseMap visual_phase_map(float hz)const{return phase_map(carrier_step(hz));}
 void visual_controls(float* out)const{for(unsigned i=0;i<3;++i)out[i]=values_[i];}
 void visual_envelope(float* out)const{out[0]=values_[3];out[1]=values_[5];out[2]=values_[6];out[3]=values_[4];}
 float scan_phase()const{return scan_;}
 float position()const{float p=values_[0]+values_[1]*sinf(scan_*6.283185307f);return fmaxf(0,fminf(127,p))/127;}
 float next(float hz,bool gate){
  if(!started_)return 0;
  for(unsigned i=0;i<kWavetableLegacyParameterCount;++i)values_[i]+=(target_[i]-values_[i])*.002f;
  float step=carrier_step(hz);
  float pos=values_[0]+values_[1]*sinf(scan_*6.283185307f);if(pos<0)pos=0;if(pos>127)pos=127;
  bool extended=target_[WavetableWarpAmount]!=0 || target_[WavetablePmAmount]!=0;
  float x;
  if(extended || wet_>0){
   for(unsigned i=WavetableWarpAmount;i<kWavetableParameterCount;++i)values_[i]+=(target_[i]-values_[i])*.002f;
   if(wet_==0){
    for(float& v:first_stage_)v=0;
    for(float& v:second_stage_)v=0;
    mod_phase_=wavetable_wrap(phase_*values_[WavetablePmRatio]);
   }
   wet_=extended?fminf(1,wet_+1.f/64):fmaxf(0,wet_-1.f/64);
   x=extended_sample(pos,step);
   if(wet_<1){float dry=legacy_sample(pos,step);x=dry+(x-dry)*wet_;}
   if(wet_==0){values_[WavetableWarpAmount]=values_[WavetablePmAmount]=0;values_[WavetablePmRatio]=target_[WavetablePmRatio];}
  }else {values_[WavetablePmRatio]=target_[WavetablePmRatio];x=legacy_sample(pos,step);}
  phase_+=step;if(phase_>=1)phase_-=1;scan_+=values_[2]/rate_;if(scan_>=1)scan_-=1;
  level_=envelope_.Process(gate);if(!gate && !envelope_.IsRunning())started_=false;return x*level_*velocity_*.35f;
 }
};
}
