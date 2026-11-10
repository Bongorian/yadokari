#pragma once
#include "Synthesis/harmonic_osc.h"
#include "Control/adsr.h"
#include <math.h>
namespace groove {
inline void additive_spectrum(const float* p,float* out,float motion=0){float sum=0;for(unsigned i=0;i<16;++i){float n=float(i+1),distance=(n-p[2]-motion*4)/p[3];float parity=i&1?fminf(1,2*p[1]):fminf(1,2*(1-p[1]));out[i]=p[9+i]*powf(n,-2*(1-p[0]))*expf(-.5f*distance*distance)*parity;sum+=out[i];}if(sum>0)for(unsigned i=0;i<16;++i)out[i]*=.95f/sum;}
class SpectralEnvelope {
protected:
 float visual_envelope_[4]{};daisysp::Adsr envelope_;float rate_=48000,velocity_=0,level_=0;bool active_=false;
 void envelope_parameters(const float* p){for(unsigned i=0;i<4;++i)visual_envelope_[i]=p[5+i];envelope_.SetTime(daisysp::ADSR_SEG_ATTACK,p[5]);envelope_.SetTime(daisysp::ADSR_SEG_DECAY,p[6]);envelope_.SetSustainLevel(p[7]);envelope_.SetTime(daisysp::ADSR_SEG_RELEASE,p[8]);}
 bool envelope_next(bool gate){level_=envelope_.Process(gate);if(!gate && !envelope_.IsRunning())active_=false;return active_;}
public:
 void visual_envelope(float* out)const{for(unsigned i=0;i<4;++i)out[i]=visual_envelope_[i];}
 bool active()const{return active_;}
 float level()const{return level_;}
};
class AdditiveVoice:public SpectralEnvelope {
 daisysp::HarmonicOscillator<16> oscillator_{};float values_[25]{},amplitudes_[16]{},targets_[16]{},motion_=0;unsigned frame_=0;
public:
 void spectrum(float* out)const{for(unsigned i=0;i<16;++i)out[i]=active_?amplitudes_[i]:0;}
 // The pinned numeric-only vendor object has an empty user constructor;
 // Init reads its prior frequency/index/recalc fields before setting them.
 AdditiveVoice(){__builtin_memset(static_cast<void*>(&oscillator_),0,sizeof oscillator_);}
 void init(unsigned rate){rate_=float(rate);oscillator_.Init(rate_);envelope_.Init(rate_);active_=false;}
 void parameters(const float* p){for(unsigned i=0;i<25;++i)values_[i]=p[i];envelope_parameters(p);}
 void trigger(float velocity,const float* p){init(unsigned(rate_));parameters(p);velocity_=velocity;motion_=0;frame_=0;additive_spectrum(values_,amplitudes_);oscillator_.SetAmplitudes(amplitudes_);active_=true;}
 float next(float hz,bool gate){if(!active_ || !envelope_next(gate))return 0;if((frame_++&63)==0)additive_spectrum(values_,targets_,sinf(motion_*6.2831853f)*values_[4]);for(unsigned i=0;i<16;++i)amplitudes_[i]+=(targets_[i]-amplitudes_[i])*.01f;oscillator_.SetAmplitudes(amplitudes_);oscillator_.SetFreq(fminf(hz,rate_*.45f));motion_+=.3f/rate_;if(motion_>=1)motion_-=1;return oscillator_.Process()*level_*velocity_*.4f;}
};
inline float phase_distortion_map(float phase,unsigned wave,float distortion){float corner=.5f-.48f*distortion;float x=phase<corner?phase*.5f/corner:.5f+(phase-corner)*.5f/(1-corner);if(wave==1)x=phase<.5f?x:phase;else if(wave==2)x=phase<corner?phase/corner:1;else if(wave==3)x=phase<.5f?powf(phase*2,1+3*distortion)*.5f:1-powf((1-phase)*2,1+3*distortion)*.5f;return x;}
class PhaseDistVoice:public SpectralEnvelope {
 float values_[9]{},phase_=0,motion_=0,distortion_=0,dc_in_=0,dc_out_=0,dc_coefficient_=0;
public:
 void init(unsigned rate){rate_=float(rate);envelope_.Init(rate_);dc_in_=dc_out_=0;dc_coefficient_=expf(-6.2831853f*5/rate_);active_=false;}
 void parameters(const float* p){for(unsigned i=0;i<9;++i)values_[i]=p[i];envelope_parameters(p);}
 void trigger(float velocity,const float* p){init(unsigned(rate_));parameters(p);velocity_=velocity;phase_=motion_=0;distortion_=p[1];active_=true;}
 float next(float hz,bool gate){if(!active_ || !envelope_next(gate))return 0;float target=fmaxf(0,fminf(1,values_[1]+values_[2]*level_+values_[3]*sinf(motion_*6.2831853f)*.25f));distortion_+=(target-distortion_)*.002f;float sum=0,step=fminf(hz/rate_,.45f)*.25f;for(unsigned i=0;i<4;++i){sum+=sinf(phase_distortion_map(phase_,unsigned(values_[0]),distortion_)*6.2831853f);phase_+=step;if(phase_>=1)phase_-=1;}motion_+=values_[4]/rate_;if(motion_>=1)motion_-=1;float raw=sum*.25f,ac=raw-dc_in_+dc_coefficient_*dc_out_;dc_in_=raw;dc_out_=ac;return ac*.4f*level_*velocity_;}
};
}
