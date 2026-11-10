#pragma once
#include "project.h"
#include "note_transform.h"
#include "generative.h"
namespace groove {
enum class PatternVariation:uint8_t {Thin,Transpose,DropTrack};
enum class HarmonyMode:uint8_t {Bass,Arp};
struct VariationOptions {PatternVariation kind=PatternVariation::Thin;uint8_t density=50,drop_track=0;int8_t transpose=1;};
template<class D,class S> inline void copy_phrase_fields(D& dst,const S& src){
#define COPY(T,N,...) copy_track_field(dst.N,src.N);
 PHRASE_FIELDS(COPY)
#undef COPY
 for(unsigned pad=0;pad<16;++pad)for(unsigned i=0;i<kSteps;++i)dst.kit.steps[pad][i]=src.kit.steps[pad][i];
}
inline bool variation_valid(const VariationOptions& o){return unsigned(o.kind)<=2 && o.density<=100 && o.drop_track<16 && o.transpose>=-12 && o.transpose<=12 && o.transpose!=0;}
inline uint32_t composition_random(uint32_t seed,unsigned track,unsigned step,unsigned pad=16){uint32_t x=seed^(0x9e3779b9u*(track+1))^(0x85ebca6bu*(step+1))^(0xc2b2ae35u*(pad+1));x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline uint8_t variation_note(uint8_t note,ConstTrackView t,int delta){int input=bounded(int(t.root_note)+note-60,0,127);int heard=transform_note(input+delta,t);return uint8_t(bounded(heard-int(t.transpose)-int(t.root_note)+60,0,127));}
inline void vary_pattern(PatternState& out,const ProjectState& p,unsigned from,const VariationOptions& o){
 snapshot_pattern(out,p,from);
 for(unsigned t=0;t<kTracks;++t){auto& target=out.tracks[t];const auto state=track_at(p,from,t);
  if(o.kind==PatternVariation::DropTrack)continue;
  for(unsigned i=0;i<kSteps;++i){auto edit=[&](StepState& step){if(o.kind==PatternVariation::Thin){if(step.enabled && composition_random(p.random_seed,t,i)%100>=o.density){step.enabled=false;for(const auto& lock:step.locks)step.lock_only|=lock.enabled;}}else{step.note=variation_note(step.note,state,o.transpose);for(auto& note:step.chord)if(note<128)note=variation_note(note,state,o.transpose);}};
   edit(target.steps[i]);if(target.mutation.frozen)edit(target.mutation_source[i]);
   for(unsigned pad=0;pad<16;++pad){auto& step=target.kit.steps[pad][i];if(o.kind==PatternVariation::Thin){if(step.enabled && composition_random(p.random_seed,t,i,pad)%100>=o.density)step.enabled=false;}else if(step.enabled)step.pitch_lock=int8_t(bounded(int(step.pitch_lock)+o.transpose,-48,48));}
  }
  if(o.kind==PatternVariation::Transpose)for(auto& note:target.lanes[0].values)note=variation_note(note,state,o.transpose);
 }
}
inline unsigned harmony_chord(ConstTrackView source,const StepState& step,uint8_t notes[kStepNotes]){
 if(!step.enabled)return 0;
 bool declared=false;for(auto n:step.chord)declared|=n<128;if(!declared)return 0;
 unsigned count=0;auto add=[&](unsigned stored){int input=bounded(int(source.root_note)+int(stored)-60,0,127);uint8_t pitch=step.chromatic?uint8_t(bounded(input+source.transpose,0,127)):transform_note(input,source);for(unsigned i=0;i<count;++i)if(notes[i]%12==pitch%12)return;if(count<kStepNotes)notes[count++]=pitch;};add(step.note);for(auto note:step.chord)if(note<128)add(note);return count>=2?count:0;
}
inline unsigned harmony_chord_count(ConstTrackView source){unsigned n=0;uint8_t notes[kStepNotes];for(unsigned i=0;i<source.length;++i)n+=harmony_chord(source,source.steps[i],notes)!=0;return n;}
// Ordinary step data only. Root is anchored in the target's existing octave.
inline bool make_harmony(TrackState& result,ConstTrackView source,ConstTrackView target,HarmonyMode mode){
 if(unsigned(mode)>1 || !harmony_chord_count(source))return false;
 for(auto& step:result.steps){step=StepState{};}
 result.length=source.length;result.direction=PlaybackDirection::Forward;result.speed_num=result.speed_den=1;result.lanes_enabled=false;result.mutation.amount=0;result.mutation.frozen=false;result.frozen_asset=255;
 uint8_t chord[kStepNotes]{};unsigned count=0,rotation=0;const StepState* current=nullptr;
 for(unsigned i=0;i<source.length;++i){uint8_t incoming[kStepNotes];unsigned n=harmony_chord(source,source.steps[i],incoming);if(n){count=n;rotation=0;current=&source.steps[i];for(unsigned j=0;j<n;++j)chord[j]=incoming[j];}
  if(!count || (mode==HarmonyMode::Bass && !n))continue;
  int base=int(target.root_note/12)*12+chord[0]%12;unsigned interval=mode==HarmonyMode::Bass?0:unsigned((int(chord[rotation++%count])-int(chord[0])+12)%12);int pitch=base+int(interval);while(pitch>127)pitch-=12;
  int stored=pitch-int(target.root_note)-int(target.transpose)+60;if(stored<0 || stored>127)return false;
  auto& step=result.steps[i];step.enabled=true;step.note=uint8_t(stored);step.chromatic=true;step.velocity=current->velocity;step.gate=mode==HarmonyMode::Bass?current->gate:72;
 }
 return true;
}
}
