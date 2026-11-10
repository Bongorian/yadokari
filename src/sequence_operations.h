#pragma once
#include "project.h"
namespace groove {
// Pattern-level operations preserve both the shared grid (including FX locks)
// and all16 kit lanes. Callers retain their existing transaction/Undo owner.
template<class Track> inline bool sequence_empty(const Track& t){
 if(t.frozen_asset<kSampleAssets || t.sound_override!=kNoPreset)return false;
 if(t.mutation.frozen)for(const auto& s:t.mutation_source)if(s.enabled || s.lock_only)return false;
 for(const auto& s:t.steps)if(s.enabled || s.lock_only)return false;
 for(const auto& pad:t.kit.steps)for(const auto& s:pad)if(s.enabled)return false;
 for(const auto& lane:t.automation)if(lane.enabled)return false;
 return true;
}
template<class Track> inline void repeat_sequence_steps(Track& t,unsigned length){
 for(unsigned i=0;i<length;++i){t.steps[length+i]=t.steps[i];
  for(unsigned pad=0;pad<16;++pad)t.kit.steps[pad][length+i]=t.kit.steps[pad][i];
  if(t.mutation.frozen)t.mutation_source[length+i]=t.mutation_source[i];
 }
}
template<class Track> inline void rotate_sequence_steps(Track& target,const Track& source,unsigned length,int direction){
 for(unsigned i=0;i<length;++i){unsigned from=direction>0?(i+length-1)%length:(i+1)%length;target.steps[i]=source.steps[from];
  for(unsigned pad=0;pad<16;++pad)target.kit.steps[pad][i]=source.kit.steps[pad][from];
  if(source.mutation.frozen)target.mutation_source[i]=source.mutation_source[from];
 }
}
}
