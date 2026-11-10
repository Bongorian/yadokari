#pragma once
#include "project.h"
#include "spsc_queue.h"
#include "normal_copy.h"
namespace groove {
// DSP is the only producer/planner; UI is the only consumer. A published token
// pins a complete immutable snapshot, including inactive state and extensions.
// The slot cannot be reused until the UI finishes its independent value copy.
class PreparedSoundQueue {
 static constexpr unsigned Slots=65,Wanted=48;
 static constexpr uint8_t None=255;
 struct Entry {uint64_t epoch=0;uint32_t pins=0,copied=0;uint16_t slot=kNoPreset;bool ready=false;};
 FixedRecordStorage<SoundState,Slots> records_;
 Entry entries_[Slots]{};uint8_t map_[kPresetSlots]{},building_=None;unsigned victim_=0;
 SpscQueue<uint8_t,Slots> pending_;
 uint16_t wanted_[Wanted]{};unsigned wanted_count_=0,cursor_=0;uint64_t wanted_epoch_=0;bool preparing_=false;
 uint64_t prepared_bytes_=0,chunks_=0,completed_=0,warm_=0,cold_=0;unsigned max_chunk_=0;
 uint8_t ready(unsigned slot,uint64_t epoch)const{
  if(slot>=kPresetSlots)return None;
  auto i=map_[slot];
  return i<Slots && entries_[i].ready && entries_[i].slot==slot && entries_[i].epoch==epoch?i:None;
 }
 uint8_t reserve(unsigned slot,uint64_t epoch){
  for(unsigned n=0;n<Slots;++n){unsigned i=(victim_+n)%Slots;auto& e=entries_[i];if(__atomic_load_n(&e.pins,__ATOMIC_ACQUIRE))continue;
   if(e.slot<kPresetSlots && map_[e.slot]==i)map_[e.slot]=None;
   if(building_==i)building_=None;
   e.ready=false;e.slot=uint16_t(slot);e.epoch=epoch;e.copied=0;victim_=(i+1)%Slots;return uint8_t(i);
  }return None;
 }
 void finish(uint8_t i){auto& e=entries_[i];e.ready=true;e.copied=sizeof(SoundState);map_[e.slot]=i;if(building_==i)building_=None;}
public:
 static constexpr unsigned chunk_bytes=16384;
 struct Counters {uint64_t prepared_bytes=0,chunks=0,completed=0,warm=0,cold=0;unsigned max_chunk=0;};
 PreparedSoundQueue(){for(auto& i:map_)i=None;}
 unsigned free_space()const{return pending_.free_space();}
 bool push(const SoundState& source,unsigned slot,uint64_t epoch){
  if(slot>=kPresetSlots || !pending_.free_space())return false;
  uint8_t i=ready(slot,epoch);
  if(i==None){
   if(building_<Slots && entries_[building_].slot==slot && entries_[building_].epoch==epoch)i=building_;else i=reserve(slot,epoch);
   if(i==None)return false;
   copy_normal_record(records_.values[i],source);finish(i);__atomic_add_fetch(&cold_,uint64_t(1),__ATOMIC_RELAXED);
  }else __atomic_add_fetch(&warm_,uint64_t(1),__ATOMIC_RELAXED);
  __atomic_add_fetch(&entries_[i].pins,1u,__ATOMIC_RELAXED);
  // Space cannot disappear: there is only one producer, and the consumer only frees it.
  if(pending_.push(i))return true;
  __atomic_sub_fetch(&entries_[i].pins,1u,__ATOMIC_RELEASE);return false;
 }
 bool pop(SoundState& destination){
  uint8_t i;if(!pending_.pop(i))return false;
  copy_normal_record(destination,records_.values[i]);
  __atomic_sub_fetch(&entries_[i].pins,1u,__ATOMIC_RELEASE);return true;
 }
 // A new plan may cancel an unpublished partial copy. Old published versions
 // remain pinned even across project replacement and pool edits.
 void request_prepare(const uint16_t* slots,unsigned count,uint64_t epoch){
  wanted_count_=count<Wanted?count:Wanted;for(unsigned i=0;i<wanted_count_;++i)wanted_[i]=slots[i];
  if(building_<Slots){bool keep=entries_[building_].epoch==epoch;bool wanted=false;for(unsigned i=0;i<wanted_count_;++i)wanted|=wanted_[i]==entries_[building_].slot;if(!keep || !wanted){entries_[building_].ready=false;building_=None;}}
  wanted_epoch_=epoch;cursor_=0;preparing_=wanted_count_!=0;
 }
 bool prepare_one(const SoundState* pool,uint64_t epoch){
  if(!preparing_)return false;
  if(epoch!=wanted_epoch_){if(building_<Slots)entries_[building_].ready=false;building_=None;preparing_=false;return false;}
  while(cursor_<wanted_count_ && (wanted_[cursor_]>=kPresetSlots || ready(wanted_[cursor_],epoch)!=None))++cursor_;
  if(cursor_==wanted_count_){preparing_=false;return false;}
  unsigned slot=wanted_[cursor_];
  if(building_>=Slots || entries_[building_].slot!=slot || entries_[building_].epoch!=epoch)building_=reserve(slot,epoch);
  if(building_>=Slots)return false; // bounded refusal while all records are pinned
  auto& e=entries_[building_];unsigned bytes=sizeof(SoundState)-e.copied;if(bytes>chunk_bytes)bytes=chunk_bytes;
  copy_normal_bytes(reinterpret_cast<unsigned char*>(&records_.values[building_])+e.copied,reinterpret_cast<const unsigned char*>(pool+slot)+e.copied,bytes);e.copied+=bytes;
  __atomic_add_fetch(&prepared_bytes_,uint64_t(bytes),__ATOMIC_RELAXED);__atomic_add_fetch(&chunks_,uint64_t(1),__ATOMIC_RELAXED);
  if(bytes>__atomic_load_n(&max_chunk_,__ATOMIC_RELAXED))__atomic_store_n(&max_chunk_,bytes,__ATOMIC_RELAXED);
  if(e.copied==sizeof(SoundState)){finish(building_);++cursor_;__atomic_add_fetch(&completed_,uint64_t(1),__ATOMIC_RELAXED);}return true;
 }
 Counters counters()const{return {__atomic_load_n(&prepared_bytes_,__ATOMIC_RELAXED),__atomic_load_n(&chunks_,__ATOMIC_RELAXED),__atomic_load_n(&completed_,__ATOMIC_RELAXED),__atomic_load_n(&warm_,__ATOMIC_RELAXED),__atomic_load_n(&cold_,__ATOMIC_RELAXED),__atomic_load_n(&max_chunk_,__ATOMIC_RELAXED)};}
};
static_assert(sizeof(PreparedSoundQueue)<=4u*1024*1024,"Preserve the old attachment queue's Circle allocation bucket");
}
