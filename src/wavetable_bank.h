#pragma once
#include <stdint.h>
namespace groove {
constexpr unsigned kWavetableWaves=128,kWavetableLevels=7,kWavetableSamples=256;
// Immutable process-lifetime decoded bank. Reserve on the control owner before
// any voice starts; trigger/render never allocate or decode.
extern const int16_t (*kWavetableData)[kWavetableLevels][kWavetableSamples];
bool prepare_wavetable_bank();
inline bool wavetable_bank_ready(){return kWavetableData!=nullptr;}
}
