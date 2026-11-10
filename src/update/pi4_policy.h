#pragma once
#include <stdint.h>
namespace groove { namespace update {
// New-style board revision encoding: bit23, board type 0x11 (Pi4B), low nibble
// PCB revision. Unknown boards fail closed; rev1.0/1.1 need operator evidence
// that EEPROM write protection is disabled before the firmware tryboot request.
inline bool pi4_tryboot_allowed(uint32_t revision,bool eeprom_writable_ack){
 if(!(revision&(1u<<23))||((revision>>4)&255)!=0x11)return false;
 return (revision&15)>1||eeprom_writable_ack;
}
inline void reboot_flags_tag(uint32_t words[4],bool trial){words[0]=0x00038064;words[1]=4;words[2]=4;words[3]=trial?1:0;}
inline void reboot_flags_query_tag(uint32_t words[4]){words[0]=0x00030064;words[1]=4;words[2]=0;words[3]=0;}
inline bool journal_trial_expired(bool trial_identity,bool still_pending,uint32_t now,uint32_t start){return trial_identity&&still_pending&&uint32_t(now-start)>=120000000u;}
// Zero-payload notify plus an end-tag/padding word. Circle GetTags requires a
// >=16-byte buffer; GetTag rejects a zero-length response and cannot be used.
inline void reboot_notify_tag(uint32_t words[4]){words[0]=0x00030048;words[1]=words[2]=words[3]=0;}
}}
