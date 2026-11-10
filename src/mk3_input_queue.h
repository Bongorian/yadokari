#pragma once
#include <stdint.h>
#include "spsc_queue.h"
namespace groove {
enum class Mk3EventType : uint8_t { Pad, Knob, ControlByte, ControlBaseline, MainEncoder, Strip, Resync };
struct Mk3Pad { Mk3EventType type; uint8_t index, tag; uint16_t pressure; uint32_t generation=0; };
// Task-side first observations only. Baselines and resync markers are not
// physical actions. Separate pad edges and pressure so an idle/held snapshot
// cannot be mistaken for an observed press or release.
class Mk3InputObservation {
public:
 void reset(){seen_=0;}
 bool first(const Mk3Pad& event){
  unsigned bit;
  switch(event.type){
   case Mk3EventType::Pad:
    if(event.tag==0x10)bit=0;
    else if(event.tag==0x20 || event.tag==0x30)bit=1;
    else if(event.tag==0x40 || event.tag==0)bit=2;
    else return false;
    break;
   case Mk3EventType::ControlByte:bit=3;break;
   case Mk3EventType::Knob:bit=4;break;
   case Mk3EventType::MainEncoder:bit=5;break;
   case Mk3EventType::Strip:bit=6;break;
   default:return false;
  }
  const unsigned mask=1u<<bit;if(seen_&mask)return false;seen_|=mask;return true;
 }
private:unsigned seen_=0;
};
inline bool mk3_input_report_length_valid(uint8_t id,unsigned size,unsigned control,unsigned pads){return size && (id!=1 || size==control) && (id!=2 || size==pads);}
// One HID callback producer and one task consumer. A dropped release can
// leave a note held, so the consumer discards old events and requests a
// generation-tagged snapshot before accepting subsequent events.
class Mk3InputQueue {
public:
 bool push(const Mk3Pad& event){if(queue_.push(event))return true;__atomic_add_fetch(&dropped_,1,__ATOMIC_RELEASE);return false;}
 void mark_loss(){__atomic_add_fetch(&dropped_,1,__ATOMIC_RELEASE);}
 bool pop(Mk3Pad& event){return queue_.pop(event);}
 unsigned dropped()const{return __atomic_load_n(&dropped_,__ATOMIC_ACQUIRE);}
 uint32_t request_resync(){uint32_t value=__atomic_add_fetch(&requested_,1,__ATOMIC_ACQ_REL);if(!value)value=__atomic_add_fetch(&requested_,1,__ATOMIC_ACQ_REL);return value;}
 // Called only by the producer before processing a report. All 11 entries fit
 // before publishing the marker; the consumer can only free more capacity.
 bool prepare(const uint8_t raw[10],bool& resynced,bool has_control_snapshot=true){
  resynced=false;
  uint32_t generation=__atomic_load_n(&requested_,__ATOMIC_ACQUIRE);
  if(generation==published_)return true;
  if(!has_control_snapshot || queue_.free_space()<11)return false;
  queue_.push({Mk3EventType::Resync,0,0,0,generation});
  for(unsigned i=0;i<10;++i)queue_.push({Mk3EventType::ControlBaseline,uint8_t(i+1),raw[i],0,generation});
  published_=generation;resynced=true;return true;
 }
private:
 SpscQueue<Mk3Pad,128> queue_;
 unsigned dropped_=0;
 uint32_t requested_=0,published_=0;
};
}
