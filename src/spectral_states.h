#pragma once
#include "parameters.h"
namespace groove {
inline constexpr ParameterDescriptor kAdditiveParameters[]={
 {0,"BRIGHTNESS",0,1,0.6f,Interpolation::Linear,"",2,0.01f},
 {1,"ODD / EVEN",0,1,0.5f,Interpolation::Linear,"",2,0.01f},
 {2,"FOCUS",1,16,1,Interpolation::Linear,"harm",1,0.1f},
 {3,"WIDTH",0.5f,16,16,Interpolation::Linear,"harm",1,0.1f},
 {4,"MOTION",0,1,0,Interpolation::Linear,"",2,0.01f},
 {5,"ATTACK",0.001f,10,0.005f,Interpolation::Logarithmic,"s",3,0.005f},
 {6,"DECAY",0.001f,10,0.3f,Interpolation::Logarithmic,"s",3,0.01f},
 {7,"SUSTAIN",0,1,0.65f,Interpolation::Linear,"",2,0.01f},
 {8,"RELEASE",0.005f,10,0.4f,Interpolation::Logarithmic,"s",3,0.01f},
 {9,"HARMONIC 1",0,1,1,Interpolation::Linear,"",2,0.01f},
 {10,"HARMONIC 2",0,1,1,Interpolation::Linear,"",2,0.01f},
 {11,"HARMONIC 3",0,1,1,Interpolation::Linear,"",2,0.01f},
 {12,"HARMONIC 4",0,1,1,Interpolation::Linear,"",2,0.01f},
 {13,"HARMONIC 5",0,1,1,Interpolation::Linear,"",2,0.01f},
 {14,"HARMONIC 6",0,1,1,Interpolation::Linear,"",2,0.01f},
 {15,"HARMONIC 7",0,1,1,Interpolation::Linear,"",2,0.01f},
 {16,"HARMONIC 8",0,1,1,Interpolation::Linear,"",2,0.01f},
 {17,"HARMONIC 9",0,1,1,Interpolation::Linear,"",2,0.01f},
 {18,"HARMONIC 10",0,1,1,Interpolation::Linear,"",2,0.01f},
 {19,"HARMONIC 11",0,1,1,Interpolation::Linear,"",2,0.01f},
 {20,"HARMONIC 12",0,1,1,Interpolation::Linear,"",2,0.01f},
 {21,"HARMONIC 13",0,1,1,Interpolation::Linear,"",2,0.01f},
 {22,"HARMONIC 14",0,1,1,Interpolation::Linear,"",2,0.01f},
 {23,"HARMONIC 15",0,1,1,Interpolation::Linear,"",2,0.01f},
 {24,"HARMONIC 16",0,1,1,Interpolation::Linear,"",2,0.01f},
};
struct AdditiveState {float values[25]={0.6f,0.5f,1,16,0,0.005f,0.3f,0.65f,0.4f,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};};
inline bool valid_additive(const AdditiveState& s){for(unsigned i=0;i<25;++i){const auto& p=kAdditiveParameters[i];if(!(s.values[i]>=p.minimum && s.values[i]<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;}return true;}
inline constexpr ParameterDescriptor kPhaseDistParameters[]={
 {0,"WAVE",0,3,0,Interpolation::Discrete,"",0,1},
 {1,"DISTORTION",0,1,0.3f,Interpolation::Linear,"",2,0.01f},
 {2,"ENV DEPTH",-1,1,0.4f,Interpolation::Linear,"",2,0.01f},
 {3,"MOTION",0,1,0,Interpolation::Linear,"",2,0.01f},
 {4,"RATE",0.01f,10,0.3f,Interpolation::Logarithmic,"Hz",2,0.05f},
 {5,"ATTACK",0.001f,10,0.005f,Interpolation::Logarithmic,"s",3,0.005f},
 {6,"DECAY",0.001f,10,0.3f,Interpolation::Logarithmic,"s",3,0.01f},
 {7,"SUSTAIN",0,1,0.65f,Interpolation::Linear,"",2,0.01f},
 {8,"RELEASE",0.005f,10,0.4f,Interpolation::Logarithmic,"s",3,0.01f},
};
struct PhaseDistState {float values[9]={0,0.3f,0.4f,0,0.3f,0.005f,0.3f,0.65f,0.4f};};
inline bool valid_phase_dist(const PhaseDistState& s){for(unsigned i=0;i<9;++i){const auto& p=kPhaseDistParameters[i];if(!(s.values[i]>=p.minimum && s.values[i]<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;}return true;}
inline constexpr const char* kPhaseDistWaves[]={"SKEW","HALF","PULSE","CURVE"};
}
