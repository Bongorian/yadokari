#include "texture_dsp.h"
#include "alloc_policy.h"
#include "clouds/dsp/granular_processor.h"
#include "stmlib/utils/random.h"
#include <string.h>
namespace groove {
struct TextureDsp::Impl {
 clouds::GranularProcessor processor;
 alignas(16) uint8_t large[kLargeBytes],small[kSmallBytes];
 clouds::ShortFrame input[32],output[32];
};
unsigned TextureDsp::allocation_bytes(){return sizeof(Impl);}
TextureDsp::TextureDsp():impl_(alloc_new<Impl>()){}
TextureDsp::~TextureDsp(){delete impl_;}
void TextureDsp::init(uint32_t seed){
 if(!impl_)return;
 memset(static_cast<void*>(impl_),0,sizeof *impl_);random_=seed;freeze_=trigger_=false;
 uint32_t saved=stmlib::Random::state();stmlib::Random::Seed(random_);
 impl_->processor.Init(impl_->large,kLargeBytes,impl_->small,kSmallBytes);
 random_=stmlib::Random::state();stmlib::Random::Seed(saved);
}
bool TextureDsp::frozen()const{return impl_ && impl_->processor.frozen();}
void TextureDsp::process(const float* p,const TextureFrame* input,TextureFrame* output,bool strum){
 if(!impl_){memset(output,0,32*sizeof *output);return;}
 uint32_t saved=stmlib::Random::state();stmlib::Random::Seed(random_);
 auto& dsp=impl_->processor;dsp.set_playback_mode(clouds::PlaybackMode(unsigned(p[0])));dsp.set_quality(int(p[11]));
 // One deterministic Prepare per audio quantum, on the same DSP owner.
 // Prepare intentionally clears Freeze on buffer/mode reset as upstream does.
 auto& q=*dsp.mutable_parameters();q.position=p[1];q.size=p[2];q.pitch=p[3];q.density=p[4];q.texture=p[5];q.dry_wet=p[6];q.stereo_spread=p[7];q.feedback=p[8];q.reverb=p[9];
 bool freeze=p[10]>=.5f,trigger=p[13]>=.5f;
 if(freeze!=freeze_)dsp.set_freeze(freeze);
 freeze_=freeze;
 q.trigger=strum || (trigger && !trigger_);trigger_=trigger;q.gate=p[14]>=.5f;
 dsp.Prepare();
 for(unsigned i=0;i<32;++i)impl_->input[i]={input[i].l,input[i].r};
 dsp.Process(impl_->input,impl_->output,32);
 for(unsigned i=0;i<32;++i)output[i]={impl_->output[i].l,impl_->output[i].r};
 random_=stmlib::Random::state();stmlib::Random::Seed(saved);
}
}
