// Network MIDI (RTP-MIDI, RFC 6295, with the AppleMIDI session protocol):
// session handshake for several hosts, clock sync, the command section in
// both directions, recovery journals built byte by byte from the RFC, an
// end-to-end lossy link between two endpoints, jitter-buffer timing, and the
// audio-sample clock mapping. Packets are laid out as macOS, rtpMIDI
// (Windows) and rtpmidid (Linux) send them.
#include "../src/rtp_midi_session.h"
#include "../src/rtp_midi_timing.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace groove::rtpmidi;

namespace {
using Bytes=std::vector<uint8_t>;
struct Sent { Port port;Endpoint to;Bytes bytes; };
struct Got { unsigned slot;Bytes bytes;uint64_t due; };
struct Host {
 std::vector<Sent> sent;std::vector<Got> got;std::vector<unsigned> releases,opens;std::vector<ReleaseReason> release_reasons;std::vector<std::pair<unsigned,EndReason>> endings;std::vector<Endpoint> refusals;
 void send(Port port,const Endpoint& to,const uint8_t* b,unsigned n){sent.push_back({port,to,Bytes(b,b+n)});}
 void midi(unsigned slot,const uint8_t* b,unsigned n,uint64_t due){got.push_back({slot,Bytes(b,b+n),due});}
 void release(unsigned slot,ReleaseReason why){releases.push_back(slot);release_reasons.push_back(why);}
 void opened(unsigned slot){opens.push_back(slot);}
 void ended(unsigned slot,EndReason r){endings.push_back({slot,r});}
 void refused(const KnownHost& k){refusals.push_back(k.control);}
 std::vector<Bytes> messages()const{std::vector<Bytes> out;for(const auto& g:got)out.push_back(g.bytes);return out;}
 std::vector<Sent> data_packets()const{std::vector<Sent> out;for(const auto& s:sent)if(s.port==Port::Data && s.bytes.size()>=12 && s.bytes[0]==0x80)out.push_back(s);return out;}
 void clear(){sent.clear();got.clear();releases.clear();release_reasons.clear();opens.clear();endings.clear();refusals.clear();}
 std::vector<Sent> probes()const{std::vector<Sent> out;for(const auto& s:sent)if(s.bytes.size()==36 && s.bytes[2]=='C' && s.bytes[3]=='K' && s.bytes[8]==0)out.push_back(s);return out;}
 std::vector<Sent> invitations(Port port)const{std::vector<Sent> out;for(const auto& s:sent)if(s.port==port && s.bytes.size()>=16 && s.bytes[0]==0xff && s.bytes[2]=='I' && s.bytes[3]=='N')out.push_back(s);return out;}
};

constexpr uint32_t kMac=0x0a2a0001,kWin=0x0a2a0002,kPi=0x0a2a0003; // opaque addresses
constexpr uint32_t kPeerSsrc=0x11223344,kToken=0x01020304;
const Endpoint kMacControl{kMac,50004},kMacData{kMac,50005},kWinControl{kWin,5008},kWinData{kWin,5009},kPiControl{kPi,5004},kPiData{kPi,5005};

Bytes session_packet(char a,char b,uint32_t token,uint32_t ssrc,const char* name){
 Bytes p{0xff,0xff,uint8_t(a),uint8_t(b),0,0,0,0,0,0,0,0,0,0,0,0};
 put32(p.data()+4,2);put32(p.data()+8,token);put32(p.data()+12,ssrc);
 if(name)for(const char* c=name;;++c){p.push_back(uint8_t(*c));if(!*c)break;}
 return p;
}
Bytes ck(uint32_t ssrc,uint8_t count,uint64_t t1,uint64_t t2=0,uint64_t t3=0){
 Bytes p(36,0);p[0]=p[1]=0xff;p[2]='C';p[3]='K';put32(p.data()+4,ssrc);p[8]=count;
 put64(p.data()+12,t1);put64(p.data()+20,t2);put64(p.data()+28,t3);return p;
}
Bytes rs(uint32_t ssrc,uint16_t hi,uint16_t lo){Bytes p{0xff,0xff,'R','S',0,0,0,0,0,0,0,0};put32(p.data()+4,ssrc);put16(p.data()+8,hi);put16(p.data()+10,lo);return p;}
// RTP header (V=2, PT=0x61) followed by the payload.
Bytes rtp(uint16_t seq,uint32_t stamp,uint32_t ssrc,const Bytes& payload){
 Bytes p{0x80,0x61,0,0,0,0,0,0,0,0,0,0};put16(p.data()+2,seq);put32(p.data()+4,stamp);put32(p.data()+8,ssrc);
 p.insert(p.end(),payload.begin(),payload.end());return p;
}
// Short command section; flags: 0x40 J, 0x20 Z.
Bytes section(const Bytes& list,uint8_t flags=0){assert(list.size()<16);Bytes s{uint8_t(flags|list.size())};s.insert(s.end(),list.begin(),list.end());return s;}
Bytes cat(Bytes a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());return a;}
// Recovery journal from system and channel journals (S bits left at 0).
Bytes journal(uint16_t checkpoint,const Bytes& system,const std::vector<Bytes>& channels){
 Bytes j{uint8_t((system.empty()?0:0x40)|(channels.empty()?0:0x20)|(channels.empty()?0:channels.size()-1)),0,0};put16(j.data()+1,checkpoint);
 j=cat(j,system);for(const auto& c:channels)j=cat(j,c);return j;
}
Bytes channel_journal(unsigned ch,uint8_t toc,const Bytes& chapters){
 const unsigned length=3+unsigned(chapters.size());
 return cat(Bytes{uint8_t(ch<<3|(length>>8)),uint8_t(length),toc},chapters);
}
// System journal: toc in S|D|V|Q|F|X order (D=0x40, V=0x20, Q=0x10).
Bytes system_journal(uint8_t toc,const Bytes& chapters){
 const unsigned length=2+unsigned(chapters.size());
 return cat(Bytes{uint8_t(toc|(length>>8)),uint8_t(length)},chapters);
}

template<class V> void feed(SessionSet& s,Port port,const Endpoint& from,const V& p,uint64_t now,Host& host){s.receive(port,from,p.data(),unsigned(p.size()),now,host);}

unsigned open_session(SessionSet& s,Host& host,const Endpoint& control,const Endpoint& data,uint32_t ssrc,const char* name,uint64_t now){
 feed(s,Port::Control,control,session_packet('I','N',kToken,ssrc,name),now,host);
 feed(s,Port::Data,data,session_packet('I','N',kToken,ssrc,name),now,host);
 assert(!host.opens.empty());const unsigned slot=host.opens.back();
 assert(s.session(slot).open());
 host.clear();return slot;
}

// ------------------------------------------------------------ wire format

void test_command_section_parse(){
 // Running status, a realtime byte inside it, delta times and a trailing delta.
 const Bytes list{0x90,0x3c,0x64, 0x05,0x3e,0x50, 0x00,0xf8, 0x81,0x00,0x40,0x00, 0x03};
 std::vector<std::pair<Bytes,uint32_t>> got;
 assert(parse_midi_list(list.data(),unsigned(list.size()),false,[&](const uint8_t* b,unsigned n,uint32_t t){got.push_back({Bytes(b,b+n),t});}));
 assert(got.size()==4);
 assert((got[0].first==Bytes{0x90,0x3c,0x64}) && got[0].second==0);
 assert((got[1].first==Bytes{0x90,0x3e,0x50}) && got[1].second==5);
 assert((got[2].first==Bytes{0xf8}) && got[2].second==5);
 assert((got[3].first==Bytes{0x90,0x40,0x00}) && got[3].second==5+128);
 // Z=1: the first command has a delta too.
 const Bytes z{0x0a,0xb0,0x07,0x64};got.clear();
 assert(parse_midi_list(z.data(),4,true,[&](const uint8_t* b,unsigned n,uint32_t t){got.push_back({Bytes(b,b+n),t});}));
 assert(got.size()==1 && got[0].second==10);
 // SysEx (whole, segmented first/last, cancelled) is skipped and counted.
 const Bytes sysex{0xf0,0x7e,0x01,0xf7, 0x00,0xf0,0x01,0xf0, 0x00,0xf7,0x02,0xf7, 0x00,0xf7,0x03,0xf4, 0x00,0xc0,0x05};
 uint32_t count=0;got.clear();
 assert(parse_midi_list(sysex.data(),unsigned(sysex.size()),false,[&](const uint8_t* b,unsigned n,uint32_t t){got.push_back({Bytes(b,b+n),t});},&count));
 assert(count==4 && got.size()==1 && (got[0].first==Bytes{0xc0,0x05}));
 // Malformed: running status without a status byte, a truncated message,
 // an unterminated delta.
 const Bytes orphan{0x3c,0x64};assert(!parse_midi_list(orphan.data(),2,false,[](const uint8_t*,unsigned,uint32_t){}));
 const Bytes cut{0x90,0x3c};assert(!parse_midi_list(cut.data(),2,false,[](const uint8_t*,unsigned,uint32_t){}));
 const Bytes delta{0x90,0x3c,0x64,0x80,0x80};assert(!parse_midi_list(delta.data(),5,false,[](const uint8_t*,unsigned,uint32_t){}));
 // Long (B=1) header.
 CommandSection cs;const Bytes big{0x80|0x40,0x10};assert(!read_command_header(big.data(),2,cs));
 Bytes big_ok(2+16,0);big_ok[0]=0x80;big_ok[1]=0x10;assert(read_command_header(big_ok.data(),unsigned(big_ok.size()),cs) && cs.header==2 && cs.length==16 && !cs.journal);
}

void test_command_section_write(){
 for(uint32_t step:{0u,1u,127u,128u,16383u,16384u,(1u<<21)-1,1u<<21}){
  OutMessage m[6];
  for(unsigned i=0;i<6;++i){m[i].bytes[0]=uint8_t(0x90|i);m[i].bytes[1]=uint8_t(60+i);m[i].bytes[2]=100;m[i].length=3;m[i].delta=(i+(step?1:0))*step;}
  m[5].bytes[0]=0xf8;m[5].length=1;
  uint8_t buffer[256];Writer w(buffer,sizeof buffer);
  assert(write_command_section(w,m,6,true));
  CommandSection cs;assert(read_command_header(buffer,w.size,cs) && cs.journal && cs.header==2);
  std::vector<std::pair<Bytes,uint32_t>> got;
  assert(parse_midi_list(buffer+cs.header,cs.length,buffer[0]&0x20,[&](const uint8_t* b,unsigned n,uint32_t t){got.push_back({Bytes(b,b+n),t});}));
  assert(got.size()==6);
  for(unsigned i=0;i<6;++i){assert(got[i].second==m[i].delta);assert(got[i].first==Bytes(m[i].bytes,m[i].bytes+m[i].length));}
  assert(bool(buffer[0]&0x20)==(step!=0));
 }
 // A short list keeps the 1-octet header.
 OutMessage one;one.bytes[0]=0xfa;one.length=1;uint8_t buffer[8];Writer w(buffer,sizeof buffer);
 assert(write_command_section(w,&one,1,false) && w.size==2 && buffer[0]==0x01 && buffer[1]==0xfa);
}

// ---------------------------------------------------------------- session

void test_handshake_several_hosts(){
 SessionSet s(42,"YADOKARI");Host host;
 feed(s,Port::Control,kMacControl,session_packet('I','N',kToken,kPeerSsrc,"MacBook Session"),1000,host);
 assert(host.sent.size()==1);
 const auto& ok=host.sent[0];
 assert(ok.port==Port::Control && ok.to==kMacControl && ok.bytes[2]=='O' && ok.bytes[3]=='K');
 assert(be32(ok.bytes.data()+8)==kToken && std::string(reinterpret_cast<const char*>(ok.bytes.data()+16))=="YADOKARI");
 const uint32_t our=be32(ok.bytes.data()+12);
 // The OK got lost: the retransmitted invitation is answered again.
 feed(s,Port::Control,kMacControl,session_packet('I','N',kToken,kPeerSsrc,"MacBook Session"),2000,host);
 assert(host.sent.size()==2 && host.opens.empty());
 feed(s,Port::Data,kMacData,session_packet('I','N',kToken,kPeerSsrc,"MacBook Session"),3000,host);
 assert(host.opens.size()==1 && host.sent.back().port==Port::Data && host.sent.back().bytes[2]=='O' && be32(host.sent.back().bytes.data()+12)==our);
 const unsigned mac=host.opens[0];
 assert(std::string(s.session(mac).peer_name())=="MacBook Session");
 // A second host (rtpMIDI on Windows) gets its own slot and SSRC.
 const unsigned win=open_session(s,host,kWinControl,kWinData,0x55667788,"PC",4000);
 assert(win!=mac && s.open_count()==2 && s.session(win).ssrc()!=s.session(mac).ssrc());
 // Data invitation without a control one, and a bad version, are refused.
 feed(s,Port::Data,kPiData,session_packet('I','N',kToken,0x999,"x"),5000,host);
 assert(host.sent.size()==1 && host.sent[0].bytes[2]=='N' && host.sent[0].bytes[3]=='O');
 auto bad=session_packet('I','N',kToken,0x999,"x");put32(bad.data()+4,1);host.clear();
 feed(s,Port::Control,kPiControl,bad,5000,host);assert(host.sent.empty());
 // Four sessions at most.
 open_session(s,host,Endpoint{kPi,6000},Endpoint{kPi,6001},0x1001,"three",6000);
 open_session(s,host,Endpoint{kPi,6002},Endpoint{kPi,6003},0x1002,"four",6000);
 feed(s,Port::Control,Endpoint{kPi,6004},session_packet('I','N',kToken,0x1003,"five"),7000,host);
 assert(host.sent.size()==1 && host.sent[0].bytes[2]=='N' && s.open_count()==4);
 // BY ends one session; its slot is free again.
 host.clear();
 feed(s,Port::Control,kWinControl,session_packet('B','Y',kToken,0x55667788,nullptr),8000,host);
 assert(host.endings.size()==1 && host.endings[0].first==win && host.endings[0].second==EndReason::Bye && s.open_count()==3);
 // A host app that restarts on the same port with a new SSRC replaces its old session.
 host.clear();
 feed(s,Port::Control,kMacControl,session_packet('I','N',kToken,0x77777777,"MacBook Session"),9000,host);
 assert(host.endings.size()==1 && host.endings[0].first==mac && host.endings[0].second==EndReason::Replaced);
}

void test_clock_sync_and_probe(){
 SessionSet s(7,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 const uint32_t our=s.session(slot).ssrc();
 feed(s,Port::Data,kMacData,ck(kPeerSsrc,0,123456789),2000000,host);
 assert(host.sent.size()==1);
 const auto& r=host.sent[0].bytes;
 assert(host.sent[0].port==Port::Data && host.sent[0].to==kMacData && r[2]=='C' && r[3]=='K' && be32(r.data()+4)==our && r[8]==1);
 assert(be64(r.data()+12)==123456789 && be64(r.data()+20)==2000000/100);
 feed(s,Port::Data,kMacData,ck(kPeerSsrc,2,123456789,20000,123456789+25),2000300,host);
 assert(host.sent.size()==1 && s.session(slot).stats().rtt_us==2500);
 // CK with an unknown SSRC, or on the control port, is ignored.
 feed(s,Port::Data,kMacData,ck(0xdead,0,1),2000400,host);feed(s,Port::Control,kMacControl,ck(kPeerSsrc,0,1),2000400,host);
 assert(host.sent.size()==1);
 // Silence: after 15 s YADOKARI probes with CK0 and answers the CK1.
 const uint64_t probe=2000300+kSilentProbeUs+1;
 host.clear();s.tick(probe,host);
 assert(host.sent.size()==1 && host.sent[0].bytes[2]=='C' && host.sent[0].bytes[8]==0);
 const uint64_t t1=be64(host.sent[0].bytes.data()+12);host.clear();
 feed(s,Port::Data,kMacData,ck(kPeerSsrc,1,t1,555),probe+3000,host);
 assert(host.sent.size()==1 && host.sent[0].bytes[8]==2 && be64(host.sent[0].bytes.data()+12)==t1 && be64(host.sent[0].bytes.data()+20)==555);
 assert(s.session(slot).stats().rtt_us==((probe+3000)/100-probe/100)*100);
 // A minute without a word ends the session.
 host.clear();s.tick(probe+3000+kTimeoutUs+1,host);
 assert(host.endings.size()==1 && host.endings[0].second==EndReason::Timeout && !s.session(slot).open());
}

void test_receiver_feedback(){
 SessionSet s(9,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 const uint32_t our=s.session(slot).ssrc();
 feed(s,Port::Data,kMacData,rtp(500,1000,kPeerSsrc,section({0xf8})),1000100,host);
 feed(s,Port::Data,kMacData,rtp(501,1010,kPeerSsrc,section({0xf8})),1001100,host);
 host.clear();s.tick(1001100+kFeedbackUs,host);
 assert(host.sent.size()==1);
 const auto& r=host.sent[0].bytes;
 assert(host.sent[0].port==Port::Control && host.sent[0].to==kMacControl && r.size()==12 && r[2]=='R' && r[3]=='S');
 assert(be32(r.data()+4)==our && be16(r.data()+8)==501 && be16(r.data()+10)==501);
 host.clear();s.tick(1001100+2*kFeedbackUs,host); // nothing new: no RS, only the probe a second after opening
 assert(host.sent.size()==1 && host.probes().size()==1);
}

void test_jitter_buffer_timing(){
 SessionSet s(11,"YADOKARI",Options{5000});Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 // RTP time is 10 kHz. Packets every 10 ms of host time arrive with 0-3 ms of jitter.
 const uint64_t start=2000000;const uint32_t stamp0=777000;
 const unsigned jitter[]={0,3000,1000,2500,0,1800};
 for(unsigned i=0;i<6;++i)feed(s,Port::Data,kMacData,rtp(uint16_t(10+i),stamp0+100*i,kPeerSsrc,section({0xf8})),start+10000*i+jitter[i],host);
 assert(host.got.size()==6);
 for(unsigned i=0;i<6;++i)assert(host.got[i].due==start+10000*i+5000); // jitter gone, spacing kept
 assert(s.session(slot).stats().late==0);
 // Deltas inside one packet keep their spacing (second message 2 ms later).
 host.clear();
 feed(s,Port::Data,kMacData,rtp(16,stamp0+600,kPeerSsrc,section({0x90,0x3c,0x64,0x14,0x80,0x3c,0x40})),start+60000,host);
 assert(host.got.size()==2 && host.got[0].due==start+65000 && host.got[1].due==start+67000);
 // A packet later than the latency plays at once and is counted.
 host.clear();
 feed(s,Port::Data,kMacData,rtp(17,stamp0+700,kPeerSsrc,section({0xf8})),start+70000+9000,host);
 assert(host.got.size()==1 && host.got[0].due==start+79000 && s.session(slot).stats().late==1);
 // A faster path than seen before moves the reference: from then on packets
 // play sooner after arrival, never out of order.
 host.clear();
 feed(s,Port::Data,kMacData,rtp(18,stamp0+800,kPeerSsrc,section({0xf8})),start+80000-2000,host);
 assert(host.got.size()==1 && host.got[0].due==start+80000-2000+5000);
 feed(s,Port::Data,kMacData,rtp(19,stamp0+900,kPeerSsrc,section({0xf8})),start+90000-2000,host);
 assert(host.got[1].due==start+90000-2000+5000);
 // A host clock restart (timestamps jump) restarts the timing instead of holding notes.
 host.clear();
 feed(s,Port::Data,kMacData,rtp(20,5,kPeerSsrc,section({0xf8})),start+100000,host);
 assert(host.got.size()==1 && host.got[0].due==start+105000);
}

void test_loss_without_journal_and_duplicates(){
 SessionSet s(13,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64})),1000100,host);
 assert(host.releases.empty() && host.got.size()==1);
 feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64})),1000200,host); // duplicate
 feed(s,Port::Data,kMacData,rtp(99,990,kPeerSsrc,section({0x90,0x3d,0x64})),1000300,host);  // late
 assert(host.got.size()==1 && s.session(slot).stats().reordered==2);
 // Two packets lost and no journal: nothing can be repaired, the slot is released.
 feed(s,Port::Data,kMacData,rtp(103,1030,kPeerSsrc,section({0x90,0x3e,0x64})),1000400,host);
 assert(host.releases.size()==1 && host.releases[0]==slot && host.release_reasons[0]==ReleaseReason::Loss && s.session(slot).stats().lost==2 && s.session(slot).stats().uncovered==1);
 assert(host.got.size()==2);
 // Wrong SSRC or a stranger's address: not this session's data.
 feed(s,Port::Data,kMacData,rtp(104,1040,0x1234,section({0xf8})),1000500,host);
 feed(s,Port::Data,kPiData,rtp(104,1040,kPeerSsrc,section({0xf8})),1000500,host);
 assert(host.got.size()==2 && s.rejected()==2);
 // Malformed: counted, treated as lost (no sequence advance).
 feed(s,Port::Data,kMacData,rtp(104,1040,kPeerSsrc,Bytes{0x05,0x90}),1000600,host);
 assert(s.session(slot).stats().malformed==1);
}

// A host that goes quiet while its notes sound (cable pulled, sleep, crash:
// no BY) is asked every second and its notes are released after 3 s; the
// session stays open and a host that answers keeps its notes.
void test_held_notes_on_silence(){
 const uint64_t t0=1000000;
 auto answer=[](SessionSet& s,Host& host,uint64_t now){
  const auto p=host.probes();assert(!p.empty());
  feed(s,Port::Data,kMacData,ck(kPeerSsrc,1,be64(p.back().bytes.data()+12),777),now,host);
 };
 {SessionSet s(21,"YADOKARI");Host host;
  const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  // One probe a second after opening; the host answers it.
  s.tick(t0+kHeldProbeUs-1,host);assert(host.probes().empty());
  s.tick(t0+kHeldProbeUs,host);assert(host.probes().size()==1);
  answer(s,host,t0+kHeldProbeUs+2000);host.clear();
  // A chord (delta 0 between the notes), then silence: probes at 1 s and 2 s, release after 3 s.
  const uint64_t t1=t0+2000000;
  feed(s,Port::Data,kMacData,rtp(100,uint32_t(t1/100),kPeerSsrc,section({0x90,0x3c,0x64,0x00,0x40,0x64})),t1,host);
  assert(host.got.size()==2 && host.got[1].bytes==(Bytes{0x90,0x40,0x64}));host.clear();
  for(uint64_t t=t1;t<=t1+kHeldReleaseUs;t+=50000)s.tick(t,host);
  assert(host.probes().size()==2 && host.releases.empty());
  s.tick(t1+kHeldReleaseUs+50000,host);
  assert(host.releases.size()==1 && host.releases[0]==slot && host.release_reasons[0]==ReleaseReason::Silent);
  assert(s.session(slot).open() && s.session(slot).stats().silent==1 && host.endings.empty());
  // Nothing left to release: no more probes or releases until the 15 s probe.
  host.clear();for(uint64_t t=t1+kHeldReleaseUs+100000;t<t1+10000000;t+=100000)s.tick(t,host);
  assert(host.releases.empty() && host.sent.empty());
  // The host comes back: its packets play again on the same session.
  feed(s,Port::Data,kMacData,rtp(101,uint32_t((t1+10000000)/100),kPeerSsrc,section({0x90,0x43,0x64})),t1+10000000,host);
  assert(host.got.size()==1 && host.releases.empty());
 }
 {// A host that answers keeps its notes however long they are held.
  SessionSet s(22,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);answer(s,host,t0+kHeldProbeUs+1000);
  feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64})),t0+1500000,host);host.clear();
  for(uint64_t t=t0+1500000;t<t0+20000000;t+=100000){
   const size_t before=host.probes().size();s.tick(t,host);
   if(host.probes().size()>before)answer(s,host,t+3000);
  }
  assert(host.releases.empty() && host.probes().size()>=15);
 }
 {// Sustain pedal down after the note off: it may still sound, so it counts.
  SessionSet s(23,"YADOKARI");Host host;
  const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);answer(s,host,t0+kHeldProbeUs+1000);
  feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0xb0,0x40,0x7f,0x00,0x90,0x3c,0x64,0x00,0x80,0x3c,0x00})),t0+1500000,host);host.clear();
  s.tick(t0+1500000+kHeldReleaseUs+1,host);
  assert(host.releases.size()==1 && host.release_reasons[0]==ReleaseReason::Silent && s.session(slot).open());
 }
 {// Nothing held: silence is normal and keeps the 15 s probe and 60 s timeout.
  SessionSet s(24,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);answer(s,host,t0+kHeldProbeUs+1000);
  feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64,0x00,0x80,0x3c,0x00})),t0+1500000,host);host.clear();
  for(uint64_t t=t0+1500000;t<t0+1500000+kSilentProbeUs;t+=100000)s.tick(t,host);
  assert(host.releases.empty() && host.probes().empty());
 }
 {// A host that never answered a probe keeps the old behaviour: notes until the timeout.
  SessionSet s(25,"YADOKARI");Host host;
  const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);host.clear();
  feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64})),t0+1500000,host);
  for(uint64_t t=t0+1500000;t<=t0+1500000+kTimeoutUs;t+=250000)s.tick(t,host);
  assert(host.releases.empty() && s.session(slot).open());
  s.tick(t0+1500000+kTimeoutUs+250000,host);
  assert(host.endings.size()==1 && host.endings[0].second==EndReason::Timeout);
 }
 {// Link down on this side: every session that holds something lets go at once and stays open.
  SessionSet s(26,"YADOKARI");Host host;
  const unsigned mac=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  const unsigned win=open_session(s,host,kWinControl,kWinData,0x55667788,"Win",t0);
  feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section({0x90,0x3c,0x64})),t0+1000,host);
  feed(s,Port::Data,kWinData,rtp(7,1000,0x55667788,section({0x90,0x3c,0x64,0x00,0x80,0x3c,0x00})),t0+1000,host);host.clear();
  s.drop_held(host);
  assert(host.releases.size()==1 && host.releases[0]==mac && host.release_reasons[0]==ReleaseReason::Link);
  assert(s.session(mac).open() && s.session(win).open());
  host.clear();s.drop_held(host);assert(host.releases.empty());
 }
}

// Opens a session, delivers seq 100 (state before the loss) and returns its slot.
unsigned loss_setup(SessionSet& s,Host& host,const Bytes& before){
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 if(!before.empty())feed(s,Port::Data,kMacData,rtp(100,1000,kPeerSsrc,section(before)),1000100,host);
 host.clear();return slot;
}
// Packet 102 ends a loss of 101: an empty command section plus the journal.
std::vector<Bytes> recover(SessionSet& s,Host& host,const Bytes& j){
 feed(s,Port::Data,kMacData,rtp(102,1020,kPeerSsrc,cat(Bytes{0x40},j)),1000300,host);
 return host.messages();
}

void test_journal_chapter_n_and_e(){
 SessionSet s(15,"YADOKARI");Host host;
 const unsigned slot=loss_setup(s,host,{0x90,0x3c,0x64,0x00,0x91,0x40,0x50});
 // Chapter N on channel 1: note log 0x45 vel 90 (Y=1), note log 0x47 vel 70
 // (Y=0, too old to play), OFFBITS for note 0x40 (LOW=HIGH=8: notes 64-71).
 // Chapter E: release velocity 20 for note 0x40.
 const Bytes n{0x82,0x88, 0x45,0x80|90, 0x47,70, 0x80};
 const Bytes e{0x00, 0x40,0x80|20};
 auto got=recover(s,host,journal(101,{},{channel_journal(1,0x08|0x04,cat(n,e))}));
 assert(got.size()==2);
 assert((got[0]==Bytes{0x91,0x45,90}));
 assert((got[1]==Bytes{0x81,0x40,20}));
 assert(host.releases.empty() && s.session(slot).stats().repairs==2 && s.session(slot).stats().lost==1);
 // Channel 0 untouched: its note 0x3c is still held.
 assert(s.session(slot).receiver().channel(0).note(0x3c) && !s.session(slot).receiver().channel(1).note(0x40));
 // Repairs come first, at the packet's due time, before its own commands.
 host.clear();
 feed(s,Port::Data,kMacData,rtp(104,1040,kPeerSsrc,cat(Bytes{0x43,0x80,0x3c,0x00},journal(103,{},{channel_journal(0,0x08,Bytes{0x81,0xf1,0x3e,0x80|33})}))),1000500,host);
 assert(host.got.size()==2 && (host.got[0].bytes==Bytes{0x90,0x3e,33}) && (host.got[1].bytes==Bytes{0x80,0x3c,0x00}) && host.got[0].due<=host.got[1].due);
 // 128 note logs: LEN=127 with LOW=15, HIGH=0.
 SessionSet s2(16,"YADOKARI");Host h2;loss_setup(s2,h2,{0xf8});
 Bytes all{0x80|127,0xf0};for(unsigned k=0;k<128;++k){all.push_back(uint8_t(k));all.push_back(0x80|1);}
 auto many=recover(s2,h2,journal(101,{},{channel_journal(2,0x08,all)}));
 assert(many.size()==128 && (many[127]==Bytes{0x92,127,1}));
}

void test_journal_chapter_c(){
 SessionSet s(17,"YADOKARI");Host host;
 // Before the loss: volume 100, sustain down (one toggle).
 loss_setup(s,host,{0xb0,0x07,0x64,0x00,0xb0,0x40,0x7f});
 // Logs (oldest first): volume 90 (value tool); sustain toggle count 3
 // (on, off, on: the "off" in between was lost, the state is on as ours);
 // All Notes Off count 1 (count tool); modulation 0 (value tool, unknown here).
 const Bytes c{0x03, 0x07,90, 0x40,0x80|3, 0x7b,0xc0|1, 0x01,0};
 auto got=recover(s,host,journal(101,{},{channel_journal(0,0x40,c)}));
 const std::vector<Bytes> want{{0xb0,0x07,90},{0xb0,0x40,0},{0xb0,0x40,127},{0xb0,0x7b,0},{0xb0,0x01,0}};
 assert(got==want);
 // Same journal again (a second loss): nothing left to repair.
 host.clear();
 feed(s,Port::Data,kMacData,rtp(104,1040,kPeerSsrc,cat(Bytes{0x40},journal(101,{},{channel_journal(0,0x40,c)}))),1000500,host);
 assert(host.got.empty());
}

void test_journal_chapters_p_w_t_a(){
 SessionSet s(19,"YADOKARI");Host host;
 loss_setup(s,host,{0xc3,0x05,0x00,0xe3,0x00,0x40});
 // P: program 9 with bank 2/1; W: pitch 0x1234; T: pressure 70; A: note 60 pressure 30 and a stale one (X=1).
 const Bytes p{0x09,0x80|2,0x01},w{0x34,0x24},t{70},a{0x01,60,30,61,0x80|99};
 auto got=recover(s,host,journal(101,{},{channel_journal(3,0x80|0x10|0x02|0x01,cat(cat(cat(p,w),t),a))}));
 const std::vector<Bytes> want{{0xb3,0,2},{0xb3,32,1},{0xc3,9},{0xe3,0x34,0x24},{0xd3,70},{0xa3,60,30}};
 assert(got==want);
}

void test_journal_chapter_m(){
 SessionSet s(21,"YADOKARI");Host host;
 const unsigned slot=loss_setup(s,host,{0xf8});
 // RPN 0/0 (pitch bend range): ENTRY-MSB 12, ENTRY-LSB 0, transaction closed
 // with the null parameter afterwards (P=0, E=0).
 const Bytes log{0x00,0x00,0x80|0x40|0x02,12,0};
 Bytes m{0x00,0x00};m=cat(m,log);put16(m.data(),uint16_t(m.size()));
 auto got=recover(s,host,journal(101,{},{channel_journal(0,0x20,m)}));
 const std::vector<Bytes> want{{0xb0,101,0},{0xb0,100,0},{0xb0,6,12},{0xb0,38,0},{0xb0,101,127},{0xb0,100,127}};
 assert(got==want);
 assert(s.session(slot).receiver().channel(0).sel.msb()==127 && !s.session(slot).receiver().channel(0).selected());
 // NRPN 1/2 with an increment count of 3, transaction still open (E=1).
 SessionSet s2(22,"YADOKARI");Host h2;loss_setup(s2,h2,{0xf8});
 const Bytes nlog{0x02,0x80|0x01,0x20|0x02,0x00,0x03};
 Bytes m2{0x20,0x00};m2=cat(m2,nlog);put16(m2.data(),uint16_t(0x2000|m2.size()));
 auto got2=recover(s2,h2,journal(101,{},{channel_journal(5,0x20,m2)}));
 const std::vector<Bytes> want2{{0xb5,99,1},{0xb5,98,2},{0xb5,96,0},{0xb5,96,0},{0xb5,96,0}};
 assert(got2==want2);
 // Pending MSB (P=1): the RPN MSB 0 was sent, the LSB not yet.
 SessionSet s3(23,"YADOKARI");Host h3;loss_setup(s3,h3,{0xf8});
 auto got3=recover(s3,h3,journal(101,{},{channel_journal(0,0x20,Bytes{0x40,0x03,0x00})}));
 assert((got3==std::vector<Bytes>{{0xb0,101,0}}));
 // X=1 (the value precedes a Reset All Controllers): RP-015 keeps parameter
 // values across that reset, so the value is still restored.
 SessionSet s4(28,"YADOKARI");Host h4;loss_setup(s4,h4,{0xf8});
 Bytes m4{0x00,0x00,0x00,0x00,0x80|0x02,0x80|12};put16(m4.data(),uint16_t(m4.size()));
 auto got4=recover(s4,h4,journal(101,{},{channel_journal(0,0x20,m4)}));
 assert((got4==std::vector<Bytes>{{0xb0,101,0},{0xb0,100,0},{0xb0,6,12},{0xb0,101,127},{0xb0,100,127}}));
 // ENTRY-MSB without ENTRY-LSB: a Data Entry MSB was sent after the last
 // LSB, which sets the LSB to zero. The MSB is resent even though it matches.
 SessionSet s5(29,"YADOKARI");Host h5;
 const unsigned slot5=loss_setup(s5,h5,{0xb0,101,0,0x00,0xb0,100,0,0x00,0xb0,6,12,0x00,0xb0,38,5});
 Bytes m5{0x20,0x00,0x00,0x00,0x80|0x02,12};put16(m5.data(),uint16_t(0x2000|m5.size()));
 auto got5=recover(s5,h5,journal(101,{},{channel_journal(0,0x20,m5)}));
 assert((got5==std::vector<Bytes>{{0xb0,6,12}}));
 const auto* p5=const_cast<Receiver::Channel&>(s5.session(slot5).receiver().channel(0)).find(kRpn,0,0);
 assert(p5 && p5->entry_msb==12 && p5->entry_lsb==0);
 // Chapter C logs Data Entry sent outside a transaction. With RPN 0/0
 // selected here, it is sent with the selection closed, then reopened, so
 // the parameter keeps its value.
 SessionSet s6(30,"YADOKARI");Host h6;
 const unsigned slot6=loss_setup(s6,h6,{0xb0,101,0,0x00,0xb0,100,0,0x00,0xb0,38,9});
 auto got6=recover(s6,h6,journal(101,{},{channel_journal(0,0x40,Bytes{0x00,38,0x61})}));
 assert((got6==std::vector<Bytes>{{0xb0,101,127},{0xb0,100,127},{0xb0,38,0x61},{0xb0,101,0},{0xb0,100,0}}));
 const auto& c6=s6.session(slot6).receiver().channel(0);
 assert(c6.selected() && c6.cc[38]==0x61 && const_cast<Receiver::Channel&>(c6).find(kRpn,0,0)->entry_lsb==9);
}

void test_journal_chapter_q(){
 // Lost Start: N=1, C=0, D=0.
 {SessionSet s(25,"YADOKARI");Host host;loss_setup(s,host,{0xf8});
  auto got=recover(s,host,journal(101,system_journal(0x10,Bytes{0x40}),{}));
  assert((got==std::vector<Bytes>{{0xfa}}));}
 // Lost clocks and Stop while running (clock 5 played): Stop, then locate
 // to where Continue would resume (clock 6 = sixteenth 1).
 {SessionSet s(26,"YADOKARI");Host host;loss_setup(s,host,{0xfa,0x00,0xf8});
  auto got=recover(s,host,journal(101,system_journal(0x10,Bytes{0x20|0x10,0x00,0x05}),{}));
  assert((got==std::vector<Bytes>{{0xfc},{0xf2,1,0}}));}
 // Only the Stop was lost, the position is the one we have.
 {SessionSet s(24,"YADOKARI");Host host;loss_setup(s,host,{0xfa,0x00,0xf8});
  auto got=recover(s,host,journal(101,system_journal(0x10,Bytes{0x20}),{}));
  assert((got==std::vector<Bytes>{{0xfc}}));}
 // Lost Continue at clock 47 (D=1: 47 played, the next clock plays 48 = sixteenth 8).
 {SessionSet s(27,"YADOKARI");Host host;loss_setup(s,host,{0xfc});
  auto got=recover(s,host,journal(101,system_journal(0x10,Bytes{0x40|0x20|0x10,0x00,47}),{}));
  assert((got==std::vector<Bytes>{{0xf2,8,0},{0xfb}}));}
 // Chapters D and V in front of Q are skipped by their lengths.
 {SessionSet s(28,"YADOKARI");Host host;loss_setup(s,host,{0xf8});
  const Bytes d{0x40|0x08, 0x05, 0x00,0x03,0x01}; // B: reset count 5; J log of 3 octets
  auto got=recover(s,host,journal(101,system_journal(0x40|0x20|0x10,cat(cat(d,Bytes{0x01}),Bytes{0x40})),{}));
  assert((got==std::vector<Bytes>{{0xfa}}));}
}

void test_uncovered_and_first_packet(){
 // Checkpoint after the last packet we have: the journal does not cover
 // the loss, so the slot is released before the repairs.
 SessionSet s(29,"YADOKARI");Host host;
 const unsigned slot=loss_setup(s,host,{0x90,0x3c,0x64});
 feed(s,Port::Data,kMacData,rtp(105,1050,kPeerSsrc,cat(Bytes{0x40},journal(103,{},{channel_journal(0,0x08,Bytes{0x01,0xf1,0x3e,0x80|50})}))),1000500,host);
 assert(host.releases.size()==1 && host.got.size()==1 && (host.got[0].bytes==Bytes{0x90,0x3e,50}));
 assert(!s.session(slot).receiver().channel(0).note(0x3c) && s.session(slot).stats().uncovered==1);
 // The first packet of a session is handled like the end of a loss: a
 // host that is already playing brings YADOKARI in step (Continue, held note).
 SessionSet f(30,"YADOKARI");Host fh;open_session(f,fh,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 feed(f,Port::Data,kMacData,rtp(7,70,kPeerSsrc,cat(Bytes{0x41,0xf8},journal(6,system_journal(0x10,Bytes{0x40|0x20|0x10,0x00,0x00}),{channel_journal(9,0x08,Bytes{0x01,0xf1,0x24,0x80|99})}))),1000100,fh);
 assert(fh.releases.empty());
 assert((fh.messages()==std::vector<Bytes>{{0xfb},{0x99,0x24,99},{0xf8}}));
 // A malformed journal: repairs stop, the packet's own commands still play.
 SessionSet m(31,"YADOKARI");Host mh;const unsigned ms=loss_setup(m,mh,{0xf8});
 feed(m,Port::Data,kMacData,rtp(102,1020,kPeerSsrc,cat(Bytes{0x41,0xf8},Bytes{0x20,0x00,0x65,0x00,0xff,0x08})),1000300,mh);
 assert(mh.got.size()==1 && m.session(ms).stats().malformed==1);
}

// ------------------------------------------------------------------ sender

void test_transmit_packet_and_journal_bytes(){
 SessionSet s(33,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 const uint32_t our=s.session(slot).ssrc();
 TxMessage on;on.bytes[0]=0x90;on.bytes[1]=0x3c;on.bytes[2]=100;on.length=3;on.us=1000000;
 s.transmit(&on,1,1000100,host);
 auto packets=host.data_packets();assert(packets.size()==1);
 const Bytes& p1=packets[0].bytes;
 assert(packets[0].to==kMacData && p1[1]==0x61 && be32(p1.data()+4)==1000000/100 && be32(p1.data()+8)==our);
 const uint16_t seq1=be16(p1.data()+2);
 // Command section: J=1, one message; empty journal (nothing sent before).
 assert(p1[12]==(0x40|3) && p1[13]==0x90 && p1[14]==0x3c && p1[15]==100);
 assert((Bytes(p1.begin()+16,p1.end())==Bytes{0x80,uint8_t(seq1>>8),uint8_t(seq1)}));
 // Next packet: the journal codes the Note On of packet I-1 (S bits 0):
 // header A=1; channel 0 journal LENGTH 7, TOC N; chapter N B=1 LEN=1,
 // LOW/HIGH=15/1 (no OFFBITS); note log 60, Y=1, velocity 100.
 host.clear();
 TxMessage clk;clk.bytes[0]=0xf8;clk.length=1;clk.us=1002000;
 s.transmit(&clk,1,1002100,host);
 const Bytes p2=host.data_packets().at(0).bytes;
 assert(be16(p2.data()+2)==uint16_t(seq1+1) && p2[12]==(0x40|1) && p2[13]==0xf8);
 const Bytes want{0x20,uint8_t(seq1>>8),uint8_t(seq1), 0x00,0x07,0x08, 0x81,0xf1, 0x3c,0x80|100};
 assert(Bytes(p2.begin()+14,p2.end())==want);
 // The host acknowledges packet 1 (Apple layout: number in the upper half):
 // the checkpoint moves to packet 2, which only had a clock while stopped.
 feed(s,Port::Control,kMacControl,rs(kPeerSsrc,seq1,0),1003000,host);
 assert(s.session(slot).checkpoint()==uint16_t(seq1+1));
 host.clear();s.transmit(&clk,1,1004100,host);
 const Bytes p3=host.data_packets().at(0).bytes;
 assert((Bytes(p3.begin()+14,p3.end())==Bytes{0x80,uint8_t((seq1+1)>>8),uint8_t(seq1+1)}));
 // rtpmidid-style feedback (number in the lower half) works as well.
 feed(s,Port::Control,kMacControl,rs(kPeerSsrc,0,uint16_t(seq1+2)),1005000,host);
 assert(s.session(slot).checkpoint()==uint16_t(seq1+3));
 // Feedback for a packet never sent is ignored.
 feed(s,Port::Control,kMacControl,rs(kPeerSsrc,uint16_t(seq1+100),uint16_t(seq1+100)),1005000,host);
 assert(s.session(slot).checkpoint()==uint16_t(seq1+3));
}

void test_transmit_deltas_guard_and_fanout(){
 SessionSet s(35,"YADOKARI");Host host;
 const unsigned a=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 const unsigned b=open_session(s,host,kWinControl,kWinData,0x55667788,"PC",1000000);
 TxMessage m[3];
 for(unsigned i=0;i<3;++i){m[i].bytes[0]=0xf8;m[i].length=1;m[i].us=2000000+i*20833;}
 s.transmit(m,3,2050000,host);
 auto packets=host.data_packets();assert(packets.size()==2);
 for(const auto& p:packets){
  CommandSection cs;assert(read_command_header(p.bytes.data()+12,unsigned(p.bytes.size()-12),cs));
  std::vector<uint32_t> deltas;
  assert(parse_midi_list(p.bytes.data()+12+cs.header,cs.length,p.bytes[12]&0x20,[&](const uint8_t*,unsigned,uint32_t t){deltas.push_back(t);}));
  assert((deltas==std::vector<uint32_t>{0,208,416}));
 }
 assert(packets[0].to==kMacData && packets[1].to==kWinData && be32(packets[0].bytes.data()+8)!=be32(packets[1].bytes.data()+8));
 assert(s.session(a).stats().sent==1 && s.session(b).stats().sent==1);
 // No feedback within 100 ms: one journal-only guard packet per session.
 host.clear();s.tick(2050000+kGuardUs,host);
 auto guards=host.data_packets();assert(guards.size()==2 && guards[0].bytes[1]==0x61 && guards[0].bytes[12]==0x40);
 host.clear();s.tick(2050000+3*kGuardUs,host);assert(host.data_packets().empty());
 // A fully acknowledged stream needs no guard.
 const uint16_t last=be16(guards[0].bytes.data()+2);
 host.clear();s.transmit(m,1,2500000,host);
 feed(s,Port::Control,kMacControl,rs(kPeerSsrc,uint16_t(last+1),0),2500100,host);
 host.clear();s.tick(2500000+kGuardUs,host);
 auto after=host.data_packets();assert(after.size()==1 && after[0].to==kWinData);
}

void test_journal_too_large(){
 SessionSet s(37,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",1000000);
 std::vector<TxMessage> m;
 for(unsigned ch=0;ch<16;++ch)for(unsigned n=0;n<48;++n){TxMessage x;x.bytes[0]=uint8_t(0xb0|ch);x.bytes[1]=uint8_t(n);x.bytes[2]=1;x.length=3;x.us=1000000;m.push_back(x);}
 s.transmit(m.data(),unsigned(m.size()),1000100,host);
 host.clear();TxMessage clk;clk.bytes[0]=0xf8;clk.length=1;clk.us=1001000;
 s.transmit(&clk,1,1001100,host);
 const Bytes p=host.data_packets().at(0).bytes;
 assert(p.size()<=kMaxPacket && p[12]==0x01 && s.session(slot).stats().journal_full>=1);
}

// --------------------------------------------------------- end to end

uint32_t rng_state=0x2468ace1;
uint32_t rnd(uint32_t n){rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state%n;}

// Random traffic on four channels with transport, controllers, programs,
// bends, aftertouch and RPN/NRPN transactions.
struct Transaction { unsigned step=0;bool nrpn=false;uint8_t msb=0,lsb=0; };
Transaction transactions[4];
void random_message(TxMessage& m){
 const uint8_t ch=uint8_t(rnd(4));
 auto set=[&](uint8_t a,uint8_t b,uint8_t c,uint8_t l){m.bytes[0]=a;m.bytes[1]=b;m.bytes[2]=c;m.length=l;};
 const uint32_t r=rnd(100);
 if(r<22)set(uint8_t(0x90|ch),uint8_t(36+rnd(12)),uint8_t(1+rnd(127)),3);
 else if(r<34)set(uint8_t(0x80|ch),uint8_t(36+rnd(12)),uint8_t(rnd(3)?64:rnd(128)),3);
 else if(r<40)set(uint8_t(0x90|ch),uint8_t(36+rnd(12)),0,3);
 else if(r<50){const uint8_t ccs[]={1,7,10,11,74,0,32};set(uint8_t(0xb0|ch),ccs[rnd(7)],uint8_t(rnd(128)),3);}
 else if(r<55)set(uint8_t(0xb0|ch),uint8_t(64+rnd(2)),uint8_t(rnd(2)?127:0),3);
 else if(r<57){const uint8_t modes[]={120,123,121};set(uint8_t(0xb0|ch),modes[rnd(3)],0,3);}
 else if(r<61)set(uint8_t(0xc0|ch),uint8_t(rnd(128)),0,2);
 else if(r<66)set(uint8_t(0xe0|ch),uint8_t(rnd(128)),uint8_t(rnd(128)),3);
 else if(r<69)set(uint8_t(0xd0|ch),uint8_t(rnd(128)),0,2);
 else if(r<72)set(uint8_t(0xa0|ch),uint8_t(36+rnd(12)),uint8_t(rnd(128)),3);
 else if(r<80){
  // RPN/NRPN transactions in the usual order: number MSB, LSB, then data
  // entry or increment/decrement; sometimes the null parameter.
  auto& t=transactions[ch];
  if(t.step==0 || rnd(5)==0){
   t.nrpn=rnd(2);const bool null=rnd(6)==0;t.msb=null?127:uint8_t(t.nrpn?1:0);t.lsb=null?127:uint8_t(rnd(3));
   set(uint8_t(0xb0|ch),t.nrpn?99:101,t.msb,3);t.step=1;
  }
  else if(t.step==1){set(uint8_t(0xb0|ch),t.nrpn?98:100,t.lsb,3);t.step=2;}
  else{const uint8_t data[]={6,38,96,97};const uint8_t n=data[rnd(4)];set(uint8_t(0xb0|ch),n,n>=96?0:uint8_t(rnd(128)),3);}
 }
 else if(r<94)set(0xf8,0,0,1);
 else{
  const uint32_t k=rnd(4);
  if(k==0)set(0xfa,0,0,1);
  else if(k==1)set(0xfc,0,0,1);
  else if(k==2)set(0xfb,0,0,1);
  else set(0xf2,uint8_t(rnd(128)),uint8_t(rnd(4)),3);
 }
}

// What the journal must restore, as RFC 6295 defines it.
void compare(const Receiver& want,const Receiver& have,const SenderHistory& history,unsigned packet){
 auto fail=[&](const char* what,unsigned ch,unsigned n){std::fprintf(stderr,"packet %u: %s differs (channel %u, %u)\n",packet,what,ch,n);assert(false);};
 for(unsigned ch=0;ch<4;++ch){
  const auto& w=want.channel(ch);const auto& h=have.channel(ch);const auto& s=history.channel(ch);
  for(unsigned n=0;n<128;++n)if(w.note(n)!=h.note(n))fail("note",ch,n);
  for(unsigned n=0;n<128;++n){
   if(parameter_number_controller(n) || parameter_data_controller(n))continue;
   if(toggle_controller(n)){if((w.cc[n]!=kUnknown && w.cc[n]>=64)!=(h.cc[n]!=kUnknown && h.cc[n]>=64))fail("switch",ch,n);continue;}
   if(count_controller(n)){if(w.counts[n]!=h.counts[n])fail("count",ch,n);continue;}
   if(w.cc[n]!=h.cc[n])fail("controller",ch,n);
  }
  // Chapter P codes the bank LSB only if it was sent after the MSB.
  if(w.program!=h.program || w.bank_msb!=h.bank_msb || (s.bank && s.bank_lsb && w.bank_lsb!=h.bank_lsb))fail("program",ch,w.program);
  if(w.pitch!=h.pitch)fail("pitch",ch,w.pitch);
  if(s.pressure_order>=s.nactive_order && w.pressure!=h.pressure)fail("pressure",ch,w.pressure);
  for(unsigned n=0;n<128;++n)if(s.poly[n].order && s.poly[n].order>=s.nactive_order && w.poly[n]!=h.poly[n])fail("poly",ch,n);
  for(const auto& p:w.params){
   if(p.type==kNoParam)continue;
   auto* q=const_cast<Receiver::Channel&>(h).find(p.type,p.msb,p.lsb);
   if(!q){if(p.entry_msb!=kUnknown || p.entry_lsb!=kUnknown || p.buttons)fail("parameter",ch,p.lsb);continue;}
   if(q->entry_msb!=p.entry_msb || q->entry_lsb!=p.entry_lsb || q->buttons!=p.buttons)fail("parameter value",ch,p.lsb);
  }
  // The pending MSB is restored; the LSB register behind it is not coded.
  if(w.sel.pending!=h.sel.pending)fail("pending parameter",ch,0);
  if(w.sel.pending && (w.sel.type!=h.sel.type || w.sel.msb()!=h.sel.msb()))fail("pending number",ch,0);
  if(!w.sel.pending && (w.selected()!=h.selected() || (w.selected() && (w.sel.type!=h.sel.type || w.sel.msb()!=h.sel.msb() || w.sel.lsb()!=h.sel.lsb()))))fail("selection",ch,0);
 }
 if(want.sequencer().running!=have.sequencer().running)fail("transport",0,0);
}

void test_end_to_end_lossy_link(){
 for(uint32_t seed:{1u,2u,3u,4u,5u}){
  rng_state=0x2468ace1u*seed;for(auto& t:transactions)t=Transaction{};
  // A is YADOKARI, sending; B is a second endpoint receiving from it.
  SessionSet a(100+seed,"YADOKARI"),b(200+seed,"HOST");Host ha,hb;
  uint64_t now=1000000;
  open_session(a,ha,kMacControl,kMacData,kPeerSsrc,"Mac",now);
  const uint32_t a_ssrc=a.session(0).ssrc();
  open_session(b,hb,kPiControl,kPiData,a_ssrc,"YADOKARI",now);
  Receiver reference;unsigned dropped_run=0,lost=0;
  for(unsigned packet=0;packet<4000;++packet){
   now+=500+rnd(3000);
   TxMessage m[4];const unsigned count=1+rnd(4);
   for(unsigned i=0;i<count;++i){random_message(m[i]);m[i].us=now-200;reference.apply(m[i].bytes,m[i].length);}
   a.transmit(m,count,now,ha);
   a.tick(now,ha);
   bool last_delivered=false;
   for(const auto& p:ha.data_packets()){
    // Lose about one packet in six, in bursts of up to four.
    if(dropped_run<4 && rnd(6)==0){++dropped_run;++lost;last_delivered=false;continue;}
    dropped_run=0;last_delivered=true;
    feed(b,Port::Data,kPiData,p.bytes,now+100,hb);
   }
   ha.clear();
   b.tick(now+200,hb);
   for(auto& sent:hb.sent)if(sent.port==Port::Control && sent.bytes.size()==12 && sent.bytes[2]=='R'){
    Bytes fb=sent.bytes;put32(fb.data()+4,kPeerSsrc);feed(a,Port::Control,kMacControl,fb,now+300,ha);
   }
   hb.clear();
   if(last_delivered)compare(reference,b.session(0).receiver(),a.history(),packet);
  }
  const auto& st=b.session(0).stats();
  assert(lost>300 && st.lost+10>=lost && st.repairs>100 && st.uncovered==0 && st.malformed==0);
  assert(a.session(0).stats().acks>10 && a.session(0).stats().journal_full==0);
 }
}

// ------------------------------------------------------------ timing glue

void test_sample_clock(){
 // Audio renders 128-frame blocks at 48 kHz, each one a block ahead of real
 // time; the main loop samples the counter at odd intervals.
 SampleClock clock(48000);
 uint64_t rendered=0;
 auto counter=[&](uint64_t us){const uint64_t block=us*48000/1000000/128;return (block+1)*128;};
 for(uint64_t us=1000000;us<1500000;us+=173){rendered=counter(us);clock.observe(us,rendered);}
 // A future time maps to a sample not rendered yet then, in linear steps.
 for(uint64_t us=1500000;us<1600000;us+=1000){
  const uint64_t s=clock.sample_at(us);
  assert(s+1>=counter(us) && s<=counter(us)+128);
  assert(clock.sample_at(us+1000)-s==48);
 }
 // Rendered samples map back to a time that has passed.
 assert(clock.us_at(rendered)<=1500000+21);
 assert(clock.us_at(clock.sample_at(1600000))<=1600000 && clock.us_at(clock.sample_at(1600000))+21>=1600000);
 // An audio stall decays the envelope slowly and recovers at once.
 const uint64_t before=clock.sample_at(2000000);
 clock.observe(2000000,rendered);
 assert(clock.sample_at(2000000)<before);
 clock.observe(2000100,counter(2000100));
 assert(clock.sample_at(2000100)>=counter(2000100));
}

void test_due_queue(){
 DueQueue q;const uint8_t a[3]{0x90,60,100},b[3]{0x80,60,0};
 assert(q.push(1000,a,3) && q.push(1000,b,3) && q.push(2000,a,3));
 std::vector<Bytes> out;
 assert(q.release(999,[&](const uint8_t* m,unsigned n){out.push_back(Bytes(m,m+n));})==0);
 assert(q.release(1500,[&](const uint8_t* m,unsigned n){out.push_back(Bytes(m,m+n));})==2 && out.size()==2 && out[1][0]==0x80);
 for(unsigned i=0;i<DueQueue::kCapacity-1;++i)assert(q.push(3000,a,3));
 assert(q.full() && !q.push(3000,a,3));
 q.flush([&](const uint8_t*,unsigned){});assert(q.empty());
}
} // namespace


// ------------------------------------------------------------- reconnect

// The host's OK to the newest invitation on that port.
void accept_invitation(SessionSet& s,Host& host,Port port,const Endpoint& from,uint32_t ssrc,uint64_t now){
 const auto in=host.invitations(port);assert(!in.empty());
 feed(s,port,from,session_packet('O','K',be32(in.back().bytes.data()+8),ssrc,"MacBook Session"),now,host);
}
// Ticks every 100 ms through [from, to).
void run(SessionSet& s,Host& host,uint64_t from,uint64_t to){for(uint64_t t=from;t<to;t+=100000)s.tick(t,host);}

// A host whose session timed out is invited again: IN on its control port,
// then on its data port, and the session YADOKARI opened keeps the clock in
// sync. A host that said goodbye or refused is forgotten.
void test_reconnect_after_timeout(){
 const uint64_t t0=1000000;
 SessionSet s(31,"YADOKARI");Host host;
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"MacBook Session",t0);
 assert(s.known_count()==1 && s.known(0).control==kMacControl && std::string(s.known(0).name)=="MacBook Session" && s.take_known_changed() && !s.take_known_changed());
 // No packets for 60 s: the session ends, the host stays remembered.
 const uint64_t end=t0+kTimeoutUs+100000;
 s.tick(t0+500000,host);s.tick(end,host);
 assert(host.endings.size()==1 && host.endings[0].second==EndReason::Timeout && !s.session(slot).open() && s.known_count()==1);
 host.clear();
 run(s,host,end,end+kInviteFirstUs);assert(host.invitations(Port::Control).empty());
 s.tick(end+kInviteFirstUs,host);
 auto in=host.invitations(Port::Control);
 assert(in.size()==1 && in[0].to==kMacControl && std::string(reinterpret_cast<const char*>(in[0].bytes.data()+16))=="YADOKARI");
 accept_invitation(s,host,Port::Control,kMacControl,kPeerSsrc+1,end+kInviteFirstUs+2000);
 in=host.invitations(Port::Data);assert(in.size()==1 && in[0].to==kMacData && host.opens.empty());
 accept_invitation(s,host,Port::Data,kMacData,kPeerSsrc+1,end+kInviteFirstUs+4000);
 assert(host.opens.size()==1);const unsigned again=host.opens[0];
 assert(s.session(again).open() && s.session(again).initiator() && s.session(again).peer_ssrc()==kPeerSsrc+1);
 // The inviting side starts clock sync at once and repeats it every 10 s.
 assert(host.probes().size()==1);
 host.clear();run(s,host,end+kInviteFirstUs+100000,end+kInviteFirstUs+4000+kSyncUs+100000);
 assert(host.probes().size()==1 && host.invitations(Port::Control).empty());
 // Its data plays like any other session's.
 feed(s,Port::Data,kMacData,rtp(1,1000,kPeerSsrc+1,section({0x90,0x3c,0x64})),end+kSyncUs+3000000,host);
 assert(host.got.size()==1);
 // Goodbye: forgotten, no more invitations.
 host.clear();feed(s,Port::Control,kMacControl,session_packet('B','Y',0,kPeerSsrc+1,nullptr),end+kSyncUs+3100000,host);
 assert(host.endings.size()==1 && host.endings[0].second==EndReason::Bye && s.known_count()==0 && s.take_known_changed());
 run(s,host,end+kSyncUs+3200000,end+kSyncUs+70000000);
 assert(host.invitations(Port::Control).empty());
}

void test_reconnect_refused_and_backoff(){
 const uint64_t t0=1000000;
 {// NO: the host does not want us; forgotten and reported.
  SessionSet s(32,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  const uint64_t end=t0+kTimeoutUs+100000;s.tick(t0+500000,host);s.tick(end,host);host.clear();
  s.tick(end+kInviteFirstUs,host);
  const auto in=host.invitations(Port::Control);assert(in.size()==1);
  feed(s,Port::Control,kMacControl,session_packet('N','O',be32(in[0].bytes.data()+8),kPeerSsrc,nullptr),end+kInviteFirstUs+1000,host);
  assert(host.refusals.size()==1 && host.refusals[0]==kMacControl && s.known_count()==0);
  host.clear();run(s,host,end+kInviteFirstUs+100000,end+kInviteFirstUs+120000000);
  assert(host.invitations(Port::Control).empty());
 }
 {// No answer at all: tried after 1 s, then less and less often, at most 30 s apart.
  SessionSet s(33,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  const uint64_t end=t0+kTimeoutUs+100000;s.tick(t0+500000,host);s.tick(end,host);host.clear();
  std::vector<uint64_t> times;
  for(uint64_t t=end;t<end+300000000;t+=100000){const size_t before=host.invitations(Port::Control).size();s.tick(t,host);if(host.invitations(Port::Control).size()>before)times.push_back(t-end);}
  assert(times.size()>=10 && times[0]==kInviteFirstUs);
  for(size_t i=1;i<times.size();++i){const uint64_t gap=times[i]-times[i-1];assert(gap>=kInviteRetryUs);assert(gap<=kInviteMaxUs+kInviteAnswerUs+100000);if(i>1)assert(gap>=times[i-1]-times[i-2] || gap>=kInviteMaxUs);}
  assert(times.back()-times[times.size()-2]>=kInviteMaxUs);
  // A wrong token or a stranger's OK opens nothing.
  host.clear();
  for(uint64_t t=end+300000000;!host.invitations(Port::Control).size();t+=100000)s.tick(t,host);
  const uint32_t token=be32(host.invitations(Port::Control)[0].bytes.data()+8);
  feed(s,Port::Control,kMacControl,session_packet('O','K',token+1,kPeerSsrc,nullptr),end+300000000+kInviteMaxUs,host);
  feed(s,Port::Control,kWinControl,session_packet('O','K',token,kPeerSsrc,nullptr),end+300000000+kInviteMaxUs,host);
  assert(host.invitations(Port::Data).empty() && s.rejected()>=2);
 }
}

// The host invites while YADOKARI invites it: the host's session is taken.
void test_reconnect_crossing_invitations(){
 const uint64_t t0=1000000;
 SessionSet s(34,"YADOKARI");Host host;
 open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
 const uint64_t end=t0+kTimeoutUs+100000;s.tick(t0+500000,host);s.tick(end,host);host.clear();
 s.tick(end+kInviteFirstUs,host);assert(host.invitations(Port::Control).size()==1);
 const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc+7,"Mac",end+kInviteFirstUs+500);
 assert(s.session(slot).open() && !s.session(slot).initiator() && s.open_count()==1);
 host.clear();run(s,host,end+kInviteFirstUs+100000,end+kInviteFirstUs+10000000);
 assert(host.invitations(Port::Control).empty() && s.open_count()==1);
}

// After a long main-loop pause, a host that answers probes must answer a
// new one within 3 s; one that does not is invited again.
void test_stall_check(){
 const uint64_t t0=1000000;
 auto answer=[](SessionSet& s,Host& host,uint64_t now){
  const auto p=host.probes();assert(!p.empty());
  feed(s,Port::Data,kMacData,ck(kPeerSsrc,1,be64(p.back().bytes.data()+12),777),now,host);
 };
 {SessionSet s(35,"YADOKARI");Host host;
  const unsigned slot=open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);answer(s,host,t0+kHeldProbeUs+1000);host.clear();
  // 5 minutes without a pass; packets that waited arrive "now".
  const uint64_t back=t0+kHeldProbeUs+300000000;
  feed(s,Port::Data,kMacData,rtp(1,1000,kPeerSsrc,section({0xf8})),back,host);
  s.tick(back,host);assert(host.probes().size()==1);
  answer(s,host,back+2000);host.clear();
  run(s,host,back+100000,back+kStallAnswerUs+1000000);
  assert(host.endings.empty() && s.session(slot).open());
 }
 {SessionSet s(36,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+kHeldProbeUs,host);answer(s,host,t0+kHeldProbeUs+1000);host.clear();
  const uint64_t back=t0+kHeldProbeUs+300000000;
  feed(s,Port::Data,kMacData,rtp(1,1000,kPeerSsrc,section({0xf8})),back,host);
  s.tick(back,host);
  // A stale answer (to an older probe) does not count.
  feed(s,Port::Data,kMacData,ck(kPeerSsrc,1,1,777),back+1000,host);
  run(s,host,back+100000,back+kStallAnswerUs+200000);
  assert(host.endings.size()==1 && host.endings[0].second==EndReason::Timeout);
  host.clear();run(s,host,back+kStallAnswerUs+200000,back+kStallAnswerUs+200000+kInviteFirstUs+200000);
  assert(host.invitations(Port::Control).size()==1);
 }
}

// A goodbye read right after a long pause most likely came from a host that
// gave up waiting during it (macOS does after about 50 s): the host stays
// remembered and is invited again. A goodbye well after the pause is final.
void test_bye_after_pause(){
 const uint64_t t0=1000000,back=t0+100000+300000000; // 5 minutes without a pass
 auto bye=[](SessionSet& s,Host& host,uint64_t now){feed(s,Port::Control,kMacControl,session_packet('B','Y',0,kPeerSsrc,nullptr),now,host);};
 {// Read before the pass that notices the pause: packets are read first.
  SessionSet s(39,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+100000,host);host.clear();
  bye(s,host,back);
  assert(host.endings.size()==1 && host.endings[0].second==EndReason::PauseBye && s.known_count()==1);
  assert(std::string(end_reason_name(EndReason::PauseBye))=="bye after a pause");
  host.clear();run(s,host,back,back+kInviteFirstUs);assert(host.invitations(Port::Control).empty());
  s.tick(back+kInviteFirstUs,host);
  const auto in=host.invitations(Port::Control);assert(in.size()==1 && in[0].to==kMacControl);
  accept_invitation(s,host,Port::Control,kMacControl,kPeerSsrc+1,back+kInviteFirstUs+1000);
  accept_invitation(s,host,Port::Data,kMacData,kPeerSsrc+1,back+kInviteFirstUs+2000);
  assert(host.opens.size()==1 && s.open_count()==1);
 }
 {// Read in a later pass, within kStallAnswerUs of the one that noticed it.
  SessionSet s(40,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+100000,host);
  feed(s,Port::Data,kMacData,rtp(1,1000,kPeerSsrc,section({0xf8})),back,host);
  s.tick(back,host);run(s,host,back+100000,back+kStallAnswerUs-100000);host.clear();
  bye(s,host,back+kStallAnswerUs-100000);
  assert(host.endings.size()==1 && host.endings[0].second==EndReason::PauseBye && s.known_count()==1);
 }
 {// Well after the pause: the host meant it, and is forgotten.
  SessionSet s(41,"YADOKARI");Host host;
  open_session(s,host,kMacControl,kMacData,kPeerSsrc,"Mac",t0);
  s.tick(t0+100000,host);
  feed(s,Port::Data,kMacData,rtp(1,1000,kPeerSsrc,section({0xf8})),back,host);
  s.tick(back,host);run(s,host,back+100000,back+kStallAnswerUs+200000);
  assert(host.endings.empty());
  bye(s,host,back+kStallAnswerUs+200000);
  assert(host.endings.size()==1 && host.endings[0].second==EndReason::Bye && s.known_count()==0);
  host.clear();run(s,host,back+kStallAnswerUs+300000,back+kStallAnswerUs+70000000);
  assert(host.invitations(Port::Control).empty());
 }
}

// The remembered hosts survive a restart through the stored bytes; closing
// the ports keeps them, and they are invited when the ports open again.
void test_known_hosts_storage(){
 const uint64_t t0=1000000;
 SessionSet s(37,"YADOKARI");Host host;
 open_session(s,host,kMacControl,kMacData,kPeerSsrc,"MacBook Session",t0);
 open_session(s,host,kWinControl,kWinData,0x55667788,"Win",t0+1000);
 assert(s.known_count()==2 && s.known(0).control==kWinControl);
 uint8_t bytes[kKnownBytes];const unsigned n=s.encode_known(bytes,sizeof bytes);
 assert(n==6+2*7+15+3+4);
 SessionSet restarted(38,"YADOKARI");Host h2;
 assert(restarted.decode_known(bytes,n) && restarted.known_count()==2 && std::string(restarted.known(1).name)=="MacBook Session");
 run(restarted,h2,5000000,5000000+kInviteFirstUs+100000);
 const auto in=h2.invitations(Port::Control);
 assert(in.size()==2 && ((in[0].to==kWinControl && in[1].to==kMacControl) || (in[0].to==kMacControl && in[1].to==kWinControl)));
 // Damaged or foreign bytes change nothing.
 SessionSet other(39,"YADOKARI");
 for(unsigned i=0;i<n;++i){uint8_t copy[kKnownBytes];memcpy(copy,bytes,n);copy[i]^=0x20;assert(!other.decode_known(copy,n));}
 assert(!other.decode_known(bytes,n-1) && other.known_count()==0);
 // Closing every session (setting off) keeps the hosts; reopening invites them.
 host.clear();s.close_all(host);
 assert(host.endings.size()==2 && s.known_count()==2);
 host.clear();run(s,host,t0+2000000,t0+2000000+kInviteFirstUs+100000);
 assert(host.invitations(Port::Control).size()==2);
 // The list keeps the newest hosts.
 SessionSet many(40,"YADOKARI");Host h3;
 for(unsigned i=0;i<kKnownHosts+2;++i){
  const Endpoint c{kMac+i,uint16_t(5004)},d{kMac+i,uint16_t(5005)};
  open_session(many,h3,c,d,0x1000+i,"Host",t0+i);
  many.close_all(h3);
 }
 assert(many.known_count()==kKnownHosts);
 for(unsigned i=0;i<kKnownHosts;++i)assert(many.known(i).control.ip==kMac+kKnownHosts+1-i);
}

int main(){
 test_command_section_parse();
 test_command_section_write();
 test_handshake_several_hosts();
 test_clock_sync_and_probe();
 test_receiver_feedback();
 test_jitter_buffer_timing();
 test_loss_without_journal_and_duplicates();
 test_held_notes_on_silence();
 test_journal_chapter_n_and_e();
 test_journal_chapter_c();
 test_journal_chapters_p_w_t_a();
 test_journal_chapter_m();
 test_journal_chapter_q();
 test_uncovered_and_first_packet();
 test_transmit_packet_and_journal_bytes();
 test_transmit_deltas_guard_and_fanout();
 test_journal_too_large();
 test_end_to_end_lossy_link();
 test_sample_clock();
 test_due_queue();
 test_reconnect_after_timeout();
 test_reconnect_refused_and_backoff();
 test_reconnect_crossing_invitations();
 test_stall_check();
 test_bye_after_pause();
 test_known_hosts_storage();
 std::puts("rtp midi: all tests passed");
}
