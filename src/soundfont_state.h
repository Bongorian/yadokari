#pragma once
#include "parameters.h"
namespace groove {
inline constexpr ParameterDescriptor kSoundFontParameters[]={
 {0,"BANK",0,16383,0,Interpolation::Discrete,"",0,1,ApplyNextTrigger},
 {1,"PROGRAM",0,127,0,Interpolation::Discrete,"",0,1,ApplyNextTrigger},
 {2,"TUNE",-24,24,0,Interpolation::Linear,"st",2,.1f},
 {3,"GAIN",0,2,1,Interpolation::Linear,"x",2,.01f},
 {4,"VELOCITY",0,1,1,Interpolation::Linear,"",2,.01f,ApplyNextTrigger}
};
struct SoundFontState {float values[5]={0,0,0,1,1};};
inline bool valid_soundfont(const SoundFontState& s){for(unsigned i=0;i<5;++i){const auto& p=kSoundFontParameters[i];if(!(s.values[i]>=p.minimum && s.values[i]<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;}return true;}
}
