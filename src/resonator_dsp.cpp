#include "resonator_dsp.h"
#include "alloc_policy.h"
#include "rings/dsp/part.h"
#include "stmlib/utils/random.h"
#include <string.h>
namespace groove {
unsigned ResonatorDsp::allocation_bytes(){return sizeof(rings::Part);}
ResonatorDsp::ResonatorDsp():part_(alloc_new<rings::Part>()){}
ResonatorDsp::~ResonatorDsp(){delete part_;}
void ResonatorDsp::init(uint32_t seed){
 if(!part_)return;
 // Upstream uses static zero-initialized storage, including before Init.
 memset(static_cast<void*>(part_),0,sizeof *part_);memset(reverb_,0,sizeof reverb_);
 uint32_t saved=stmlib::Random::state();stmlib::Random::Seed(seed);
 part_->Init(reverb_);random_=stmlib::Random::state();stmlib::Random::Seed(saved);
 model_=0;polyphony_=1;
}
void ResonatorDsp::process(unsigned model,unsigned polyphony,const rings::Patch& patch,
 const rings::PerformanceState& performance,const float* input,float* out,float* aux){
 if(!part_){memset(out,0,kBlock*sizeof(float));memset(aux,0,kBlock*sizeof(float));return;}
 uint32_t saved=stmlib::Random::state();stmlib::Random::Seed(random_);
 if(model!=model_){part_->set_model(rings::ResonatorModel(model));model_=model;}
 if(polyphony!=polyphony_){part_->set_polyphony(int(polyphony));polyphony_=polyphony;}
 part_->Process(performance,patch,input,out,aux,kBlock);
 random_=stmlib::Random::state();stmlib::Random::Seed(saved);
}
}
