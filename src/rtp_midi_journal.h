#pragma once
// RTP-MIDI recovery journal (RFC 6295 section 5, appendices A and B).
//
// Receiver: tracks the MIDI state a session has delivered and, after a packet
// loss, compares it with the journal of the packet that ends the loss and
// emits the commands that repair it (chapters P, C, M, W, N, E, T, A and the
// sequencer chapter Q). Unknown or unused chapters are skipped by length.
//
// Sender: SenderHistory keeps the latest state of the outgoing stream (one
// history shared by every session); each session adds its own toggle, count
// and note reference counters (SessionCounts, which RFC 6295 counts from the
// session start) and writes the closed-loop journal: everything since the
// checkpoint packet the receiver has acknowledged.
// SysEx is not journalled in either direction (chapter X is skipped).
#include "rtp_midi.h"

namespace groove::rtpmidi {

constexpr uint8_t kUnknown=0x80;
constexpr unsigned kParamSlots=16;
constexpr uint32_t kPlayMs=500; // a recovered Note On older than this is a hint to skip it (Y=0)

inline bool toggle_controller(unsigned n){return n>=64 && n<=69;}
inline bool count_controller(unsigned n){return n==120 || n==121 || n>=123;}
inline bool all_notes_off_controller(unsigned n){return n==120 || n>=123;}
inline bool parameter_number_controller(unsigned n){return n>=98 && n<=101;}
inline bool parameter_data_controller(unsigned n){return n==6 || n==38 || n==96 || n==97;}
inline bool reset_controller(unsigned n){return n==1 || n==11 || (n>=64 && n<=67);} // RP-015 Reset All Controllers

enum ParamType:uint8_t { kNoParam=0,kRpn=1,kNrpn=2 };

// RPN and NRPN number registers (CC 101/100 and 99/98) and the pair the last
// number CC addressed. An unknown register reads as 0.
struct ParamSelect {
 uint8_t type=kNoParam,reg[3][2];bool pending=false; // pending: MSB sent, LSB not yet
 ParamSelect(){memset(reg,kUnknown,sizeof reg);}
 uint8_t msb()const{return type==kNoParam || reg[type][0]==kUnknown?0:reg[type][0];}
 uint8_t lsb()const{return type==kNoParam || reg[type][1]==kUnknown?0:reg[type][1];}
 bool selected()const{return type!=kNoParam && !(msb()==127 && lsb()==127);}
 bool number(uint8_t cc,uint8_t value){
  if(!parameter_number_controller(cc))return false;
  type=cc==101 || cc==100?kRpn:kNrpn;pending=cc==101 || cc==99;reg[type][pending?0:1]=value;
  return true;
 }
 void reset(){for(unsigned t=kRpn;t<=kNrpn;++t)reg[t][0]=reg[t][1]=127;pending=false;} // RP-015: null
};

// The checkpoint history covers a loss if the checkpoint packet is at most
// one past the highest sequence number received before it (modulo 2^16).
inline bool journal_covers(const uint8_t* journal,uint16_t highest_seen){
 const uint16_t ahead=uint16_t(be16(journal+1)-uint16_t(highest_seen+1));
 return ahead==0 || ahead>=0x8000;
}

// Toggle/count tool and Chapter E reference counters of one sending session,
// counted from the session start as RFC 6295 A.3.2 and A.7 define them.
struct SessionCounts {
 uint8_t toggles[16][128],counts[16][128],refs[16][128],state[16][16];
 SessionCounts(){reset();}
 void reset(){memset(toggles,0,sizeof toggles);memset(counts,0,sizeof counts);memset(refs,0,sizeof refs);memset(state,0,sizeof state);}
 bool on(unsigned c,unsigned n)const{return state[c][n>>3]&(0x80>>(n&7));}
 void toggle(unsigned c,unsigned n,bool now){
  if(on(c,n)==now)return;
  toggles[c][n]=uint8_t((toggles[c][n]+1)&63);
  if(now)state[c][n>>3]|=uint8_t(0x80>>(n&7));else state[c][n>>3]&=uint8_t(~(0x80>>(n&7)));
 }
 void record(const uint8_t* b,unsigned n){
  if(!n)return;
  if(b[0]==0xff){reset();return;}
  if(b[0]>=0xf0)return;
  const unsigned c=b[0]&15,type=b[0]&0xf0,d1=n>1?b[1]&0x7f:0,d2=n>2?b[2]&0x7f:0;
  if(type==0x90 && d2){if(refs[c][d1]<127)++refs[c][d1];return;}
  if(type==0x80 || type==0x90){if(refs[c][d1])--refs[c][d1];return;}
  if(type!=0xb0)return;
  toggle(c,d1,d2>=64);
  counts[c][d1]=uint8_t((counts[c][d1]+1)&63);
  if(all_notes_off_controller(d1))memset(refs[c],0,sizeof refs[c]);
  if(d1==121)for(unsigned k=64;k<=67;++k)toggle(c,k,false); // RP-015 reset
 }
};

// ---------------------------------------------------------------- receiver

class Receiver {
public:
 struct Param { uint8_t type=kNoParam,msb=0,lsb=0,entry_msb=kUnknown,entry_lsb=kUnknown;int16_t buttons=0; };
 struct Channel {
  uint8_t on[16]{};
  uint8_t cc[128],toggles[128]{},counts[128]{},poly[128];
  uint8_t program=kUnknown,bank_msb=kUnknown,bank_lsb=kUnknown,pressure=kUnknown;
  uint16_t pitch=0xffff;
  ParamSelect sel;
  Param params[kParamSlots];uint8_t next_param=0;
  Channel(){memset(cc,kUnknown,sizeof cc);memset(poly,kUnknown,sizeof poly);}
  bool note(unsigned n)const{return on[n>>3]&(0x80>>(n&7));}
  void set_note(unsigned n,bool v){if(v)on[n>>3]|=uint8_t(0x80>>(n&7));else on[n>>3]&=uint8_t(~(0x80>>(n&7)));}
  Param* find(uint8_t type,uint8_t msb,uint8_t lsb){for(auto& p:params)if(p.type==type && p.msb==msb && p.lsb==lsb)return &p;return nullptr;}
  Param& param(uint8_t type,uint8_t msb,uint8_t lsb){if(auto* p=find(type,msb,lsb))return *p;auto& p=params[next_param];next_param=uint8_t((next_param+1)%kParamSlots);p=Param{};p.type=type;p.msb=msb;p.lsb=lsb;return p;}
  bool selected()const{return sel.selected();}
 };
 struct Sequencer { bool known=false,running=false,d=false;uint32_t position=0; };

 void reset(){for(auto& c:channels_)c=Channel{};sequencer_={};}
 // The caller released every note (an uncovered loss): forget them and the
 // pedal, so the journal that follows restores what still sounds.
 void forget_notes(){for(auto& c:channels_){memset(c.on,0,sizeof c.on);c.cc[64]=kUnknown;}}
 const Channel& channel(unsigned c)const{return channels_[c&15];}
 const Sequencer& sequencer()const{return sequencer_;}
 bool holds_notes()const{for(const auto& c:channels_)for(auto b:c.on)if(b)return true;return false;}
 // A note on, or the sustain pedal down (a released note may still sound).
 bool holds_sound()const{if(holds_notes())return true;for(const auto& c:channels_)if(c.cc[64]>=64 && c.cc[64]!=kUnknown)return true;return false;}

 // Track one delivered message (from the stream or from a repair).
 void apply(const uint8_t* b,unsigned n){
  if(!n)return;
  const uint8_t status=b[0];
  if(status>=0xf0){
   auto& s=sequencer_;
   if(status==0xfa){s.known=true;s.running=true;s.position=0;s.d=false;}
   else if(status==0xfb){s.known=true;s.running=true;}
   else if(status==0xfc){s.known=true;s.running=false;}
   else if(status==0xf8){if(s.running){if(s.d)s.position=(s.position+1)&0x7ffff;else s.d=true;}}
   else if(status==0xf2 && n>=3){s.position=6u*(unsigned(b[1])|unsigned(b[2])<<7);s.d=false;}
   else if(status==0xff)reset();
   return;
  }
  auto& c=channels_[status&15];const uint8_t type=status&0xf0;
  const uint8_t d1=n>1?b[1]&0x7f:0,d2=n>2?b[2]&0x7f:0;
  switch(type){
   case 0x80:c.set_note(d1,false);break;
   case 0x90:c.set_note(d1,d2!=0);break;
   case 0xa0:c.poly[d1]=d2;break;
   case 0xb0:control(c,d1,d2);break;
   case 0xc0:c.program=d1;c.bank_msb=c.cc[0];c.bank_lsb=c.cc[32];break; // the bank registers at the change
   case 0xd0:c.pressure=d1;break;
   case 0xe0:c.pitch=uint16_t(d1|d2<<7);break;
  }
 }

 // Repair the state from a journal (the bytes after the command section).
 // emit(bytes,length) must deliver the command and call apply() for it.
 // Returns false if the journal is malformed (repairs before the fault stay).
 template<class Emit> bool recover(const uint8_t* j,unsigned n,Emit emit){
  if(n<3)return false;
  const bool y=j[0]&0x40,a=j[0]&0x20;const unsigned channels=(j[0]&0x0f)+1u;
  unsigned at=3;bool ok=true;
  if(y){
   if(at+2>n)return false;
   const unsigned length=be16(j+at)&0x3ff;
   if(length<2 || at+length>n)return false;
   if(!system_journal(j+at,length,emit))ok=false;
   at+=length;
  }
  if(a)for(unsigned i=0;i<channels;++i){
   if(at+3>n)return false;
   const uint8_t* h=j+at;const unsigned length=unsigned(h[0]&3)<<8|h[1];
   if(length<3 || at+length>n)return false;
   if(!channel_journal((h[0]>>3)&15,h[2],h+3,length-3,emit))ok=false;
   at+=length;
  }
  return ok;
 }

private:
 static void control(Channel& c,uint8_t number,uint8_t value){
  if(parameter_data_controller(number) && c.sel.selected()){
   // Part of an RPN/NRPN transaction: a parameter value, not a controller.
   c.sel.pending=false;
   auto& p=c.param(c.sel.type,c.sel.msb(),c.sel.lsb());
   // MIDI 1.0: a new MSB sets the LSB to zero (chapter M codes no ENTRY-LSB
   // sent before the latest Data Entry MSB).
   if(number==6){p.entry_msb=value;p.entry_lsb=0;p.buttons=0;}
   else if(number==38){p.entry_lsb=value;p.buttons=0;}
   else p.buttons=int16_t(p.buttons+(number==96?1:-1));
   return;
  }
  const uint8_t old=c.cc[number];
  const bool was_on=old!=kUnknown && old>=64,now_on=value>=64;
  if(was_on!=now_on)c.toggles[number]=uint8_t((c.toggles[number]+1)&63);
  c.counts[number]=uint8_t((c.counts[number]+1)&63);
  c.cc[number]=value;
  if(all_notes_off_controller(number))memset(c.on,0,sizeof c.on);
  if(number==121){
   // RP-015 Reset All Controllers.
   c.pitch=0x2000;c.pressure=0;memset(c.poly,0,sizeof c.poly);
   c.cc[1]=0;c.cc[11]=127;
   for(unsigned k=64;k<=67;++k){if(c.cc[k]!=kUnknown && c.cc[k]>=64)c.toggles[k]=uint8_t((c.toggles[k]+1)&63);c.cc[k]=0;}
   c.sel.reset();
  }
  c.sel.number(number,value);
 }

 template<class Out> bool system_journal(const uint8_t* p,unsigned n,Out out){
  const uint8_t toc=p[0];unsigned at=2;
  if(toc&0x40){ // chapter D
   if(at>=n)return false;
   const uint8_t h=p[at++];
   if(h&0x40)++at;
   if(h&0x20)++at;
   if(h&0x10)++at;
   for(uint8_t bit=0x08;bit>=0x04;bit>>=1)if(h&bit){if(at+2>n)return false;unsigned l=be16(p+at)&0x3ff;if(l<2)return false;at+=l;}
   for(uint8_t bit=0x02;bit>=0x01;bit>>=1)if(h&bit){if(at>=n)return false;unsigned l=p[at]&0x1f;if(l<1)return false;at+=l;}
   if(at>n)return false;
  }
  if(toc&0x20)++at; // chapter V
  if(toc&0x10){ // chapter Q
   if(at>=n)return false;
   const uint8_t h=p[at++];
   const bool running=h&0x40,d=h&0x20,c=h&0x10,t=h&0x08;
   uint32_t position=0;
   if(c){if(at+2>n)return false;position=uint32_t(h&7)<<16|be16(p+at);at+=2;}
   if(t){if(at+3>n)return false;at+=3;}
   sequencer(running,d,c,position,out);
  }
  return at<=n;
 }

 template<class Out> void sequencer(bool running,bool d,bool c,uint32_t position,Out out){
  auto& s=sequencer_;
  // Song Position Pointer counts sixteenths (6 clocks). The next clock plays
  // position+1 when D=1, otherwise position itself.
  const uint32_t next=d?position+1:position,have=s.d?s.position+1:s.position;
  auto locate=[&](){const unsigned beat=next/6;if(beat!=have/6 && beat<16384){const uint8_t spp[3]{0xf2,uint8_t(beat&0x7f),uint8_t((beat>>7)&0x7f)};out(spp,3);}};
  if(running){
   if(s.known && s.running)return; // a lost clock is the clock follower's job
   if(!c && !d){const uint8_t start=0xfa;out(&start,1);return;}
   locate();
   const uint8_t resume=0xfb;out(&resume,1);
   return;
  }
  if(!s.known || s.running){const uint8_t stop=0xfc;out(&stop,1);}
  locate();
 }

 template<class Out> bool channel_journal(unsigned ch,uint8_t toc,const uint8_t* p,unsigned n,Out out){
  // Locate every chapter first: N needs E's release velocities.
  struct Span { const uint8_t* p=nullptr;unsigned n=0; } P,C,M,W,N,E,T,A;
  unsigned at=0;
  auto take=[&](Span& s,unsigned length)->bool{if(at+length>n)return false;s.p=p+at;s.n=length;at+=length;return true;};
  if((toc&0x80) && !take(P,3))return false;
  if(toc&0x40){if(at>=n)return false;if(!take(C,1+2u*((p[at]&0x7f)+1)))return false;}
  if(toc&0x20){if(at+2>n)return false;unsigned l=be16(p+at)&0x3ff;if(l<2 || !take(M,l))return false;}
  if((toc&0x10) && !take(W,2))return false;
  if(toc&0x08){
   if(at+2>n)return false;
   unsigned logs=p[at]&0x7f,low=p[at+1]>>4,high=p[at+1]&15;
   if(logs==127 && low==15 && high==0)logs=128;
   unsigned offbits=low<=high?high-low+1:0;
   if(low>high && !(low==15 && (high==0 || high==1)))return false;
   if(!take(N,2+2*logs+offbits))return false;
  }
  if(toc&0x04){if(at>=n)return false;if(!take(E,1+2u*((p[at]&0x7f)+1)))return false;}
  if((toc&0x02) && !take(T,1))return false;
  if(toc&0x01){if(at>=n)return false;if(!take(A,1+2u*((p[at]&0x7f)+1)))return false;}
  auto& c=channels_[ch];const uint8_t chan=uint8_t(ch);
  auto cc=[&](uint8_t number,uint8_t value){const uint8_t m[3]{uint8_t(0xb0|chan),number,value};out(m,3);};
  // Chapter C logs data entry only outside a transaction. Sent while this
  // receiver has a parameter selected it would change that parameter, so
  // close the selection around it and reopen it.
  auto general=[&](uint8_t number,uint8_t value){
   if(!parameter_data_controller(number) || !c.sel.selected()){cc(number,value);return;}
   const ParamSelect saved=c.sel;
   const uint8_t high=saved.type==kRpn?101:99,low=saved.type==kRpn?100:98;
   cc(high,127);cc(low,127);cc(number,value);
   cc(high,saved.msb());cc(low,saved.lsb());
   if(saved.pending)cc(high,saved.msb());
  };
  if(P.p){
   const uint8_t program=P.p[0]&0x7f,b=P.p[1]&0x80,msb=P.p[1]&0x7f,lsb=P.p[2]&0x7f;
   // BANK-LSB 0 also means "no LSB after the MSB": then the receiver's LSB
   // register stands, and chapter C (processed next) restores the latest one.
   if(c.program!=program || (b && (c.bank_msb!=msb || (lsb && c.bank_lsb!=lsb)))){
    if(b){cc(0,msb);if(lsb)cc(32,lsb);}
    const uint8_t m[2]{uint8_t(0xc0|chan),program};out(m,2);
   }
  }
  // A log older than a Reset All Controllers log codes a value the reset
  // has replaced since (for the controllers RP-015 resets): skip it.
  unsigned last_reset=0;
  if(C.p)for(unsigned i=1;i+1<C.n;i+=2)if((C.p[i]&0x7f)==121)last_reset=i;
  if(C.p)for(unsigned i=1;i+1<C.n;i+=2){
   const uint8_t number=C.p[i]&0x7f,v=C.p[i+1];
   if(i<last_reset && reset_controller(number))continue;
   if(!(v&0x80)){if(c.cc[number]!=(v&0x7f))general(number,v&0x7f);continue;}
   const uint8_t alt=v&0x3f;
   if(!(v&0x40)){ // toggle tool
    if(c.toggles[number]==alt)continue;
    const bool want=alt&1,have=c.cc[number]!=kUnknown && c.cc[number]>=64;
    if(want!=have)cc(number,want?127:0);else{cc(number,have?0:127);cc(number,have?127:0);}
    c.toggles[number]=alt;
   } else { // count tool
    if(c.counts[number]==alt)continue;
    cc(number,0);c.counts[number]=alt;
   }
  }
  if(M.p)parameters(c,M,cc);
  if(W.p){const uint16_t pitch=uint16_t((W.p[0]&0x7f)|(W.p[1]&0x7f)<<7);if(c.pitch!=pitch){const uint8_t m[3]{uint8_t(0xe0|chan),uint8_t(W.p[0]&0x7f),uint8_t(W.p[1]&0x7f)};out(m,3);}}
  if(N.p){
   uint8_t release[128];memset(release,64,sizeof release);
   if(E.p)for(unsigned i=1;i+1<E.n;i+=2)if(E.p[i+1]&0x80)release[E.p[i]&0x7f]=E.p[i+1]&0x7f;
   unsigned logs=N.p[0]&0x7f,low=N.p[1]>>4,high=N.p[1]&15;if(logs==127 && low==15 && high==0)logs=128;
   for(unsigned i=0;i<logs;++i){
    const uint8_t note=N.p[2+2*i]&0x7f,y=N.p[3+2*i]&0x80,velocity=N.p[3+2*i]&0x7f;
    if(velocity && y && !c.note(note)){const uint8_t m[3]{uint8_t(0x90|chan),note,velocity};out(m,3);}
   }
   if(low<=high)for(unsigned k=0;k<=high-low;++k){
    const uint8_t bits=N.p[2+2*logs+k];
    for(unsigned b=0;b<8;++b)if(bits&(0x80>>b)){
     const unsigned note=8*(low+k)+b;
     if(note<128 && c.note(note)){const uint8_t m[3]{uint8_t(0x80|chan),uint8_t(note),release[note]};out(m,3);}
    }
   }
  }
  if(T.p){const uint8_t pressure=T.p[0]&0x7f;if(c.pressure!=pressure){const uint8_t m[2]{uint8_t(0xd0|chan),pressure};out(m,2);}}
  if(A.p)for(unsigned i=1;i+1<A.n;i+=2){
   const uint8_t note=A.p[i]&0x7f,x=A.p[i+1]&0x80,pressure=A.p[i+1]&0x7f;
   if(!x && c.poly[note]!=pressure){const uint8_t m[3]{uint8_t(0xa0|chan),note,pressure};out(m,3);}
  }
  return true;
 }

 template<class Span,class CC> void parameters(Channel& c,const Span& M,CC cc){
  const uint16_t h=be16(M.p);
  const bool pending=h&0x4000,in_progress=h&0x2000,u=h&0x1000,w=h&0x0800,z=h&0x0400;
  unsigned at=2;uint8_t pending_value=0;bool pending_nrpn=false;
  if(pending){if(at>=M.n)return;pending_nrpn=M.p[at]&0x80;pending_value=M.p[at]&0x7f;++at;}
  const bool short_header=z && (u || w);
  uint8_t last_type=kNoParam,last_msb=0,last_lsb=0;
  auto select=[&](uint8_t type,uint8_t msb,uint8_t lsb){
   if(c.sel.type==type && !c.sel.pending && c.sel.msb()==msb && c.sel.lsb()==lsb)return;
   cc(type==kRpn?101:99,msb);cc(type==kRpn?100:98,lsb);
  };
  while(at<M.n){
   uint8_t lsb=M.p[at]&0x7f,msb=0,type=u?kRpn:kNrpn,toc=0;
   if(short_header){if(at+2>M.n)return;toc=M.p[at+1];at+=2;}
   else{if(at+3>M.n)return;type=(M.p[at+1]&0x80)?kNrpn:kRpn;msb=M.p[at+1]&0x7f;toc=M.p[at+2];at+=3;}
   // The X bits mark values older than the latest Reset All Controllers.
   // RP-015 keeps parameter values across that reset, so they still apply.
   int entry_msb=-1,entry_lsb=-1,buttons=0;bool has_buttons=false;
   if(toc&0x80){if(at>=M.n)return;entry_msb=M.p[at]&0x7f;++at;}
   if(toc&0x40){if(at>=M.n)return;entry_lsb=M.p[at]&0x7f;++at;}
   if(toc&0x20){if(at+2>M.n)return;const bool negative=M.p[at]&0x80;const int magnitude=(M.p[at]&0x3f)<<8|M.p[at+1];buttons=negative?-magnitude:magnitude;has_buttons=true;at+=2;}
   if(toc&0x10){if(at+2>M.n)return;at+=2;}
   if(toc&0x08){if(at>=M.n)return;++at;}
   last_type=type;last_msb=msb;last_lsb=lsb;
   if(!(toc&0x02))continue; // no value tool
   const Param* p=c.find(type,msb,lsb);
   const int have=p?p->buttons:0;
   // ENTRY-MSB without ENTRY-LSB: the LSB is zero since that MSB.
   const bool lsb_zero=entry_msb>=0 && entry_lsb<0;
   const bool fix_entry=(entry_msb>=0 && (!p || p->entry_msb!=entry_msb || (lsb_zero && p->entry_lsb!=0))) || (entry_lsb>=0 && (!p || p->entry_lsb!=entry_lsb));
   const bool fix_buttons=has_buttons && have!=buttons;
   if(!fix_entry && !fix_buttons)continue;
   select(type,msb,lsb);
   // Data entry restarts the increment count, so a wrong count is redone
   // from the entry value when there is one.
   int from=have;
   if(fix_entry || (fix_buttons && (entry_msb>=0 || entry_lsb>=0))){
    if(entry_msb>=0)cc(6,uint8_t(entry_msb));
    if(entry_lsb>=0)cc(38,uint8_t(entry_lsb));
    from=0;
   }
   if(has_buttons){int diff=buttons-from;if(diff>64)diff=64;if(diff<-64)diff=-64;while(diff>0){cc(96,0);--diff;}while(diff<0){cc(97,0);++diff;}}
  }
  // Leave the transaction state as the sender has it.
  if(pending){const uint8_t type=pending_nrpn?kNrpn:kRpn;if(!c.sel.pending || c.sel.type!=type || c.sel.msb()!=pending_value)cc(type==kRpn?101:99,pending_value);}
  else if(in_progress && last_type!=kNoParam)select(last_type,last_msb,last_lsb);
  else if(!in_progress && (c.sel.selected() || c.sel.pending)){const uint8_t type=c.sel.type;cc(type==kRpn?101:99,127);cc(type==kRpn?100:98,127);}
 }

 Channel channels_[16];Sequencer sequencer_;
};

// ------------------------------------------------------------------ sender

// Latest state of the outgoing stream, shared by every session. Orders are a
// global command counter (0 = never); a session maps its checkpoint packet to
// the order of that packet's first command.
class SenderHistory {
public:
 struct Note { uint32_t order=0,on_ms=0;uint8_t velocity=0,release=64; };
 struct Control { uint32_t order=0;uint8_t value=0;bool general=true; };
 struct Poly { uint32_t order=0;uint8_t value=0; };
 struct Param { uint32_t order=0,msb_order=0,lsb_order=0,buttons_order=0;uint8_t type=kNoParam,msb=0,lsb=0,entry_msb=0,entry_lsb=0;int16_t buttons=0;bool has_msb=false,has_lsb=false,has_buttons=false; };
 struct Channel {
  Note notes[128];Control cc[128];Poly poly[128];
  uint32_t program_order=0,pitch_order=0,pressure_order=0,nactive_order=0,cactive_order=0,noteoff_order=0,select_order=0;
  uint8_t program=0,bank_msb=0,bank_lsb=0,pressure=0;bool bank=false,bank_reset=false;uint16_t pitch=0x2000;
  ParamSelect sel;
  Param params[kParamSlots];uint8_t next_param=0;
  Param& param(){const uint8_t t=sel.type,m=sel.msb(),l=sel.lsb();for(auto& p:params)if(p.order && p.type==t && p.msb==m && p.lsb==l)return p;auto& p=params[next_param];next_param=uint8_t((next_param+1)%kParamSlots);p=Param{};p.type=t;p.msb=m;p.lsb=l;return p;}
 };
 struct Sequencer { uint32_t order=0,position=0;bool running=false,d=false,continued=false; };

 uint32_t order()const{return order_;}
 const Channel& channel(unsigned c)const{return channels_[c&15];}
 const Sequencer& sequencer()const{return sequencer_;}

 void record(const uint8_t* b,unsigned n,uint32_t now_ms){
  if(!n)return;
  const uint8_t status=b[0];
  if(status>=0xf0){
   auto& s=sequencer_;
   switch(status){
    case 0xfa:s.order=++order_;s.running=true;s.position=0;s.d=false;s.continued=false;break;
    case 0xfb:s.order=++order_;s.running=true;s.continued=true;break;
    case 0xfc:s.order=++order_;s.running=false;break;
    case 0xf8:if(s.running){s.order=++order_;if(s.d)s.position=(s.position+1)&0x7ffff;else s.d=true;}break;
    case 0xf2:if(n>=3){s.order=++order_;s.position=6u*(unsigned(b[1]&0x7f)|unsigned(b[2]&0x7f)<<7)&0x7ffff;s.d=false;}break;
    case 0xff:{const uint32_t o=++order_;for(auto& c:channels_){c=Channel{};c.nactive_order=c.cactive_order=o;}sequencer_=Sequencer{};break;}
    default:break;
   }
   return;
  }
  const uint32_t o=++order_;
  auto& c=channels_[status&15];const uint8_t type=status&0xf0;
  const uint8_t d1=n>1?b[1]&0x7f:0,d2=n>2?b[2]&0x7f:0;
  switch(type){
   case 0x90:if(d2){auto& x=c.notes[d1];x.order=o;x.velocity=d2;x.on_ms=now_ms;break;}
    [[fallthrough]];
   case 0x80:{auto& x=c.notes[d1];x.order=o;x.velocity=0;x.release=type==0x80?d2:64;c.noteoff_order=o;break;}
   case 0xa0:c.poly[d1]={o,d2};break;
   case 0xb0:control(c,d1,d2,o);break;
   case 0xc0:c.program_order=o;c.program=d1;c.bank=c.cc[0].order!=0;c.bank_msb=c.cc[0].value;c.bank_lsb=c.cc[32].order>c.cc[0].order?c.cc[32].value:0;c.bank_reset=c.bank && c.cactive_order>c.cc[0].order;break;
   case 0xd0:c.pressure_order=o;c.pressure=d1;break;
   case 0xe0:c.pitch_order=o;c.pitch=uint16_t(d1|d2<<7);break;
  }
 }

 // Writes the journal of one session's next packet. Commands with order >=
 // threshold form the checkpoint history; orders >= previous were sent in the
 // packet just before this one (S bits). checkpoint is the checkpoint
 // packet's sequence number, now_ms the time for the Y (play) hint.
 bool write(Writer& w,uint16_t checkpoint,uint32_t threshold,uint32_t previous,const SessionCounts& counts,uint32_t now_ms)const{
  if(!threshold)threshold=1;
  const unsigned top=w.mark();
  w.byte(0);w.word(checkpoint);
  bool single=true,system=false;
  const auto& s=sequencer_;
  if(s.order>=threshold){
   system=true;
   const bool fresh=s.order>=previous;if(fresh)single=false;
   // C=0 codes the song start; a Continue from the start needs C=1, CLOCK=0.
   const bool c=s.position!=0 || (s.running && !s.d && s.continued);
   const unsigned length=2+1+(c?2:0);
   w.word(uint16_t((fresh?0:0x8000)|0x1000|length));
   w.byte(uint8_t((fresh?0:0x80)|(s.running?0x40:0)|(s.d?0x20:0)|(c?0x10:0)|(c?(s.position>>16)&7:0)));
   if(c)w.word(uint16_t(s.position));
  }
  unsigned journals=0;
  for(unsigned ch=0;ch<16;++ch){
   uint8_t buffer[1100];Writer cw(buffer,sizeof buffer);bool fresh=false;
   const uint8_t toc=channel_journal(ch,cw,threshold,previous,counts,now_ms,fresh);
   if(!toc)continue;
   if(!cw.ok() || cw.size+3>1023)return false;
   const unsigned length=cw.size+3;
   if(fresh)single=false;
   w.byte(uint8_t((fresh?0:0x80)|ch<<3|(length>>8)));w.byte(uint8_t(length));w.byte(toc);w.bytes(buffer,cw.size);
   ++journals;
  }
  if(w.size>top)w.data[top]=uint8_t((single?0x80:0)|(system?0x40:0)|(journals?0x20:0)|(journals?journals-1:0));
  return w.ok();
 }

private:
 static bool in(uint32_t order,uint32_t threshold){return order && order>=threshold;}

 void control(Channel& c,uint8_t number,uint8_t value,uint32_t o){
  auto& x=c.cc[number];
  x.value=value;x.order=o;
  x.general=!parameter_number_controller(number) && !(parameter_data_controller(number) && c.sel.selected());
  if(all_notes_off_controller(number))c.nactive_order=o;
  if(number==121){c.cactive_order=o;c.sel.reset();}
  if(c.sel.number(number,value)){c.select_order=o;if(!c.sel.pending && c.sel.selected())c.param().order=o;return;}
  if(parameter_data_controller(number) && c.sel.selected()){
   c.sel.pending=false;
   auto& p=c.param();p.order=o;c.select_order=o;
   if(number==6){p.entry_msb=value;p.has_msb=true;p.msb_order=o;p.buttons=0;}
   else if(number==38){p.entry_lsb=value;p.has_lsb=true;p.lsb_order=o;p.buttons=0;}
   else{int v=p.buttons+(number==96?1:-1);if(v>16383)v=16383;if(v<-16383)v=-16383;p.buttons=int16_t(v);p.has_buttons=true;p.buttons_order=o;}
  }
 }

 // Writes the chapters of one channel journal; returns its TOC (0 = none).
 uint8_t channel_journal(unsigned ch,Writer& w,uint32_t thr,uint32_t prev,const SessionCounts& counts,uint32_t now_ms,bool& fresh)const{
  const Channel& c=channels_[ch];
  uint8_t toc=0;
  auto s=[&](uint32_t order){const bool f=order>=prev;if(f)fresh=true;return uint8_t(f?0:0x80);};
  // P
  if(in(c.program_order,thr)){toc|=0x80;w.byte(uint8_t(s(c.program_order)|c.program));w.byte(uint8_t((c.bank?0x80:0)|(c.bank?c.bank_msb:0)));w.byte(uint8_t((c.bank_reset?0x80:0)|(c.bank?c.bank_lsb:0)));}
  // C: oldest first, one log per controller, tool by controller class.
  {
   uint8_t list[128];unsigned count=0;
   // RP-015 controllers set before the latest Reset All Controllers may be left out.
   for(unsigned n=0;n<128;++n){const auto& x=c.cc[n];if(in(x.order,thr) && x.general && !(reset_controller(n) && x.order<c.cactive_order))list[count++]=uint8_t(n);}
   sort(list,count,[&](uint8_t n){return c.cc[n].order;});
   if(count){
    toc|=0x40;bool any=false;for(unsigned i=0;i<count;++i)if(c.cc[list[i]].order>=prev)any=true;
    w.byte(uint8_t((any?0:0x80)|(count-1)));if(any)fresh=true;
    for(unsigned i=0;i<count;++i){
     const uint8_t n=list[i];const auto& x=c.cc[n];w.byte(uint8_t(s(x.order)|n));
     if(count_controller(n))w.byte(uint8_t(0xc0|(counts.counts[ch][n]&0x3f)));
     else if(toggle_controller(n))w.byte(uint8_t(0x80|(counts.toggles[ch][n]&0x3f)));
     else w.byte(x.value);
    }
   }
  }
  // M
  {
   uint8_t list[kParamSlots];unsigned count=0;
   for(unsigned i=0;i<kParamSlots;++i){const auto& p=c.params[i];if(in(p.order,thr) && !(p.msb==127 && p.lsb==127))list[count++]=uint8_t(i);}
   sort(list,count,[&](uint8_t i){return c.params[i].order;});
   if(count || in(c.select_order,thr)){
    toc|=0x20;
    const unsigned start=w.mark();
    const bool pending=c.sel.pending,progress=!pending && c.sel.selected();
    const bool f=in(c.select_order,thr) && c.select_order>=prev;if(f)fresh=true;
    w.word(uint16_t((f?0:0x8000)|(pending?0x4000:0)|(progress?0x2000:0)));
    if(pending)w.byte(uint8_t((c.sel.type==kNrpn?0x80:0)|c.sel.msb()));
    for(unsigned i=0;i<count;++i){
     const auto& p=c.params[list[i]];
     const bool j=p.has_msb,k=p.has_lsb && p.lsb_order>p.msb_order,l=p.has_buttons;
     w.byte(uint8_t(s(p.order)|p.lsb));w.byte(uint8_t((p.type==kNrpn?0x80:0)|p.msb));
     w.byte(uint8_t((j?0x80:0)|(k?0x40:0)|(l?0x20:0)|0x02));
     if(j)w.byte(uint8_t((p.msb_order<c.cactive_order?0x80:0)|p.entry_msb));
     if(k)w.byte(uint8_t((p.lsb_order<c.cactive_order?0x80:0)|p.entry_lsb));
     if(l){const int m=p.buttons<0?-p.buttons:p.buttons;w.byte(uint8_t((p.buttons<0?0x80:0)|(p.buttons_order<c.cactive_order?0x40:0)|((m>>8)&0x3f)));w.byte(uint8_t(m));}
    }
    const unsigned length=w.mark()-start;
    if(w.ok())w.patch16(start,uint16_t((be16(w.data+start)&0xfc00)|length));
   }
  }
  // W
  if(in(c.pitch_order,thr) && c.pitch_order>=c.cactive_order){toc|=0x10;w.byte(uint8_t(s(c.pitch_order)|(c.pitch&0x7f)));w.byte(uint8_t((c.pitch>>7)&0x7f));}
  // N, then E for overlapping notes and release velocities
  uint8_t on[128];unsigned ons=0;uint8_t off[16]{};int low=16,high=-1;
  uint8_t extras[256];unsigned extra_count=0; // pairs: note, value
  for(unsigned n=0;n<128;++n){
   const auto& x=c.notes[n];if(!in(x.order,thr) || x.order<c.nactive_order)continue;
   if(x.velocity)on[ons++]=uint8_t(n);
   else{off[n>>3]|=uint8_t(0x80>>(n&7));if(int(n>>3)<low)low=int(n>>3);if(int(n>>3)>high)high=int(n>>3);}
  }
  if(ons || high>=0){
   toc|=0x08;
   sort(on,ons,[&](uint8_t n){return c.notes[n].order;});
   const bool b=in(c.noteoff_order,thr) && c.noteoff_order>=prev;if(b)fresh=true;
   const unsigned len=ons==128?127:ons;
   const uint8_t lh=high>=0?uint8_t(low<<4|high):(ons==128?0xf0:0xf1);
   w.byte(uint8_t((b?0:0x80)|len));w.byte(lh);
   for(unsigned i=0;i<ons;++i){const auto& x=c.notes[on[i]];const bool play=now_ms-x.on_ms<kPlayMs;w.byte(uint8_t(s(x.order)|on[i]));w.byte(uint8_t((play?0x80:0)|x.velocity));}
   if(high>=0)for(int k=low;k<=high;++k)w.byte(off[k]);
   uint8_t touched[128];unsigned tc=0;
   auto counted=[&](unsigned n){const uint8_t refs=counts.refs[ch][n];return c.notes[n].velocity?refs>1:refs>0;};
   for(unsigned n=0;n<128;++n){
    const auto& x=c.notes[n];if(!in(x.order,thr) || x.order<c.nactive_order)continue;
    if(counted(n) || (!x.velocity && x.release!=64))touched[tc++]=uint8_t(n);
   }
   sort(touched,tc,[&](uint8_t n){return c.notes[n].order;});
   // At most 128 logs: reference counts must stay, release velocities may go.
   unsigned required=0;for(unsigned i=0;i<tc;++i)if(counted(touched[i]))++required;
   unsigned velocities=required<128?128-required:0;
   for(unsigned i=0;i<tc;++i){
    const uint8_t n=touched[i];const auto& x=c.notes[n];
    if(counted(n) && extra_count<128){extras[2*extra_count]=n;extras[2*extra_count+1]=counts.refs[ch][n];++extra_count;}
    if(!x.velocity && x.release!=64 && velocities && extra_count<128){extras[2*extra_count]=n;extras[2*extra_count+1]=uint8_t(0x80|x.release);++extra_count;--velocities;}
   }
  }
  if(extra_count){
   toc|=0x04;bool any=false;
   for(unsigned i=0;i<extra_count;++i)if(c.notes[extras[2*i]].order>=prev)any=true;
   if(any)fresh=true;
   w.byte(uint8_t((any?0:0x80)|(extra_count-1)));
   for(unsigned i=0;i<extra_count;++i){w.byte(uint8_t(s(c.notes[extras[2*i]].order)|extras[2*i]));w.byte(extras[2*i+1]);}
  }
  // T
  if(in(c.pressure_order,thr) && c.pressure_order>=c.cactive_order && c.pressure_order>=c.nactive_order){toc|=0x02;w.byte(uint8_t(s(c.pressure_order)|c.pressure));}
  // A
  {
   uint8_t list[128];unsigned count=0;
   for(unsigned n=0;n<128;++n){const auto& x=c.poly[n];if(in(x.order,thr) && x.order>=c.cactive_order)list[count++]=uint8_t(n);}
   sort(list,count,[&](uint8_t n){return c.poly[n].order;});
   if(count){
    toc|=0x01;bool any=false;for(unsigned i=0;i<count;++i)if(c.poly[list[i]].order>=prev)any=true;if(any)fresh=true;
    w.byte(uint8_t((any?0:0x80)|(count-1)));
    for(unsigned i=0;i<count;++i){const auto& x=c.poly[list[i]];w.byte(uint8_t(s(x.order)|list[i]));w.byte(uint8_t((x.order<c.nactive_order?0x80:0)|x.value));}
   }
  }
  return toc;
 }

 template<class Key> static void sort(uint8_t* items,unsigned count,Key key){
  for(unsigned i=1;i<count;++i){uint8_t v=items[i];uint32_t k=key(v);unsigned j=i;while(j && key(items[j-1])>k){items[j]=items[j-1];--j;}items[j]=v;}
 }

 Channel channels_[16];Sequencer sequencer_;uint32_t order_=0;
};

} // namespace groove::rtpmidi
