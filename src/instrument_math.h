#pragma once
#include <math.h>
namespace groove {
inline float fm_ratio(float p){return 1+7*p;}
inline float fm_index(float p){return 8*p;}
inline float acid_cutoff(const float* p,float envelope){return 80+p[0]*1800+p[2]*envelope*5000;}
inline float acid_damping(float resonance){return 1.5f-resonance*1.3f;}
inline float acid_decay(float p){return 0.999f+p*0.0009f;}
}
