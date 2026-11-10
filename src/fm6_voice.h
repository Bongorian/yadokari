#pragma once
#include "fm6_state.h"
namespace groove {
// Fixed storage: all vendor tables are initialized before the DSP owner runs.
class Fm6Voice {
 alignas(16) unsigned char storage_[2048]{};
public:
 Fm6Voice();~Fm6Voice();
 Fm6Voice(const Fm6Voice&)=delete;Fm6Voice& operator=(const Fm6Voice&)=delete;
 void init(unsigned rate);
 void trigger(float hz,unsigned velocity,const Fm6State& state);
 void parameters(const float* values);
 float next(float hz,bool gate);
 bool active()const;
};
}
