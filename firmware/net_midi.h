#pragma once
// Network MIDI over Ethernet: RTP-MIDI (RFC 6295) with the AppleMIDI session
// protocol on UDP 5004 (control) and 5005 (data), announced over mDNS as
// _apple-midi._udp. Up to four hosts at once (macOS Network MIDI, rtpMIDI on
// Windows, rtpmidid on Linux), each in both directions with a recovery
// journal. Switched by SETTINGS → MIDI → NETWORK MIDI (device settings) and
// polled from the kernel main task (core 0); switching it off ends every
// session (BY) and frees the ports, while Ethernet itself stays up.
// Held notes do not outlive a lost connection: a host that stops answering
// releases its notes after 3 s, and a link loss on the Pi releases them at
// once; the sessions themselves stay open (rtp_midi_session.h).
// Hosts that connected are remembered (kernel: GROOVE/NETHOSTS.CFG) and
// invited again after a restart, a timeout or a long main-loop pause, until
// they say goodbye or refuse (a goodbye read right after a long pause counts
// as a loss).
//
// Input: each session is its own logical router cable (12..15), so a host
// that leaves releases only its own notes. Channel messages wait in a
// per-session queue until the local time their RTP timestamp names (plus
// the latency); clock, transport and song position go to the engine's
// sample-timed queue at once, stamped with the audio sample of that time.
// Output: every message the engine sends to MK3 MIDI OUT also goes to every
// open session, stamped with the time its audio sample was rendered.
#include "../src/rtp_midi_session.h"
#include "../src/rtp_midi_timing.h"
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/net/mdnspublisher.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/timer.h>

class CNetMidi {
public:
 static constexpr unsigned kFirstCable=12; // router cables 12..15; MK3 DIN uses cable 0
 static constexpr unsigned kSessions=groove::rtpmidi::kMaxSessions;
 static constexpr uint64_t kStatsUs=60000000;

 static constexpr const char* kName="YADOKARI"; // session and mDNS name

 ~CNetMidi(){delete m_Sessions;delete m_Control;delete m_Data;} // the mDNS task is never deleted
 void Attach(CNetSubSystem* net){m_Net=net;}
 bool attached()const{return m_Net!=nullptr;}
 bool open()const{return m_Data!=nullptr;}
 void SetLatency(unsigned ms){m_Options.latency_us=ms*1000;if(m_Sessions)m_Sessions->options().latency_us=m_Options.latency_us;}
 // The remembered hosts as stored on the card (before or after Attach).
 void LoadHosts(const uint8_t* bytes,unsigned length){
  if(length>sizeof m_Stored)return;
  memcpy(m_Stored,bytes,length);m_StoredLength=length;
  if(m_Sessions)ApplyStoredHosts();
 }
 bool TakeHostsChanged(){return m_Sessions && m_Sessions->take_known_changed();}
 unsigned EncodeHosts(uint8_t* out,unsigned size)const{return m_Sessions?m_Sessions->encode_known(out,size):0;}
 // A short state line for the screen: address and hosts, or what it waits for.
 void Status(char* out,unsigned size)const{
  CString text;
  if(!m_Net)text="";
  else if(m_Failed)text="UDP 5004/5005 unavailable";
  else if(!m_Net->IsRunning())text="Waiting for DHCP";
  else{CString ip;m_Net->GetConfig()->GetIPAddress()->Format(&ip);const unsigned hosts=m_Sessions?m_Sessions->open_count():0;
   text.Format("%s:%u / %u host%s",static_cast<const char*>(ip),unsigned(groove::rtpmidi::kControlPort),hosts,hosts==1?"":"s");}
  const char* t=text;unsigned n=0;while(t[n] && n+1<size){out[n]=t[n];++n;}if(size)out[n]=0;
 }

 void SetSampleRate(uint32_t rate){if(rate)m_Clock=groove::rtpmidi::SampleClock(rate);}
 // Every main-loop pass: the engine's rendered-sample counter, which maps
 // local time to audio samples in both directions.
 void ObserveAudio(uint64_t samples){m_Clock.observe(CTimer::GetClockTicks64(),samples);}

 // deliver(cable,bytes,length)        : a channel message whose time has come
 // timed(cable,bytes,length,sample)   : clock/transport for that audio sample (0: now)
 // release(cable)                     : drop every note the cable holds
 // With input false (probes, updates) sessions keep running but nothing is
 // delivered, and what the sessions held is released once. With enabled
 // false every session ends and the ports close.
 template<class Deliver,class Timed,class Release> void Poll(bool enabled,bool input,Deliver deliver,Timed timed,Release release){
  if(!m_Net)return;
  const uint64_t now=CTimer::GetClockTicks64();
  Host<Deliver,Timed,Release> host{*this,deliver,timed,release,input,now};
  if(!enabled){m_Failed=false;if(m_Data)Close(host);return;}
  if(m_Failed)return;
  if(!m_Net->IsRunning()){
   // Cable out (or the DHCP lease lost): no host can reach us, so what
   // sounds is released now rather than when the link returns.
   if(m_Data && m_LinkUp){m_LinkUp=false;m_Sessions->drop_held(host);CLogger::Get()->Write("netmidi",LogWarning,"Ethernet link down");}
   return;
  }
  if(m_Data && !m_LinkUp){m_LinkUp=true;CLogger::Get()->Write("netmidi",LogNotice,"Ethernet link up");}
  if(!m_Data && !Open())return;
  if(m_Input && !input)for(unsigned slot=0;slot<kSessions;++slot){m_Queue[slot].clear();release(kFirstCable+slot);}
  m_Input=input;
  // Bounded per pass so a flood cannot starve the UI/USB loop.
  for(unsigned i=0;i<8;++i){if(!Drain(m_Control,groove::rtpmidi::Port::Control,now,host))break;}
  for(unsigned i=0;i<64;++i){if(!Drain(m_Data,groove::rtpmidi::Port::Data,now,host))break;}
  m_Sessions->tick(now,host);
  for(unsigned slot=0;slot<kSessions;++slot)m_Queue[slot].release(now,[&](const uint8_t* bytes,unsigned length){deliver(kFirstCable+slot,bytes,length);});
  if(now-m_LastStats>=kStatsUs){m_LastStats=now;LogStats();}
 }

 // Output: one message from the engine's MIDI output (sample: its render
 // position). Sent by FlushOutput.
 void Queue(const uint8_t* bytes,unsigned length,uint64_t sample){
  if(!m_Sessions || !m_Sessions->open_count())return;
  if(!length || length>3 || bytes[0]<0x80 || bytes[0]==0xf0 || bytes[0]==0xf7 || length!=groove::rtpmidi::message_length(bytes[0]))return;
  if(m_Pending==kOutBatch)FlushOutput();
  const uint64_t now=CTimer::GetClockTicks64();
  uint64_t us=sample && m_Clock.valid()?m_Clock.us_at(sample):now;
  if(us>now)us=now;
  if(us+kMaxOutputAgeUs<now)us=now-kMaxOutputAgeUs; // a backlog plays now, not long ago
  if(us<m_LastOutUs)us=m_LastOutUs;
  m_LastOutUs=us;
  auto& m=m_Out[m_Pending++];memcpy(m.bytes,bytes,length);m.length=uint8_t(length);m.us=us;
 }
 void FlushOutput(){
  if(!m_Pending || !m_Sessions){m_Pending=0;return;}
  OutputHost host{*this};
  m_Sessions->transmit(m_Out,m_Pending,CTimer::GetClockTicks64(),host);
  m_Pending=0;
 }

private:
 static constexpr unsigned kOutBatch=64;
 static constexpr uint64_t kMaxOutputAgeUs=50000;

 template<class Deliver,class Timed,class Release> struct Host {
  CNetMidi& self;Deliver& deliver;Timed& timed;Release& on_release;bool input;uint64_t now;
  void send(groove::rtpmidi::Port port,const groove::rtpmidi::Endpoint& to,const uint8_t* bytes,unsigned length){self.Send(port,to,bytes,length);}
  void midi(unsigned slot,const uint8_t* bytes,unsigned length,uint64_t due){
   if(!input)return;
   const unsigned cable=kFirstCable+slot;
   if(bytes[0]>=0xf0){timed(cable,bytes,length,self.SampleFor(due));return;}
   auto& queue=self.m_Queue[slot];
   if(due<=now && queue.empty()){deliver(cable,bytes,length);return;}
   if(queue.full()){const auto item=queue.front();queue.pop();deliver(cable,item.bytes,unsigned(item.length));++self.m_Early;}
   queue.push(due,bytes,length);
  }
  // A loss the journal cannot repair, a host gone quiet or the link down:
  // play what waits, then release.
  void release(unsigned slot,groove::rtpmidi::ReleaseReason reason){
   const unsigned cable=kFirstCable+slot;
   if(input)self.m_Queue[slot].flush([&](const uint8_t* bytes,unsigned length){deliver(cable,bytes,length);});
   else self.m_Queue[slot].clear();
   on_release(cable);
   using groove::rtpmidi::ReleaseReason;
   CLogger::Get()->Write("netmidi",LogWarning,"Session %u: %s; released its notes",slot,
    reason==ReleaseReason::Loss?"loss without journal":reason==ReleaseReason::Silent?"host silent for 3 s":"Ethernet link down");
  }
  void opened(unsigned slot){
   const auto& s=self.m_Sessions->session(slot);
   CString text;FormatIp(s.peer_control().ip,text);
   CLogger::Get()->Write("netmidi",LogNotice,"Session %u open with \"%s\" (%s:%u), cable %u%s",slot,s.peer_name(),static_cast<const char*>(text),unsigned(s.peer_control().port),kFirstCable+slot,s.initiator()?", invited by YADOKARI":"");
  }
  void refused(const groove::rtpmidi::KnownHost& host){
   CString text;FormatIp(host.control.ip,text);
   CLogger::Get()->Write("netmidi",LogNotice,"\"%s\" (%s:%u) refused the invitation; not inviting it again",host.name,static_cast<const char*>(text),unsigned(host.control.port));
  }
  void ended(unsigned slot,groove::rtpmidi::EndReason reason){
   self.m_Queue[slot].clear();
   on_release(kFirstCable+slot);
   CLogger::Get()->Write("netmidi",LogNotice,"Session %u ended (%s)",slot,groove::rtpmidi::end_reason_name(reason));
   self.LogSession(slot);
  }
 };
 struct OutputHost {
  CNetMidi& self;
  void send(groove::rtpmidi::Port port,const groove::rtpmidi::Endpoint& to,const uint8_t* bytes,unsigned length){self.Send(port,to,bytes,length);}
  void midi(unsigned,const uint8_t*,unsigned,uint64_t){}
  void release(unsigned,groove::rtpmidi::ReleaseReason){}
  void opened(unsigned){}
  void ended(unsigned,groove::rtpmidi::EndReason){}
  void refused(const groove::rtpmidi::KnownHost&){}
 };
 static void FormatIp(uint32_t ip,CString& text){u8 bytes[4];memcpy(bytes,&ip,4);CIPAddress address(bytes);address.Format(&text);}
 void ApplyStoredHosts(){
  if(!m_StoredLength)return;
  if(m_Sessions->decode_known(m_Stored,m_StoredLength))CLogger::Get()->Write("netmidi",LogNotice,"Remembered hosts: %u",m_Sessions->known_count());
  else CLogger::Get()->Write("netmidi",LogWarning,"Remembered hosts file is damaged; starting without");
  m_StoredLength=0;
 }

 static uint32_t Seed(){uint64_t t=CTimer::GetClockTicks64();uint32_t x=uint32_t(t^(t>>29))*2654435761u;return x?x:1;}

 // Engine timed messages must not go back in time.
 uint64_t SampleFor(uint64_t due_us){
  if(!m_Clock.valid())return 0;
  uint64_t sample=m_Clock.sample_at(due_us);
  if(sample<m_LastTimed)sample=m_LastTimed;
  m_LastTimed=sample;
  return sample;
 }

 void Send(groove::rtpmidi::Port port,const groove::rtpmidi::Endpoint& to,const uint8_t* bytes,unsigned length){
  CSocket* socket=port==groove::rtpmidi::Port::Control?m_Control:m_Data;
  u8 ip[4];memcpy(ip,&to.ip,4);CIPAddress address(ip);
  if(!socket || socket->SendTo(bytes,length,MSG_DONTWAIT,address,to.port)<0)++m_SendErrors;
 }

 bool Open(){
  m_Control=new CSocket(m_Net,IPPROTO_UDP);m_Data=new CSocket(m_Net,IPPROTO_UDP);
  if(!m_Control || !m_Data || m_Control->Bind(groove::rtpmidi::kControlPort)<0 || m_Data->Bind(groove::rtpmidi::kDataPort)<0){
   CLogger::Get()->Write("netmidi",LogError,"Cannot bind UDP %u/%u",unsigned(groove::rtpmidi::kControlPort),unsigned(groove::rtpmidi::kDataPort));
   delete m_Control;delete m_Data;m_Control=m_Data=nullptr;m_Failed=true;return false;
  }
  // DHCP took a variable time by now, so the tick count seeds the SSRCs.
  if(!m_Sessions)m_Sessions=new groove::rtpmidi::SessionSet(Seed(),kName,m_Options);
  if(!m_Sessions){CLogger::Get()->Write("netmidi",LogError,"Session allocation failed");delete m_Control;delete m_Data;m_Control=m_Data=nullptr;m_Failed=true;return false;}
  m_Sessions->options().latency_us=m_Options.latency_us;
  ApplyStoredHosts();
  CString ip;m_Net->GetConfig()->GetIPAddress()->Format(&ip);
  if(!m_Publisher)m_Publisher=new CmDNSPublisher(m_Net);
  const bool published=m_Publisher && m_Publisher->PublishService(kName,CmDNSPublisher::ServiceTypeAppleMIDI,groove::rtpmidi::kControlPort);
  CLogger::Get()->Write("netmidi",LogNotice,"RTP-MIDI \"%s\" listening on %s:%u, mDNS %s, latency %u ms, up to %u sessions",kName,static_cast<const char*>(ip),unsigned(groove::rtpmidi::kControlPort),published?"published":"unavailable",unsigned(m_Options.latency_us/1000),kSessions);
  m_LastStats=CTimer::GetClockTicks64();m_LinkUp=true;
  return true;
 }
 // Ends every session with BY (the host callbacks release their notes),
 // then frees the ports and withdraws the mDNS announcement.
 template<class H> void Close(H& host){
  m_Sessions->close_all(host);
  for(auto& queue:m_Queue)queue.clear();
  m_Pending=0;
  delete m_Control;delete m_Data;m_Control=m_Data=nullptr;
  if(m_Publisher)m_Publisher->UnpublishService(kName);
  CLogger::Get()->Write("netmidi",LogNotice,"RTP-MIDI stopped (setting off)");
 }


 template<class H> bool Drain(CSocket* socket,groove::rtpmidi::Port port,uint64_t now,H& host){
  CIPAddress address;u16 from_port=0;
  const int n=socket->ReceiveFrom(m_Buffer,sizeof m_Buffer,MSG_DONTWAIT,&address,&from_port);
  if(n<=0)return false;
  groove::rtpmidi::Endpoint from;u8 ip[4];address.CopyTo(ip);memcpy(&from.ip,ip,4);from.port=from_port;
  m_Sessions->receive(port,from,m_Buffer,unsigned(n),now,host);
  return true;
 }

 void LogSession(unsigned slot){
  const auto& s=m_Sessions->session(slot).stats();
  CLogger::Get()->Write("netmidi",LogNotice,"Session %u in: packets %u messages %u lost %u repairs %u uncovered %u late %u reordered %u malformed %u sysex %u silent %u",
   slot,unsigned(s.packets),unsigned(s.messages),unsigned(s.lost),unsigned(s.repairs),unsigned(s.uncovered),unsigned(s.late),unsigned(s.reordered),unsigned(s.malformed),unsigned(s.sysex),unsigned(s.silent));
  CLogger::Get()->Write("netmidi",LogNotice,"Session %u out: packets %u messages %u guards %u acks %u journal-full %u rtt-us %u send-errors %u early %u",
   slot,unsigned(s.sent),unsigned(s.sent_messages),unsigned(s.guards),unsigned(s.acks),unsigned(s.journal_full),unsigned(s.rtt_us),m_SendErrors,m_Early);
 }
 void LogStats(){for(unsigned slot=0;slot<kSessions;++slot)if(m_Sessions->session(slot).open())LogSession(slot);}

 CNetSubSystem* m_Net=nullptr;CSocket* m_Control=nullptr;CSocket* m_Data=nullptr;CmDNSPublisher* m_Publisher=nullptr;
 groove::rtpmidi::Options m_Options;
 groove::rtpmidi::SessionSet* m_Sessions=nullptr;
 groove::rtpmidi::SampleClock m_Clock;
 groove::rtpmidi::DueQueue m_Queue[kSessions];
 groove::rtpmidi::TxMessage m_Out[kOutBatch];unsigned m_Pending=0;
 uint64_t m_LastTimed=0,m_LastOutUs=0,m_LastStats=0;
 bool m_Failed=false,m_Input=true,m_LinkUp=true;unsigned m_SendErrors=0,m_Early=0;
 u8 m_Buffer[1500];
 uint8_t m_Stored[groove::rtpmidi::kKnownBytes];unsigned m_StoredLength=0;
};
