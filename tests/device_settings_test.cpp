#include <initializer_list>
#include "device_settings.h"
#include "input_tuner.h"
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){DeviceSettings d,decoded;d.input_db=-12;d.local=false;d.theme=8;uint8_t bytes[16];encode_device(d,bytes);assert(decode_device(bytes,decoded)&&decoded.input_db==-12&&!decoded.local&&decoded.theme==8);bytes[9]^=1;assert(!decode_device(bytes,decoded));
 // Version 2 stays byte-identical without network MIDI; version 3 carries it.
 assert(bytes[4]==2);bytes[9]^=1;
 {DeviceSettings n=d;n.network_midi=true;n.net_latency_ms=12;uint8_t v3[16];encode_device(n,v3);assert(v3[4]==3);DeviceSettings back;assert(decode_device(v3,back)&&back.network_midi&&back.net_latency_ms==12&&!back.local&&back.theme==8&&back.input_db==-12);
  n.network_midi=false;n.net_latency_ms=0;encode_device(n,v3);assert(v3[4]==3&&decode_device(v3,back)&&!back.network_midi&&back.net_latency_ms==0);
  DeviceSettings old;assert(decode_device(bytes,old)&&!old.network_midi&&old.net_latency_ms==kNetLatencyDefaultMs);
  n.net_latency_ms=51;assert(!valid_device(n));}
 for(unsigned c=0;c<3;++c){unsigned previous=0;for(unsigned p=0;p<=4095;++p){unsigned v=pad_velocity(p,c);assert(v>=previous&&v<=127);previous=v;}assert(pad_velocity(0,c)==1&&pad_velocity(4095,c)==127);}assert(pad_velocity(1500,1)>pad_velocity(1500,0)&&pad_velocity(1500,2)<pad_velocity(1500,0));float samples[1024]{};assert(tune_input(samples).note<0);for(float hz:{55.f,110.f,220.f,440.f,880.f}){for(unsigned i=0;i<1024;++i)samples[i]=.2f*sinf(6.283185307f*hz*i/12000);auto r=tune_input(samples);assert(r.note>=0&&fabsf(1200*log2f(r.hz/hz))<8);}printf("Device: checksum/range, network MIDI v3, monotone velocity curves and tuner silence/55-880 Hz passed\n");}
