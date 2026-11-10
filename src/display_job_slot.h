#pragma once
#include "display_rectangle.h"
#include "alloc_policy.h"
namespace groove {
struct DisplayJob {
 const uint32_t* pixels=nullptr;const uint16_t* shadow=nullptr;uint8_t* packet=nullptr;
 bool valid[17]{},compact=false,coalesce=false;unsigned side=0,cursor=0,epoch=0,frame=0,max_rows=32;ui::PixelOrder order=ui::PixelOrder::RGB;
 unsigned acknowledged=0,ack_first=0,ack_rows=0;
 uint32_t (*clock)()=nullptr;unsigned worker_begin=0,worker_end=0,scan_us=0,encode_us=0,coalesce_us=0,coalesce_attempted=0;
 PreparedDisplayRect result{};
};
// Exactly one producer and one worker. The owner keeps every referenced buffer
// alive and unchanged until collect(). No Circle or USB calls occur on worker.
class DisplayJobSlot {
 static_assert(__atomic_always_lock_free(sizeof(unsigned),nullptr),"display jobs require lock-free state");
 public:
  DisplayJobSlot()=default;DisplayJobSlot(const DisplayJobSlot&)=delete;DisplayJobSlot& operator=(const DisplayJobSlot&)=delete;
  ~DisplayJobSlot(){delete[] alternate_;}
  bool enable_coalescing(){if(!alternate_ && allocation_admitted(kDisplayPacketCapacity))alternate_=alloc_array<uint8_t>(kDisplayPacketCapacity);return alternate_!=nullptr;}
  bool submit(const DisplayJob& job){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0)return false;job_=job;__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
   // Publish the job to the coherency domain before waking a WFE waiter.
   asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
   return true;}
  bool process_one(){
   if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;
   job_.coalesce_attempted=0;auto clock=job_.clock;job_.worker_begin=clock?clock():0;unsigned mark=job_.worker_begin;DisplayScanPlan* plan=nullptr;
   if(job_.side<2 && job_.pixels && job_.shadow && job_.cursor<17){
    auto& cached=cache_[job_.side];
    if(!cached.ready || cached.epoch!=job_.epoch || cached.frame!=job_.frame || cached.pixels!=job_.pixels || cached.shadow!=job_.shadow || cached.order!=job_.order){
     cached.plan.scan(job_.pixels,job_.shadow,job_.valid,job_.order);cached.ready=true;cached.epoch=job_.epoch;cached.frame=job_.frame;cached.pixels=job_.pixels;cached.shadow=job_.shadow;cached.order=job_.order;
    }else if(cached.acknowledged!=job_.acknowledged){
     cached.plan.acknowledged(job_.ack_first,job_.ack_rows);
    }
    cached.acknowledged=job_.acknowledged;plan=&cached.plan;
   }
   if(clock){unsigned now=clock();job_.scan_us=now-mark;mark=now;}
   job_.result=prepare_display_rectangle(job_.side,job_.pixels,job_.shadow,job_.valid,job_.cursor,job_.order,job_.packet,job_.max_rows,job_.compact,plan);
   if(clock){unsigned now=clock();job_.encode_us=now-mark;mark=now;}
   if(job_.coalesce && alternate_ && plan && job_.result.dirty && !job_.result.last){
    DisplayScanPlan combined=*plan;unsigned first=job_.result.first,last=first;int left=480,right=-1;
    for(unsigned b=first;b<17;++b)if(plan->right[b]>=plan->left[b]){last=b;if(plan->left[b]<left)left=plan->left[b];if(plan->right[b]>right)right=plan->right[b];}
    // Reject expensive speculation before encoding. A raw upper bound
    // proves the full union fits both the packet and the existing byte budget.
    const uint64_t raw_bound=uint64_t(last-first+1)*16*unsigned(((right+4)&~3)-(left&~3))*2+28;
    if(last>first && raw_bound<=kDisplayPacketCapacity && raw_bound<=uint64_t(job_.result.bytes)+2048){++job_.coalesce_attempted;for(unsigned b=first;b<=last;++b){combined.left[b]=left;combined.right[b]=right;}
     auto candidate=prepare_display_rectangle(job_.side,job_.pixels,job_.shadow,job_.valid,job_.cursor,job_.order,alternate_,272,job_.compact,&combined);
     if(candidate.valid && candidate.dirty && candidate.rows>job_.result.rows && candidate.bytes<=job_.result.bytes+2048){__builtin_memcpy(job_.packet,alternate_,candidate.bytes);job_.result=candidate;}
    }
   }
   if(clock){job_.worker_end=clock();job_.coalesce_us=job_.worker_end-mark;}
   __atomic_store_n(&state_,2u,__ATOMIC_RELEASE);return true;
  }
  bool collect(DisplayJob& job){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=2)return false;job=job_;__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;}
  bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
  bool ready()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==2;}
 private:
  struct CachedScan {DisplayScanPlan plan{};const uint32_t* pixels=nullptr;const uint16_t* shadow=nullptr;unsigned epoch=0,frame=0,acknowledged=0;ui::PixelOrder order=ui::PixelOrder::RGB;bool ready=false;} cache_[2];
  uint8_t* alternate_=nullptr;
  DisplayJob job_{};unsigned state_=0;
};
}
