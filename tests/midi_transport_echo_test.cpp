#include "engine.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*p));float frame[2];e->render(frame,0);MidiEvent packet;while(e->pop_midi(packet)){}EngineEvent event;while(e->pop_event(event)){}
 for(auto command:{CommandType::Start,CommandType::Stop,CommandType::Continue}){
  assert(e->submit({command,0,0,0}));unsigned transport=0;bool received=false;
  for(unsigned round=0;round<16;++round){e->render(frame,1);while(e->pop_midi(packet)){if(packet.bytes[0]==0xfa || packet.bytes[0]==0xfb || packet.bytes[0]==0xfc){++transport;packet.sample=e->samples_rendered();assert(e->receive_midi({packet.sample,packet.cable,1,{packet.bytes[0],0,0}},0));received=true;}}}
  assert(received && transport==1); // A DIN cable cannot create a transport storm.
 }
 for(uint8_t status:{uint8_t(0xfa),uint8_t(0xfb),uint8_t(0xfc)}){
  while(e->pop_event(event)){}MidiPacket in{e->samples_rendered(),0,1,{status,0,0}};assert(e->receive_midi(in,0));e->render(frame,1);bool applied=false;
  while(e->pop_event(event))if(event.type==EngineEventType::Transport){assert(event.value==(status==0xfa?1:status==0xfb?2:0));applied=true;}
  assert(applied);
  while(e->pop_midi(packet))assert(packet.bytes[0]!=0xfa && packet.bytes[0]!=0xfb && packet.bytes[0]!=0xfc);
 }
 std::puts("MIDI transport: local output once, loopback converges, received transport applied without echo");
}
