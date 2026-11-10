#pragma once
#include <math.h>
#include "creative_state.h"
#include "instrument_types.h"
#include "wav_sampler.h"
#include "Effects/wavefolder.h"
namespace groove {
// Original bounded DSP. No vendor PCM, circuit/ROM emulation or audio-time allocation.
class CreativeVoice {
 static constexpr unsigned N=256;
 float phase_=0,mod_=0,gate_=0,low_=0,previous_=0,noise_phase_=0;daisysp::Wavefolder folder_;uint32_t noise_=0x7319ab35;
 uint64_t age_=0,step_age_=0,step_frames_=1;unsigned step_=0,slot_=0,old_slot_=0,kind_=0;const uint8_t* reel_source_=nullptr;ISampleReader* reel_reader_=nullptr;uint32_t reel_rate_=0,reel_frames_=0;uint16_t reel_channels_=0,reel_bits_=0,reel_format_=0;float read_=0,old_read_=0,blend_=1,old_speed_=1,speed_=1;
 float reel_[4096]{};unsigned write_=0;bool initialized_=false;
 float real_[N]{},imag_[N]{},magnitude_[N/2]{},phasor_re_[N/2]{},phasor_im_[N/2]{},temp_re_[N/2]{},temp_im_[N/2]{},ola_[N*2]{};unsigned fft_write_=0,ola_read_=0;bool frozen_=false,frame_pending_=false;float freeze_mix_=0;
 float clock_p0_=-1,ratio_=1,ratio_exp_=1e30f,decay_=1,decay_step_=1,decay_p3_=-2;unsigned clock_age_=0,ratio_age_=0,decay_age_=0;
 // Hann window for the spectral frames, built once with the same
 // cosf evaluations the per-frame code used to repeat 512 times.
 static const float* hann(){static float table[N];static bool built=false;if(!built){for(unsigned i=0;i<N;++i)table[i]=0.5f-0.5f*cosf(6.283185307f*i/N);built=true;}return table;}
 static float wrap(float x,float size){x=fmodf(x,size);return x<0?x+size:x;}
 static void fft(float* re,float* im,bool inverse){for(unsigned i=1,j=0;i<N;++i){unsigned bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){float v=re[i];re[i]=re[j];re[j]=v;v=im[i];im[i]=im[j];im[j]=v;}}for(unsigned len=2;len<=N;len<<=1){float a=(inverse?6.283185307f:-6.283185307f)/len,wr=cosf(a),wi=sinf(a);for(unsigned base=0;base<N;base+=len){float ur=1,ui=0;for(unsigned j=0;j<len/2;++j){unsigned x=base+j,y=x+len/2;float tr=ur*re[y]-ui*im[y],ti=ur*im[y]+ui*re[y],r=re[x],v=im[x];re[x]=r+tr;im[x]=v+ti;re[y]=r-tr;im[y]=v-ti;float next=ur*wr-ui*wi;ui=ur*wi+ui*wr;ur=next;}}}if(inverse)for(unsigned i=0;i<N;++i){re[i]/=N;im[i]/=N;}}
 static float pcm(WavView s,float pos){if((!s.data && !s.reader) || !s.frames)return 0;pos=wrap(pos,float(s.frames));unsigned a=unsigned(pos),b=(a+1)%s.frames;float f=pos-a;float v=0;for(unsigned ch=0;ch<s.channels;++ch){float x=wav_sample(s,a,ch),y=wav_sample(s,b,ch);v+=x+(y-x)*f;}return v/s.channels;}
 static float wave(unsigned shape,float p){p-=floorf(p);switch(shape%4){case 0:return sinf(p*6.283185307f);case 1:return p*2-1;case 2:return p<0.5f?1:-1;default:return 1-4*fabsf(p-0.5f);}}
 uint32_t random(){noise_^=noise_<<13;noise_^=noise_>>17;noise_^=noise_<<5;return noise_;}
 // O-05: one spectral frame used to run entirely inside a single
 // sample (two 256-point FFTs plus ~127 atan2/powf/trig triples).
 // It is now split across the 64-sample hop - analysis at the hop
 // boundary, synthesis 32 samples later - halving the spike, and
 // the bin phase is kept as a unit phasor instead of an angle:
 // the hold rotation per frame is exactly k*90 degrees, so it is
 // exact sign/swap arithmetic, and atan2f/cosf/sinf leave the bin
 // loop entirely. The shift multiplier is frame-invariant and
 // hoisted; the Hann window comes from a table.
 void spectral_analyze(const float* p){const float* w=hann();for(unsigned i=0;i<N;++i){real_[i]=reel_[(fft_write_+4096-N+i)%4096]*w[i];imag_[i]=0;}fft(real_,imag_,false);bool hold=p[0]>=0.5f;float tilt=(p[2]-0.5f)*2,shift=(p[1]-0.5f)*24;float shift_mult=powf(2,shift/12);for(unsigned i=0;i<N/2;++i){temp_re_[i]=0;temp_im_[i]=0;}for(unsigned k=1;k<N/2;++k){if(!hold || !frozen_){float mag=sqrtf(real_[k]*real_[k]+imag_[k]*imag_[k]);magnitude_[k]=mag;if(mag>0){phasor_re_[k]=real_[k]/mag;phasor_im_[k]=imag_[k]/mag;}else{phasor_re_[k]=1;phasor_im_[k]=0;}}else switch(k&3){case 1:{float pr=phasor_re_[k];phasor_re_[k]=-phasor_im_[k];phasor_im_[k]=pr;break;}case 2:phasor_re_[k]=-phasor_re_[k];phasor_im_[k]=-phasor_im_[k];break;case 3:{float pr=phasor_re_[k];phasor_re_[k]=phasor_im_[k];phasor_im_[k]=-pr;break;}default:break;}float mag=k<=unsigned(4+p[4]*123)?magnitude_[k]*powf(float(k)/16,tilt):0;int dest=int(float(k)*shift_mult);if(dest>0 && dest<int(N/2)){temp_re_[dest]+=mag*phasor_re_[k];temp_im_[dest]+=mag*phasor_im_[k];}}for(unsigned i=0;i<16;++i){unsigned k=1+i*7;spectrum[i]=sqrtf(temp_re_[k]*temp_re_[k]+temp_im_[k]*temp_im_[k])/N;}frozen_=hold;frame_pending_=true;}
 void spectral_synthesize(){const float* w=hann();real_[0]=imag_[0]=real_[N/2]=imag_[N/2]=0;for(unsigned k=1;k<N/2;++k){real_[k]=temp_re_[k];imag_[k]=temp_im_[k];real_[N-k]=real_[k];imag_[N-k]=-imag_[k];}fft(real_,imag_,true);for(unsigned i=0;i<N;++i)ola_[(ola_read_+i)%(N*2)]+=real_[i]*w[i]/1.5f;frame_pending_=false;}
public:
 void reset(){__builtin_memset(static_cast<void*>(this),0,sizeof(*this));noise_=0x7319ab35;step_frames_=1;blend_=1;divider=1;folder_.Init();}
 uint8_t lanes[6]{};float second_position=0,position=0,clock=0,divider=1,grain=0;float spectrum[16]{};
 void trigger(const CreativeState& c,unsigned kind){if(kind_!=kind){initialized_=false;kind_=kind;}age_=step_age_=0;step_frames_=1;step_=0;noise_=c.seed;phase_=mod_=0;gate_=0;blend_=1,old_speed_=1,speed_=1;for(auto& l:lanes)l=0;clock_age_=16;ratio_age_=16;decay_age_=64;frame_pending_=false;}
 void trigger_leaf(const CreativeState& c,unsigned kind){initialized_=false;write_=slot_=old_slot_=0;reel_source_=nullptr;read_=old_read_=0;low_=previous_=noise_phase_=0;frozen_=false;freeze_mix_=0;fft_write_=ola_read_=0;for(float& x:ola_)x=0;trigger(c,kind);}
 float next(unsigned kind,float hz,float rate,float bpm,const float* p,const CreativeState& c,const WavView* assets,unsigned sample_slot,bool released,float input){
  float x=0;hz=hz<rate*0.4f?hz:rate*0.4f;
  if(kind==unsigned(Instrument::WaveSequence)){ // Sample/Pitch/Timing/Gate/Shape/Mod, with independent loop lengths.
   if(!age_ || step_age_>=step_frames_){if(age_)++step_;step_age_=0;for(unsigned l=0;l<6;++l){lanes[l]=uint8_t(step_%c.length[l]);if(c.mutation && random()%100<c.mutation)lanes[l]=uint8_t(random()%c.length[l]);}old_speed_=speed_;old_read_=read_;old_slot_=slot_;slot_=(sample_slot+c.value[0][lanes[0]])%64;read_=0;blend_=0;step_frames_=uint64_t(rate*60/(bpm*96)*(6+c.value[2][lanes[2]])*(0.5f+p[2]*1.5f));if(!step_frames_)step_frames_=1;}
   unsigned slot=(sample_slot+c.value[0][lanes[0]])%64;WavView s=assets[slot];float exponent=(float(c.value[1][lanes[1]])-48+(p[1]-0.5f)*24)/12;++ratio_age_;if(ratio_age_>=16 || fabsf(exponent-ratio_exp_)>0.004f){ratio_=powf(2,exponent);ratio_exp_=exponent;ratio_age_=0;}float ratio=ratio_;float speed=hz/261.625565f*ratio*((s.data || s.reader)?float(s.rate)/rate:1);phase_=wrap(phase_+hz*ratio/rate,1);float source=(s.data || s.reader)?pcm(s,read_):wave(c.value[0][lanes[0]],phase_);WavView old_view=assets[old_slot_];float old=(old_view.data || old_view.reader)?pcm(old_view,old_read_):previous_;speed_=speed;read_+=speed_;old_read_+=old_speed_;blend_+=1/(1+p[0]*rate*0.08f);if(blend_>1)blend_=1,old_speed_=1,speed_=1;previous_=source;x=old+(source-old)*blend_;float phase=float(step_age_)/step_frames_,duty=float(c.value[3][lanes[3]]+1)/128;float shape=(float(c.value[4][lanes[4]])/127+p[3])*0.5f;float contour=phase<duty?powf(1-phase/duty,0.2f+shape*3):0;float target=contour*(0.4f+float(c.value[5][lanes[5]])/127*p[4]*1.2f);gate_+=(target-gate_)*0.03f;x*=gate_;++step_age_;position=phase;
  }else if(kind==unsigned(Instrument::ChipClock)){ // SN-inspired one-tone + noise laboratory, not a register-exact chip emulator.
   ++clock_age_;if(clock_age_>=16 || fabsf(p[0]-clock_p0_)>0.02f){clock=500000*powf(16,p[0]);clock_p0_=p[0];clock_age_=0;}bool follow=p[1]>=0.5f;divider=follow?floorf(clock/(32*hz)+0.5f):1+floorf(p[2]*1022);if(divider<1)divider=1;if(divider>1023)divider=1023;float tone=clock/(32*divider);unsigned arp=unsigned(p[4]*3.99f);if(arp){unsigned n=unsigned(age_/unsigned(rate*60/bpm/4))%3;const int steps[]={0,4,7};{static const float arp_mult[13]={1,1,1,1,1.25992105f,1,1,1.49830708f,1,1,1,1,1};tone*=arp_mult[steps[(n+arp-1)%3]];}}tone=tone<rate*0.4f?tone:rate*0.4f;phase_=wrap(phase_+tone/rate,1);noise_phase_+=clock/(32*16*rate);unsigned budget=0;while(noise_phase_>=1 && budget++<32){noise_phase_-=1;uint32_t bit=((noise_>>0)^(noise_>>3))&1;noise_=(noise_>>1)|(bit<<14);if(!noise_)noise_=1;}float sq=phase_<0.5f?1:-1,n=(noise_&1)?1:-1;x=sq*(1-p[3])+n*p[3];
  }else if(kind==unsigned(Instrument::TapeMicrosound)){ // Two overlapping windowed genes from immutable PCM; bounded internal overdub reel.
   WavView s=assets[sample_slot];if(!initialized_ || reel_source_!=s.data || reel_reader_!=s.reader || reel_rate_!=s.rate || reel_frames_!=s.frames || reel_channels_!=s.channels || reel_bits_!=s.bits || reel_format_!=s.format){reel_source_=s.data;reel_reader_=s.reader;reel_rate_=s.rate;reel_frames_=s.frames;reel_channels_=s.channels;reel_bits_=s.bits;reel_format_=s.format;for(unsigned i=0;i<4096;++i)reel_[i]=(s.data || s.reader)?pcm(s,float(i)*s.rate/rate):wave(0,float(i)*hz/rate);initialized_=true;}
   float speed=(p[0]-0.5f)*4*hz/261.625565f,size=32+floorf(p[1]*1984),start=p[2]*(4096-size);phase_=wrap(phase_+fabsf(speed),size);float a=phase_,b=wrap(a+size*0.5f,size);float pa=start+(speed<0?wrap(size-1-a,size):a),pb=start+(speed<0?wrap(size-1-b,size):b);float wa=0.5f-0.5f*cosf(6.283185307f*a/size),wb=0.5f-0.5f*cosf(6.283185307f*b/size);float va=reel_[unsigned(pa)%4096],vb=reel_[unsigned(pb)%4096];x=(va*wa+vb*wb*p[3])/(wa+wb*p[3]+0.001f);if(p[4]>0.001f){reel_[write_]=tanhf(reel_[write_]*0.995f+input*p[4]*0.5f);write_=(write_+1)%4096;}position=pa/4096;second_position=pb/4096;grain=size/4096;
  }else if(kind==unsigned(Instrument::WestCoast)){ // Phase-modulated complex oscillator -> periodic wavefolder -> LPG.
   phase_=wrap(phase_+hz/rate,1);float ratio=0.25f+floorf(p[2]*31)/4;mod_=wrap(mod_+hz*ratio/rate,1);float source=sinf(phase_*6.283185307f+sinf(mod_*6.283185307f)*p[1]*8);float drive=1+p[0]*7;folder_.SetGain(drive);float folded=folder_.Process(source);if(age_==0){decay_=1;decay_p3_=-2;}++decay_age_;if(decay_age_>=64 || fabsf(p[3]-decay_p3_)>0.01f){decay_=expf(-float(age_)/(rate*(0.015f+p[3]*1.8f)));decay_step_=expf(-1.0f/(rate*(0.015f+p[3]*1.8f)));decay_p3_=p[3];decay_age_=0;}else decay_*=decay_step_;float decay=decay_;gate_+=((released?0:decay)-gate_)*0.01f;float coef=0.003f+gate_*(0.1f+p[4]*0.7f);low_+=(folded-low_)*coef;x=low_*gate_;position=gate_;
  }else if(kind==unsigned(Instrument::SpectralFreeze)){ // 256-point STFT; source PCM (or built-in harmonic source), hop64, Hann OLA.
   WavView s=assets[sample_slot];phase_=wrap(phase_+hz/rate,1);float source=(s.data || s.reader)?pcm(s,float(age_)*hz/261.625565f*s.rate/rate):sinf(6.283185307f*phase_)+0.3f*sinf(6.283185307f*phase_*3);reel_[fft_write_]=source;fft_write_=(fft_write_+1)%4096;if(age_%64==0 && age_>=N)spectral_analyze(p);else if(age_%64==32 && frame_pending_)spectral_synthesize();float wet=ola_[ola_read_];ola_[ola_read_]=0;ola_read_=(ola_read_+1)%(N*2);freeze_mix_+=(p[3]-freeze_mix_)*0.005f;x=source*(1-freeze_mix_)+wet*freeze_mix_;position=frozen_?1:0;
  }
  ++age_;return tanhf(x)*0.16f;
 }
};
}
