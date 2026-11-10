#pragma once
#include <stdint.h>
namespace groove { namespace mk3 {
// Source-profile facts: pinned encdr 5e7821c.
// Protocol hypotheses, not measurements on the user's controller.
enum class LedKind:uint8_t { None, SingleColor, Indexed, Uncertain };
struct Button { const char* name; uint8_t report_byte,mask; int8_t led_offset; LedKind led; };
constexpr Button buttons[]={
 {"encoder_press",1,0x01,-1,LedKind::None},
 {"encoder_right",1,0x08,60,LedKind::Indexed},
 {"encoder_up",1,0x04,58,LedKind::Indexed},
 {"encoder_left",1,0x20,59,LedKind::Indexed},
 {"encoder_down",1,0x10,61,LedKind::Indexed},
 {"shift",1,0x40,44,LedKind::SingleColor},
 {"top_8",1,0x80,19,LedKind::SingleColor},
 {"group_a",2,0x01,29,LedKind::Indexed},
 {"group_b",2,0x02,30,LedKind::Indexed},
 {"group_c",2,0x04,31,LedKind::Indexed},
 {"group_d",2,0x08,32,LedKind::Indexed},
 {"group_e",2,0x10,33,LedKind::Indexed},
 {"group_f",2,0x20,34,LedKind::Indexed},
 {"group_g",2,0x40,35,LedKind::Indexed},
 {"group_h",2,0x80,36,LedKind::Indexed},
 {"notes",3,0x01,28,LedKind::SingleColor},
 {"volume",3,0x02,20,LedKind::SingleColor},
 {"swing",3,0x04,21,LedKind::SingleColor},
 {"tempo",3,0x08,23,LedKind::SingleColor},
 {"note_repeat",3,0x10,22,LedKind::SingleColor},
 {"lock",3,0x20,24,LedKind::SingleColor},
 {"pad_mode",4,0x01,46,LedKind::SingleColor},
 {"keyboard",4,0x02,47,LedKind::SingleColor},
 {"chords",4,0x04,48,LedKind::SingleColor},
 {"step",4,0x08,49,LedKind::SingleColor},
 {"fixed_vel",4,0x10,45,LedKind::SingleColor},
 {"scene",4,0x20,50,LedKind::SingleColor},
 {"pattern",4,0x40,51,LedKind::SingleColor},
 {"events",4,0x80,52,LedKind::SingleColor},
 {"variations",5,0x02,53,LedKind::SingleColor},
 {"duplicate",5,0x04,54,LedKind::SingleColor},
 {"select",5,0x08,55,LedKind::SingleColor},
 {"solo",5,0x10,56,LedKind::SingleColor},
 {"mute",5,0x20,57,LedKind::SingleColor},
 {"pitch",5,0x40,25,LedKind::SingleColor},
 {"mod",5,0x80,26,LedKind::SingleColor},
 {"perform",6,0x01,27,LedKind::SingleColor},
 {"restart",6,0x02,37,LedKind::SingleColor},
 {"erase",6,0x04,38,LedKind::SingleColor},
 {"tap",6,0x08,39,LedKind::SingleColor},
 {"follow",6,0x10,40,LedKind::SingleColor},
 {"play",6,0x20,41,LedKind::SingleColor},
 {"rec",6,0x40,42,LedKind::SingleColor},
 {"stop",6,0x80,43,LedKind::SingleColor},
 {"macro",7,0x01,11,LedKind::SingleColor},
 {"settings",7,0x02,9,LedKind::SingleColor},
 {"arrow_right",7,0x04,7,LedKind::SingleColor},
 {"sampling",7,0x08,5,LedKind::Uncertain},
 {"mixer",7,0x10,3,LedKind::SingleColor},
 {"plugin",7,0x20,1,LedKind::SingleColor},
 {"channel",8,0x01,0,LedKind::SingleColor},
 {"arranger",8,0x02,2,LedKind::SingleColor},
 {"browser",8,0x04,4,LedKind::SingleColor},
 {"arrow_left",8,0x08,6,LedKind::SingleColor},
 {"file",8,0x10,8,LedKind::SingleColor},
 {"auto",8,0x20,10,LedKind::SingleColor},
 {"top_1",9,0x01,12,LedKind::SingleColor},
 {"top_2",9,0x02,13,LedKind::SingleColor},
 {"top_3",9,0x04,14,LedKind::SingleColor},
 {"top_4",9,0x08,15,LedKind::SingleColor},
 {"top_5",9,0x10,16,LedKind::SingleColor},
 {"top_6",9,0x20,17,LedKind::SingleColor},
 {"top_7",9,0x40,18,LedKind::SingleColor},
};
constexpr unsigned button_count=sizeof(buttons)/sizeof(buttons[0]);
inline const Button* button(const char* name){for(const auto& b:buttons){const char* x=b.name;const char* y=name;while(*x&&*x==*y){++x;++y;}if(!*x&&!*y)return &b;}return nullptr;}
constexpr unsigned knob_side(unsigned knob){return knob/4;}
constexpr unsigned knob_column(unsigned knob){return knob%4;}
constexpr unsigned knob_x(unsigned knob){return 12+knob_column(knob)*117;}
constexpr unsigned pad_logical(unsigned hardware){return (3-hardware/4)*4+hardware%4;}
// Factory palette hypothesis: 16 hues plus white, levels 0..3. Zero byte is off.
// Names/previews are UI approximations; level0 and exact colors need calibration.
constexpr const char* palette_names[]={"OFF","RED","ORANGE RED","ORANGE","AMBER","YELLOW","LIME","GREEN","MINT","CYAN","SKY","BLUE","INDIGO","VIOLET","MAGENTA","ROSE","CRIMSON","WHITE"};
constexpr uint32_t palette_preview[]={0xff09121c,0xffff0000,0xffff3f00,0xffff7f00,0xffffcf00,0xffffff00,0xff7fff00,0xff00ff00,0xff00ff7f,0xff00ffff,0xff007fff,0xff0000ff,0xff3f00ff,0xff7f00ff,0xffff00ff,0xffff007f,0xffff003f,0xffffffff};
constexpr uint8_t indexed(unsigned color,unsigned level){return color>0&&color<=17&&level<=3?uint8_t((color<<2)|level):0;}
// Keep existing encdr mono profile until physical calibration selects a profile.
constexpr uint8_t mono_active=0x9e,mono_dim=0xe4;
}}
