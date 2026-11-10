#include "midi_loopback_probe.h"
#include <cassert>
#include <cstdio>
using groove::MidiLoopbackProbe;
int main(){
 MidiLoopbackProbe probe;assert(probe.start(0,7) && !probe.start(1,7));
 uint64_t now=200000;unsigned sent=0;
 for(unsigned i=0;i<MidiLoopbackProbe::kMessages;++i){
  groove::MidiPacket wire{};
  probe.service(now,true,7,[&](const groove::MidiPacket& p){assert(groove::valid_midi_packet(p));wire=p;++sent;return true;});
  assert(probe.receive(wire,now+1200));now+=17000;
 }
 probe.service(now+200000,true,7,[](auto){assert(false);return false;});
 assert(!probe.active() && probe.result().passed() && sent==261);
 assert(probe.result().cables==1 && probe.result().min_us==1200 && probe.result().max_us==1200);
 assert(probe.start(0,0));probe.service(200000,true,0,[](auto){return true;});
 auto bad=MidiLoopbackProbe::message(0);bad.bytes[2]^=1;
 assert(!probe.receive(bad,201000));assert(probe.receive(MidiLoopbackProbe::message(0),202000));
 assert(!probe.receive(MidiLoopbackProbe::message(0),203000));assert(probe.result().unexpected==2);
 probe.service(204000,false,1,[](auto){return true;});assert(!probe.active() && !probe.result().passed() && probe.result().disconnected==1 && probe.result().dropped==1);
 assert(probe.start(0,0));now=200000;
 for(unsigned i=0;i<4;++i){probe.service(now,true,0,[](auto){return true;});probe.service(now+250000,true,0,[](auto){assert(false);return false;});now+=270000;}
 probe.service(now+200000,true,0,[](auto){assert(false);return false;});assert(!probe.active() && probe.result().timeouts==4 && !probe.result().passed());
 assert(probe.start(0,0));probe.service(200000,true,0,[](auto){return false;});probe.service(400000,true,0,[](auto){assert(false);return false;});assert(!probe.active() && probe.result().send_errors==1);
 puts("DIN probe byte matching, bounded timeouts, disconnect, loss and send failure passed");
}
