#pragma once
#include <stdint.h>
namespace groove {
// One callback owner records starts; task-side reads use atomic snapshots.
class CallbackGap {
public:
 struct Stats {uint32_t maximum,over500,over1000;};
 void record(uint32_t now){
  if(seen_){uint32_t gap=now-last_;if(gap>500)__atomic_add_fetch(&over500_,1u,__ATOMIC_RELAXED);if(gap>1000)__atomic_add_fetch(&over1000_,1u,__ATOMIC_RELAXED);uint32_t old=__atomic_load_n(&maximum_,__ATOMIC_RELAXED);while(gap>old && !__atomic_compare_exchange_n(&maximum_,&old,gap,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){} }
  last_=now;seen_=true;
 }
 Stats stats()const{return {__atomic_load_n(&maximum_,__ATOMIC_RELAXED),__atomic_load_n(&over500_,__ATOMIC_RELAXED),__atomic_load_n(&over1000_,__ATOMIC_RELAXED)};}
private:
 uint32_t last_=0,maximum_=0,over500_=0,over1000_=0;bool seen_=false;
};
}
