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
 ChimeraSpeech* banks_=nullptr;
 unsigned active_mask_=0;
 // Nonzero means a producer has published a prepared bank for track n-1.
 unsigned pending_=0;
 ChimeraSpeech& bank(unsigned track,unsigned mask){return banks_[track*2+((mask>>track)&1)];}
public:
 ChimeraSpeechBanks()=default;
 ChimeraSpeechBanks(const ChimeraSpeechBanks&)=delete;
 ChimeraSpeechBanks& operator=(const ChimeraSpeechBanks&)=delete;
 ~ChimeraSpeechBanks(){delete[] banks_;}
 void init(){banks_=alloc_array<ChimeraSpeech>(tracks*2);}
 bool busy()const{return __atomic_load_n(&pending_,__ATOMIC_ACQUIRE)!=0;}
 bool prepare(unsigned track,const char* text,size_t bytes){
  if(!banks_ || track>=tracks || !bytes || bytes>kChimeraIpaBytes || busy())return false;
  unsigned inactive=~__atomic_load_n(&active_mask_,__ATOMIC_ACQUIRE);
  if(!yadokari_chimera_compile_ipa(text,bytes,&bank(track,inactive)))return false;
  __atomic_store_n(&pending_,track+1,__ATOMIC_RELEASE);
  return true;
 }
 bool prepare_project(const ChimeraIpaText text[tracks]){
  if(busy())return false;
  unsigned inactive=~__atomic_load_n(&active_mask_,__ATOMIC_ACQUIRE);
  for(unsigned t=0;t<tracks;++t){
   if(text[t].size){
    if(!banks_ || !yadokari_chimera_compile_ipa(text[t].data,text[t].size,&bank(t,inactive)))return false;
   }else if(banks_)bank(t,inactive).count=0;
  }
  return true;
 }
 // Project publication shares the existing ApplySnapshot command.
 void apply_project(){__atomic_fetch_xor(&active_mask_,0xffffu,__ATOMIC_RELEASE);}
 bool consume(){
  unsigned pending=__atomic_load_n(&pending_,__ATOMIC_ACQUIRE);
  if(!pending)return false;
  __atomic_fetch_xor(&active_mask_,1u<<(pending-1),__ATOMIC_RELEASE);
  __atomic_store_n(&pending_,0,__ATOMIC_RELEASE);
  return true;
 }
 const ChimeraPhrase* phrase(unsigned track,unsigned note)const{
  if(!banks_ || track>=tracks)return nullptr;
  unsigned mask=__atomic_load_n(&active_mask_,__ATOMIC_RELAXED);
  const auto& selected=banks_[track*2+((mask>>track)&1)];
  for(unsigned i=0;i<selected.count;++i)if(selected.phrases[i].note==note)return &selected.phrases[i];
  return nullptr;
 }
};
}
