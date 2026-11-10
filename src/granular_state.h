#pragma once
#include "parameters.h"
namespace groove {
inline constexpr unsigned kGranularParameterCount=12;
inline constexpr ParameterDescriptor kGranularParameters[]={
 {0,"POSITION",0,1,.5f,Interpolation::Linear,"",2,.01f},
 {1,"SIZE",0,1,.5f,Interpolation::Linear,"",2,.01f},
 {2,"DENSITY",0,1,.6f,Interpolation::Linear,"",2,.01f},
 {3,"TEXTURE",0,1,.5f,Interpolation::Linear,"",2,.01f},
 {4,"SPREAD",0,1,.3f,Interpolation::Linear,"",2,.01f},
 {5,"ATTACK",.001f,10,.005f,Interpolation::Logarithmic,"s",3,.005f},
 {6,"DECAY",.001f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {7,"SUSTAIN",0,1,.7f,Interpolation::Linear,"",2,.01f},
 {8,"RELEASE",.005f,10,.3f,Interpolation::Logarithmic,"s",3,.01f},
 {9,"JITTER",0,1,.1f,Interpolation::Linear,"",2,.01f},
 {10,"FREEZE",0,1,0,Interpolation::Discrete,"",0,1},
 {11,"PITCH OFFSET",-24,24,0,Interpolation::Linear,"st",1,.1f}
};
struct GranularState {float values[kGranularParameterCount]={.5f,.5f,.6f,.5f,.3f,.005f,.2f,.7f,.3f,.1f,0,0};};
inline bool valid_granular(const GranularState& s){for(unsigned i=0;i<kGranularParameterCount;++i){const auto& p=kGranularParameters[i];float v=s.values[i];if(!(v>=p.minimum && v<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(v)!=v))return false;}return true;}
}
