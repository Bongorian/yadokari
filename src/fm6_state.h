#pragma once
#include "parameters.h"
namespace groove {
inline constexpr ParameterDescriptor kFm6Parameters[]={
 {0,"PATCH",-1,95,0,Interpolation::Discrete,"",0,1,ApplyNextTrigger},
 {1,"BRIGHTNESS",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {2,"ATTACK",0.25f,4,1,Interpolation::Logarithmic,"x",2,0.05f,ApplyNextTrigger},
 {3,"RELEASE",0.25f,4,1,Interpolation::Logarithmic,"x",2,0.05f,ApplyNextTrigger},
 {4,"MOD DEPTH",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {5,"OP 6 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {6,"OP 5 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {7,"OP 4 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {8,"OP 3 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {9,"OP 2 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {10,"OP 1 LEVEL",0,2,1,Interpolation::Linear,"x",2,0.01f},
 {11,"OP 6 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {12,"OP 5 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {13,"OP 4 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {14,"OP 3 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {15,"OP 2 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {16,"OP 1 DETUNE",-50,50,0,Interpolation::Linear,"ct",2,0.5f},
 {17,"ALGORITHM",-1,31,-1,Interpolation::Discrete,"",0,1},
 {18,"FEEDBACK",-1,7,-1,Interpolation::Discrete,"",0,1},
 {19,"TRANSPOSE",-24,24,0,Interpolation::Discrete,"st",0,1},
 {20,"VELOCITY",0,1,1,Interpolation::Linear,"",2,0.01f,ApplyNextTrigger},
};
struct Fm6State {float values[21]={0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,-1,-1,0,1};uint8_t patch[128]{};};
inline bool valid_fm6(const Fm6State& s){for(unsigned i=0;i<21;++i){const auto& d=kFm6Parameters[i];if(!(s.values[i]>=d.minimum && s.values[i]<=d.maximum) || (d.interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;}for(auto b:s.patch)if(b>127)return false;return true;}
}
