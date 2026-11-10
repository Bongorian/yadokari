#pragma once
#include <math.h>
#include "Filters/svf.h"
#include "Effects/wavefolder.h"
#include "daisy_track_drive.h"
#include "vocoder.h"
#include "fx_registry.h"
#include "pitch_shift.h"
#include "rotary.h"
#ifdef GROOVE_PLAITS
#include "plaits/dsp/oscillator/sine_oscillator.h"
#endif

namespace groove {
// [3/3] Pade approximation of exp(-x) on [0,1]; the 48 kHz phaser
// cutoff range stays within this interval. Unusual rates use libm.
inline float modulation_decay(float x){if(x>1)return expf(-x);float x2=x*x,x3=x2*x;return (120-60*x+12*x2-x3)/(120+60*x+12*x2+x3);}
// Finite inputs only: preserve signed zero and exact clamp values without
// out-of-line libm calls in every pad/track/group sample.
inline float finite_insert_clip(float value,float limit){return value>limit?limit:value< -limit?-limit:value;}
// One audio-owned stereo slot. Type changes fade down before the processor
// state is reset and fade back in; bypass is exactly dry when idle.
class TrackInsertSlot {
public:
 void bind_pitch(PitchShiftStereo::Pool& pool,unsigned index){pitch_.bind(pool,index);}
 bool ready()const{return pitch_.ready();}
 static constexpr unsigned kModFrames=2048,kModFloats=kModFrames*2;
 void bind_modulation(float* storage){mod_buffer_=storage;mod_write_=mod_age_=0;}
 void init(unsigned rate){rate_=rate?rate:48000;inverse_rate_=1.0f/float(rate_);attack_=1-expf(-inverse_rate_/0.002f);vocoder_.init(rate_);pitch_.init(rate_);rotary_.init(rate_);reset();}
 void reset(){mode_=0;wet_=phase_=0;counter_=0;clear_extra();cached_a_=cached_b_=-1;for(auto& f:filter_)f.Init(float(rate_));folder_.Init();}
 float activity()const{return idle()?0:activity_;}
 void vocoder_levels(float* out)const{if(mode_==19 && wet_>.00001f)vocoder_.levels(out);else for(unsigned i=0;i<16;++i)out[i]=0;}
 bool idle()const{return mode_==0 && wet_<0.0001f;}
 __attribute__((always_inline)) inline void process(float& left,float& right,unsigned requested,float a,float b,float mix,float input_l=0,float input_r=0){
  // Exact dry bypass: no coefficient, fade, validation or DSP work.
  if(!requested && !mode_ && wet_==0)return;
  if(!fx_descriptor(requested))requested=0;
  if(mode_!=requested){wet_+=(0-wet_)*0.025f;if(wet_<0.001f){mode_=requested;wet_=phase_=0;counter_=0;clear_extra();cached_a_=cached_b_=-1;for(auto& f:filter_)f.Init(float(rate_));}}
  else wet_+=((mode_?mix:0)-wet_)*0.01f;
  if(!mode_ || wet_<0.00001f)return;
  if(!counter_){if(mode_<=4){bool change_a=a!=cached_a_,change_b=b!=cached_b_,first=cached_a_<0;if(change_a || change_b || first){float hz=20+(float(rate_)*0.30f-20)*a*a;auto& f=filter_[0];if(change_a)f.SetFreq(hz);if(change_b)f.SetRes(b*0.85f);if(first)f.SetDrive(0);filter_[1].CopyCoefficientsFrom(f);cached_a_=a;cached_b_=b;}}else if(mode_==6 && a!=cached_a_){folder_.SetGain(1+8*a);folder_.SetOffset(0);cached_a_=a;}else if(mode_==10){hold_period_=1+unsigned(a*a*96);quant_=float(1u<<(1+unsigned((1-b)*14)));inverse_quant_=1/quant_;}else if(mode_==12 && b!=cached_b_){coefficient_=1-expf(-inverse_rate_/(0.01f+b*b));cached_b_=b;}else if(mode_==13 && b!=cached_b_){float hz=200+7800*b*b;coefficient_=1-expf(-6.2831853f*hz*inverse_rate_);cached_b_=b;}else if(mode_>=16 && mode_<=18){mod_increment_=(0.1f+5*a*a)*inverse_rate_;mod_depth_parameter_=b;if(mode_<=17){mod_base_=float(rate_)*(mode_==16?0.012f:0.002f);mod_depth_=float(rate_)*(mode_==16?0.007f:0.0015f)*b;mod_needs_clip_=mod_base_-mod_depth_<1 || mod_base_+mod_depth_>kModFrames-2;}else{float p=phase_+mod_increment_;if(p>=1)p-=1;float q=p+0.25f;if(q>=1)q-=1;float t[2]={1-4*__builtin_fabsf(p-0.5f),1-4*__builtin_fabsf(q-0.5f)};for(unsigned ch=0;ch<2;++ch)mod_target_[ch]=modulation_decay(6.2831853f*(300+(200+4500*b)*(0.5f+0.5f*t[ch]))*inverse_rate_);}}counter_=64;}--counter_;
  float in_l=left,in_r=right;float safe_l=isfinite(left)?finite_insert_clip(left,4):0,safe_r=isfinite(right)?finite_insert_clip(right,4):0;float out_l=safe_l,out_r=safe_r;
  if(mode_<=4){out_l=filter_[0].ProcessMode(safe_l,mode_);out_r=filter_[1].ProcessMode(safe_r,mode_);}
  else if(mode_==5){out_l=drive_.process(safe_l,a);out_r=drive_.process(safe_r,a);}
  else if(mode_==6){out_l=folder_.Process(safe_l);out_r=folder_.Process(safe_r);}
  else if(mode_<=9){phase_+=(0.1f+a*(mode_==9?999.9f:19.9f))*inverse_rate_;if(phase_>=1)phase_-=1;
#ifdef GROOVE_PLAITS
   float wave=plaits::SineNoWrap(phase_);
#else
   float wave=sinf(phase_*6.2831853f);
#endif
   if(mode_==7){float gain=1-b*(0.5f+0.5f*wave);out_l*=gain;out_r*=gain;}else if(mode_==8){out_l*=1-b*(0.5f+0.5f*wave);out_r*=1-b*(0.5f-0.5f*wave);}else{out_l*=wave;out_r*=wave;}}
  else if(mode_==10){if(!hold_count_){held_l_=safe_l;held_r_=safe_r;hold_count_=hold_period_;}--hold_count_;out_l=roundf(held_l_*quant_)*inverse_quant_;out_r=roundf(held_r_*quant_)*inverse_quant_;}
  else if(mode_==11){float gain=1+7*a,trim=1-0.75f*b;float l=safe_l*gain,r=safe_r*gain;out_l=trim*l/(1+__builtin_fabsf(l));out_r=trim*r/(1+__builtin_fabsf(r));}
  else if(mode_==12){float threshold=0.001f+0.4f*a*a,peak=__builtin_fmaxf(__builtin_fabsf(safe_l),__builtin_fabsf(safe_r));if(peak>=threshold)gate_open_=true;else if(peak<threshold*0.8f)gate_open_=false;float target=gate_open_?1.0f:0.0f;gate_+=(target-gate_)*(gate_open_?attack_:coefficient_);out_l*=gate_;out_r*=gate_;}
  else if(mode_==13){low_l_+=(safe_l-low_l_)*coefficient_;low_r_+=(safe_r-low_r_)*coefficient_;float tilt=(a-0.5f)*1.5f;out_l=low_l_*(1-tilt)+(safe_l-low_l_)*(1+tilt);out_r=low_r_*(1-tilt)+(safe_r-low_r_)*(1+tilt);}
  else if(mode_==14){float mid=(safe_l+safe_r)*0.5f,side=(safe_l-safe_r)*a;out_l=(mid+side)*(b<=0.5f?1:2*(1-b));out_r=(mid-side)*(b>=0.5f?1:2*b);}
  else if(mode_==15){float limit=1-0.9f*b,gain=1+7*a;out_l=__builtin_fmaxf(-limit,__builtin_fminf(limit,safe_l*gain));out_r=__builtin_fmaxf(-limit,__builtin_fminf(limit,safe_r*gain));}
  else if(mode_==19){input_l=isfinite(input_l)?finite_insert_clip(input_l,4):0;input_r=isfinite(input_r)?finite_insert_clip(input_r,4):0;unsigned mod=fx_choice_index(a,3),carrier=fx_choice_index(b,4);float input_mod=mod==0?input_l:mod==1?input_r:(input_l+input_r)*.5f;if(carrier){out_l=carrier==1?input_l:carrier==2?input_r:input_l;out_r=carrier==1?input_l:carrier==2?input_r:input_r;}vocoder_.process(out_l,out_r,input_mod);}
  else if(mode_==21)rotary_.process(mod_buffer_,out_l,out_r,fx_choice_index(a,3),b);
  else if(mode_==20)pitch_.process(out_l,out_r,int(fx_choice_index(a,25))-12);
  else process_modulation(safe_l,safe_r,out_l,out_r);
  if(!isfinite(out_l) || !isfinite(out_r)){for(auto& f:filter_)f.Init(float(rate_));counter_=0;cached_a_=cached_b_=-1;out_l=out_r=0;}
  out_l=finite_insert_clip(out_l,2);out_r=finite_insert_clip(out_r,2);
  left=in_l+(out_l-in_l)*wet_;right=in_r+(out_r-in_r)*wet_;if(!counter_)activity_=fmaxf(fabsf(left),fabsf(right));
 }
private:
 float mod_read(unsigned channel,float distance)const{if(mod_needs_clip_){if(distance<1)distance=1;if(distance>kModFrames-2)distance=kModFrames-2;}unsigned n=unsigned(distance);float frac=distance-float(n);float a=mod_age_>=n?mod_buffer_[((mod_write_-n)&(kModFrames-1))*2+channel]:0;float b=mod_age_>n?mod_buffer_[((mod_write_-n-1)&(kModFrames-1))*2+channel]:0;return a+(b-a)*frac;}
 __attribute__((noinline)) void process_modulation(float l,float r,float& out_l,float& out_r){
  phase_+=mod_increment_;if(phase_>=1)phase_-=1;
  if(mode_<=17){if(!mod_buffer_)return;float q=phase_+0.25f;if(q>=1)q-=1;float tl=1-4*__builtin_fabsf(phase_-0.5f),tr=1-4*__builtin_fabsf(q-0.5f);float dl=mod_read(0,mod_base_+mod_depth_*tl),dr=mod_read(1,mod_base_+mod_depth_*tr);float feedback=mode_==16?0.12f:0.55f*mod_depth_parameter_;mod_buffer_[mod_write_*2]=l+dl*feedback;mod_buffer_[mod_write_*2+1]=r+dr*feedback;mod_write_=(mod_write_+1)&(kModFrames-1);if(mod_age_<kModFrames)++mod_age_;out_l=(l+dl)*0.5f;out_r=(r+dr)*0.5f;}
  else{float input[2]={l,r},out[2]{};for(unsigned ch=0;ch<2;++ch){mod_coefficient_[ch]+=(mod_target_[ch]-mod_coefficient_[ch])*0.02f;float x=input[ch]+mod_feedback_[ch]*0.35f*mod_depth_parameter_;for(unsigned stage=0;stage<4;++stage){float y=mod_state_[ch][stage]-mod_coefficient_[ch]*x;mod_state_[ch][stage]=x+mod_coefficient_[ch]*y;x=y;}mod_feedback_[ch]=x;out[ch]=(input[ch]+x)*0.5f;}out_l=out[0];out_r=out[1];}
 }
 void clear_extra(){pitch_.reset();rotary_.reset();vocoder_.reset();hold_count_=0;hold_period_=1;held_l_=held_r_=low_l_=low_r_=gate_=0;gate_open_=false;coefficient_=0;quant_=inverse_quant_=1;mod_write_=mod_age_=0;for(unsigned ch=0;ch<2;++ch){mod_target_[ch]=mod_coefficient_[ch]=0;mod_feedback_[ch]=0;for(float& v:mod_state_[ch])v=0;}}
 float activity_=0;
 unsigned rate_=48000,mode_=0,counter_=0,hold_count_=0,hold_period_=1;float inverse_rate_=1.0f/48000,wet_=0,phase_=0,cached_a_=-1,cached_b_=-1;
 float held_l_=0,held_r_=0,low_l_=0,low_r_=0,gate_=0,coefficient_=0,quant_=1,inverse_quant_=1,attack_=0;bool gate_open_=false;
 float* mod_buffer_=nullptr;unsigned mod_write_=0,mod_age_=0;bool mod_needs_clip_=false;float mod_increment_=0,mod_base_=0,mod_depth_=0,mod_depth_parameter_=0;float mod_target_[2]{},mod_coefficient_[2]{},mod_feedback_[2]{},mod_state_[2][4]{};
 Rotary rotary_;PitchShiftStereo pitch_;Vocoder vocoder_;daisysp::Svf filter_[2];daisysp::Wavefolder folder_;DaisyTrackDrive drive_;
};
inline const char* track_insert_name(unsigned type){if(auto* d=fx_descriptor(type))return d->name;static const char* names[]={"BYPASS","LOW PASS","HIGH PASS","BAND PASS","NOTCH","OVERDRIVE","FOLDER","TREMOLO","AUTO PAN","RING MOD","DECIMATOR","SOFT CLIP","NOISE GATE","TILT EQ","STEREO WIDTH","HARD CLIP","CHORUS","FLANGER","PHASER"};return names[type<=18?type:0];}
inline const char* track_insert_control_name(unsigned type,unsigned field){
 if(auto* d=fx_descriptor(type))return field>=1 && field<=2?d->controls[field-1].name:"";
 static const char* a[]={"","CUTOFF","CUTOFF","CUTOFF","CUTOFF","DRIVE","FOLD","RATE","RATE","RATE","RATE REDUCE","DRIVE","THRESHOLD","TILT","WIDTH","DRIVE","RATE","RATE","RATE"};
 static const char* b[]={"","RESONANCE","RESONANCE","RESONANCE","RESONANCE","","","DEPTH","DEPTH","","BIT CRUSH","OUTPUT","RELEASE","FREQUENCY","BALANCE","CEILING","DEPTH","DEPTH","DEPTH"};
 return type<19?(field==1?a[type]:field==2?b[type]:""):"";
}
}
