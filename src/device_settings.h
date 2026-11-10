#pragma once
#include <stdint.h>
#include <math.h>
#include "ui/theme.h"
namespace groove {
// Device preferences are independent of songs/presets and use a small versioned file.
struct DeviceSettings {uint8_t screen=100,led=3,velocity=0;int8_t input_db=0;uint8_t tuner_source=0;bool local=true;uint8_t theme=ui::Theme::kDefault;};
struct DeviceControl {const char* name;int minimum,maximum,initial;const char* unit;};
inline constexpr DeviceControl kDeviceControls[]={
 {"SCREEN LEVEL",20,100,100,"%"},{"LED LEVEL",1,3,3,""},{"VELOCITY",0,2,0,""},
 {"INPUT GAIN",-24,24,0,"dB"},{"TUNER INPUT",0,2,0,""},{"LOCAL CONTROL",0,1,1,""}};
inline unsigned pad_velocity(unsigned pressure,unsigned curve){float x=float(pressure>4095?4095:pressure)/4095;float y=curve==1?sqrtf(x):curve==2?x*x:x;return 1+unsigned(y*126+.5f);}
inline bool valid_device(const DeviceSettings& d){return d.screen>=20 && d.screen<=100 && d.led>=1 && d.led<=3 && d.velocity<=2 && d.input_db>=-24 && d.input_db<=24 && d.tuner_source<=2 && d.theme<ui::Theme::kCount;}
inline void encode_device(const DeviceSettings& d,uint8_t (&out)[16]){for(auto& b:out)b=0;out[0]='M';out[1]='D';out[2]='E';out[3]='V';out[4]=2;out[5]=d.screen;out[6]=d.led;out[7]=d.velocity;out[8]=uint8_t(d.input_db+24);out[9]=d.tuner_source;out[10]=d.local;out[11]=d.theme;uint32_t hash=2166136261u;for(unsigned i=0;i<12;++i)hash=(hash^out[i])*16777619u;for(unsigned i=0;i<4;++i)out[12+i]=uint8_t(hash>>(8*i));}
inline bool decode_device(const uint8_t (&in)[16],DeviceSettings& d){if(in[0]!='M'||in[1]!='D'||in[2]!='E'||in[3]!='V'||(in[4]!=1 && in[4]!=2)||in[10]>1||(in[4]==1 && in[11]))return false;uint32_t hash=2166136261u;for(unsigned i=0;i<12;++i)hash=(hash^in[i])*16777619u;for(unsigned i=0;i<4;++i)if(in[12+i]!=uint8_t(hash>>(8*i)))return false;DeviceSettings next{in[5],in[6],in[7],int8_t(int(in[8])-24),in[9],bool(in[10]),uint8_t(in[4]==2?in[11]:ui::Theme::kDefault)};if(!valid_device(next))return false;d=next;return true;}
}
