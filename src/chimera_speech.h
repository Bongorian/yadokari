#pragma once
#include "alloc_policy.h"
#include "chimera_state.h"
#include <stdint.h>

namespace groove {
constexpr unsigned kChimeraIpaPhrases=8;
constexpr unsigned kChimeraIpaPhones=16;

// The rows hold Rust's precompiled gestures. The audio thread selects a
// phrase and schedules those gestures against the voice or transport clock.
struct ChimeraPhrase {
 unsigned note=0,count=0;
 float seconds=0,lead=0;
 alignas(8) unsigned char gestures[kChimeraIpaPhones][320]{};
};
struct ChimeraSpeech {
 unsigned count=0;
 ChimeraPhrase phrases[kChimeraIpaPhrases]{};
};
extern "C" bool yadokari_chimera_compile_ipa(const char*,size_t,ChimeraSpeech*);

class ChimeraSpeechBanks {
 static constexpr unsigned tracks=16;
 const ChimeraSpeech* tracks_[tracks][2]{};
 struct PreparedPhrase {
  uint32_t hash=0;
  ChimeraIpaText text{};
  ChimeraSpeech score{};
 };
 // A project can hold 256 sounds with four leaves apiece. Keep published
 // entries immutable until the audio owner has switched project banks.
 static constexpr unsigned portable_capacity=2048;
 PreparedPhrase* portable_[2][portable_capacity]{};
 unsigned portable_active_=0;

 static uint32_t fingerprint(const ChimeraIpaText& text){
  uint32_t hash=2166136261u;
  for(unsigned i=0;i<text.size;++i)hash=(hash^uint8_t(text.data[i]))*16777619u;
  return hash;
 }
 unsigned find_portable(unsigned bank,const ChimeraIpaText& text,uint32_t hash)const{
  for(unsigned i=0;i<portable_capacity;++i){
   unsigned index=(hash+i)&(portable_capacity-1);
   const auto* entry=__atomic_load_n(&portable_[bank][index],__ATOMIC_ACQUIRE);
   if(!entry || (entry->hash==hash && entry->text.size==text.size &&
      !__builtin_memcmp(entry->text.data,text.data,text.size)))return index;
  }
  return portable_capacity;
 }
 void clear_portable(unsigned bank){for(auto& entry:portable_[bank]){delete entry;entry=nullptr;}}
 static const ChimeraPhrase* find_phrase(const ChimeraSpeech& score,unsigned key){
  for(unsigned i=0;i<score.count;++i)if(score.phrases[i].note==key)return &score.phrases[i];
  return nullptr;
 }
 unsigned active_mask_=0;
 // Nonzero means a producer has published a prepared bank for track n-1.
 unsigned pending_=0;
 const ChimeraSpeech*& track_bank(unsigned track,unsigned mask){return tracks_[track][(mask>>track)&1];}
 // Track overrides share the immutable cache with mounted sounds so that an
 // in-flight voice can finish its phrase after a track bank is republished.
 const ChimeraSpeech* compile_portable(const ChimeraIpaText& text,bool staging){
  if(!text.size || text.size>kChimeraIpaBytes)return nullptr;
  const unsigned bank=__atomic_load_n(&portable_active_,__ATOMIC_ACQUIRE)^unsigned(staging);
  const uint32_t hash=fingerprint(text);
  const unsigned index=find_portable(bank,text,hash);
  if(index>=portable_capacity)return nullptr;
  auto& entry=portable_[bank][index];
  if(entry)return &entry->score;
  auto* prepared=alloc_new<PreparedPhrase>();
  if(!prepared)return nullptr;
  if(!yadokari_chimera_compile_ipa(text.data,text.size,&prepared->score)){delete prepared;return nullptr;}
  prepared->text=text;prepared->hash=hash;
  __atomic_store_n(&entry,prepared,__ATOMIC_RELEASE);
  return &prepared->score;
 }
public:
 ChimeraSpeechBanks()=default;
 ChimeraSpeechBanks(const ChimeraSpeechBanks&)=delete;
 ChimeraSpeechBanks& operator=(const ChimeraSpeechBanks&)=delete;
 ~ChimeraSpeechBanks(){clear_portable(0);clear_portable(1);}
 bool busy()const{return __atomic_load_n(&pending_,__ATOMIC_ACQUIRE)!=0;}
 bool prepare(unsigned track,const char* text,size_t bytes){
  if(track>=tracks || !bytes || bytes>kChimeraIpaBytes || busy())return false;
  ChimeraIpaText phrase{};
  phrase.size=uint16_t(bytes);__builtin_memcpy(phrase.data,text,bytes);
  auto* score=compile_portable(phrase,false);
  if(!score)return false;
  unsigned inactive=~__atomic_load_n(&active_mask_,__ATOMIC_ACQUIRE);
  track_bank(track,inactive)=score;
  __atomic_store_n(&pending_,track+1,__ATOMIC_RELEASE);
  return true;
 }
 bool prepare_portable(const ChimeraIpaText& text,bool staging){
  return !text.size || compile_portable(text,staging);
 }
 bool prepare_project(const ChimeraIpaText text[tracks]){
  if(busy())return false;
  clear_portable(__atomic_load_n(&portable_active_,__ATOMIC_ACQUIRE)^1u);
  unsigned inactive=~__atomic_load_n(&active_mask_,__ATOMIC_ACQUIRE);
  for(unsigned t=0;t<tracks;++t){
   auto* score=text[t].size?compile_portable(text[t],true):nullptr;
   if(text[t].size && !score)return false;
   track_bank(t,inactive)=score;
  }
  return true;
 }
 // Project publication shares the existing ApplySnapshot command.
 void apply_project(){__atomic_fetch_xor(&active_mask_,0xffffu,__ATOMIC_RELEASE);__atomic_fetch_xor(&portable_active_,1u,__ATOMIC_RELEASE);}
 bool consume(){
  unsigned pending=__atomic_load_n(&pending_,__ATOMIC_ACQUIRE);
  if(!pending)return false;
  __atomic_fetch_xor(&active_mask_,1u<<(pending-1),__ATOMIC_RELEASE);
  __atomic_store_n(&pending_,0,__ATOMIC_RELEASE);
  return true;
 }
 const ChimeraSpeech* effective(unsigned track,const ChimeraIpaText& text)const{
  if(track>=tracks)return nullptr;
  unsigned mask=__atomic_load_n(&active_mask_,__ATOMIC_RELAXED);
  const auto* selected=tracks_[track][(mask>>track)&1];
  if(selected)return selected;
  if(!text.size || text.size>kChimeraIpaBytes)return nullptr;
  const unsigned bank=__atomic_load_n(&portable_active_,__ATOMIC_RELAXED);
  unsigned index=find_portable(bank,text,fingerprint(text));
  if(index>=portable_capacity)return nullptr;
  const auto* prepared=__atomic_load_n(&portable_[bank][index],__ATOMIC_ACQUIRE);
  return prepared?&prepared->score:nullptr;
 }
 const ChimeraPhrase* phrase(unsigned track,unsigned note,const ChimeraIpaText& text={})const{
  const auto* score=effective(track,text);
  return score?find_phrase(*score,note):nullptr;
 }
};
}
