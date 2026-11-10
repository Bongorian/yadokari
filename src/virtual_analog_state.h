#pragma once
#include "parameters.h"
namespace groove {
inline constexpr ParameterDescriptor kVirtualAnalogParameters[]={
 {0,"OSC 1",0,3,0,Interpolation::Discrete,"",0,1},
 {1,"OSC 2",0,3,1,Interpolation::Discrete,"",0,1},
 {2,"DETUNE",-50,50,7,Interpolation::Linear,"ct",1,.5f},
 {3,"OSC MIX",0,1,.5f,Interpolation::Linear,"",2,.01f},
 {4,"CUTOFF",20,18000,2200,Interpolation::Logarithmic,"Hz",0,25},
 {5,"RESONANCE",0,1.2f,.2f,Interpolation::Linear,"",2,.01f},
 {6,"SUB",0,1,.15f,Interpolation::Linear,"",2,.01f},
 {7,"NOISE",0,1,.02f,Interpolation::Linear,"",2,.01f},
 {8,"PULSE WIDTH",.05f,.95f,.5f,Interpolation::Linear,"",2,.01f},
 {9,"PWM DEPTH",0,.45f,.1f,Interpolation::Linear,"",2,.01f},
 {10,"PWM RATE",.01f,10,.3f,Interpolation::Logarithmic,"Hz",2,.05f},
 {11,"ATTACK",.001f,10,.005f,Interpolation::Logarithmic,"s",3,.005f},
 {12,"DECAY",.001f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {13,"SUSTAIN",0,1,.7f,Interpolation::Linear,"",2,.01f},
 {14,"RELEASE",.005f,10,.3f,Interpolation::Logarithmic,"s",3,.01f},
 {15,"FILTER ENV",0,6,2,Interpolation::Linear,"oct",2,.05f},
 {16,"OSC 2 TUNE",-24,24,0,Interpolation::Discrete,"st",0,1},
 {17,"FILTER DRIVE",.25f,4,1,Interpolation::Linear,"x",2,.05f},
 {18,"HARD SYNC",0,1,0,Interpolation::Discrete,"",0,1},
 {19,"CROSS MOD",0,1,0,Interpolation::Linear,"",2,.01f},
 {20,"PITCH ENV",-48,48,0,Interpolation::Linear,"st",1,.5f},
 {21,"PITCH DECAY",.005f,10,.2f,Interpolation::Logarithmic,"s",3,.01f},
 {22,"SUB WAVE",0,1,0,Interpolation::Discrete,"",0,1},
 {23,"FILTER TYPE",0,2,0,Interpolation::Discrete,"",0,1},
 {24,"SVF MODE",0,2,0,Interpolation::Discrete,"",0,1},
 {25,"FILTER MORPH",0,1,0,Interpolation::Linear,"",2,.01f}
};
inline constexpr const char* kVirtualAnalogWaves[]={"SAW","PULSE","TRIANGLE","SINE"};
inline constexpr unsigned kVirtualAnalogParameterCount=26;
inline constexpr const char* kVirtualAnalogSubs[]={"SIN","SQR"};
inline constexpr const char* kVirtualAnalogFilters[]={"LADDER","SVF","MORPH"};
inline constexpr const char* kVirtualAnalogSvfModes[]={"LP","HP","BP"};
struct VirtualAnalogState {float values[kVirtualAnalogParameterCount]={0,1,7,.5f,2200,.2f,.15f,.02f,.5f,.1f,.3f,.005f,.2f,.7f,.3f,2,0,1,0,0,0,.2f,0,0,0,0};};
inline bool valid_virtual_analog(const VirtualAnalogState& s){for(unsigned i=0;i<kVirtualAnalogParameterCount;++i){auto& d=kVirtualAnalogParameters[i];if(!(s.values[i]>=d.minimum && s.values[i]<=d.maximum) || (d.interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;}return true;}
}
