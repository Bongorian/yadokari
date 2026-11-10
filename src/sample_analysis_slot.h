#pragma once
#include "sample_analysis.h"

namespace groove {
// One task-side producer and one worker. Resident sample memory must remain
// immutable and alive until the job has been collected, including if stale.
class SampleAnalysisSlot {
 static_assert(__atomic_always_lock_free(sizeof(unsigned),nullptr),"sample analysis requires lock-free state");
public:
 bool submit(WavView view,float start,float end,uint32_t token){if(!view.data || view.reader || !view.frames || !view.rate || (view.channels!=1 && view.channels!=2) || !(start>=0 && start<end && end<=1) || __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0)return false;view_=view;start_=start;end_=end;token_=token;__atomic_store_n(&cancel_,0u,__ATOMIC_RELAXED);__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
  return true;}
 bool process_one(){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;analyze_sample_until(view_,result_,start_,end_,[this](){return __atomic_load_n(&cancel_,__ATOMIC_RELAXED)!=0;});__atomic_store_n(&state_,2u,__ATOMIC_RELEASE);return true;}
 bool collect(SampleAnalysis& result,uint32_t& token){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=2)return false;result=result_;token=token_;__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;}
 void cancel(){__atomic_store_n(&cancel_,1u,__ATOMIC_RELAXED);}
 bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
 bool ready()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==2;}
private:
 WavView view_{};float start_=0,end_=1;uint32_t token_=0;SampleAnalysis result_{};unsigned state_=0,cancel_=0;
};
}
