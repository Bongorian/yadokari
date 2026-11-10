#pragma once
#include <stdint.h>
namespace groove {
// One task publishes context. Worker and IRQ have separate single-writer bins.
// No reset, allocation, lock, clock call or text output in either audio path.
class AudioPhaseTiming {
public:
 enum Phase:unsigned {Boot,Idle,File,Save,Play,UiProbe,DspProbe,Restore,Count};
 struct Stats {uint32_t calls=0,maximum=0,over500=0,first_at=0;};
 static const char* name(unsigned p){static const char* n[]={"BOOT","IDLE","FILE","SAVE","PLAY","UI-PROBE","DSP-PROBE","RESTORE"};return p<Count?n[p]:"UNKNOWN";}
 void phase(unsigned p){p=p<Count?p:Idle;if(p!=__atomic_load_n(&phase_,__ATOMIC_RELAXED))__atomic_store_n(&phase_,p,__ATOMIC_RELEASE);}
 unsigned phase()const{return __atomic_load_n(&phase_,__ATOMIC_ACQUIRE);}
 void worker(uint32_t elapsed,uint32_t at){record(worker_,elapsed,at);}
 void worker_in(unsigned p,uint32_t elapsed,uint32_t at){record_in(worker_,p<Count?p:Idle,elapsed,at);}
 void callback(uint32_t at){if(seen_)record(callback_,at-last_,at);last_=at;seen_=true;}
 Stats worker_stats(unsigned p)const{return snapshot(worker_,p);}
 Stats callback_stats(unsigned p)const{return snapshot(callback_,p);}
private:
 void record(Stats* rows,uint32_t elapsed,uint32_t at){record_in(rows,phase(),elapsed,at);}
 void record_in(Stats* rows,unsigned p,uint32_t elapsed,uint32_t at){auto& s=rows[p];__atomic_add_fetch(&s.calls,1u,__ATOMIC_RELAXED);if(elapsed>__atomic_load_n(&s.maximum,__ATOMIC_RELAXED))__atomic_store_n(&s.maximum,elapsed,__ATOMIC_RELAXED);if(elapsed>500){if(!__atomic_load_n(&s.over500,__ATOMIC_RELAXED))__atomic_store_n(&s.first_at,at,__ATOMIC_RELAXED);__atomic_add_fetch(&s.over500,1u,__ATOMIC_RELEASE);}}
 static Stats snapshot(const Stats* rows,unsigned p){if(p>=Count)return {};const auto& s=rows[p];return {__atomic_load_n(&s.calls,__ATOMIC_RELAXED),__atomic_load_n(&s.maximum,__ATOMIC_RELAXED),__atomic_load_n(&s.over500,__ATOMIC_ACQUIRE),__atomic_load_n(&s.first_at,__ATOMIC_RELAXED)};}
 unsigned phase_=Boot;Stats worker_[Count]{},callback_[Count]{};uint32_t last_=0;bool seen_=false;
};
}
