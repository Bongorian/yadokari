#pragma once
#include <stdint.h>
#include <math.h>
namespace groove {
inline constexpr const char* kLfoDivisions[]={"2/1","1/1","1/2.","1/2","1/2T","1/4.","1/4","1/4T","1/8.","1/8","1/8T","1/16"};
// Every period divides 24 beats exactly, including dotted and triplet values.
inline float transport_lfo_phase(uint64_t beat,uint64_t numerator,uint64_t denominator,unsigned division){
 static constexpr unsigned cycles[]={3,6,8,12,18,16,24,36,32,48,72,96};
 double p=(double(beat%24)+double(numerator)/double(denominator))*cycles[division<12?division:6]/24.0;
 float phase=float(p-floor(p));return phase>=1?0:phase;
}
struct TempoLfo {
 bool active=false,requested=false,initialized=false;uint8_t division=6,pending=6;uint64_t beat=0;
 void request(float sync,float value){requested=sync>=.5f;pending=uint8_t(value<0?0:value>11?11:value);}
 float next(float phase,float hz,float inverse_rate,bool playing,uint64_t position,uint64_t fraction,uint64_t period){
  if(!initialized || !playing || position!=beat){active=requested;division=pending;initialized=true;}beat=position;
  if(active)return transport_lfo_phase(position,fraction,period,division);
  // Preserve the original free-running operations, including their rounding.
  phase+=hz*inverse_rate;if(phase>=1)phase-=1;return phase;
 }
};
}
