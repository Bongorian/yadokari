#pragma once
#include "../visual_model.h"
#include <math.h>
namespace groove { namespace ui {
// Interpolate measured endpoints only; never extrapolate a phase or waveform.
class VisualMotion {
 VisualData from_{},to_{};uint64_t token_=0,at_=0;unsigned duration_=0,key_=~0u;bool wrap_phase_=false;
 VisualData blend(uint64_t now)const{VisualData out=to_;float t=duration_?float(now-at_)/duration_:1;if(t>1)t=1;if(t<0)t=0;for(unsigned i=0;i<out.count && i<128;++i){out.values[i]=from_.values[i]+(to_.values[i]-from_.values[i])*t;out.low[i]=from_.low[i]+(to_.low[i]-from_.low[i])*t;}float delta=to_.phase-from_.phase;if(wrap_phase_){if(delta<-.5f)delta+=1;if(delta>.5f)delta-=1;}out.phase=from_.phase+delta*t;if(out.phase<0)out.phase+=1;if(out.phase>1)out.phase-=1;out.position=from_.position+(to_.position-from_.position)*t;out.level=from_.level+(to_.level-from_.level)*t;return out;}
public:
 VisualData update(unsigned key,uint64_t token,uint64_t now,const VisualData& value,bool motion,bool wrap_phase=false){
  wrap_phase_=wrap_phase;
  if(key!=key_ || !value.available || !motion){key_=key;from_=to_=value;token_=token;at_=now;duration_=0;return value;}
  if(token!=token_){from_=blend(now);to_=value;token_=token;at_=now;bool changed=from_.count!=value.count || from_.phase!=value.phase || from_.position!=value.position || from_.level!=value.level;for(unsigned i=0;i<value.count && i<128 && !changed;++i)changed=from_.values[i]!=value.values[i] || from_.low[i]!=value.low[i];duration_=changed?50:0;}
  else{to_.label=value.label;to_.live=value.live;}
  auto out=blend(now);if(duration_ && now-at_>=duration_)duration_=0;return out;
 }
 bool active(uint64_t)const{return duration_!=0;}

};
}}
