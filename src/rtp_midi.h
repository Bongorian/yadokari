#pragma once
// RTP-MIDI (RFC 6295) wire format: byte helpers, the MIDI command section
// (both directions) and the AppleMIDI session-protocol constants used by
// macOS Network MIDI, rtpMIDI (Windows) and rtpmidid (Linux).
// Pure functions, no allocation, no sockets.
#include <stdint.h>
#include <cstring>

namespace groove::rtpmidi {

constexpr uint16_t kControlPort=5004,kDataPort=5005;
constexpr uint32_t kProtocolVersion=2;
constexpr uint8_t kPayloadType=0x61;
constexpr unsigned kNameBytes=48;
constexpr unsigned kMaxPacket=1400; // stays inside one Ethernet frame

enum class Port:uint8_t { Control,Data };
struct Endpoint {
 uint32_t ip=0;uint16_t port=0;
 bool operator==(const Endpoint& o)const{return ip==o.ip && port==o.port;}
 bool operator!=(const Endpoint& o)const{return !(*this==o);}
};

inline uint16_t be16(const uint8_t* p){return uint16_t(p[0]<<8|p[1]);}
inline uint32_t be24(const uint8_t* p){return uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];}
inline uint32_t be32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline uint64_t be64(const uint8_t* p){return uint64_t(be32(p))<<32|be32(p+4);}
inline void put16(uint8_t* p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}
inline void put32(uint8_t* p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
inline void put64(uint8_t* p,uint64_t v){put32(p,uint32_t(v>>32));put32(p+4,uint32_t(v));}

// Bounded output cursor: writes past the end are dropped and remembered.
struct Writer {
 uint8_t* data;unsigned capacity,size=0;bool overflow=false;
 Writer(uint8_t* d,unsigned c):data(d),capacity(c){}
 void byte(uint8_t v){if(size<capacity)data[size++]=v;else overflow=true;}
 void word(uint16_t v){byte(uint8_t(v>>8));byte(uint8_t(v));}
 void dword(uint32_t v){word(uint16_t(v>>16));word(uint16_t(v));}
 void bytes(const uint8_t* p,unsigned n){for(unsigned i=0;i<n;++i)byte(p[i]);}
 unsigned mark()const{return size;}
 void patch16(unsigned at,uint16_t v){if(at+1<size){data[at]=uint8_t(v>>8);data[at+1]=uint8_t(v);}}
 bool ok()const{return !overflow;}
};

// Length of a MIDI message from its status byte; 0 for SysEx framing bytes.
inline unsigned message_length(uint8_t status){
 if(status<0xf0){uint8_t type=status&0xf0;return type==0xc0 || type==0xd0?2:3;}
 if(status==0xf2)return 3;
 if(status==0xf1 || status==0xf3)return 2;
 if(status==0xf0 || status==0xf7 || status==0xf4 || status==0xf5)return 0;
 return 1; // F6 and realtime F8..FF
}

struct CommandSection {
 bool journal=false,phantom=false;
 unsigned header=0,length=0; // header octets, MIDI list octets
};

inline bool read_command_header(const uint8_t* p,unsigned n,CommandSection& out){
 if(!n)return false;
 const bool big=p[0]&0x80;out.header=big?2:1;if(n<out.header)return false;
 out.length=big?unsigned(p[0]&0x0f)<<8|p[1]:p[0]&0x0f;
 out.journal=p[0]&0x40;out.phantom=p[0]&0x10;
 return out.header+out.length<=n;
}

// Walks the MIDI list of one command section. emit(bytes,length,delta) gets
// each complete message with its status byte restored and its timestamp as
// the cumulative delta (RTP units, modulo 2^32) from the packet timestamp.
// Returns false if the list is malformed; messages before the fault stay
// delivered. SysEx (whole or segmented) is consumed and counted, not emitted.
template<class Emit> bool parse_midi_list(const uint8_t* p,unsigned n,bool z,Emit emit,uint32_t* sysex=nullptr){
 const uint8_t* q=p;const uint8_t* end=p+n;
 uint8_t running=0;bool first=true;uint32_t time=0;
 while(q<end){
  if(!first || z){
   uint32_t delta=0;unsigned octets=0;bool done=false;
   while(q<end && octets<4){uint8_t b=*q++;++octets;delta=delta<<7|(b&0x7f);if(!(b&0x80)){done=true;break;}}
   if(!done)return false;
   time+=delta;
   if(q>=end)return true; // a trailing delta time without a command is legal
  }
  first=false;
  uint8_t b=*q;
  if(b==0xf0 || b==0xf7 || b==0xf4 || b==0xf5){
   // SysEx or a segment of one (also the undefined common F4/F5, which
   // RTP-MIDI terminates with F7). Ends at F7, F0 (more follows), F4
   // (cancel) or F5 ("dropped F7").
   ++q;bool closed=false;
   while(q<end){uint8_t c=*q++;if(c==0xf7 || c==0xf0 || c==0xf4 || c==0xf5){closed=true;break;}if(c&0x80)return false;}
   if(!closed)return false;
   if(sysex)++*sysex;
   running=0;continue;
  }
  uint8_t message[3];unsigned have=0;
  if(b&0x80){message[0]=b;++q;have=1;if(b<0xf0)running=b;else if(b<0xf8)running=0;}
  else{if(!running)return false;message[0]=running;have=1;}
  const unsigned want=message_length(message[0]);
  while(have<want){if(q>=end || (*q&0x80))return false;message[have++]=*q++;}
  emit(static_cast<const uint8_t*>(message),want,time);
 }
 return true;
}

// Compatibility wrapper: walks a whole command section (header + list).
template<class Emit> bool parse_command_section(const uint8_t* p,unsigned n,Emit emit,uint32_t* sysex=nullptr){
 CommandSection section;if(!read_command_header(p,n,section))return false;
 return parse_midi_list(p+section.header,section.length,p[0]&0x20,[&](const uint8_t* b,unsigned l,uint32_t){emit(b,l);},sysex);
}

inline void write_delta(Writer& w,uint32_t delta){
 delta&=0x0fffffff;
 if(delta>=1u<<21)w.byte(uint8_t(0x80|(delta>>21)));
 if(delta>=1u<<14)w.byte(uint8_t(0x80|((delta>>14)&0x7f)));
 if(delta>=1u<<7)w.byte(uint8_t(0x80|((delta>>7)&0x7f)));
 w.byte(uint8_t(delta&0x7f));
}

// One outgoing message with its delta from the packet timestamp.
struct OutMessage { uint8_t bytes[3]{};uint8_t length=0;uint32_t delta=0; };

// Writes a command section. Status bytes are always sent (no running status),
// so a lost packet never changes how the next one parses. Returns false if
// the messages do not fit.
inline bool write_command_section(Writer& w,const OutMessage* messages,unsigned count,bool journal){
 uint8_t list[kMaxPacket];Writer l(list,sizeof list);
 const bool z=count && messages[0].delta;
 uint32_t previous=0;
 for(unsigned i=0;i<count;++i){
  if(i || z)write_delta(l,messages[i].delta-previous);
  previous=messages[i].delta;
  l.bytes(messages[i].bytes,messages[i].length);
 }
 if(!l.ok())return false;
 const uint8_t flags=uint8_t((journal?0x40:0)|(z?0x20:0));
 if(l.size>15){w.byte(uint8_t(0x80|flags|(l.size>>8)));w.byte(uint8_t(l.size));}
 else w.byte(uint8_t(flags|l.size));
 w.bytes(list,l.size);
 return w.ok();
}

} // namespace groove::rtpmidi
