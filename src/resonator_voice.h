#pragma once
#include "resonator_dsp.h"
namespace groove {
// MIDI events drive the native strum input. Simultaneous chord events are
// serialized one per 24-frame quantum (up to 1.5 ms for a four-note chord).
// Hardware CV/onset detection/debounce is not part of this MIDI adapter.
class ResonatorVoice {
public:
 bool ready()const{return dsp_.ready();}
 void init(uint32_t seed=1){dsp_.init(seed);position_=24;head_=count_=0;note_=60;active_=false;dropped_=0;pending_=false;}
 void reset(uint32_t seed){seed_=seed;pending_=true;position_=24;head_=count_=dropped_=0;active_=false;}
 bool trigger(float note){if(pending_)init(seed_);if(count_==16){++dropped_;return false;}notes_[(head_+count_)&15]=note;++count_;active_=true;return true;}
 bool boundary()const{return position_==24;}
 unsigned dropped()const{return dropped_;}
 void next(const float* p,float bend,float& left,float& right){
  left=right=0;if(!active_)return;
  if(position_==24){
   rings::PerformanceState state{};state.internal_exciter=true;state.strum=count_!=0;
   if(count_){note_=notes_[head_];head_=(head_+1)&15;--count_;}
   state.tonic=12+p[7];state.note=note_-12;state.fm=p[6]+bend;
   rings::Patch patch{p[2],p[3],p[4],p[5]};
   dsp_.process(unsigned(p[0]),1u<<unsigned(p[1]),patch,state,input_,left_,right_);
   position_=0;
  }
  left=left_[position_];right=right_[position_++];
 }
private:
 ResonatorDsp dsp_;float input_[24]{},left_[24]{},right_[24]{},notes_[16]{};
 float note_=60;unsigned position_=24,head_=0,count_=0,dropped_=0;bool active_=false,pending_=false;uint32_t seed_=1;
};
}
