#pragma once
#include "render_audit.h"
#include "alloc_policy.h"
namespace groove {
class AudioSlowTrace {
public:
 struct Event {unsigned at,elapsed,phase_start,phase_end;RenderAudit::Sample work;unsigned poly_peak=0,dsp_ticks=0,dsp_visual=0,dsp_trigger=0,dsp_parameters=0;};
 ~AudioSlowTrace(){delete[] events_;}
 bool begin(){unsigned s=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);if(s==1)return true;if(s==2)return false;if(!events_ && allocation_admitted(sizeof(Event)*512))events_=alloc_array<Event>(512);if(!events_)return false;count_=read_=lost_=0;__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);return true;}
 void seal(){unsigned expected=1;__atomic_compare_exchange_n(&state_,&expected,2u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
 bool worker_enter(){unsigned s=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);if(s==2){__atomic_store_n(&state_,3u,__ATOMIC_RELEASE);return false;}return s==1;}
 void worker_exit(bool enabled,const Event& event){if(enabled && event.elapsed>500 && (event.phase_end!=6 || event.work.command_count || (event.poly_peak>0 && event.poly_peak<=16))){if(count_<512)events_[count_++]=event;else ++lost_;}acknowledge();}
 void acknowledge(){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)==2)__atomic_store_n(&state_,3u,__ATOMIC_RELEASE);}
 bool quiescent()const{unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);return state==0 || state==3;}
 bool ready()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==3;}
 const Event* next(){return ready() && read_<count_?&events_[read_++]:nullptr;}
 unsigned count()const{return count_;}unsigned lost()const{return lost_;}
private:Event* events_=nullptr;unsigned state_=0,count_=0,read_=0,lost_=0;
};
}
