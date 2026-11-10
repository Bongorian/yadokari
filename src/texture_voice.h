#pragma once
#include "texture_dsp.h"
#include "texture_rate_coefficients.h"
#include <math.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
namespace groove {
struct TextureStereo {float l=0,r=0;};
// Fixed polyphase FIR. No dynamic allocation or coefficient generation in audio.
template<unsigned L,unsigned M> class TextureRateConverter {
public:
 void reset(){for(auto& f:history_)f={};head_=0;}
 // Called with the fixed 48/32-frame quanta (multiples of M).
 void block(const TextureStereo* input,unsigned count,TextureStereo* output){
  const unsigned start=head_;
  for(unsigned i=0;i<count;++i){head_=(head_+1)&255;history_[head_]=history_[head_+256]=input[i];}
  unsigned out=0,total=count*L/M;
#if defined(__aarch64__)
  // Independent output pairs hide FMA latency without changing any output's
  // tap order, rounded coefficient or accumulator. The 2:3 phase repeats
  // every two outputs; the 3:2 converter keeps its original pair schedule.
  if constexpr(L==2){for(;out+3<total;out+=4){
   unsigned phase=out*M%L;
   const TextureStereo* a=&history_[256+((start+1+out*M/L)&255)];
   const TextureStereo* b=&history_[256+((start+1+(out+1)*M/L)&255)];
   const TextureStereo* c=&history_[256+((start+1+(out+2)*M/L)&255)];
   const TextureStereo* d=&history_[256+((start+1+(out+3)*M/L)&255)];
   float32x4_t x=vdupq_n_f32(0),y=vdupq_n_f32(0);
   for(unsigned k=0;k<coefficients_.pair_count[phase];++k){
    auto coefficient=vld1q_f32(coefficients_.pair[phase][k]);
    x=vfmaq_f32(x,vcombine_f32(vld1_f32(&a[-int(k)].l),vld1_f32(&b[-int(k)].l)),coefficient);
    y=vfmaq_f32(y,vcombine_f32(vld1_f32(&c[-int(k)].l),vld1_f32(&d[-int(k)].l)),coefficient);
   }
   vst1q_f32(&output[out].l,x);vst1q_f32(&output[out+2].l,y);
  }}
  for(;out+1<total;out+=2){unsigned phase=out*M%L;
   const TextureStereo* a=&history_[256+((start+1+out*M/L)&255)];
   const TextureStereo* b=&history_[256+((start+1+(out+1)*M/L)&255)];
   float32x4_t value=vdupq_n_f32(0);
   for(unsigned k=0;k<coefficients_.pair_count[phase];++k){
    auto samples=vcombine_f32(vld1_f32(&a[-int(k)].l),vld1_f32(&b[-int(k)].l));
    value=vfmaq_f32(value,samples,vld1q_f32(coefficients_.pair[phase][k]));
   }
   vst1q_f32(&output[out].l,value);
  }
#endif
  for(;out<total;++out){unsigned phase=out*M%L;const TextureStereo* recent=&history_[256+((start+1+out*M/L)&255)];TextureStereo value{};
   for(unsigned k=0;k<coefficients_.count[phase];++k){float coefficient=coefficients_.value[phase][k];const auto& in=recent[-int(k)];value.l+=in.l*coefficient;value.r+=in.r*coefficient;}
   output[out]=value;
  }
 }
private:
 // Same tap order and rounded coefficients; contiguous phase tables and mirrored
 // history remove strided coefficient loads and a modulo from every MAC.
 struct Coefficients {float value[L][(193+L-1)/L]{};unsigned count[L]{};float pair[L][(193+L-1)/L][4]{};unsigned pair_count[L]{};
  constexpr Coefficients(){
   for(unsigned phase=0;phase<L;++phase)for(unsigned k=0;phase+k*L<193;++k){value[phase][k]=kTextureRateFilter[phase+k*L]*L;++count[phase];}
   for(unsigned phase=0;phase<L;++phase){unsigned next=(phase+M)%L;pair_count[phase]=count[phase]>count[next]?count[phase]:count[next];for(unsigned k=0;k<pair_count[phase];++k){pair[phase][k][0]=pair[phase][k][1]=value[phase][k];pair[phase][k][2]=pair[phase][k][3]=value[next][k];}}
  }
 };
 inline static constexpr Coefficients coefficients_{};
 TextureStereo history_[512]{};unsigned head_=0;
};
class TextureVoice {
public:
 bool ready()const{return dsp_.ready();}
 void init(uint32_t seed){dsp_.init(seed);down_.reset();up_.reset();position_=0;strum_=false;for(auto& f:output_)f={};pending_=false;}
 // Logical reset is constant-time. Old buffers cannot be read before the next
 // audio-owner use has performed the same seeded init as an eager reset.
 void reset(uint32_t seed){seed_=seed;pending_=true;position_=0;strum_=false;}
 void trigger(){prepare();strum_=true;}
 bool boundary()const{return position_==0;}
 bool frozen()const{return !pending_ && dsp_.frozen();}
 void next(const float* parameters,float in_l,float in_r,float& l,float& r){
  prepare();
  l=output_[position_].l;r=output_[position_].r;input_[position_]={in_l,in_r};
  if(++position_==48){
   down_.block(input_,48,native_);
   for(unsigned i=0;i<32;++i){short_in_[i]={pcm(native_[i].l),pcm(native_[i].r)};}
   dsp_.process(parameters,short_in_,short_out_,strum_);strum_=false;
   for(unsigned i=0;i<32;++i)native_[i]={short_out_[i].l/32768.f,short_out_[i].r/32768.f};
   up_.block(native_,32,output_);position_=0;
  }
 }
private:
 void prepare(){if(pending_)init(seed_);}
 static int16_t pcm(float v){if(!isfinite(v))return 0;return int16_t(fmaxf(-32768,fminf(32767,roundf(v*32768))));}
 TextureDsp dsp_;TextureRateConverter<2,3> down_;TextureRateConverter<3,2> up_;
 TextureStereo input_[48]{},output_[48]{},native_[32]{};TextureFrame short_in_[32]{},short_out_[32]{};
 unsigned position_=0;bool strum_=false,pending_=false;uint32_t seed_=1;
};
}
