#pragma once
#include <stdint.h>
#include "rings/dsp/patch.h"
#include "rings/dsp/performance_state.h"
namespace rings {class Part;}
namespace groove {
// Keep upstream resource macros in a separate translation unit from Plaits.
// Construction/destruction is control-time only; init/process never allocate.
class ResonatorDsp {
public:
 static constexpr unsigned kBlock=24;
 static unsigned allocation_bytes();
 ResonatorDsp();~ResonatorDsp();
 ResonatorDsp(const ResonatorDsp&)=delete;ResonatorDsp& operator=(const ResonatorDsp&)=delete;
 bool ready()const{return part_!=nullptr;}
 void init(uint32_t seed=1);
 void process(unsigned model,unsigned polyphony,const rings::Patch& patch,
              const rings::PerformanceState& performance,const float* input,float* out,float* aux);
private:
 rings::Part* part_=nullptr;uint16_t reverb_[32768]{};
 uint32_t random_=1;unsigned model_=0,polyphony_=1;
};
}
