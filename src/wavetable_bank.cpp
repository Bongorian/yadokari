#include "wavetable_bank.h"
#include "alloc_policy.h"
#include "project_codec.h"
namespace groove {
#include "generated/akwf_tables.inc"
const int16_t (*kWavetableData)[kWavetableLevels][kWavetableSamples]=nullptr;
namespace {
struct BankStorage {unsigned char* bytes=nullptr;unsigned state=0;~BankStorage(){delete[] bytes;}} storage;
}
bool prepare_wavetable_bank(){
 if(__atomic_load_n(&storage.state,__ATOMIC_ACQUIRE)==2)return true;
 unsigned expected=0;if(!__atomic_compare_exchange_n(&storage.state,&expected,1u,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return false;
 constexpr unsigned samples=kWavetableWaves*kWavetableLevels*kWavetableSamples,bytes=samples*sizeof(int16_t);
 auto* raw=allocation_admitted(bytes)?alloc_uninitialized_bytes(bytes):nullptr;bool ok=raw!=nullptr;unsigned at=0;ProjectCrc crc;
 for(unsigned i=0;ok && i<samples;){
  int state[4]={0,0,0,0};
  for(unsigned group=0;ok && group<16;++group){
   if(at==sizeof kWavetablePacked){ok=false;break;}
   const unsigned exponent=kWavetablePacked[at++];
   if(exponent>19){ok=false;break;}
   unsigned bit=8;uint8_t packed=0;
   auto read_bit=[&]()->unsigned{
    if(bit==8){if(at==sizeof kWavetablePacked){ok=false;return 0;}packed=kWavetablePacked[at++];bit=0;}
    return (packed>>bit++)&1u;
   };
   for(unsigned n=0;ok && n<16;++n,++i){
    uint32_t quotient=0;
    while(ok && !read_bit())if(++quotient>(1048560u>>exponent)){ok=false;break;}
    uint32_t code=quotient<<exponent;
    for(unsigned shift=0;ok && shift<exponent;++shift)code|=read_bit()<<shift;
    if(!ok || code>1048560u){ok=false;break;}
    int decoded=(code&1)?-int(code>>1)-1:int(code>>1);
    // Validate every derivative before the next addition, including corrupt ROM.
    const int limits[4]={32768,65535,131070,262140};
    for(int level=3;level>=0;--level){
     decoded+=state[level];
     if(decoded< -limits[level] || decoded>limits[level] || (level==0 && decoded==32768)){ok=false;break;}
     state[level]=decoded;
    }
    if(!ok)break;
    uint16_t word=uint16_t(int16_t(decoded));raw[2*i]=uint8_t(word);raw[2*i+1]=uint8_t(word>>8);crc.add(raw[2*i]);crc.add(raw[2*i+1]);
   }
  }
 }
 ok=ok && at==sizeof kWavetablePacked && crc.value()==kWavetableCrc;
 if(!ok){delete[] raw;__atomic_store_n(&storage.state,0u,__ATOMIC_RELEASE);return false;}
 storage.bytes=raw;kWavetableData=reinterpret_cast<const int16_t (*)[kWavetableLevels][kWavetableSamples]>(raw);
 __atomic_store_n(&storage.state,2u,__ATOMIC_RELEASE);return true;
}
}
