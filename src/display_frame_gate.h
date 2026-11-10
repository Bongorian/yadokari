#pragma once
#include <stdint.h>
namespace groove {
// A completed full scan needs repeating only after a newly rendered frame or
// transport invalidation. Pending transfers are always reaped independently.
// Absolute release slots; preparation may lead them without sending early.
// A late consumer skips missed slots, never queues rendered frames.
class UiFrameClock {
 uint64_t slot_=~uint64_t(0),due_at_=0,release_at_=0;uint32_t context_=~uint32_t(0);
 public: uint64_t due_at()const{return due_at_;}uint64_t release_at()const{return release_at_;}
 bool due(uint64_t us,bool output_ready=true,uint32_t context=0,uint32_t prepare_ahead=0){
  // A blocked LCD must not consume a slot or let its immutable frame be
  // replaced. Navigation gets one immediate frame; steady views stay30Hz.
  if(!output_ready)return false;
  if(prepare_ahead>33333)prepare_ahead=33333;
  uint64_t slot=(us+prepare_ahead)*30/1000000;if(slot==slot_ && context==context_)return false;
  bool immediate=slot_==~uint64_t(0) || context!=context_;
  due_at_=immediate?us:((slot_+1)*1000000+29)/30-prepare_ahead;
  release_at_=immediate?us:(slot*1000000+29)/30;
  slot_=slot;context_=context;return true;
 }
};
struct DisplayFrameStats {
 unsigned complete[2]{},discarded[2]{},max_gap_us[2]{},over100ms[2]{};uint64_t last[2]{};
 void reset(uint64_t now){*this={};last[0]=last[1]=now;}
 void completed(unsigned side,uint64_t now){if(side>=2)return;if(last[side]){uint64_t gap=now-last[side];if(gap>max_gap_us[side])max_gap_us[side]=unsigned(gap);if(gap>100000)++over100ms[side];}last[side]=now;++complete[side];}
};
class DisplayFrameGate {
 public:
  void invalidate() { scanned_[0]=scanned_[1]=false; }
  void new_frame() { invalidate(); }
  bool complete() const { return scanned_[0] && scanned_[1]; }
  bool needs_scan(unsigned side) const { return side<2 && !scanned_[side]; }
  void scanned(unsigned side) { if(side<2) scanned_[side]=true; }
 private:
  bool scanned_[2]{};
};
}
