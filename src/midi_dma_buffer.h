#pragma once
#include <stdint.h>
#include "alloc_policy.h"
namespace groove {
// Task-owned and used by one MIDI sender at a time. A quarantined allocation
// deliberately survives destruction because xHCI cancellation is unconfirmed.
class MidiDmaBuffer {
 public:
  MidiDmaBuffer()=default;
  MidiDmaBuffer(const MidiDmaBuffer&)=delete;
  MidiDmaBuffer& operator=(const MidiDmaBuffer&)=delete;
  ~MidiDmaBuffer(){if(!quarantined_)delete[] allocation_;}
  bool prepare(unsigned bytes,unsigned alignment){
   if(quarantined_ || !bytes || !alignment || (alignment&(alignment-1)))return false;
   if(bytes<=capacity_ && alignment==alignment_)return true;
   // Reserve whole cache lines after alignment so cache maintenance cannot
   // touch an unrelated heap allocation in the final line.
   uint64_t padded=(uint64_t(bytes)+alignment-1)&~uint64_t(alignment-1);
   uint64_t total=padded+alignment-1;if(total>~0u)return false;
   auto* next=alloc_array<uint8_t>(unsigned(total));if(!next)return false;
   uintptr_t aligned=(reinterpret_cast<uintptr_t>(next)+alignment-1)&~uintptr_t(alignment-1);
   delete[] allocation_;allocation_=next;data_=reinterpret_cast<uint8_t*>(aligned);capacity_=unsigned(padded);alignment_=alignment;return true;
  }
  uint8_t* data(){return data_;}
  void quarantine(){quarantined_=true;}
  bool quarantined()const{return quarantined_;}
 private:
  uint8_t* allocation_=nullptr;uint8_t* data_=nullptr;unsigned capacity_=0,alignment_=0;bool quarantined_=false;
};
}
