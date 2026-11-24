#pragma once
#include "alloc_policy.h"
#include "project.h"

namespace groove {
enum class BlockAction:uint8_t {Copy,Clear,Move};
struct BlockOrigin {unsigned pattern,track,step;};

// Both rectangles are captured before the first write. History owns a reference,
// and the UI retains another until the audio consumer acknowledges the command.
class BlockEdit {
 struct Cell {
  StepState step{},mutation{};KitStepState kit[16]{};
  void read(const PhraseState& phrase,unsigned i){
   step=phrase.steps[i];mutation=phrase.mutation_source[i];
   for(unsigned pad=0;pad<16;++pad)kit[pad]=phrase.kit.steps[pad][i];
  }
  void write(PhraseState& phrase,unsigned i,bool use_step_for_mutation=false)const{
   phrase.steps[i]=step;phrase.mutation_source[i]=use_step_for_mutation?step:mutation;
   for(unsigned pad=0;pad<16;++pad)phrase.kit.steps[pad][i]=kit[pad];
  }
 };
 unsigned references_=1;
 Cell* cells_=nullptr;
 uint16_t lengths_[kTracks]{};
 bool frozen_[kTracks]{};
 ~BlockEdit(){delete[] cells_;}
public:
 const BlockAction action;
 const BlockOrigin source,destination;
 const unsigned tracks,steps;
 BlockEdit(BlockAction op,BlockOrigin src,BlockOrigin dst,unsigned rows,unsigned width):
  action(op),source(src),destination(dst),tracks(rows),steps(width){}
 BlockEdit(const BlockEdit&)=delete;
 void retain(){++references_;}
 void release(){if(!--references_)delete this;}
 bool capture(const ProjectState& p){
  const unsigned area=tracks*steps,capacity=action==BlockAction::Clear?area:2*area;
  if(!allocation_admitted(sizeof(Cell),capacity) || !(cells_=alloc_array<Cell>(capacity)))return false;
  for(unsigned t=0;t<tracks;++t){
   const auto& src=p.patterns[source.pattern].tracks[source.track+t];
   const auto& dst=p.patterns[destination.pattern].tracks[destination.track+t];
   frozen_[t]=src.mutation.frozen;lengths_[t]=dst.length;
   for(unsigned s=0;s<steps;++s){
    const unsigned index=t*steps+s;
    cells_[index].read(src,source.step+s);
    if(action!=BlockAction::Clear)cells_[area+index].read(dst,destination.step+s);
   }
  }
  return true;
 }
 void apply(ProjectState& p,bool forward)const{
  static const Cell empty{};
  if(action!=BlockAction::Copy){
   for(unsigned t=0;t<tracks;++t){
    auto& src=p.patterns[source.pattern].tracks[source.track+t];
    for(unsigned s=0;s<steps;++s)(forward?empty:cells_[t*steps+s]).write(src,source.step+s);
   }
  }
  if(action==BlockAction::Clear)return;
  const unsigned offset=forward?0:tracks*steps;
  for(unsigned t=0;t<tracks;++t){
   auto& dst=p.patterns[destination.pattern].tracks[destination.track+t];
   const unsigned end=destination.step+steps;
   dst.length=forward && end>lengths_[t]?uint16_t(end):lengths_[t];
   for(unsigned s=0;s<steps;++s)cells_[offset+t*steps+s].write(dst,destination.step+s,forward && !frozen_[t]);
  }
 }
 template<class Visit>void changed_ranges(Visit visit)const{
  uint16_t mask=uint16_t((1u<<tracks)-1);
  uint16_t src=action==BlockAction::Copy?0:uint16_t(mask<<source.track);
  uint16_t dst=action==BlockAction::Clear?0:uint16_t(mask<<destination.track);
  if(source.pattern==destination.pattern)visit(source.pattern,uint16_t(src|dst));
  else{if(src)visit(source.pattern,src);if(dst)visit(destination.pattern,dst);}
 }
};
}
