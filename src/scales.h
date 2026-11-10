#pragma once
#include <stdint.h>
namespace groove {
struct ScalePreset {const char* name;uint16_t mask;};
inline const ScalePreset* scale_presets(){static const ScalePreset presets[]={
 {"Chromatic",0x0fff},{"Major",0x0ab5},{"Minor",0x05ad},{"Harm minor",0x09ad},{"Mel minor",0x0aad},
 {"Dorian",0x06ad},{"Phrygian",0x05ab},{"Lydian",0x0ad5},{"Mixolydian",0x06b5},{"Locrian",0x056b},
 {"Maj pent",0x0295},{"Min pent",0x04a9},{"Blues",0x04e9},{"Whole tone",0x0555},{"Diminished",0x0b6d},{"Harm major",0x09b5},{"Lyd dominant",0x06d5},{"Altered",0x055b},{"Hungarian min",0x09cd},{"In-sen",0x04a3},{"Hirajoshi",0x018d}};return presets;}
constexpr unsigned kScalePresets=21;
inline unsigned scale_index(uint16_t mask){for(unsigned i=0;i<kScalePresets;++i)if(scale_presets()[i].mask==mask)return i;return kScalePresets;}
inline const char* scale_name(unsigned index){return index<kScalePresets?scale_presets()[index].name:"Custom";}
}
