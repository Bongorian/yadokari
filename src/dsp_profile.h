#pragma once
#include <stdint.h>
#include "fx_registry.h"
namespace groove {
// Task requests; the single audio owner acknowledges and owns all counters.
// A Ready result is immutable until the next begin. No allocation or logging.
class DspProfile {
public:
 using Clock=uint32_t(*)();
 static constexpr unsigned engine_bins=32*32,fx_base=engine_bins,system_base=fx_base+5*kFxCount,stage_base=system_base+12,bins=stage_base+5;
 enum Stage:unsigned{Control,Events,Tracks,Output,Total};
 enum System:unsigned{Drive,Filter,TrackCompressor,Delay,Reverb,Throws,BeatRepeat,Performance,MasterCompressor,Limiter,RecordCapture,AssistDispatch};
 struct LastBlock {unsigned ticks=0,visual=0,trigger=0,parameters=0,poly_peak=0;};
 const LastBlock& last_block()const{return last_;}
 struct Counter {uint64_t ticks=0,active_ticks=0,frames=0,calls=0,max_voice_q16=0;uint32_t max_block_ticks=0;};
 struct NativeCounter {uint64_t ticks=0,frames=0;unsigned sampled_blocks=0,max_block_ticks=0,bin=0;};
 const NativeCounter& native_counter(unsigned track)const{return native_[track];}
 struct Snapshot {unsigned blocks=0,sampled=0,main_peak=0,poly_peak=0;uint64_t control_ticks=0;unsigned control_calls=0,control_max=0,snapshot_control_max=0;unsigned va_trigger_calls=0,va_trigger_max=0,va_parameter_calls=0,va_parameter_max=0;uint64_t va_trigger_ticks=0,va_parameter_ticks=0;unsigned all_block_max=0,peak_visual=0,peak_va_trigger=0,peak_va_parameters=0,visual_block_max=0;uint64_t visual_ticks=0;uint8_t layer_reasons[16]{};};
 bool begin(Clock clock,unsigned generation=0){if(!clock)return false;unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);if(state!=0 && state!=4)return false;requested_=clock;generation_=generation;measuring_=false;__atomic_store_n(&measure_requested_,0u,__ATOMIC_RELAXED);__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);return true;}
 void stop(){unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);while((state==1 || state==2) && !__atomic_compare_exchange_n(&state_,&state,3u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){} }
 bool quiescent()const{unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);return state==0 || state==4;}
 void begin_measurement(){__atomic_store_n(&measure_requested_,1u,__ATOMIC_RELEASE);}
 bool ready()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==4;}
 const Counter& counter(unsigned bin)const{return counters_[bin];}
 const Snapshot& snapshot()const{return snapshot_;}
 static unsigned voice(unsigned engine,unsigned variant){return (engine<32?engine:31)*32+(variant<32?variant:31);}
 static unsigned fx(unsigned path,unsigned type){return fx_base+(path<5?path:4)*kFxCount+(type<kFxCount?type:kFxCount-1);}
 void block_begin(){
  last_={};visual_block_=va_trigger_block_=va_parameter_block_=0;
  unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);
  if(state==1){for(auto& c:counters_)c={};for(auto& c:native_)c={};for(auto& c:native_scratch_)c={};snapshot_={};stride_=0;__atomic_store_n(&state_,2u,__ATOMIC_RELEASE);state=2;}
  if(state==3){clock_=all_clock_=nullptr;__atomic_store_n(&state_,4u,__ATOMIC_RELEASE);return;}
  if(state==2 && __atomic_exchange_n(&measure_requested_,0u,__ATOMIC_ACQ_REL)){measuring_=true;snapshot_.all_block_max=snapshot_.peak_visual=snapshot_.peak_va_trigger=snapshot_.peak_va_parameters=snapshot_.visual_block_max=0;}
  all_clock_=state==2?requested_:nullptr;all_start_=all_clock_?all_clock_():0;
  clock_=state==2 && stride_++%127==0?requested_:nullptr;
  if(state==2)++snapshot_.blocks;
  touched_=0;if(clock_){block_start_=mark_=all_start_;}
 }
 void visual_work(unsigned ticks){visual_block_+=ticks;snapshot_.visual_ticks+=ticks;}
 void control_end(unsigned generation,bool snapshot){if(!all_clock_)return;unsigned elapsed=all_clock_()-all_start_;snapshot_.control_ticks+=elapsed;++snapshot_.control_calls;if(elapsed>snapshot_.control_max)snapshot_.control_max=elapsed;if(snapshot && elapsed>snapshot_.snapshot_control_max)snapshot_.snapshot_control_max=elapsed;
  if(generation_ && generation!=generation_){clock_=nullptr;}if(clock_)++snapshot_.sampled;
 }
 Clock clock()const{return clock_;}
 Clock control_clock()const{return all_clock_;}
 void va_trigger(unsigned ticks){va_trigger_block_+=ticks;++snapshot_.va_trigger_calls;snapshot_.va_trigger_ticks+=ticks;if(ticks>snapshot_.va_trigger_max)snapshot_.va_trigger_max=ticks;}
 void va_parameters(unsigned ticks){va_parameter_block_+=ticks;++snapshot_.va_parameter_calls;snapshot_.va_parameter_ticks+=ticks;if(ticks>snapshot_.va_parameter_max)snapshot_.va_parameter_max=ticks;}
 uint32_t stamp()const{return clock_?clock_():0;}
 void record(unsigned bin,uint32_t elapsed,bool active=true){if(!clock_ || bin>=bins)return;auto& s=scratch_[bin];if(!s.calls)touched_bins_[touched_++]=bin;s.ticks+=elapsed;++s.calls;if(active){s.active_ticks+=elapsed;++s.frames;}}
 void native_sample(unsigned track,unsigned bin,unsigned elapsed,bool active){if(!clock_ || track>=16 || !active)return;auto& c=native_scratch_[track];c.ticks+=elapsed;++c.frames;native_[track].bin=bin;}
 void cut(Stage stage,unsigned frames){if(!clock_)return;uint32_t now=clock_();auto& c=counters_[stage_base+stage];uint32_t elapsed=now-mark_;c.ticks+=elapsed;c.active_ticks+=elapsed;c.frames+=frames;++c.calls;if(elapsed>c.max_block_ticks)c.max_block_ticks=elapsed;mark_=now;}
 void population(unsigned main,unsigned poly,const uint8_t reasons[16]){if(!clock_)return;if(main>snapshot_.main_peak)snapshot_.main_peak=main;if(poly>snapshot_.poly_peak)snapshot_.poly_peak=poly;for(unsigned i=0;i<16;++i)if(reasons[i])snapshot_.layer_reasons[i]=reasons[i];}
 void block_end(unsigned frames){if(all_clock_ && measuring_){unsigned elapsed=all_clock_()-all_start_;last_={elapsed,visual_block_,va_trigger_block_,va_parameter_block_,snapshot_.poly_peak};if(elapsed>snapshot_.all_block_max){snapshot_.all_block_max=elapsed;snapshot_.peak_visual=visual_block_;snapshot_.peak_va_trigger=va_trigger_block_;snapshot_.peak_va_parameters=va_parameter_block_;}if(visual_block_>snapshot_.visual_block_max)snapshot_.visual_block_max=visual_block_;}if(!clock_)return;uint32_t elapsed=clock_()-block_start_;auto& total=counters_[stage_base+Total];total.ticks+=elapsed;total.frames+=frames;++total.calls;if(elapsed>total.max_block_ticks)total.max_block_ticks=elapsed;
  for(unsigned n=0;n<touched_;++n){unsigned i=touched_bins_[n];auto& c=counters_[i];auto& b=scratch_[i];c.ticks+=b.ticks;c.active_ticks+=b.active_ticks;c.frames+=b.frames;c.calls+=b.calls;if(b.ticks>c.max_block_ticks)c.max_block_ticks=uint32_t(b.ticks);if(b.frames){uint64_t ratio=(b.active_ticks<<16)/b.frames;if(ratio>c.max_voice_q16)c.max_voice_q16=ratio;}b={};}
  for(unsigned t=0;t<16;++t){auto& b=native_scratch_[t];if(b.frames){auto& c=native_[t];c.ticks+=b.ticks;c.frames+=b.frames;++c.sampled_blocks;if(b.ticks>c.max_block_ticks)c.max_block_ticks=unsigned(b.ticks);b={};}}
  clock_=nullptr;
 }
private:
 struct Block {uint64_t ticks=0,active_ticks=0;unsigned frames=0,calls=0;};
 LastBlock last_{};bool measuring_=false;unsigned measure_requested_=0;
 unsigned visual_block_=0,va_trigger_block_=0,va_parameter_block_=0;
 unsigned state_=0,stride_=0,touched_=0,touched_bins_[bins]{};Clock requested_=nullptr,clock_=nullptr,all_clock_=nullptr;unsigned generation_=0;uint32_t mark_=0,block_start_=0,all_start_=0;
 NativeCounter native_[16]{};Block native_scratch_[16]{};
 Counter counters_[bins]{};Block scratch_[bins]{};Snapshot snapshot_{};
};
class DspTimingScope {
 DspProfile& profile_;DspProfile::Clock clock_;unsigned bin_,native_track_;uint32_t start_;bool active_;
public:
 DspTimingScope(DspProfile& p,unsigned bin,bool active=true,unsigned native_track=16):profile_(p),clock_(p.clock()),bin_(bin),native_track_(native_track),start_(clock_?clock_():0),active_(active){}
 ~DspTimingScope(){if(clock_){unsigned elapsed=clock_()-start_;profile_.record(bin_,elapsed,active_);profile_.native_sample(native_track_,bin_,elapsed,active_);}}
};
}
