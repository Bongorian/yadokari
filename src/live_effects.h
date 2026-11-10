#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
namespace groove {
// All storage is allocated by Engine construction. No callback allocation.
class StereoDelay {
public:
 explicit StereoDelay(unsigned rate):rate_(rate?rate:48000),buffer_(new float[size_t(rate_)*2]()),fade_length_(rate_/100?rate_/100:1) {}
 ~StereoDelay() { delete[] buffer_; }
 void reset(){write_=age_=distance_=next_distance_=fade_=0;}
 void process(float in_l,float in_r,float feedback,unsigned ms,float& out_l,float& out_r) {
  unsigned requested=unsigned(uint64_t(rate_)*ms/1000); if(requested<1) requested=1; if(requested>rate_) requested=rate_;
  if(!distance_)distance_=requested;
  if(!fade_ && requested!=distance_){next_distance_=requested;fade_=1;}
  read(distance_,out_l,out_r);
  if(fade_){float next_l,next_r;read(next_distance_,next_l,next_r);float blend=float(fade_)/float(fade_length_);if(blend>1)blend=1;out_l+=(next_l-out_l)*blend;out_r+=(next_r-out_r)*blend;if(++fade_>fade_length_){distance_=next_distance_;fade_=0;}}
  if(!isfinite(feedback))feedback=0;
  feedback=__builtin_fmaxf(-0.98f,__builtin_fminf(0.98f,feedback));
  if(!isfinite(in_l))in_l=0;
  if(!isfinite(in_r))in_r=0;
  float next_l=in_l+out_r*feedback,next_r=in_r+out_l*feedback;
  buffer_[write_*2]=isfinite(next_l)?next_l:0; buffer_[write_*2+1]=isfinite(next_r)?next_r:0; if(++write_>=rate_)write_=0;
  if(age_<rate_)++age_;
 }
 StereoDelay(const StereoDelay&)=delete; StereoDelay& operator=(const StereoDelay&)=delete;
private:
 void read(unsigned distance,float& l,float& r)const{unsigned p=write_+rate_-distance;if(p>=rate_)p-=rate_;l=age_>=distance?buffer_[p*2]:0;r=age_>=distance?buffer_[p*2+1]:0;}
 unsigned rate_,write_=0,age_=0; float* buffer_;unsigned fade_length_,distance_=0,next_distance_=0,fade_=0;
};
// Parallel damped combs and serial all-pass diffusers. Fixed storage is
// allocated before playback; feedback is bounded for all parameter values.
class StereoReverb {
 struct Line {float* data=nullptr;unsigned length=0,pos=0,age=0;float filtered=0;};
 Line comb_[2][4]{},diffuser_[2][2]{};
 static void init(Line& l,unsigned rate,unsigned us){l.length=unsigned((uint64_t(rate)*us)/1000000);if(!l.length)l.length=1;l.data=new float[l.length]();}
 static float comb(Line& l,float input,float feedback,float damping){float output=l.age>=l.length?l.data[l.pos]:0;if(l.age<l.length)++l.age;l.filtered=output*(1-damping)+l.filtered*damping;l.data[l.pos]=input+l.filtered*feedback;if(++l.pos==l.length)l.pos=0;return output;}
 static float diffuse(Line& l,float input){float delayed=l.age>=l.length?l.data[l.pos]:0;if(l.age<l.length)++l.age;float output=delayed-input*0.5f;l.data[l.pos]=input+delayed*0.5f;if(++l.pos==l.length)l.pos=0;return output;}
public:
 explicit StereoReverb(unsigned rate):freeze_alpha_(1-expf(-1.0f/(float(rate?rate:48000)*0.02f))){const unsigned comb_us[]={29700,37100,41100,43700},diff_us[]={5000,1700};for(unsigned c=0;c<2;++c){for(unsigned i=0;i<4;++i)init(comb_[c][i],rate,comb_us[i]+c*700);for(unsigned i=0;i<2;++i)init(diffuser_[c][i],rate,diff_us[i]+c*200);}}
 ~StereoReverb(){for(auto& channel:comb_)for(auto& line:channel)delete[] line.data;for(auto& channel:diffuser_)for(auto& line:channel)delete[] line.data;}
 void reset(){freeze_=0;for(auto& channel:comb_)for(auto& line:channel){line.pos=line.age=0;line.filtered=0;}for(auto& channel:diffuser_)for(auto& line:channel){line.pos=line.age=0;line.filtered=0;}}
 void process(float l,float r,float decay,float damping,float& out_l,float& out_r,bool freeze=false){float feedback=0.2f+decay*0.75f,damp=damping*0.9f;float inputs[]={l,r},out[2]{};if(freeze || freeze_>0.000001f){freeze_+=((freeze?1.0f:0.0f)-freeze_)*freeze_alpha_;if(freeze && freeze_>0.99999f)freeze_=1;feedback+=(0.9995f-feedback)*freeze_;damp*=1-freeze_;inputs[0]*=1-freeze_;inputs[1]*=1-freeze_;}else freeze_=0;for(unsigned c=0;c<2;++c){for(auto& line:comb_[c])out[c]+=comb(line,inputs[c]*0.25f,feedback,damp);for(auto& line:diffuser_[c])out[c]=diffuse(line,out[c]);}out_l=out[0];out_r=out[1];}
 StereoReverb(const StereoReverb&)=delete;StereoReverb& operator=(const StereoReverb&)=delete;
private:float freeze_alpha_,freeze_=0;
};
// Isolated performance returns: ordinary track sends never feed these lines.
// Delay and reverb both receive the same pre-throw input, so neither can
// accidentally feed the other. Idle processors are reset once, without I/O.
class PerformanceThrows {
public:
 explicit PerformanceThrows(unsigned rate):delay_(rate),reverb_(rate),freeze_prime_(rate/10?rate/10:1){}
 void reset(){delay_.reset();reverb_.reset();delay_active_=reverb_active_=false;freeze_frames_=0;}
 void process(float& l,float& r,float delay_send,float reverb_send,float delay_return,float reverb_return,float feedback,unsigned ms,float decay,float damping,bool freeze=false){
  const float in_l=l,in_r=r;float wet_l=0,wet_r=0;
  if(delay_send>0.000001f || delay_return>0.000001f){if(!delay_active_){delay_.reset();delay_active_=true;}delay_.process(in_l*delay_send,in_r*delay_send,feedback,ms,wet_l,wet_r);l+=wet_l*delay_return;r+=wet_r*delay_return;}
  else if(delay_active_){delay_.reset();delay_active_=false;}
  if(freeze){if(freeze_frames_<freeze_prime_)++freeze_frames_;}else freeze_frames_=0;
  if(reverb_send>0.000001f || reverb_return>0.000001f){if(!reverb_active_){reverb_.reset();reverb_active_=true;}reverb_.process(in_l*reverb_send,in_r*reverb_send,decay,damping,wet_l,wet_r,freeze && freeze_frames_>=freeze_prime_);l+=wet_l*reverb_return;r+=wet_r*reverb_return;}
  else if(reverb_active_){reverb_.reset();reverb_active_=false;}
 }
private:StereoDelay delay_;StereoReverb reverb_;unsigned freeze_prime_,freeze_frames_=0;bool delay_active_=false,reverb_active_=false;
};
// Instantaneous linked sample-peak protection; no lookahead or true-peak claim.
// Both channels share gain, preserving stereo balance. Release is audio-owned.
class StereoSafetyLimiter {
public:
 explicit StereoSafetyLimiter(unsigned rate=48000):release_(10.0f/float(rate?rate:48000)){}
 float process(float& left,float& right,bool& invalid){
  if(!isfinite(left)){left=0;invalid=true;}if(!isfinite(right)){right=0;invalid=true;}
  const float ceiling=0.98f;float peak=__builtin_fmaxf(__builtin_fabsf(left),__builtin_fabsf(right));float target=peak>ceiling?ceiling/peak:1.0f;
  if(target<gain_)gain_=target;else{gain_+=(1.0f-gain_)*release_;if(gain_>target)gain_=target;}
  left*=gain_;right*=gain_;return gain_;
 }
private:float gain_=1,release_;
};
// Linked stereo peak compressor. All state is fixed-size and audio-owned.
// Coefficients and gain curve are updated at a bounded control rate.
class StereoCompressor {
public:
 explicit StereoCompressor(unsigned rate=48000):rate_(rate){}
 void init(unsigned rate){rate_=rate;reset();}
 void reset(){envelope_=0;gain_=target_=1;attack_=release_=0;makeup_=1;counter_=0;cached_attack_=cached_release_=cached_makeup_=-1;}
 void process(float& l,float& r,float key,float threshold,float ratio,float attack_ms,float release_ms,float makeup_db,float mix){
  if(mix<=0.000001f){reset();return;}
  if(!counter_){if(attack_ms!=cached_attack_){attack_=1-expf(-1/(float(rate_)*attack_ms*0.001f));cached_attack_=attack_ms;}if(release_ms!=cached_release_){release_=1-expf(-1/(float(rate_)*release_ms*0.001f));cached_release_=release_ms;}if(makeup_db!=cached_makeup_){makeup_=powf(10,makeup_db/20);cached_makeup_=makeup_db;}target_=envelope_>threshold?powf(threshold/envelope_,1-1/ratio):1;counter_=16;}--counter_;
  float magnitude=key>=0?key:fmaxf(__builtin_fabsf(l),__builtin_fabsf(r));envelope_+=(magnitude-envelope_)*(magnitude>envelope_?attack_:release_);gain_+=(target_-gain_)*0.05f;float wet=gain_*makeup_;float combined=1+mix*(wet-1);l*=combined;r*=combined;
 }
 float reduction()const{return 1-gain_;}
 float detector()const{return envelope_;}
private: unsigned rate_,counter_=0;float envelope_=0,gain_=1,target_=1,attack_=0,release_=0,makeup_=1,cached_attack_=-1,cached_release_=-1,cached_makeup_=-1;
};
// One audio owner. Copying captured data must happen on that owner or after
// pausing the backend. The ring never exposes a concurrently mutable pointer.
class MasterCapture {
public:
 explicit MasterCapture(unsigned frames):capacity_(frames),data_(new float[size_t(frames)*2]()) {}
 ~MasterCapture() { delete[] data_; }
 void push(float l,float r) { data_[write_*2]=l; data_[write_*2+1]=r; if(++write_>=capacity_)write_=0; if(used_<capacity_) ++used_; }
 unsigned copy_recent(float* out,unsigned frames) const {
  if(!out) return 0;
  if(frames>used_) frames=used_;
  unsigned start=write_+capacity_-frames;if(start>=capacity_)start-=capacity_;
  for(unsigned i=0;i<frames;++i) { unsigned p=start+i;if(p>=capacity_)p-=capacity_; out[i*2]=data_[p*2]; out[i*2+1]=data_[p*2+1]; } return frames;
 }
 void read_back(unsigned offset,float& l,float& r) const { if(offset>=used_) { l=r=0; return; } unsigned p=write_+capacity_-1-offset;if(p>=capacity_)p-=capacity_; l=data_[p*2]; r=data_[p*2+1]; }
 void reset() { write_=used_=0; }
 unsigned available() const { return used_; }
 MasterCapture(const MasterCapture&)=delete; MasterCapture& operator=(const MasterCapture&)=delete;
private: unsigned capacity_,write_=0,used_=0; float* data_;
};
// Freeze the existing history while repeating a selected beat subdivision.
// Reading the same fixed offset from a moving ring would only delay the audio.
class BeatRepeat {
public:
 void reset(){length_=phase_=0;wet_=0;}
 bool process(float& l,float& r,const MasterCapture& history,unsigned rate,unsigned bpm,bool active,unsigned subdivision=4){
  if(subdivision<1 || subdivision>16)subdivision=4;
  if(active && !length_){unsigned n=unsigned(uint64_t(rate)*60/(unsigned(bpm?bpm:120)*subdivision));if(n>=64 && history.available()>=n){length_=n;phase_=0;}}
  if(!length_)return false;
  float a=0,b=0;history.read_back(length_-1-phase_,a,b);
  const unsigned fade=32;float edge=phase_<fade?float(phase_)/fade:phase_>=length_-fade?float(length_-1-phase_)/fade:1.0f;a*=edge;b*=edge;
  wet_+=((active?1.0f:0.0f)-wet_)*0.01f;l+=(a-l)*wet_;r+=(b-r)*wet_;
  if(++phase_==length_)phase_=0;
  if(!active && wet_<0.001f){reset();return false;}
  return true;
 }
private:unsigned length_=0,phase_=0;float wet_=0;
};
enum class PerformanceFx : uint8_t { Off, Filter, Gate, Crush, TapeStop, Reverse, Tremolo, Isolator, Ring, DelayThrow, ReverbThrow, ReverbFreeze, PitchDive, PitchRise };
// One shared performance bus after the mixer. The existing dry history feeds
// time effects; output is never written back to that history, avoiding a
// recursive feedback path. All state is audio-owned and allocated up front.
class PerformanceFxBus {
public:
 explicit PerformanceFxBus(unsigned rate):rate_(rate?rate:48000),bar_increment_(1.0f/(float(rate_)*240.0f)),tail_decay_(expf(-1.0f/float(rate_))),dry_decay_(expf(-1.0f/(float(rate_)*0.01f))){}
 float visual_phase()const{return current_==PerformanceFx::Off?0:phase_;}
 unsigned visual_mode()const{return unsigned(current_);}
 void reset(){current_=target_=PerformanceFx::Off;wet_=amount_=target_amount_=phase_=brake_=read_offset_=0;low_l_=low_r_=high_l_=high_r_=held_l_=held_r_=0;hold_=0;delay_tail_=reverb_tail_=0;}
 void set(PerformanceFx mode,float amount){if(unsigned(mode)>unsigned(PerformanceFx::PitchRise) || !(amount>=0 && amount<=1))return;target_=mode;target_amount_=amount;}
 void set_return_tail(bool tail){return_tail_=tail;}
 float delay_send()const{return current_==PerformanceFx::DelayThrow?wet_*amount_:0;}
 float reverb_send()const{return current_==PerformanceFx::ReverbThrow || current_==PerformanceFx::ReverbFreeze?wet_*amount_:0;}
 bool reverb_freeze()const{return current_==PerformanceFx::ReverbFreeze && target_==current_;}
 float delay_return()const{return delay_tail_;}
 float reverb_return()const{return reverb_tail_;}
 void process(float& l,float& r,const MasterCapture& history,unsigned bpm){
  if(current_!=target_){wet_+=(0-wet_)*0.025f;if(wet_<0.002f){current_=target_;wet_=phase_=brake_=read_offset_=0;hold_=0;}}
  else wet_+=((current_==PerformanceFx::Off?0.0f:1.0f)-wet_)*0.012f;
  amount_+=(target_amount_-amount_)*0.012f;
  float decay=return_tail_?tail_decay_:dry_decay_;
  delay_tail_=current_==PerformanceFx::DelayThrow && target_==current_?__builtin_fmaxf(delay_tail_,wet_*amount_*0.75f):delay_tail_*decay;
  reverb_tail_=(current_==PerformanceFx::ReverbThrow || current_==PerformanceFx::ReverbFreeze) && target_==current_?__builtin_fmaxf(reverb_tail_,wet_*amount_*0.75f):reverb_tail_*decay;
  if(delay_tail_<0.000001f)delay_tail_=0;
  if(reverb_tail_<0.000001f)reverb_tail_=0;
  if(current_==PerformanceFx::Off || wet_<0.0001f)return;
  const float dry_l=l,dry_r=r,a=amount_;float out_l=l,out_r=r;
  switch(current_){
   case PerformanceFx::Filter:{float x=a*2-1;float coefficient=x<0?1+x*x*-0.995f:0.005f+x*x*0.94f;low_l_+=(l-low_l_)*coefficient;low_r_+=(r-low_r_)*coefficient;if(x<0){out_l=low_l_;out_r=low_r_;}else{out_l=l-low_l_;out_r=r-low_r_;if(x<0.04f){out_l=l+(out_l-l)*(x/0.04f);out_r=r+(out_r-r)*(x/0.04f);}}break;}
   case PerformanceFx::Gate:{phase_+=float(bpm)*bar_increment_*float(a>0.65f?32:a>0.3f?16:8);if(phase_>=1)phase_-=1;float duty=0.75f-a*0.6f;float edge=0.02f;float gate=phase_<duty?__builtin_fminf(1,phase_/edge):fmaxf(0,1-(phase_-duty)/edge);out_l=l*gate;out_r=r*gate;break;}
   case PerformanceFx::Crush:{unsigned interval=1+unsigned(a*15);if(!hold_){held_l_=l;held_r_=r;hold_=interval;}--hold_;float steps=float(1u<<unsigned(4+(1-a)*11));out_l=__builtin_floorf(held_l_*steps+0.5f)/steps;out_r=__builtin_floorf(held_r_*steps+0.5f)/steps;break;}
   case PerformanceFx::TapeStop:{brake_+=1.0f/(float(rate_)*(0.15f+0.85f*a));if(brake_>1)brake_=1;float speed=(1-brake_)*(1-brake_);read_offset_+=1-speed;if(history.available()>unsigned(read_offset_)+1)history.read_back(unsigned(read_offset_),out_l,out_r);out_l*=speed;out_r*=speed;break;}
   case PerformanceFx::Reverse:{unsigned window=unsigned(float(rate_)*60.0f/float(bpm?bpm:120));if(window>history.available())window=history.available();if(window>128){read_offset_+=2;if(read_offset_>=window)read_offset_-=window;history.read_back(unsigned(read_offset_),out_l,out_r);if(read_offset_>window-64){float a2=(read_offset_-(window-64))/64.0f,other_l=0,other_r=0;history.read_back(unsigned(read_offset_-(window-64)),other_l,other_r);out_l=out_l*(1-a2)+other_l*a2;out_r=out_r*(1-a2)+other_r*a2;}}break;}
   case PerformanceFx::PitchDive:
   case PerformanceFx::PitchRise:{
    // Two overlap-added, linearly interpolated reads from the existing dry
    // history. The windows wrap at zero gain, avoiding hard splice clicks.
    const float window=float(rate_)*0.043f;
    if(history.available()>unsigned(window)+3){
     float speed=current_==PerformanceFx::PitchDive?1.0f-0.5f*a:1.0f+a;
     phase_+=(1.0f-speed)/window;if(phase_>=1)phase_-=1;if(phase_<0)phase_+=1;
     float q=phase_+0.5f;if(q>=1)q-=1;
     float first=1.0f-__builtin_fabsf(phase_*2.0f-1.0f),second=1.0f-first;
     float offsets[]={1.0f+phase_*window,1.0f+q*window};float sum_l=0,sum_r=0;
     for(unsigned i=0;i<2;++i){unsigned n=unsigned(offsets[i]);float f=offsets[i]-float(n),l0,r0,l1,r1;history.read_back(n,l0,r0);history.read_back(n+1,l1,r1);float weight=i?second:first;sum_l+=(l0+(l1-l0)*f)*weight;sum_r+=(r0+(r1-r0)*f)*weight;}
     out_l=l+(sum_l-l)*a;out_r=r+(sum_r-r)*a;
    }break;
   }
   case PerformanceFx::Tremolo:{phase_+=(2+14*a)/float(rate_);if(phase_>=1)phase_-=1;float gain=1-a*(0.5f+0.5f*sinf(phase_*6.2831853f));out_l=l*gain;out_r=r*gain;break;}
   case PerformanceFx::Isolator:{low_l_+=(l-low_l_)*0.025f;low_r_+=(r-low_r_)*0.025f;high_l_+=(l-high_l_)*0.28f;high_r_+=(r-high_r_)*0.28f;out_l=low_l_+(l-high_l_)+(high_l_-low_l_)*(1-a);out_r=low_r_+(r-high_r_)+(high_r_-low_r_)*(1-a);break;}
   case PerformanceFx::Ring:{phase_+=(60+640*a)/float(rate_);if(phase_>=1)phase_-=1;float carrier=sinf(phase_*6.2831853f);out_l=l*carrier;out_r=r*carrier;break;}
   default:break;
  }
  if(!isfinite(out_l))out_l=0;
  if(!isfinite(out_r))out_r=0;
  out_l=__builtin_fmaxf(-2,__builtin_fminf(2,out_l));out_r=__builtin_fmaxf(-2,__builtin_fminf(2,out_r));
  l=dry_l+(out_l-dry_l)*wet_;r=dry_r+(out_r-dry_r)*wet_;
 }
private:
 unsigned rate_;float bar_increment_,tail_decay_,dry_decay_;PerformanceFx current_=PerformanceFx::Off,target_=PerformanceFx::Off;bool return_tail_=true;
 float wet_=0,amount_=0,target_amount_=0,phase_=0,brake_=0,read_offset_=0;
 float low_l_=0,low_r_=0,high_l_=0,high_r_=0,held_l_=0,held_r_=0,delay_tail_=0,reverb_tail_=0;unsigned hold_=0;
};
inline float soft_clip(float x,float drive) { if(drive<=0) return x; float g=1+drive*15; x*=g; return (x/(1+__builtin_fabsf(x)))*(1+g)/g; }
}
