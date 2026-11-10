#pragma once
#include "Effects/pitchshifter.h"
#include "alloc_policy.h"
namespace groove {
class PitchShiftStereo {
 struct State {daisysp::PitchShifter channels[2];State(){__builtin_memset(static_cast<void*>(channels),0,sizeof channels);}};
 State* state_=nullptr;unsigned fresh_=0;int shift_=100;bool owned_=false,bound_=false;
public:
 // A single array uses Circle's32MiB bucket instead of96 separate512KiB
 // buckets. This is still full pre-render reservation, not audio-time lazy work.
 class Pool {
  friend class PitchShiftStereo;State* states_=nullptr;unsigned count_=0;
 public:
  explicit Pool(unsigned count):states_(count?alloc_array<State>(count):nullptr),count_(states_?count:0){}
  ~Pool(){delete[] states_;}Pool(const Pool&)=delete;Pool& operator=(const Pool&)=delete;
  unsigned count()const{return count_;}
 };
 static constexpr unsigned state_bytes(){return sizeof(State);}
 PitchShiftStereo()=default;
 ~PitchShiftStereo(){if(owned_)delete state_;}
 // Control owner only, before init/render. A failed pool bind must not fall
 // back to per-slot allocation and publish a partially provisioned engine.
 void bind(Pool& pool,unsigned index){if(owned_)delete state_;owned_=false;bound_=true;state_=index<pool.count_?pool.states_+index:nullptr;}
 PitchShiftStereo(const PitchShiftStereo&)=delete;PitchShiftStereo& operator=(const PitchShiftStereo&)=delete;
 bool ready()const{return state_!=nullptr;}
 void init(unsigned rate){if(!state_ && !bound_){state_=alloc_new<State>();owned_=state_!=nullptr;}if(!state_)return;for(auto& ch:state_->channels){ch.Init(float(rate),true);ch.SetDelSize(2048);ch.SetFun(0);}reset();}
 void reset(){fresh_=0;shift_=100;}
 // No allocation/large clearing at type changes or in the audio callback.
 void process(float& left,float& right,int shift){if(!state_)return;if(shift!=shift_){for(auto& ch:state_->channels)ch.SetTransposition(float(shift));shift_=shift;}float l=state_->channels[0].Process(left),r=state_->channels[1].Process(right);if(fresh_<2304)++fresh_;float fade=fresh_<=2048?0:float(fresh_-2048)/256;left+=(l-left)*fade;right+=(r-right)*fade;}
};
}
