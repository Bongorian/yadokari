#pragma once
// AppleMIDI sessions: the RTP-MIDI session protocol of macOS Network MIDI,
// rtpMIDI (Windows) and rtpmidid (Linux). Hosts invite YADOKARI, and up to
// kMaxSessions hosts can be connected at once on one pair of UDP ports. Each
// session sends and receives RTP-MIDI with a recovery journal
// (rtp_midi_journal.h) and turns the sender's RTP timestamps into local due
// times through a jitter buffer.
// YADOKARI remembers the hosts that connected and invites them again when
// their session is gone without a goodbye: after a restart of the Pi, a
// timeout, or a long pause of the main loop. A host that said goodbye (BY)
// or refused an invitation (NO) is forgotten until it connects by itself;
// a goodbye read right after a long pause counts as a loss (PauseBye).
// Pure logic, no sockets: the Host callback object does the I/O.
#include "rtp_midi_journal.h"

namespace groove::rtpmidi {

constexpr unsigned kMaxSessions=4;
constexpr uint64_t kFeedbackUs=500000;     // receiver feedback (RS) at most this often
constexpr uint64_t kGuardUs=100000;        // journal-only packet after the last one if unacknowledged
constexpr uint64_t kHalfOpenUs=10000000;   // control invitation without its data invitation
constexpr uint64_t kSilentProbeUs=15000000,kProbeIntervalUs=5000000,kTimeoutUs=60000000;
// While a session's notes or sustain pedal sound, a quiet host is asked every
// second and its notes are released after 3 s without an answer: a pulled
// cable, a sleeping or crashed host sends no BY. The session itself stays.
constexpr uint64_t kHeldProbeUs=1000000,kHeldReleaseUs=3000000;
constexpr uint64_t kFloorWindowUs=1000000; // transit floor = minimum over the last 1-2 s
constexpr uint32_t kResyncUnits=10000;     // a 1 s jump in RTP time restarts the timing
constexpr uint64_t kMaxAheadUs=2000000;
constexpr unsigned kTxRing=256,kMaxPerPacket=48;
// Invitations to remembered hosts: the first 1 s after the ports open or a
// session is lost, then every 2 s doubling to every 30 s while the host does
// not answer. A host that answers the control invitation but not the data
// one within 3 s is tried again later. A session YADOKARI started keeps the
// clock synchronised (CK) every 10 s, as the inviting side does.
constexpr uint64_t kInviteFirstUs=1000000,kInviteRetryUs=2000000,kInviteMaxUs=30000000,kInviteAnswerUs=3000000;
constexpr uint64_t kSyncUs=10000000;
// A main-loop pause longer than kStallUs (a long file check, for example)
// leaves packets that waited looking fresh, so every host that answers
// probes must answer a new one within kStallAnswerUs or its session ends.
constexpr uint64_t kStallUs=5000000,kStallAnswerUs=3000000;
// A goodbye (BY) read in the first pass after such a pause, or within
// kStallAnswerUs of it, most likely came from a host that gave up waiting
// during the pause (macOS ends a session after about 50 s without answers).
// That session ends as PauseBye: the host stays remembered and is invited
// again, as after a timeout.
constexpr unsigned kKnownHosts=kMaxSessions;

enum class EndReason:uint8_t { Bye,Timeout,Replaced,Closed,PauseBye };
inline const char* end_reason_name(EndReason r){
 switch(r){case EndReason::Bye:return "bye";case EndReason::PauseBye:return "bye after a pause";case EndReason::Timeout:return "timeout";case EndReason::Replaced:return "replaced";default:return "closed";}
}
// Why an open session let go of its notes: a loss its journal cannot repair,
// a host that stopped answering while notes sounded, or the local link going down.
enum class ReleaseReason:uint8_t { Loss,Silent,Link };

// latency_us: SETTINGS -> MIDI -> NET LATENCY (0..50 ms).
struct Options { uint32_t latency_us=5000; };

struct Stats {
 uint32_t packets=0,messages=0,lost=0,malformed=0,rejected=0,sysex=0;
 uint32_t repairs=0,uncovered=0,late=0,reordered=0,silent=0;
 uint32_t sent=0,sent_messages=0,guards=0,acks=0,journal_full=0;
 uint32_t rtt_us=0;
};

// One outgoing message and its local time (microseconds).
struct TxMessage { uint8_t bytes[3]{};uint8_t length=0;uint64_t us=0; };

// Session-level sink (SessionSet adapts its Host to it):
//   void send(Port port,const Endpoint& to,const uint8_t* bytes,unsigned length);
//   void midi(const uint8_t* bytes,unsigned length,uint64_t due_us);
//   void release(ReleaseReason);  // drop everything this session holds
//   void opened();
//   void ended(EndReason reason,const Endpoint& control);  // the caller releases what the session held
class Session {
public:
 // Inviting / InvitingData: YADOKARI invited the host and waits for its OK
 // on the control port, then on the data port.
 enum class State:uint8_t { Idle,ControlOnly,Open,Inviting,InvitingData };

 void setup(uint32_t ssrc,const char* name,const Options* options,const SenderHistory* history){ssrc_=ssrc;name_=name;options_=options;history_=history;}
 State state()const{return state_;}
 bool open()const{return state_==State::Open;}
 bool idle()const{return state_==State::Idle;}
 bool inviting()const{return state_==State::Inviting || state_==State::InvitingData;}
 bool initiator()const{return initiator_;}
 uint32_t ssrc()const{return ssrc_;}
 uint32_t peer_ssrc()const{return peer_ssrc_;}
 const char* peer_name()const{return peer_name_;}
 const Endpoint& peer_control()const{return control_;}
 const Endpoint& peer_data()const{return data_;}
 const Stats& stats()const{return stats_;}
 const Receiver& receiver()const{return receiver_;}
 uint16_t checkpoint()const{return checkpoint_seq_;}
 bool matches(uint32_t ssrc,uint32_t ip)const{return state_!=State::Idle && ssrc==peer_ssrc_ && ip==control_.ip;}
 bool at_control(const Endpoint& from)const{return state_!=State::Idle && from==control_;}

 template<class Sink> void invite_control(const Endpoint& from,const uint8_t* p,unsigned n,uint64_t now,Sink& sink){
  const uint32_t token=be32(p+8),ssrc=be32(p+12);
  if(inviting())reset(); // the host invites us while we invite it: take its session
  if(state_!=State::Idle){
   const bool same=ssrc==peer_ssrc_ && from==control_;
   if(same && state_==State::ControlOnly){token_=token;heard_=now;send_session(Port::Control,from,'O','K',token,true,sink);return;} // retransmitted
   end(same?EndReason::Bye:EndReason::Replaced,sink); // the host restarted, or reuses its port
  }
  state_=State::ControlOnly;peer_ssrc_=ssrc;token_=token;control_=from;heard_=now;
  take_name(p,n);
  send_session(Port::Control,from,'O','K',token,true,sink);
 }

 // YADOKARI invites a remembered host: IN on its control port, then, after
 // its OK, IN on the data port (control port + 1).
 template<class Sink> void invite(const Endpoint& control,const char* name,uint32_t token,uint64_t now,Sink& sink){
  reset();state_=State::Inviting;initiator_=true;control_=control;data_={control.ip,uint16_t(control.port+1)};token_=token;heard_=now;
  unsigned size=0;while(name && name[size] && size<kNameBytes-1)++size;
  for(unsigned i=0;i<size;++i)peer_name_[i]=name[i];
  peer_name_[size]=0;
  send_session(Port::Control,control_,'I','N',token_,true,sink);
 }

 // The host's answer to our invitation (ok: OK, else NO). Returns false when
 // the packet is not the answer this session waits for.
 template<class Sink> bool answer(Port port,const Endpoint& from,const uint8_t* p,unsigned n,bool ok,uint64_t now,Sink& sink){
  if(!inviting() || n<16 || be32(p+8)!=token_)return false;
  if(state_==State::Inviting?(port!=Port::Control || from!=control_):(port!=Port::Data || from!=data_))return false;
  if(!ok){reset();return true;}
  if(state_==State::Inviting){
   peer_ssrc_=be32(p+12);take_name(p,n);state_=State::InvitingData;heard_=now;
   send_session(Port::Data,data_,'I','N',token_,true,sink);
   return true;
  }
  state_=State::Open;start(now);heard_=now;
  asked_=true;probe_=sync_=now;send_ck(0,ts(now),0,0,sink); // the inviting side starts the clock sync
  sink.opened();
  return true;
 }

 // The main loop stood still for a while: a host that answers probes must
 // answer this one within kStallAnswerUs, or its session ends.
 template<class Sink> void stalled(uint64_t now,Sink& sink){
  if(state_!=State::Open || !answered_)return;
  stall_t1_=ts(now)?ts(now):1;stall_at_=probe_=now;send_ck(0,stall_t1_,0,0,sink);
 }

 template<class Sink> void invite_data(const Endpoint& from,const uint8_t* p,uint64_t now,Sink& sink){
  const uint32_t token=be32(p+8);
  if(state_==State::ControlOnly){state_=State::Open;data_=from;start(now);heard_=now;send_session(Port::Data,from,'O','K',token,true,sink);sink.opened();return;}
  if(state_==State::Open && from==data_){heard_=now;send_session(Port::Data,from,'O','K',token,true,sink);return;}
  ++stats_.rejected;send_session(Port::Data,from,'N','O',token,false,sink);
 }

 // after_pause: the goodbye was read right after a main-loop pause (kStallUs).
 template<class Sink> void bye(Sink& sink,bool after_pause=false){end(after_pause?EndReason::PauseBye:EndReason::Bye,sink);}

 template<class Sink> void clock(const uint8_t* p,unsigned n,uint64_t now,Sink& sink){
  if(state_!=State::Open || n<36){++stats_.rejected;return;}
  heard_=now;
  const uint8_t count=p[8];const uint64_t t1=be64(p+12),t2=be64(p+20),t3=be64(p+28);
  if(count==0)send_ck(1,t1,ts(now),0,sink);
  else if(count==1){send_ck(2,t1,t2,ts(now),sink);stats_.rtt_us=uint32_t((ts(now)-t1)*100);answered_=true;if(t1==stall_t1_)stall_t1_=0;} // answer to our probe
  else if(count==2 && t3>=t1)stats_.rtt_us=uint32_t((t3-t1)*100);
 }

 // Receiver feedback (RS) from the host: the newest of our packets it has.
 // Apple puts the 16-bit sequence number in the upper half of the 32-bit
 // field; some implementations put an extended number there instead, so the
 // half that names a recently sent packet wins.
 void feedback(const uint8_t* p,unsigned n,uint64_t now){
  if(state_!=State::Open || n<12){++stats_.rejected;return;}
  heard_=now;
  const uint16_t last=uint16_t(tx_seq_-1);
  auto age=[&](uint16_t a)->unsigned{
   const uint16_t back=uint16_t(last-a);
   if(!tx_count_ || back>=tx_count_ || back>=kTxRing)return 0x10000;
   if(have_ack_ && uint16_t(a-acked_)>=0x8000)return 0x10000; // older than what we know
   return back;
  };
  const uint16_t hi=be16(p+8),lo=be16(p+10);
  const unsigned age_hi=age(hi),age_lo=age(lo);
  if(age_hi>0xffff && age_lo>0xffff)return;
  const uint16_t a=age_hi<=age_lo?hi:lo;
  acked_=a;have_ack_=true;++stats_.acks;
  // Closed-loop policy: the next journal starts at the packet after it.
  const uint16_t next=uint16_t(a+1);
  if(next==tx_seq_){checkpoint_seq_=next;threshold_=history_->order()+1;return;}
  const auto& e=ring_[next%kTxRing];
  if(e.valid && e.seq==next){checkpoint_seq_=next;threshold_=e.first_order;}
 }

 template<class Sink> void data(const uint8_t* p,unsigned n,uint64_t now,Sink& sink){
  if(state_!=State::Open || n<12 || (p[0]>>6)!=2){++stats_.rejected;return;}
  unsigned header=12+4u*(p[0]&0x0f);
  if(p[0]&0x10){if(n<header+4){++stats_.malformed;return;}header+=4+4u*be16(p+header+2);}
  unsigned size=n;
  if(p[0]&0x20){const unsigned pad=p[n-1];if(!pad || pad>n){++stats_.malformed;return;}size-=pad;}
  if(header>=size){++stats_.malformed;return;}
  const uint8_t* payload=p+header;const unsigned length=size-header;
  CommandSection section;
  // A malformed packet is treated as lost: the next journal repairs it.
  if(!read_command_header(payload,length,section)){++stats_.malformed;return;}
  const uint16_t seq=be16(p+2);const uint32_t stamp=be32(p+4);
  const bool first=!have_seq_;uint16_t lost=0;
  if(!first){
   const uint16_t gap=uint16_t(seq-next_seq_);
   if(gap>=0x8000){++stats_.reordered;return;} // late or duplicate: already handled
   lost=gap;
  }
  const uint16_t highest=uint16_t(next_seq_-1);
  have_seq_=true;next_seq_=uint16_t(seq+1);feedback_due_=true;heard_=now;++stats_.packets;
  track_floor(stamp,now);
  const uint64_t base=due(stamp,now,!first);
  // RFC 6295: the first packet is handled as the end of a loss event.
  if(lost || first){
   stats_.lost+=lost;
   const uint8_t* journal=payload+section.header+section.length;
   const unsigned journal_length=length-(section.header+section.length);
   const bool have_journal=section.journal && journal_length>=3;
   if(lost && (!have_journal || !journal_covers(journal,highest))){
    ++stats_.uncovered;receiver_.forget_notes();sink.release(ReleaseReason::Loss);
   }
   if(have_journal){
    auto repair=[&](const uint8_t* bytes,unsigned count){++stats_.repairs;receiver_.apply(bytes,count);sink.midi(bytes,count,base);};
    if(!receiver_.recover(journal,journal_length,repair))++stats_.malformed;
   }
  }
  auto deliver=[&](const uint8_t* bytes,unsigned count,uint32_t delta){
   ++stats_.messages;receiver_.apply(bytes,count);
   sink.midi(bytes,count,delta?due(stamp+delta,now,false):base);
  };
  if(!parse_midi_list(payload+section.header,section.length,payload[0]&0x20,deliver,&stats_.sysex))++stats_.malformed;
 }

 // One RTP-MIDI packet with these messages (none: a journal-only guard).
 // The caller records the messages into the shared history afterwards.
 template<class Sink> bool transmit(const TxMessage* m,unsigned count,uint64_t now,uint32_t now_ms,Sink& sink){
  if(state_!=State::Open)return false;
  uint8_t out[kMaxPacket];Writer w(out,sizeof out);
  const uint16_t seq=tx_seq_;
  const uint32_t stamp=uint32_t((count?m[0].us:now)/100);
  // M stays 0 even with commands, although RFC 6295 2.1 asks for 1: macOS
  // ignores the command section of an M=1 packet and plays it only from the
  // next packet's journal (one packet late), and rtpMIDI for Windows drops
  // it. macOS itself sends M=0.
  w.byte(0x80);w.byte(kPayloadType);w.word(seq);w.dword(stamp);w.dword(ssrc_);
  OutMessage list[kMaxPerPacket];if(count>kMaxPerPacket)count=kMaxPerPacket;
  uint32_t previous=0;
  for(unsigned i=0;i<count;++i){
   list[i].length=m[i].length;memcpy(list[i].bytes,m[i].bytes,3);
   uint32_t delta=uint32_t(m[i].us/100)-stamp;
   if(delta>=0x80000000u || delta<previous)delta=previous; // keep deltas monotonic
   list[i].delta=previous=delta;
  }
  const unsigned section=w.mark();
  if(!write_command_section(w,list,count,true))return false;
  uint8_t journal[kMaxPacket];Writer jw(journal,kMaxPacket-w.size);
  const uint32_t first_order=history_->order()+1;
  if(history_->write(jw,checkpoint_seq_,threshold_,previous_order_,counts_,now_ms) && jw.ok())w.bytes(journal,jw.size);
  else{out[section]&=uint8_t(~0x40);++stats_.journal_full;} // too large: send without
  sink.send(Port::Data,data_,out,w.size);
  ring_[seq%kTxRing]={seq,first_order,true};
  previous_order_=first_order;tx_seq_=uint16_t(seq+1);if(tx_count_<0x10000)++tx_count_;
  for(unsigned i=0;i<count;++i)counts_.record(m[i].bytes,m[i].length);
  last_tx_=now;guard_due_=count!=0;++stats_.sent;stats_.sent_messages+=count;
  return true;
 }

 template<class Sink> void tick(uint64_t now,uint32_t now_ms,Sink& sink){
  if(state_==State::Idle)return;
  const uint64_t silent=now-heard_;
  if(state_==State::ControlOnly){if(silent>kHalfOpenUs)reset();return;}
  if(inviting()){if(silent>kInviteAnswerUs)reset();return;} // tried again later
  if(silent>kTimeoutUs || (stall_t1_ && now-stall_at_>kStallAnswerUs)){end(EndReason::Timeout,sink);return;}
  if(initiator_ && now-sync_>=kSyncUs){sync_=now;send_ck(0,ts(now),0,0,sink);}
  // One probe soon after opening shows whether the host answers ours; only
  // then is a quiet host with sounding notes taken for gone. One that never
  // answers keeps its notes until the timeout above.
  if(!asked_ && now-opened_>=kHeldProbeUs){asked_=true;probe_=now;send_ck(0,ts(now),0,0,sink);}
  if(answered_ && silent>kHeldProbeUs && receiver_.holds_sound()){
   if(silent>kHeldReleaseUs){++stats_.silent;receiver_.forget_notes();sink.release(ReleaseReason::Silent);}
   else if(now-probe_>=kHeldProbeUs){probe_=now;send_ck(0,ts(now),0,0,sink);}
  }
  if(silent>kSilentProbeUs && now-probe_>kProbeIntervalUs){probe_=now;send_ck(0,ts(now),0,0,sink);}
  if(feedback_due_ && now-feedback_>=kFeedbackUs){
   feedback_due_=false;feedback_=now;
   // Apple's layout (sequence number in the upper half) with the same number
   // repeated below for receivers that read the field as 32 bits.
   uint8_t out[12]{0xff,0xff,'R','S'};const uint16_t seq=uint16_t(next_seq_-1);
   put32(out+4,ssrc_);put16(out+8,seq);put16(out+10,seq);
   sink.send(Port::Control,control_,out,sizeof out);
  }
  if(guard_due_ && now-last_tx_>=kGuardUs){
   guard_due_=false;
   if(!have_ack_ || acked_!=uint16_t(tx_seq_-1)){if(transmit(nullptr,0,now,now_ms,sink))++stats_.guards;}
  }
 }

 // Politely leave (BY on the control port), e.g. before the network stops.
 template<class Sink> void close(Sink& sink){
  if(state_==State::Idle)return;
  if(inviting()){reset();return;}
  send_session(Port::Control,control_,'B','Y',token_,false,sink);
  end(EndReason::Closed,sink);
 }

 // The local network went away (cable out on this side): release what sounds
 // now. The session stays; the host's next packets, or its journal, restore
 // what is still held there.
 template<class Sink> void drop_held(Sink& sink){
  if(state_!=State::Open || !receiver_.holds_sound())return;
  receiver_.forget_notes();sink.release(ReleaseReason::Link);
 }

private:
 struct Sent { uint16_t seq=0;uint32_t first_order=0;bool valid=false; };
 static uint64_t ts(uint64_t now_us){return now_us/100;} // AppleMIDI clock: 10 kHz

 void reset(){state_=State::Idle;peer_ssrc_=0;token_=0;control_={};data_={};peer_name_[0]=0;have_seq_=false;feedback_due_=false;guard_due_=false;initiator_=false;stall_t1_=0;}
 template<class Sink> void end(EndReason reason,Sink& sink){const bool was_open=state_==State::Open;const Endpoint control=control_;reset();if(was_open)sink.ended(reason,control);}
 void take_name(const uint8_t* p,unsigned n){
  unsigned size=0;while(16+size<n && size<kNameBytes-1 && p[16+size])++size;
  memcpy(peer_name_,p+16,size);peer_name_[size]=0;
  for(unsigned i=0;i<size;++i)if(uint8_t(peer_name_[i])<0x20)peer_name_[i]='?';
 }

 void start(uint64_t now){
  stats_=Stats{};receiver_.reset();counts_.reset();
  have_seq_=false;floor_valid_=false;last_due_=0;
  probe_=feedback_=last_tx_=opened_=now;feedback_due_=guard_due_=false;asked_=answered_=false;
  uint32_t x=uint32_t(now)^ssrc_^peer_ssrc_;x^=x<<13;x^=x>>17;x^=x<<5;
  tx_seq_=uint16_t(x);tx_count_=0;have_ack_=false;acked_=0;
  checkpoint_seq_=tx_seq_;threshold_=history_->order()+1;previous_order_=0xffffffffu;
  for(auto& e:ring_)e.valid=false;
 }

 // Jitter buffer: (RTP time - arrival time) is largest for the packet with
 // the fastest transit; its maximum over the last 1-2 s is the reference.
 // Messages play at their RTP time relative to that reference plus the
 // configured latency, so network jitter up to the latency is removed and
 // the sender's spacing is kept. The reference follows a faster path at once
 // and a slower one (clock drift) at 1 ms per second.
 void track_floor(uint32_t stamp,uint64_t now){
  const uint32_t d=stamp-uint32_t(now/100);
  auto above=[](uint32_t a,uint32_t b){return int32_t(a-b)>0;};
  const int32_t jump=int32_t(d-floor_);
  if(!floor_valid_ || jump>int32_t(kResyncUnits) || jump<-int32_t(kResyncUnits)){floor_=window_[0]=window_[1]=d;window_start_=slew_=now;floor_valid_=true;return;}
  if(now-window_start_>=kFloorWindowUs){window_[1]=window_[0];window_[0]=d;window_start_=now;}
  else if(above(d,window_[0]))window_[0]=d;
  const uint32_t target=above(window_[1],window_[0])?window_[1]:window_[0];
  if(above(target,floor_)){floor_=target;slew_=now;return;}
  const uint64_t steps=(now-slew_)/100000; // one 100 us unit per 100 ms
  if(steps){slew_+=steps*100000;const uint32_t gap=floor_-target;floor_-=gap<steps?gap:uint32_t(steps);}
 }
 uint64_t due(uint32_t stamp,uint64_t now,bool count_late){
  const int64_t offset=int64_t(int32_t(stamp-floor_-uint32_t(now/100)))*100+options_->latency_us;
  uint64_t at=now;
  if(offset<0){if(count_late)++stats_.late;}
  else at=now+uint64_t(offset>int64_t(kMaxAheadUs)?options_->latency_us:offset);
  if(at<last_due_)at=last_due_; // one session's messages keep their order
  last_due_=at;return at;
 }

 template<class Sink> void send_session(Port port,const Endpoint& to,char a,char b,uint32_t token,bool with_name,Sink& sink){
  uint8_t out[16+kNameBytes]{0xff,0xff,uint8_t(a),uint8_t(b)};
  put32(out+4,kProtocolVersion);put32(out+8,token);put32(out+12,ssrc_);
  unsigned length=16;
  if(with_name && name_){unsigned size=0;while(name_[size] && size<kNameBytes-1)++size;memcpy(out+16,name_,size);out[16+size]=0;length+=size+1;}
  sink.send(port,to,out,length);
 }
 template<class Sink> void send_ck(uint8_t count,uint64_t t1,uint64_t t2,uint64_t t3,Sink& sink){
  uint8_t out[36]{0xff,0xff,'C','K'};put32(out+4,ssrc_);out[8]=count;
  put64(out+12,t1);put64(out+20,t2);put64(out+28,t3);
  sink.send(Port::Data,data_,out,sizeof out);
 }

 uint32_t ssrc_=1;const char* name_=nullptr;const Options* options_=nullptr;const SenderHistory* history_=nullptr;
 State state_=State::Idle;uint32_t peer_ssrc_=0,token_=0;Endpoint control_,data_;char peer_name_[kNameBytes]{};
 uint64_t heard_=0,probe_=0,feedback_=0,last_tx_=0,opened_=0,sync_=0,stall_at_=0,stall_t1_=0;bool asked_=false,answered_=false,initiator_=false;
 // receive
 Receiver receiver_;uint16_t next_seq_=0;bool have_seq_=false,feedback_due_=false;
 uint32_t floor_=0,window_[2]{};uint64_t window_start_=0,slew_=0,last_due_=0;bool floor_valid_=false;
 // send
 SessionCounts counts_;Sent ring_[kTxRing];
 uint16_t tx_seq_=0,acked_=0,checkpoint_seq_=0;uint32_t tx_count_=0,threshold_=1,previous_order_=0xffffffffu;bool have_ack_=false,guard_due_=false;
 Stats stats_;
};

// A host YADOKARI invites again: its control port and its session name.
struct KnownHost { Endpoint control;char name[kNameBytes]{};uint64_t next=0,wait=0;bool valid=false; };
// The remembered hosts as stored on the card: "MNET", version 1, count, then
// per host IPv4 (4 bytes), control port (2), name length (1) and name,
// followed by an FNV-1a hash of everything before it.
constexpr unsigned kKnownBytes=4+1+1+kKnownHosts*(4+2+1+kNameBytes)+4;

// Up to kMaxSessions sessions on one pair of ports, sharing one output
// history. Host must provide:
//   void send(Port port,const Endpoint& to,const uint8_t* bytes,unsigned length);
//   void midi(unsigned slot,const uint8_t* bytes,unsigned length,uint64_t due_us);
//   void release(unsigned slot,ReleaseReason reason);
//   void opened(unsigned slot);
//   void ended(unsigned slot,EndReason reason);
//   void refused(const KnownHost& host);  // a remembered host answered NO
class SessionSet {
public:
 SessionSet(uint32_t seed,const char* name,const Options& options={}):options_(options){
  for(unsigned i=0;i<kMaxSessions;++i){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;sessions_[i].setup(seed?seed:i+1,name,&options_,&history_);}
  token_=seed|1;
 }
 Options& options(){return options_;}
 const Session& session(unsigned slot)const{return sessions_[slot];}
 const SenderHistory& history()const{return history_;}
 unsigned rejected()const{return rejected_;}
 unsigned open_count()const{unsigned n=0;for(const auto& s:sessions_)if(s.open())++n;return n;}
 const KnownHost& known(unsigned i)const{return known_[i];}
 unsigned known_count()const{unsigned n=0;for(const auto& k:known_)if(k.valid)++n;return n;}
 // True once after the remembered hosts changed (to store them).
 bool take_known_changed(){const bool changed=known_changed_;known_changed_=false;return changed;}

 template<class Host> void receive(Port port,const Endpoint& from,const uint8_t* p,unsigned n,uint64_t now,Host& host){
  if(n>=4 && p[0]==0xff && p[1]==0xff){command(port,from,p,n,now,host);return;}
  if(port==Port::Data && n>=12){
   const int slot=find(be32(p+8),from.ip);
   if(slot>=0){Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].data(p,n,now,sink);return;}
  }
  ++rejected_;
 }

 // Every main-loop pass: session timers, then invitations to remembered
 // hosts that are not connected.
 template<class Host> void tick(uint64_t now,Host& host){
  const uint32_t ms=uint32_t(now/1000);
  const bool stalled=after_stall(now);last_tick_=now;
  if(stalled)resumed_=now;
  for(unsigned i=0;i<kMaxSessions;++i){Sink<Host> sink{*this,host,i,now};if(stalled)sessions_[i].stalled(now,sink);sessions_[i].tick(now,ms,sink);}
  invite(now,host);
 }

 // Sends the messages to every open session (in packets of at most
 // kMaxPerPacket), then adds them to the shared journal history.
 template<class Host> void transmit(const TxMessage* m,unsigned count,uint64_t now,Host& host){
  const uint32_t ms=uint32_t(now/1000);
  while(count){
   const unsigned chunk=count<kMaxPerPacket?count:kMaxPerPacket;
   for(unsigned i=0;i<kMaxSessions;++i)if(sessions_[i].open()){Sink<Host> sink{*this,host,i,now};sessions_[i].transmit(m,chunk,now,ms,sink);}
   for(unsigned i=0;i<chunk;++i)history_.record(m[i].bytes,m[i].length,ms);
   m+=chunk;count-=chunk;
  }
 }

 // Ends every session with BY. The hosts stay remembered: they are invited
 // again when the ports open next time.
 template<class Host> void close_all(Host& host){
  closing_=true;
  for(unsigned i=0;i<kMaxSessions;++i){Sink<Host> sink{*this,host,i,0};sessions_[i].close(sink);}
  closing_=false;
  for(auto& k:known_)if(k.valid){k.next=0;k.wait=0;}
 }
 template<class Host> void drop_held(Host& host){for(unsigned i=0;i<kMaxSessions;++i){Sink<Host> sink{*this,host,i,0};sessions_[i].drop_held(sink);}}

 unsigned encode_known(uint8_t* out,unsigned size)const{
  if(size<kKnownBytes)return 0;
  Writer w(out,size);w.byte('M');w.byte('N');w.byte('E');w.byte('T');w.byte(1);w.byte(uint8_t(known_count()));
  for(const auto& k:known_)if(k.valid){
   w.dword(k.control.ip);w.word(k.control.port);
   unsigned n=0;while(k.name[n] && n<kNameBytes-1)++n;w.byte(uint8_t(n));w.bytes(reinterpret_cast<const uint8_t*>(k.name),n);
  }
  w.dword(fnv(out,w.size));
  return w.ok()?w.size:0;
 }
 // Replaces the remembered hosts; false (and nothing changed) when the
 // bytes are not a valid list.
 bool decode_known(const uint8_t* in,unsigned n){
  if(n<10 || in[0]!='M' || in[1]!='N' || in[2]!='E' || in[3]!='T' || in[4]!=1 || in[5]>kKnownHosts)return false;
  KnownHost next[kKnownHosts];unsigned at=6;
  for(unsigned i=0;i<in[5];++i){
   if(at+7>n)return false;
   next[i].control.ip=be32(in+at);next[i].control.port=be16(in+at+4);const unsigned size=in[at+6];at+=7;
   if(size>=kNameBytes || at+size>n || !next[i].control.ip || !next[i].control.port)return false;
   memcpy(next[i].name,in+at,size);next[i].name[size]=0;at+=size;next[i].valid=true;
  }
  if(at+4!=n || be32(in+at)!=fnv(in,at))return false;
  for(unsigned i=0;i<kKnownHosts;++i)known_[i]=next[i];
  return true;
 }

private:
 template<class Host> struct Sink {
  SessionSet& set;Host& host;unsigned slot;uint64_t now;
  void send(Port port,const Endpoint& to,const uint8_t* bytes,unsigned length){host.send(port,to,bytes,length);}
  void midi(const uint8_t* bytes,unsigned length,uint64_t due){host.midi(slot,bytes,length,due);}
  void release(ReleaseReason reason){host.release(slot,reason);}
  void opened(){set.remember(set.sessions_[slot]);host.opened(slot);}
  void ended(EndReason reason,const Endpoint& control){set.lost(control,reason,now);host.ended(slot,reason);}
 };

 static uint32_t fnv(const uint8_t* p,unsigned n){uint32_t h=2166136261u;for(unsigned i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
 int known_at(const Endpoint& control)const{for(unsigned i=0;i<kKnownHosts;++i)if(known_[i].valid && known_[i].control==control)return int(i);return -1;}

 // A session opened: its host goes first in the list (the oldest one drops
 // out when the list is full).
 void remember(const Session& s){
  int at=known_at(s.peer_control());
  KnownHost entry;
  if(at>=0)entry=known_[at];else at=int(kKnownHosts-1);
  const bool changed=!entry.valid || at!=0 || strcmp_name(entry.name,s.peer_name())!=0;
  for(int i=at;i>0;--i)known_[i]=known_[i-1];
  entry.control=s.peer_control();entry.valid=true;entry.next=entry.wait=0;
  unsigned n=0;while(s.peer_name()[n] && n<kNameBytes-1){entry.name[n]=s.peer_name()[n];++n;}entry.name[n]=0;
  known_[0]=entry;
  if(changed)known_changed_=true;
 }
 // A session ended: a host that said goodbye is forgotten; any other, also
 // one whose goodbye came right after a pause (PauseBye), is invited again
 // soon. Closing the ports ourselves keeps it.
 void lost(const Endpoint& control,EndReason reason,uint64_t now){
  const int at=known_at(control);
  if(at<0 || closing_)return;
  if(reason==EndReason::Bye){forget(unsigned(at));return;}
  known_[at].next=now+kInviteFirstUs;known_[at].wait=kInviteRetryUs;
 }
 void forget(unsigned at){for(unsigned i=at;i+1<kKnownHosts;++i)known_[i]=known_[i+1];known_[kKnownHosts-1]=KnownHost{};known_changed_=true;}
 static int strcmp_name(const char* a,const char* b){while(*a && *a==*b){++a;++b;}return int(uint8_t(*a))-int(uint8_t(*b));}

 bool connected(const Endpoint& control)const{for(const auto& s:sessions_)if(!s.idle() && s.peer_control()==control)return true;return false;}
 template<class Host> void invite(uint64_t now,Host& host){
  for(auto& k:known_){
   if(!k.valid || connected(k.control))continue;
   if(!k.next){k.next=now+kInviteFirstUs;k.wait=kInviteRetryUs;continue;}
   if(now<k.next)continue;
   int slot=-1;for(unsigned i=0;i<kMaxSessions;++i)if(sessions_[i].idle()){slot=int(i);break;}
   if(slot<0)return; // every slot is busy
   token_^=token_<<13;token_^=token_>>17;token_^=token_<<5;
   Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].invite(k.control,k.name,token_,now,sink);
   k.next=now+k.wait;k.wait=k.wait*2>kInviteMaxUs?kInviteMaxUs:k.wait*2;
  }
 }

 // The main loop stood still for more than kStallUs before this pass.
 bool after_stall(uint64_t now)const{return last_tick_ && now-last_tick_>kStallUs;}
 // A packet read now was read right after such a pause: before the pass that
 // notices it (packets are read before tick), or within kStallAnswerUs of it.
 bool after_pause(uint64_t now)const{return after_stall(now) || (resumed_ && now-resumed_<kStallAnswerUs);}
 int find(uint32_t ssrc,uint32_t ip)const{for(unsigned i=0;i<kMaxSessions;++i)if(sessions_[i].matches(ssrc,ip))return int(i);return -1;}

 template<class Host> void refuse(Port port,const Endpoint& to,const uint8_t* p,Host& host){
  uint8_t out[16]{0xff,0xff,'N','O'};put32(out+4,kProtocolVersion);put32(out+8,be32(p+8));put32(out+12,sessions_[0].ssrc());
  host.send(port,to,out,sizeof out);++rejected_;
 }

 template<class Host> void command(Port port,const Endpoint& from,const uint8_t* p,unsigned n,uint64_t now,Host& host){
  const char a=char(p[2]),b=char(p[3]);
  if(a=='I' && b=='N'){
   if(n<16 || be32(p+4)!=kProtocolVersion){++rejected_;return;}
   const uint32_t ssrc=be32(p+12);
   int slot=find(ssrc,from.ip);
   if(port==Port::Control){
    // Same host again, or a new session from a port an old one used
    // (the host restarted without saying goodbye), or a free slot.
    if(slot<0)for(unsigned i=0;i<kMaxSessions;++i)if(sessions_[i].at_control(from)){slot=int(i);break;}
    if(slot<0)for(unsigned i=0;i<kMaxSessions;++i)if(sessions_[i].idle()){slot=int(i);break;}
    if(slot<0){refuse(port,from,p,host);return;}
    Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].invite_control(from,p,n,now,sink);
    return;
   }
   if(slot<0){refuse(port,from,p,host);return;}
   Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].invite_data(from,p,now,sink);
   return;
  }
  if(a=='B' && b=='Y'){
   const int slot=n>=16?find(be32(p+12),from.ip):-1;
   if(slot<0){++rejected_;return;}
   Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].bye(sink,after_pause(now));return;
  }
  if(a=='C' && b=='K'){
   const int slot=n>=36 && port==Port::Data?find(be32(p+4),from.ip):-1;
   if(slot<0){++rejected_;return;}
   Sink<Host> sink{*this,host,unsigned(slot),now};sessions_[slot].clock(p,n,now,sink);return;
  }
  if(a=='R' && b=='S'){
   const int slot=n>=12?find(be32(p+4),from.ip):-1;
   if(slot<0){++rejected_;return;}
   sessions_[slot].feedback(p,n,now);return;
  }
  if((a=='O' && b=='K') || (a=='N' && b=='O')){
   const bool ok=a=='O';
   for(unsigned i=0;i<kMaxSessions;++i){
    Sink<Host> sink{*this,host,i,now};const Endpoint control=sessions_[i].peer_control();
    if(!sessions_[i].answer(port,from,p,n,ok,now,sink))continue;
    if(!ok){const int at=known_at(control);if(at>=0){host.refused(known_[at]);forget(unsigned(at));}}
    return;
   }
  }
  ++rejected_; // an answer to no invitation of ours
 }

 Options options_;SenderHistory history_;Session sessions_[kMaxSessions];unsigned rejected_=0;
 KnownHost known_[kKnownHosts];uint32_t token_=1;uint64_t last_tick_=0,resumed_=0;bool known_changed_=false,closing_=false;
};

} // namespace groove::rtpmidi
