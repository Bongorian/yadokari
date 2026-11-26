#pragma once
#include "hardware_profile.h"
#include "voice_budget.h"
#include <stdint.h>
namespace groove {
// Pi 5 DSPTEST, 24 frames: 385 us peak for a tract, 334 us for the
// 16-carrier TALK workload. Price the entire slot, including a fading tail.
inline unsigned chimera_capacity(unsigned talk){
 const unsigned budget=VoiceBudget::deadline_us*hardware::active_profile.budget_scale_permille/hardware::kPi5BudgetScale;
 return budget>22*talk?(budget-22*talk)/400:0;
}
enum class VoiceRefusal:uint8_t {None,Board,Rate,Unison,Talk,Busy,NoVoice};
inline VoiceRefusal chimera_refusal(unsigned requested,unsigned talk,bool ready){
 if(!requested)return VoiceRefusal::None;
 if(!chimera_capacity(0))return VoiceRefusal::Board;
 if(!ready)return VoiceRefusal::Rate;
 if(requested>chimera_capacity(0))return VoiceRefusal::Unison;
 return requested>chimera_capacity(talk)?VoiceRefusal::Talk:VoiceRefusal::None;
}
inline void voice_refusal_text(char (&out)[64],VoiceRefusal reason,unsigned talks){
 const char* message="";
 switch(reason){
 case VoiceRefusal::Board:message="CHIMERA: BOARD LIMIT / 0 TRACTS";break;
 case VoiceRefusal::Rate:message="CHIMERA: 48 KHZ ENGINE UNAVAILABLE";break;
 case VoiceRefusal::Unison:message="1 TRACT MAX / SET UNISON 1 OR SPLIT KEYS";break;
 case VoiceRefusal::Busy:message="CHIMERA TRACT IN USE";break;
 case VoiceRefusal::NoVoice:message="PREVIEW: NO PLAYABLE VOICE";break;
 case VoiceRefusal::Talk:message="TALK SLOTS IN USE: ";break;
 case VoiceRefusal::None:break;
 }
 unsigned n=0;while(*message)out[n++]=*message++;
 if(reason==VoiceRefusal::Talk){if(talks>=100)out[n++]=char('0'+talks/100);if(talks>=10)out[n++]=char('0'+talks/10%10);out[n++]=char('0'+talks%10);}
 out[n]=0;
}
struct ChimeraMix {
 uint8_t talk=0,tracts=0,owner=255,last_stopped=255,last_stop_talk=0;
 bool layer=false;
 uint32_t track_slots=0;uint16_t group_slots=0;
 uint8_t pad_extra[16][16]{}; // live pad instances affected by changing the base type to TALK
};
}
