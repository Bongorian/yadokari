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
// PI5 EXPERIMENT (dev-loop only, not for release): Pi 5 board type 0x17 in
// the new-style revision encoding. Unknown encodings fail closed. Round 2:
// the PCB-revision nibble and the operator EEPROM acknowledgement are
// ignored -- this experiment build runs only on an operator-provisioned dev
// card, and round 1 proved the gate rejects the real board without showing
// which term failed. The ack caution returns if this ever moves toward
// release.
inline bool pi5_tryboot_allowed(uint32_t revision,bool eeprom_writable_ack){
 (void)eeprom_writable_ack;
 return (revision&(1u<<23))&&((revision>>4)&255)==0x17;
}
// Release form of the Pi 5 rule (for 0.9.1): same fail-closed encoding and
// board-type checks, with the original caution restored -- a rev 1.0 board
// (low nibble 0) needs the operator EEPROM acknowledgement, later PCB
// revisions do not. Which rule CUpdateReboot::supported() applies is a
// build-time choice (GROOVE_STRICT_UPDATE_GATE); the relaxed experiment
// rule above stays this tree's default so the dev loop keeps flowing.
inline bool pi5_tryboot_allowed_strict(uint32_t revision,bool eeprom_writable_ack){
 if(!(revision&(1u<<23))||((revision>>4)&255)!=0x17)return false;
 return (revision&15)>0||eeprom_writable_ack;
}
inline void reboot_flags_tag(uint32_t words[4],bool trial){words[0]=0x00038064;words[1]=4;words[2]=4;words[3]=trial?1:0;}
inline void reboot_flags_query_tag(uint32_t words[4]){words[0]=0x00030064;words[1]=4;words[2]=0;words[3]=0;}
inline bool journal_trial_expired(bool trial_identity,bool still_pending,uint32_t now,uint32_t start){return trial_identity&&still_pending&&uint32_t(now-start)>=120000000u;}
// Zero-payload notify plus an end-tag/padding word. Circle GetTags requires a
// >=16-byte buffer; GetTag rejects a zero-length response and cannot be used.
inline void reboot_notify_tag(uint32_t words[4]){words[0]=0x00030048;words[1]=words[2]=words[3]=0;}
}}
