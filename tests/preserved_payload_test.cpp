#include "preserved_payload.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
struct Copier {
 unsigned calls=0,fail_at=0;size_t bytes=0;
 static bool copy(void* p,void* dst,const void* src,size_t n){auto& c=*static_cast<Copier*>(p);++c.calls;if(c.calls==c.fail_at)return false;c.bytes+=n;std::memcpy(dst,src,n);return true;}
};
int main(){using namespace groove;
 std::vector<uint8_t> bytes(65536,0xa5);PreservedStore source,destination;
 assert(source.tracks[3].engn.set(bytes.data(),bytes.size()));
 assert(source.tracks[3].extra_sub.set(bytes.data(),bytes.size()));source.tracks[3].extra_for=7;
 assert(source.sounds[4].engn.set(bytes.data(),bytes.size()));
 assert(source.add_chunk(0x1111,9,13,bytes.data(),bytes.size()));
 assert(source.add_chunk(0x2222,4,21,bytes.data(),bytes.size()));source.dropped_unknown=true;
 uint8_t old[]={1,2,3};assert(destination.add_chunk(0x3333,3,5,old,sizeof old));
 // Failure after the first complete top-level chunk used to leak it: the
 // temporary store did not yet count its owned chunks. ASan/LSan covers this.
 for(unsigned fail=1;fail<=5;++fail){Copier c;c.fail_at=fail;assert(!destination.copy_from(source,Copier::copy,&c));assert(destination.chunk_count==1 && destination.used_bytes()==3 && destination.chunks[0].kind==0x3333 && !std::memcmp(destination.chunks[0].payload.data,old,3));}
 Copier c;assert(destination.copy_from(source,Copier::copy,&c));assert(c.calls==5 && c.bytes==5*bytes.size());assert(destination.dropped_unknown && destination.chunks[1].flags==21 && destination.tracks[3].extra_for==7);
 source.chunks[0].payload.data[0]=0;assert(destination.chunks[0].payload.data[0]==0xa5);
 destination.clear();source.clear();bytes.resize(PreservedStore::kMaxEntryBytes,0x5a);
 for(unsigned i=0;i<4;++i)assert(source.add_chunk(i,1,0,bytes.data(),bytes.size()));
 Copier maximum;assert(destination.copy_from(source,Copier::copy,&maximum) && maximum.bytes==PreservedStore::kMaxTotalBytes);
 assert(!source.add_chunk(5,1,0,old,1));
 std::puts("Preserved payload: transactional failure cleanup, deep ownership, metadata and 8 MiB budget passed");
}
