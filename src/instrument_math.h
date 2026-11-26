#pragma once
#include <math.h>
namespace groove {
inline float fm_ratio(float p){return 1+7*p;}
inline float fm_index(float p){return 8*p;}
inline float acid_cutoff(const float* p,float envelope){return 80+p[0]*1800+p[2]*envelope*5000;}
inline float acid_damping(float resonance){return 1.5f-resonance*1.3f;}
// Interpolate the decay rate: 20 ms at minimum, 1.5 s at maximum.
inline float acid_filter_decay(float decay,bool accent,float inverse_rate){
 const float shortness=1.0f-decay;
 const float slow=1.0f/1.5f,fast=1.0f/.020f;
 const float rate=slow+(fast-slow)*shortness*shortness*shortness;
 return 1.0f-rate*(accent?2.0f:1.0f)*inverse_rate;
}
}
