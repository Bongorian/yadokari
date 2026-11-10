#pragma once
#include "engine.h"
namespace groove {
// Task-context MIDI consumer. Failed note releases, song positions and panic messages must not
// disappear between the engine queue and the physical USB driver.
class MidiOutputPump {
public:
 template<class Source,class Sender> unsigned flush(Source& source,Sender sender,unsigned budget=32){
  return flush_impl(source,sender,budget,[](){return false;});
 }
 // A failed send starts an outage. On the next attempt, fence the queue so
 // Note On and clock events accumulated during that outage cannot play late.
 // Critical releases and transport state are still sent in original order.
 template<class Source,class Sender> unsigned flush_realtime(Source& source,Sender sender,unsigned budget=32){
  if(outage_ && !filter_){boundary_=source.midi_boundary();filter_=true;}
  return flush_impl(source,sender,budget,[&](){return source.midi_at_boundary(boundary_);});
 }
 bool pending()const{return pending_;}
 bool recovering()const{return outage_ || filter_ || needs_sync_;}
 bool take_recovered(){bool ready=recovered_;recovered_=false;return ready;}
 // A drained outage fence needs a fresh snapshot even if no transfer has yet
 // succeeded (for example, an outage containing only stale Note On/CC).
 bool take_resync_request(){bool ready=resync_requested_;resync_requested_=false;return ready;}
private:
 template<class Source,class Sender,class AtBoundary> unsigned flush_impl(Source& source,Sender sender,unsigned budget,AtBoundary at_boundary){
  unsigned failures=0;
  while(budget--){
   if(!pending_){if(filter_ && at_boundary()){filter_=false;outage_=false;resync_requested_=true;if(needs_sync_ && sent_after_loss_){recovered_=true;needs_sync_=false;}}if(!source.pop_midi(event_))break;if(filter_ && stale(event_))continue;}
   if(sender(event_.cable,event_.bytes,event_.length)){pending_=false;outage_=false;if(needs_sync_){sent_after_loss_=true;if(!filter_){recovered_=true;needs_sync_=false;}}continue;}
   ++failures;
   outage_=true;filter_=false;needs_sync_=true;sent_after_loss_=false;recovered_=false;
   if(critical(event_)){pending_=true;break;}
   pending_=false; // stale notes, CC and clock are not replayed after a disconnect
   break; // do not issue another potentially blocking transfer after a failure
  }
  return failures;
 }
 // Replaying only part of a stale bank-select or RPN/NRPN transaction can
 // change the wrong parameter. Reconstruct current controls from the engine.
 static bool stale(const MidiEvent& e){return !critical(e);}
 static bool critical(const MidiEvent& e){uint8_t status=e.bytes[0]&0xf0;return e.bytes[0]==0xf2 || e.bytes[0]==0xfc || status==0x80 || (status==0x90 && e.length>=3 && e.bytes[2]==0) || (status==0xb0 && e.length>=3 && (e.bytes[1]==120 || e.bytes[1]==123));}
 MidiEvent event_{};unsigned boundary_=0;bool pending_=false,outage_=false,filter_=false,needs_sync_=false,sent_after_loss_=false,recovered_=false,resync_requested_=false;
};
}
