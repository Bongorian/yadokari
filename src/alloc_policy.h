#pragma once
// Keep this header free of C++ standard-library includes outside the
// hosted branch: Circle builds with -nostdinc++, where <new>/<utility>
// do not resolve (and its -MG dependency pass would record them as
// unbuildable bare prerequisites). <stddef.h> is a C header and is safe.
#include <stddef.h>
#if defined(__linux__)
#include <new>
#else
#include <circle/new.h>
// A noexcept placement form makes the compiler check null before construction.
struct GrooveFallibleNew {};
inline void* operator new(size_t n,GrooveFallibleNew) noexcept {return CMemorySystem::HeapTryAllocate(n,HEAP_DEFAULT_NEW);}
inline void* operator new[](size_t n,GrooveFallibleNew) noexcept {return CMemorySystem::HeapTryAllocate(n,HEAP_DEFAULT_NEW);}
#endif
namespace groove {
// Allocation policy (SPEC section 8, item M4) — the single rule for all
// groove code in src/ and firmware/:
//
//  * Every fallible allocation goes through alloc_new / alloc_array below.
//    Failure is reported as nullptr identically on every target: on hosted
//    builds the helpers use nothrow new (plain new would throw instead,
//    which made the null checks already present in the file/session layer
//    unreachable there), and under Circle plain new already returns
//    nullptr through the explicit fallible heap API, preserving the reserve.
//  * The caller must check the result before any use and fail the current
//    operation cleanly, leaving previously valid state intact.
//    Dereferencing an unchecked allocation result is a policy violation.
//  * Allocations stay out of the audio callback; these helpers are for the
//    control/file/worker paths that already allocate today.
//  * Release with the matching delete / delete[] (the alloc_delete helpers
//    also null the pointer so a stale handle cannot be reused).
// Remaining legacy plain-new call sites are inventoried in
// docs/allocation-policy.md and are being migrated in stages.
// Arguments are passed as lvalues (no std::forward, see the include note
// above); every current call site passes lvalues or literals binding to
// value parameters, and move-only construction arguments are not used.
#if defined(__linux__)
template<class T,class... Args> inline T* alloc_new(Args&&... args){return new(std::nothrow) T(args...);}
template<class T> inline T* alloc_array(size_t count){return new(std::nothrow) T[count]();}
inline unsigned char* alloc_uninitialized_bytes(size_t count){return new(std::nothrow) unsigned char[count];}
#else
template<class T,class... Args> inline T* alloc_new(Args&&... args){return new(GrooveFallibleNew{}) T(args...);}
template<class T> inline T* alloc_array(size_t count){return new(GrooveFallibleNew{}) T[count]();}
inline unsigned char* alloc_uninitialized_bytes(size_t count){return new(GrooveFallibleNew{}) unsigned char[count];}
#endif
// Admission includes reusable buckets on Circle; allocation itself remains fallible.
using AllocationAdmissionHook=bool(*)(size_t,unsigned,size_t);
inline AllocationAdmissionHook allocation_admission_hook=nullptr;
inline bool allocation_admitted(size_t bytes,unsigned count=1,size_t headroom=8u*1024u*1024u){
 if(allocation_admission_hook)return allocation_admission_hook(bytes,count,headroom);
#if defined(__linux__)
 return count && bytes<=size_t(-1)/count;
#else
 return CMemorySystem::HeapCanAllocate(bytes,count,headroom,HEAP_DEFAULT_NEW);
#endif
}
template<class T> inline void alloc_delete(T*& p){delete p;p=nullptr;}
template<class T> inline void alloc_delete_array(T*& p){delete[] p;p=nullptr;}
}
