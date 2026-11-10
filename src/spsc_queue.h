#pragma once
namespace groove {
// Shared construction of large value-record arrays; no cursors or ownership.
template<class T,unsigned N> struct FixedRecordStorage {T values[N]{};};
// Single producer (UI) and single consumer (audio), one slot reserved.
template<class T, unsigned N> class SpscQueue {
 static_assert(N>1,"queue must have room for data");
 static_assert(__atomic_always_lock_free(sizeof(unsigned), nullptr),"audio queue requires lock-free atomics");
public:
 bool push(const T& v) { unsigned w=__atomic_load_n(&write_,__ATOMIC_RELAXED), next=(w+1)%N; if(next==__atomic_load_n(&read_,__ATOMIC_ACQUIRE)) return false; data_.values[w]=v; __atomic_store_n(&write_,next,__ATOMIC_RELEASE); return true; }
 // Zero-copy producer claim (SPEC section 5, O-02 stage 1): the producer
 // fills the next slot in place and publishes it with push_commit(). The
 // claimed slot is never touched by the consumer, and the producer must
 // not touch it again after commit. Returns nullptr when the queue is
 // full. Producer thread only, like push().
 T* push_begin() { unsigned w=__atomic_load_n(&write_,__ATOMIC_RELAXED), next=(w+1)%N; if(next==__atomic_load_n(&read_,__ATOMIC_ACQUIRE)) return nullptr; return &data_.values[w]; }
 void push_commit() { unsigned w=__atomic_load_n(&write_,__ATOMIC_RELAXED); __atomic_store_n(&write_,(w+1)%N,__ATOMIC_RELEASE); }
 bool peek(T& v)const{unsigned r=__atomic_load_n(&read_,__ATOMIC_RELAXED);if(r==__atomic_load_n(&write_,__ATOMIC_ACQUIRE))return false;v=data_.values[r];return true;}
 bool pop(T& v) { unsigned r=__atomic_load_n(&read_,__ATOMIC_RELAXED); if(r==__atomic_load_n(&write_,__ATOMIC_ACQUIRE)) return false; v=data_.values[r]; __atomic_store_n(&read_,(r+1)%N,__ATOMIC_RELEASE); return true; }
 // Zero-copy consumer access: peek_front() exposes the head element in
 // place; the producer can never write that slot while it is unconsumed
 // (one slot always stays reserved), so the reference stays valid until
 // pop_advance(). Consumer thread only, like pop().
 const T* peek_front() const { unsigned r=__atomic_load_n(&read_,__ATOMIC_RELAXED); if(r==__atomic_load_n(&write_,__ATOMIC_ACQUIRE)) return nullptr; return &data_.values[r]; }
 void pop_advance() { unsigned r=__atomic_load_n(&read_,__ATOMIC_RELAXED); __atomic_store_n(&read_,(r+1)%N,__ATOMIC_RELEASE); }
 // Consumer-only fence: entries enqueued after this snapshot remain beyond
 // the boundary even when the producer wraps, because unread slots cannot be overwritten.
 unsigned consumer_boundary()const{return __atomic_load_n(&write_,__ATOMIC_ACQUIRE);}
 bool consumer_at_boundary(unsigned boundary)const{return __atomic_load_n(&read_,__ATOMIC_RELAXED)==boundary;}
 unsigned free_space() const {unsigned w=__atomic_load_n(&write_,__ATOMIC_RELAXED),r=__atomic_load_n(&read_,__ATOMIC_ACQUIRE);return (r+N-w-1)%N;}
private: FixedRecordStorage<T,N> data_;
 // The two cursors live on separate cache lines (SPEC section 8, item m4):
 // the producer only stores write_ and the consumer only stores read_, so
 // neither core's stores invalidate the other core's cursor line.
 alignas(64) unsigned read_=0; alignas(64) unsigned write_=0;
};
}
