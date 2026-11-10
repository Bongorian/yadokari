#pragma once
#include <stdint.h>
namespace groove {
// Shared channel grammar: mixer, Morph/Gesture and SYNTH TONE.
inline constexpr const char* kChannelNames[]={"LEVEL","PAN","CUTOFF","DRIVE","DELAY SEND","CRUSH","LFO DEPTH","LFO HZ"};
inline constexpr uint16_t channel_parameter(unsigned i){return i<8?uint16_t(i+1):0;}
inline const char* channel_name(unsigned i){return i<8?kChannelNames[i]:"";}
}
