#pragma once
namespace groove {
// Fixed storage, published once at configuration. Release before invoking the
// client callback: that callback is allowed to submit its next request.
template<class T,unsigned Capacity> class UsbRequestPool {
public:
 T& at(unsigned i){return slots_[i];}
 T* acquire(){for(unsigned i=0;i<Capacity;++i){unsigned free=0;if(__atomic_compare_exchange_n(&used_[i],&free,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return &slots_[i];}return nullptr;}
 bool release(T* slot){for(unsigned i=0;i<Capacity;++i)if(slot==&slots_[i])return __atomic_exchange_n(&used_[i],0u,__ATOMIC_RELEASE)==1;return false;}
 bool busy()const{for(unsigned i=0;i<Capacity;++i)if(__atomic_load_n(&used_[i],__ATOMIC_ACQUIRE))return true;return false;}
private:T slots_[Capacity]{};unsigned used_[Capacity]{};
};
}
