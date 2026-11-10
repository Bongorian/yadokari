#pragma once
#include "spectral_voices.h"
#include "granular_state.h"
#include "texture_voice.h"
#include "granular_dsp.h"
#include "wav_sampler.h"
namespace groove {
// Four instances are allocated at Engine construction, not one per possible
// generic voice. The resource allocator owns each independently for its note.
class GranularVoice:public SpectralEnvelope {
 GranularDsp dsp_;WavVoice source_;TextureRateConverter<3,2> up_;
 TextureFrame input_[32]{},output_[32]{};TextureStereo native_[32]{},audio_[48]{};
 float values_[kGranularParameterCount]{},root_hz_=261.625565f,jitter_=0;unsigned position_=0,blocks_=0;uint32_t random_=1;bool first_=true;
 static int16_t pcm(float x){return int16_t(fmaxf(-32768,fminf(32767,x*32768)));}
public:
 bool ready()const{return dsp_.ready();}
 void init(unsigned rate){rate_=float(rate);envelope_.Init(rate_);dsp_.init(1);up_.reset();active_=false;}
 void parameters(const float* p){for(unsigned i=0;i<kGranularParameterCount;++i)values_[i]=p[i];envelope_parameters(p);}
 bool trigger(WavView sample,uint8_t root,float velocity,const float* p,uint32_t seed){if(!ready() || !sample.frames)return false;dsp_.init(seed);up_.reset();envelope_.Init(rate_);parameters(p);velocity_=velocity;root_hz_=440*powf(2,(float(root)-69)/12);random_=seed?seed:1;position_=blocks_=0;jitter_=0;first_=true;for(auto& f:audio_)f={};active_=source_.start(sample,32000,1,true);return active_;}
 void next(float hz,bool gate,float& l,float& r){l=r=0;if(!active_ || !envelope_next(gate))return;
  l=audio_[position_].l*level_*velocity_*.5f;r=audio_[position_].r*level_*velocity_*.5f;
  if(++position_==48){for(unsigned i=0;i<32;++i){float a,b;source_.next(a,b);input_[i]={pcm(a),pcm(b)};}
   random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;float target=(float(int32_t(random_))/2147483648.f)*values_[9]*.15f;jitter_+=(target-jitter_)*.2f;
   // Native Clouds granular mode, 32k stereo. Keep the existing TEXTURE
   // wrapper and its source untouched; no feedback/reverb or dry leakage.
   float p[15]={0,fmaxf(0,fminf(1,values_[0]+jitter_)),values_[1],fmaxf(-48,fminf(48,12*log2f(fmaxf(.001f,hz)/root_hz_)+values_[11])),values_[2],values_[3],1,values_[4],0,0,values_[10]>.5f && blocks_>=256?1.f:0.f,0,0,0,gate?1.f:0.f};
   dsp_.process(p,input_,output_,first_);first_=false;++blocks_;for(unsigned i=0;i<32;++i)native_[i]={output_[i].l/32768.f,output_[i].r/32768.f};up_.block(native_,32,audio_);position_=0;
  }
 }
};
}
