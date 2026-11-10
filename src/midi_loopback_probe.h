#pragma once
#include "backends.h"
namespace groove {
// Task-owned physical loopback diagnostic. It observes input; it never echoes it.
// One outstanding message makes byte equality and host-loop round-trip timing
// meaningful. These timings include USB and task scheduling, not just DIN wire.
class MidiLoopbackProbe {
public:
 static constexpr unsigned kChannelMessages=14, kMessages=16*kChannelMessages+8+29;
 enum class Phase:uint8_t {Idle,Settling,Running,Draining,Done};
 struct Result {
  unsigned sent=0,received=0,unexpected=0,timeouts=0,send_errors=0,disconnected=0,dropped=0;
  uint16_t cables=0;uint64_t total_us=0;unsigned min_us=0,max_us=0;
  bool passed()const{return sent==kMessages && received==kMessages && !unexpected && !timeouts && !send_errors && !disconnected && !dropped;}
 };
 static MidiPacket message(unsigned index){
  MidiPacket p{};p.cable=0;p.length=3;
  if(index<16*kChannelMessages){
   unsigned channel=index/kChannelMessages;uint8_t note=uint8_t(48+channel);
   switch(index%kChannelMessages){
    case 0:p.bytes[0]=0x90;p.bytes[1]=note;p.bytes[2]=80;break;
    case 1:p.bytes[0]=0xa0;p.bytes[1]=note;p.bytes[2]=55;break;
    case 2:p.bytes[0]=0x80;p.bytes[1]=note;p.bytes[2]=23;break;
    case 3:p.bytes[0]=0x90;p.bytes[1]=note;break;
    case 4:p.bytes[0]=0xb0;p.bytes[1]=1;p.bytes[2]=65;break;
    case 5:p.bytes[0]=0xb0;p.bytes[1]=64;p.bytes[2]=127;break;
    case 6:p.bytes[0]=0xb0;p.bytes[1]=64;break;
    case 7:p.length=2;p.bytes[0]=0xc0;p.bytes[1]=uint8_t(10+channel);break;
    case 8:p.length=2;p.bytes[0]=0xd0;p.bytes[1]=70;break;
    case 9:p.bytes[0]=0xe0;break;
    case 10:p.bytes[0]=0xe0;p.bytes[2]=64;break;
    case 11:p.bytes[0]=0xe0;p.bytes[1]=127;p.bytes[2]=127;break;
    case 12:p.bytes[0]=0xb0;p.bytes[1]=120;break;
    case 13:p.bytes[0]=0xb0;p.bytes[1]=123;break;
   }
   p.bytes[0]|=uint8_t(channel);return p;
  }
  index-=16*kChannelMessages;
  if(index<8){static const uint8_t cc[]={99,98,6,38,101,100,6,38};p.bytes[0]=0xb0;p.bytes[1]=cc[index];p.bytes[2]=uint8_t(index+1);return p;}
  index-=8;p.length=1;
  if(index==0)p.bytes[0]=0xfa;
  else if(index<=24)p.bytes[0]=0xf8;
  else if(index==25 || index==28)p.bytes[0]=0xfc;
  else if(index==26){p.length=3;p.bytes[0]=0xf2;p.bytes[1]=1;p.bytes[2]=2;}
  else if(index==27)p.bytes[0]=0xfb;
  else p.length=0;
  return p;
 }
 bool active()const{return phase_!=Phase::Idle && phase_!=Phase::Done;}
 bool owns_output()const{return phase_==Phase::Running || phase_==Phase::Draining;}
 unsigned index()const{return index_;}
 unsigned last_index()const{return last_index_;}
 unsigned last_rtt()const{return last_rtt_;}
 const Result& result()const{return result_;}
 bool start(uint64_t now,unsigned dropped){
  if(active())return false;
  phase_=Phase::Settling;result_={};index_=consecutive_timeouts_=0;waiting_=false;
  baseline_dropped_=dropped;next_=now+200000;return true;
 }
 // Called only for bytes actually received from the USB MIDI backend.
 bool receive(const MidiPacket& p,uint64_t now){
  if(!active() || phase_==Phase::Settling)return false;
  MidiPacket expected=message(index_);bool equal=waiting_ && p.cable<16 && p.length==expected.length;
  for(unsigned i=0;i<p.length && i<3;++i)equal=equal && p.bytes[i]==expected.bytes[i];
  if(!equal){++result_.unexpected;return false;}
  last_index_=index_;last_rtt_=unsigned(now-sent_at_);result_.total_us+=last_rtt_;
  if(!result_.received || last_rtt_<result_.min_us)result_.min_us=last_rtt_;
  if(last_rtt_>result_.max_us)result_.max_us=last_rtt_;
  ++result_.received;result_.cables|=uint16_t(1u<<p.cable);
  waiting_=false;consecutive_timeouts_=0;++index_;next_=now+15000;
  if(index_==kMessages)drain(now);
  return true;
 }
 template<class Sender> void service(uint64_t now,bool connected,unsigned dropped,Sender send){
  if(!active())return;
  result_.dropped=dropped-baseline_dropped_;
  if(!connected){++result_.disconnected;phase_=Phase::Done;return;}
  if(phase_==Phase::Draining){if(now>=next_)phase_=Phase::Done;return;}
  if(phase_==Phase::Settling){if(now<next_)return;phase_=Phase::Running;}
  if(waiting_){
   if(now-sent_at_<250000)return;
   ++result_.timeouts;++consecutive_timeouts_;++index_;waiting_=false;next_=now+15000;
   if(index_==kMessages || consecutive_timeouts_>=4)drain(now);
   return;
  }
  if(now<next_)return;
  sent_at_=now;
  if(!send(message(index_))){++result_.send_errors;drain(now);return;}
  ++result_.sent;waiting_=true;
 }
private:
 void drain(uint64_t now){phase_=Phase::Draining;waiting_=false;next_=now+200000;}
 Phase phase_=Phase::Idle;Result result_{};bool waiting_=false;
 unsigned index_=0,baseline_dropped_=0,consecutive_timeouts_=0,last_index_=0,last_rtt_=0;
 uint64_t next_=0,sent_at_=0;
};
}
