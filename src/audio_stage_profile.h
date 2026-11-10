#pragma once
#include <stdint.h>

namespace groove {
// Optional sampled timing. One audio owner writes; the task reads snapshots.
class AudioStageProfile {
public:
 using Clock=uint32_t (*)();
 static_assert(__atomic_always_lock_free(sizeof(Clock),nullptr) && __atomic_always_lock_free(sizeof(uint64_t),nullptr),"audio profiling requires lock-free atomics");
 struct Snapshot {uint32_t calls=0;uint64_t control=0,events=0,tracks=0,output=0;uint32_t max_control=0,max_events=0,max_tracks=0,max_output=0,max_total=0,peak_control=0,peak_events=0,peak_tracks=0,peak_output=0;};
 void begin(Clock clock,unsigned interval=64){__atomic_store_n(&interval_,interval?interval:64,__ATOMIC_RELAXED);__atomic_store_n(&clock_,nullptr,__ATOMIC_RELEASE);__atomic_store_n(&stride_,0u,__ATOMIC_RELAXED);__atomic_store_n(&calls_,0u,__ATOMIC_RELAXED);for(unsigned i=0;i<4;++i){__atomic_store_n(&total_[i],uint64_t(0),__ATOMIC_RELAXED);__atomic_store_n(&max_[i],0u,__ATOMIC_RELAXED);__atomic_store_n(&peak_[i],0u,__ATOMIC_RELAXED);}__atomic_store_n(&max_total_,0u,__ATOMIC_RELAXED);__atomic_store_n(&clock_,clock,__ATOMIC_RELEASE);}
 Clock sample(){Clock clock=__atomic_load_n(&clock_,__ATOMIC_ACQUIRE);if(!clock)return nullptr;unsigned stride=__atomic_fetch_add(&stride_,1u,__ATOMIC_RELAXED);unsigned interval=__atomic_load_n(&interval_,__ATOMIC_RELAXED);return (interval==64?(stride&63u):stride%interval)==0?clock:nullptr;}
 void record(uint32_t control,uint32_t events,uint32_t tracks,uint32_t output){const uint32_t values[]={control,events,tracks,output};uint32_t sum=0;for(unsigned i=0;i<4;++i){__atomic_add_fetch(&total_[i],uint64_t(values[i]),__ATOMIC_RELAXED);unsigned old=__atomic_load_n(&max_[i],__ATOMIC_RELAXED);if(values[i]>old)__atomic_store_n(&max_[i],values[i],__ATOMIC_RELAXED);sum+=values[i];}if(sum>__atomic_load_n(&max_total_,__ATOMIC_RELAXED)){for(unsigned i=0;i<4;++i)__atomic_store_n(&peak_[i],values[i],__ATOMIC_RELAXED);__atomic_store_n(&max_total_,sum,__ATOMIC_RELAXED);}__atomic_add_fetch(&calls_,1u,__ATOMIC_RELEASE);}
 Snapshot end(){__atomic_store_n(&clock_,nullptr,__ATOMIC_RELEASE);Snapshot s;s.calls=__atomic_load_n(&calls_,__ATOMIC_ACQUIRE);s.control=__atomic_load_n(&total_[0],__ATOMIC_RELAXED);s.events=__atomic_load_n(&total_[1],__ATOMIC_RELAXED);s.tracks=__atomic_load_n(&total_[2],__ATOMIC_RELAXED);s.output=__atomic_load_n(&total_[3],__ATOMIC_RELAXED);s.max_control=__atomic_load_n(&max_[0],__ATOMIC_RELAXED);s.max_events=__atomic_load_n(&max_[1],__ATOMIC_RELAXED);s.max_tracks=__atomic_load_n(&max_[2],__ATOMIC_RELAXED);s.max_output=__atomic_load_n(&max_[3],__ATOMIC_RELAXED);s.max_total=__atomic_load_n(&max_total_,__ATOMIC_RELAXED);s.peak_control=__atomic_load_n(&peak_[0],__ATOMIC_RELAXED);s.peak_events=__atomic_load_n(&peak_[1],__ATOMIC_RELAXED);s.peak_tracks=__atomic_load_n(&peak_[2],__ATOMIC_RELAXED);s.peak_output=__atomic_load_n(&peak_[3],__ATOMIC_RELAXED);return s;}
private:
 Clock clock_=nullptr;uint32_t interval_=64,stride_=0,calls_=0,max_[4]{},max_total_=0,peak_[4]{};uint64_t total_[4]{};
};
}
