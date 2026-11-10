#pragma once
#include "parameters.h"
namespace groove {
inline constexpr const char* kFormantModes[]={"FORMANT","VOSIM"};
inline constexpr ParameterDescriptor kFormantParameters[]={
 {0,"MODE",0,1,0,Interpolation::Discrete,"",0,1},
 {1,"VOWEL",0,1,.4f,Interpolation::Linear,"",2,.01f},
 {2,"FORMANT SHIFT",-24,24,0,Interpolation::Linear,"st",1,.1f},
 {3,"SHAPE",0,1,.5f,Interpolation::Linear,"",2,.01f},
 {4,"MOD DEPTH",0,1,.1f,Interpolation::Linear,"",2,.01f},
 {5,"ATTACK",.001f,10,.005f,Interpolation::Logarithmic,"s",3,.005f},
 {6,"DECAY",.001f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {7,"SUSTAIN",0,1,.7f,Interpolation::Linear,"",2,.01f},
 {8,"RELEASE",.005f,10,.3f,Interpolation::Logarithmic,"s",3,.01f},
};
struct FormantState {float values[9]={0,.4f,0,.5f,.1f,.005f,.2f,.7f,.3f};};
inline bool valid_formant(const FormantState& s){for(unsigned i=0;i<9;++i){const auto& p=kFormantParameters[i];float v=s.values[i];if(!(v>=p.minimum && v<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(v)!=v))return false;}return true;}
}
