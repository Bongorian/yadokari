#pragma once
#include <stdint.h>
namespace groove {
inline constexpr uint8_t kFactoryGmNotes[16]={36,38,42,46,39,45,37,40,44,47,48,49,50,51,53,57};
inline uint8_t factory_drum_gm(unsigned note){return note>=36 && note<52?kFactoryGmNotes[note-36]:uint8_t(note);}
template<class Kit> inline uint8_t kit_gm_note(const Kit& kit,unsigned pad){return pad<16?(kit.pads[pad].gm_note<128?kit.pads[pad].gm_note:kFactoryGmNotes[pad]):255;}
template<class Kit> inline int kit_gm_exact(const Kit& kit,unsigned note){for(unsigned p=0;p<16;++p)if(kit_gm_note(kit,p)==note)return int(p);return -1;}
template<class Kit> inline int kit_gm_pad(const Kit& kit,unsigned note){
 int p=kit_gm_exact(kit,note);if(p>=0)return p;
 unsigned folded=note;switch(note){case 35:folded=36;break;case 40:folded=38;break;case 59:folded=51;break;case 52:folded=49;break;
 case 41:case 43:case 45:case 47:case 48:case 50:{static constexpr unsigned toms[]={45,47,48,50,43,41};unsigned best=255,distance=128;for(auto tom:toms){int match=kit_gm_exact(kit,tom);unsigned delta=note>tom?note-tom:tom-note;if(match>=0 && delta<distance){distance=delta;best=unsigned(match);}}return best<16?int(best):-1;}
 default:break;}return folded!=note?kit_gm_exact(kit,folded):-1;
}
}
