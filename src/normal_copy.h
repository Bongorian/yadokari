#pragma once
#include <stddef.h>
namespace groove {
// Owned, non-overlapping Normal-memory objects only; never MMIO. Circle's
// generic memcpy uses a byte loop for either pointer not aligned to eight.
// Bounded constant-size copies permit the target compiler's unaligned loads.
__attribute__((noinline,optimize("no-tree-loop-distribute-patterns")))
inline void copy_normal_bytes(void* destination,const void* source,size_t bytes){
 auto* dst=static_cast<unsigned char*>(destination);auto* src=static_cast<const unsigned char*>(source);
 while(bytes>=64){__builtin_memcpy(dst,src,64);dst+=64;src+=64;bytes-=64;}
 if(bytes)__builtin_memcpy(dst,src,bytes);
}
template<class T> inline void copy_normal_record(T& destination,const T& source){
 static_assert(__is_trivially_copyable(T),"Normal-memory record copy requires value state");
 copy_normal_bytes(&destination,&source,sizeof(T));
}
}
