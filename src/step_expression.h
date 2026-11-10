#pragma once
#include "project.h"
#include <math.h>
namespace groove {
inline bool step_monophonic(const StepState& s){if(s.ratchet!=1)return false;for(auto n:s.chord)if(n<128)return false;return true;}
inline bool step_legato_engine(Instrument i){return i==Instrument::Acid || i==Instrument::PolySaw || i==Instrument::FM;}
inline bool step_condition_passes(const StepState& s,uint32_t cycle,bool fill){switch(s.condition){case TrigCondition::Fill:return fill;case TrigCondition::NotFill:return !fill;case TrigCondition::First:return !cycle;case TrigCondition::NotFirst:return cycle!=0;case TrigCondition::Cycle:return cycle%s.cycle_every==s.cycle_phase;case TrigCondition::Always:return true;}return false;}
// Per-voice pitch motion. Only marked transitions run this path; ordinary
// phrases retain their existing oscillator arithmetic and envelope history.
struct StepPitchSlide {
 uint32_t remaining=0;float target=0,multiplier=1;
 void reset(){remaining=0;}
 void start(float from,float to,unsigned rate){target=to;remaining=rate*80/1000;if(!remaining || from<=0 || to<=0){remaining=0;return;}multiplier=powf(to/from,1.0f/float(remaining));}
 float next(float frequency){if(!remaining)return frequency;if(!--remaining)return target;return frequency*multiplier;}
};
}
