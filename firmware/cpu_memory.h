#pragma once
#include "../src/alloc_policy.h"
#include <circle/memory.h>
#include <circle/synchronize.h>
// Explicit CPU ownership only. Never use this allocator for buffers submitted
// to USB, SD, Ethernet, mailbox or other DMA controllers. HEAP_ANY stays DMA-safe.
struct GrooveCpuNew {};
inline void* operator new(size_t n,GrooveCpuNew) noexcept {
 void* p=CMemorySystem::HeapTryAllocate(n,HEAP_CPU);
 if(!p)p=CMemorySystem::HeapTryAllocate(n,HEAP_HIGH);
 return p?p:CMemorySystem::HeapTryAllocate(n,HEAP_LOW);
}
namespace groove {
template<class T,class... Args> inline T* alloc_cpu_new(Args&&... args){return new(GrooveCpuNew{}) T(args...);}
}
// Bounded cooperative check of exclusively owned CPU-heap allocations. Each
// service touches at most 256KiB, outside render, with no writes to user data.
class CCpuMemoryCheck {
 static constexpr size_t chunk=64u*1024*1024,step=256u*1024;
 void* blocks_[15]{};unsigned count_=0,index_=0;size_t offset_=0,bytes_=0,headroom_=0;
 bool reading_=false,done_=false,ok_=false;
 static uint64_t Pattern(uintptr_t p){return uint64_t(p)*0x9e3779b97f4a7c15ULL ^ 0xc35a690ff096a53cULL;}
 void Release(){for(unsigned n=count_;n;){auto& p=blocks_[--n];if(p){CMemorySystem::HeapFree(p);p=nullptr;}}}
public:
 ~CCpuMemoryCheck(){Release();}
 bool start(){
  if(done_ || count_ || !CMemorySystem::GetCpuMemSize())return false;
  headroom_=CMemorySystem::Get()->GetHeapFreeSpace(HEAP_CPU);
  for(;count_<15 && CMemorySystem::HeapCanAllocate(chunk,1,8u*1024*1024,HEAP_CPU);++count_){blocks_[count_]=CMemorySystem::HeapTryAllocate(chunk,HEAP_CPU);if(!blocks_[count_])break;bytes_+=chunk;}
  if(!count_){done_=true;return false;}return true;
 }
 void service(){
  if(done_ || !count_)return;
  auto* p=reinterpret_cast<volatile uint64_t*>(static_cast<unsigned char*>(blocks_[index_])+offset_);
  for(size_t n=0;n<step/sizeof(uint64_t);++n){auto expected=Pattern(reinterpret_cast<uintptr_t>(p+n));if(reading_){if(p[n]!=expected){done_=true;ok_=false;Release();return;}}else p[n]=expected;}
  // Force the subsequent read pass to reach physical memory, not only cache.
  if(!reading_)CleanAndInvalidateDataCacheRange(reinterpret_cast<uintptr_t>(p),step);
  offset_+=step;if(offset_==chunk){offset_=0;if(++index_==count_){index_=0;if(reading_){done_=true;Release();ok_=CMemorySystem::Get()->GetHeapFreeSpace(HEAP_CPU)==headroom_;}else reading_=true;}}
 }
 bool done()const{return done_;}bool ok()const{return ok_;}size_t bytes()const{return bytes_;}
};
