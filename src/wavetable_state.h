#pragma once
#include "parameters.h"
namespace groove {
// Declared ranges are shared by state validation, codec and editor metadata.
inline constexpr ParameterDescriptor kWavetableParameters[]={
 {0,"POSITION",0,127,0,Interpolation::Linear,"wave",2,.25f},
 {1,"SCAN DEPTH",0,63.5f,0,Interpolation::Linear,"wave",2,.25f},
 {2,"SCAN RATE",.01f,20,1,Interpolation::Logarithmic,"Hz",2,.05f},
 {3,"ATTACK",.001f,10,.005f,Interpolation::Logarithmic,"s",3,.005f},
 {4,"RELEASE",.005f,10,.3f,Interpolation::Logarithmic,"s",3,.01f},
 {5,"DECAY",.001f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {6,"SUSTAIN",0,1,.7f,Interpolation::Linear,"",2,.01f},
 {7,"BRIGHTNESS",0,1,1,Interpolation::Linear,"",2,.01f}
};
struct WavetableState {float values[8]={0,0,1,.005f,.3f,.2f,.7f,1};};
inline bool valid_wavetable(const WavetableState& state){for(unsigned i=0;i<8;++i)if(!(state.values[i]>=kWavetableParameters[i].minimum && state.values[i]<=kWavetableParameters[i].maximum))return false;return true;}
}
