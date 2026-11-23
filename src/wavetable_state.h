#pragma once
#include "parameters.h"
namespace groove {
enum WavetableParameter : unsigned {
 WavetableWarpMode=8, WavetableWarpAmount, WavetablePmRatio, WavetablePmAmount, kWavetableParameterCount
};
constexpr unsigned kWavetableLegacyParameterCount=WavetableWarpMode;
// Declared ranges are shared by state validation, codec and editor metadata.
inline constexpr ParameterDescriptor kWavetableParameters[]={
 {0,"POSITION",0,127,0,Interpolation::Linear,"wave",2,.25f},
 {1,"SCAN DEPTH",0,63.5f,0,Interpolation::Linear,"wave",2,.25f},
 {2,"SCAN RATE",.01f,20,1,Interpolation::Logarithmic,"Hz",2,.05f},
 {3,"ATTACK",.001f,10,.005f,Interpolation::Logarithmic,"s",3,.005f},
 {4,"RELEASE",.005f,10,.3f,Interpolation::Logarithmic,"s",3,.01f},
 {5,"DECAY",.001f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {6,"SUSTAIN",0,1,.7f,Interpolation::Linear,"",2,.01f},
 {7,"BRIGHTNESS",0,1,1,Interpolation::Linear,"",2,.01f},
 {WavetableWarpMode,"WARP MODE",0,3,0,Interpolation::Discrete,"",0,1},
 {WavetableWarpAmount,"WARP AMT",-1,1,0,Interpolation::Linear,"",2,.01f},
 {WavetablePmRatio,"PM RATIO",.25f,8,1,Interpolation::Logarithmic,"x",2,.05f},
 {WavetablePmAmount,"PM AMT",0,1,0,Interpolation::Linear,"",2,.01f}
};
static_assert(sizeof kWavetableParameters/sizeof *kWavetableParameters==kWavetableParameterCount);
struct WavetableState {
 float values[kWavetableParameterCount]{};
 constexpr WavetableState(){for(unsigned i=0;i<kWavetableParameterCount;++i)values[i]=kWavetableParameters[i].initial;}
};
inline bool wavetable_extension_is_default(const float* values){
 for(unsigned i=kWavetableLegacyParameterCount;i<kWavetableParameterCount;++i)
  if(values[i]!=kWavetableParameters[i].initial)return false;
 return true;
}
inline bool valid_wavetable(const WavetableState& state){
 for(unsigned i=0;i<kWavetableParameterCount;++i){
  const auto& p=kWavetableParameters[i];const float v=state.values[i];
  if(!(v>=p.minimum && v<=p.maximum) || (p.interpolation==Interpolation::Discrete && v!=float(unsigned(v))))return false;
 }
 return true;
}
}
