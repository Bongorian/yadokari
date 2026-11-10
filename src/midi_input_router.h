#pragma once
#include "engine.h"
namespace groove {
// Task-context single producer. 16 USB cable numbers are logical identifiers,
// not claims about physical DIN ports. Notes retain their original target when
// a user edits the channel map. Failed note releases are retried outside audio.
class MidiInputRouter {
 static constexpr unsigned kEntries=16*16*128;
 static constexpr uint8_t kOwnerMask=0x1f,kSustained=0x40,kPending=0x80;
 uint8_t notes_[kEntries]{},pedal_[16*16]{};unsigned scan_=0,pending_=0;uint32_t generation_=0,route_epoch_=0;
 static unsigned index(unsigned cable,unsigned channel,unsigned note){return (cable*16+channel)*128+note;}
 void sync(Engine& engine){unsigned generation=engine.staged_generation();unsigned route_epoch=engine.midi_route_epoch();if(generation_!=generation || route_epoch_!=route_epoch){route_epoch_=route_epoch;for(auto& note:notes_)note=0;for(auto& pedal:pedal_)pedal=0;scan_=pending_=0;generation_=generation;}}
 bool release(Engine& engine,unsigned idx){uint8_t value=notes_[idx];if(!value)return true;unsigned channel=(idx/128)%16,note=idx%128,cable=idx/(16*128);MidiPacket off{0,uint8_t(cable),3,{uint8_t(0x80|channel),uint8_t(note),0}};if(!engine.receive_midi(off,uint8_t((value&kOwnerMask)-1)))return false;if(value&kPending)--pending_;notes_[idx]=0;return true;}
public:
 bool accept(Engine& engine,const ProjectState& project,const MidiPacket& packet){
  sync(engine);if(packet.sample || packet.cable>=16 || !valid_midi_packet(packet))return false;uint8_t status=packet.bytes[0];if(status>=0xf0)return engine.receive_midi(packet,15);
  unsigned channel=status&15,type=status&0xf0,target=engine.multitimbral()?channel:project.midi_input_route[channel];if(target>16)return false;
  unsigned idx=index(packet.cable,channel,packet.bytes[1]);
  if(type==0x80 || (type==0x90 && !packet.bytes[2])){if(!notes_[idx])return true;if(pedal_[packet.cable*16+channel] && !(notes_[idx]&kPending)){notes_[idx]|=kSustained;return true;}if(!engine.receive_midi(packet,uint8_t((notes_[idx]&kOwnerMask)-1))){if(!(notes_[idx]&kPending))++pending_;notes_[idx]=(notes_[idx]&~kSustained)|kPending;}else{if(notes_[idx]&kPending)--pending_;notes_[idx]=0;}return true;}
  if(type==0x90){if(notes_[idx] && !release(engine,idx)){if(!(notes_[idx]&kPending))++pending_;notes_[idx]=(notes_[idx]&~kSustained)|kPending;return false;}if(target==16)return true;if(!engine.receive_midi(packet,uint8_t(target)))return false;notes_[idx]=uint8_t(target+1);return true;}
  if(type==0xb0 && packet.bytes[1]==64){
   const unsigned pedal_index=packet.cable*16+channel;const bool down=packet.bytes[2]>=64;pedal_[pedal_index]=down;
   if(!down){unsigned first=index(packet.cable,channel,0);bool added=false;for(unsigned n=0;n<128;++n){auto& entry=notes_[first+n];if(entry&kSustained){entry=(entry&~kSustained)|kPending;++pending_;added=true;}}if(added){scan_=first;service(engine,128);}}
   // The router realizes sustain by delaying Note Off for both internal and
   // external instruments. Do not also forward CC64 to an external receiver:
   // otherwise remapping or disconnect could leave that receiver's pedal down.
   return true;
  }
  if(type==0xb0 && (packet.bytes[1]==120 || packet.bytes[1]==123)){
   uint16_t owners=0;
   for(unsigned n=0;n<128;++n){auto& entry=notes_[index(packet.cable,channel,n)];if(entry){owners|=uint16_t(1u<<((entry&kOwnerMask)-1));if(!(entry&kPending)){entry=(entry&~kSustained)|kPending;++pending_;}}}
   if(owners){
    // Use the original owners, not a new route. Scoped channel-mode
    // commands release internal voices immediately for CC120 and emit
    // individual MIDI note-offs without sending a channel-wide panic to an
    // external instrument shared by unrelated tracks or input cables.
    bool accepted=engine.command_space()>=unsigned(__builtin_popcount(unsigned(owners)));
    if(accepted)for(unsigned t=0;t<16;++t)if(owners&(1u<<t)){
     Command c{CommandType::MidiInput,uint8_t(t),packet.bytes[1],status};c.scalar=packet.bytes[2];c.parameter=uint16_t(packet.cable|0x400);
     if(!engine.submit(c)){accepted=false;break;}
    }
    // If the queue was full, normal note-off retries still prevent stuck notes.
    service(engine,128);return accepted;
   }
  }
  if(type==0xa0 && notes_[idx])target=(notes_[idx]&kOwnerMask)-1;
  // Channel-wide expression follows both the current route and all tracks
  // still holding notes from this cable/channel. Reserve the entire fan-out
  // before submitting: a saturated queue must not update only some tracks.
  if(type==0xd0 || type==0xe0){
   uint16_t targets=target<16?uint16_t(1u<<target):0;
   for(unsigned n=0;n<128;++n){uint8_t held=notes_[index(packet.cable,channel,n)];if(held)targets|=uint16_t(1u<<((held&kOwnerMask)-1));}
   if(engine.command_space()<unsigned(__builtin_popcount(unsigned(targets))))return false;
   for(unsigned t=0;t<16;++t)if(targets&(1u<<t))if(!engine.receive_midi(packet,uint8_t(t)))return false;
   return true;
  }
  if(target==16)return true;
  return engine.receive_midi(packet,uint8_t(target));
 }
 void service(Engine& engine,unsigned budget=256){sync(engine);while(pending_ && budget--){unsigned idx=scan_++;if(scan_==kEntries)scan_=0;if((notes_[idx]&kPending) && !release(engine,idx)){scan_=idx;break;}}}
 void release_all(Engine& engine){sync(engine);for(auto& pedal:pedal_)pedal=0;for(auto& note:notes_)if(note && !(note&kPending)){note=(note&~kSustained)|kPending;++pending_;}scan_=0;service(engine);}
 bool recover_dropped(Engine& engine,unsigned dropped,unsigned& observed){if(dropped==observed)return false;observed=dropped;release_all(engine);return true;}
};
}
