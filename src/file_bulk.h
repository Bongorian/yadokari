#pragma once
#include "user_files.h"
namespace groove {
// MGF1 v1, little endian, 24-byte header. No GBX1 file commands.
// FNV-1a checksums detect damaged framing; PUT integrity uses SHA-256 in UserFiles.
namespace bulk {
constexpr unsigned kHeader=24,kPayload=32776;
enum Op:uint8_t {Open=1,Close=2,List=3,Get=4,Begin=5,Data=6,Commit=7,Delete=8,Cancel=9};
constexpr uint16_t Invalid=128,Version=129;
inline uint32_t checksum(const uint8_t* p,unsigned n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
inline uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint64_t u64(const uint8_t* p){return u32(p)|(uint64_t(u32(p+4))<<32);}
inline void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(i*8));}
inline void put64(uint8_t* p,uint64_t n){put32(p,uint32_t(n));put32(p+4,uint32_t(n>>32));}
inline void header(uint8_t* p,uint8_t op,uint16_t status,uint32_t id,const uint8_t* payload,unsigned n){__builtin_memcpy(p,"MGF1",4);p[4]=1;p[5]=op;p[6]=uint8_t(status);p[7]=uint8_t(status>>8);put32(p+8,id);put32(p+12,n);put32(p+16,checksum(payload,n));put32(p+20,checksum(p,20));}
// Physical stream adapter: one reader, bounded receive and transmit, no audio I/O.
struct Port {virtual ~Port(){};virtual void activity(uint64_t)=0;virtual void tick(uint64_t)=0;virtual bool framing()const=0;virtual unsigned wanted()const=0;virtual void feed(const uint8_t*,unsigned)=0;virtual const uint8_t* output(unsigned&)const=0;virtual void sent(unsigned)=0;virtual void disconnected()=0;};
// Consume an already available frame and its reply within one scheduler turn.
// CDC's ring stays bounded; the host still waits for each response. A zero read
// or backpressure returns immediately, and 16 operations cap a busy peer's work.
template<class Link,class Clock> void pump(Link& link,Port& port,Clock clock){
 uint8_t input[4096];
 for(unsigned pass=0;pass<16 && port.framing();++pass){
  unsigned n=0;const uint8_t* output=port.output(n);
  if(n){if(n>4096)n=4096;int sent=link.Write(output,n);if(sent<0){port.disconnected();return;}if(!sent)return;port.activity(clock());port.sent(unsigned(sent));}
  else {unsigned wanted=port.wanted();if(wanted>sizeof input)wanted=sizeof input;if(!wanted)return;int got=link.Read(input,wanted);if(got<0){port.disconnected();return;}if(!got)return;port.activity(clock());port.feed(input,unsigned(got));}
 }
}
class Service:public Port {
 UserFiles& files_;bool (*open_)(void*)=nullptr;void (*close_)(void*)=nullptr;void* owner_=nullptr;
 uint8_t input_[kHeader+kPayload]{},reply_[kHeader+kPayload]{};uint8_t staged_[32768]{};unsigned staged_bytes_=0;unsigned used_=0,need_=kHeader,out_=0,sent_=0;bool closing_=false;
 uint64_t activity_=0;bool active_=false;
 const char* text(unsigned offset,unsigned n)const{if(offset>=n || input_[kHeader+n-1])return nullptr;const char* p=reinterpret_cast<const char*>(input_+kHeader+offset);for(unsigned i=offset;i+1<n;++i)if(!input_[kHeader+i])return nullptr;return p;}
 void respond(uint16_t status,unsigned n=0){header(reply_,uint8_t(input_[5]|0x80),status,u32(input_+8),reply_+kHeader,n);out_=kHeader+n;sent_=0;used_=0;need_=kHeader;}
 void invalid(unsigned reason,uint32_t expected=0,uint32_t actual=0){auto* p=reply_+kHeader;put32(p,reason);put32(p+4,expected);put32(p+8,actual);put32(p+12,u32(input_+12));put64(p+16,files_.received()+staged_bytes_);respond(Invalid,24);}
 void execute(){auto* p=input_+kHeader;auto* out=reply_+kHeader;unsigned n=u32(input_+12),size=0;bool ok=false;uint8_t op=input_[5];
  if(input_[4]!=1){respond(Version);return;}
  if(input_[6] || input_[7]){invalid(4);return;}if(u32(input_+16)!=checksum(p,n)){invalid(2,u32(input_+16),checksum(p,n));if(active_)files_.cancel();staged_bytes_=0;return;}
  if(op==Open){if(n){respond(Invalid);return;}if(active_){respond(0);return;}active_=open_ && open_(owner_);respond(active_?0:uint16_t(UserFileError::Busy));return;}
  if(!active_){respond(uint16_t(UserFileError::Closed));return;}
  switch(op){
  case Close:if(!n){files_.cancel();closing_=true;respond(0);return;}respond(Invalid);return;
  case Cancel:if(!n){staged_bytes_=0;files_.cancel();respond(0);return;}respond(Invalid);return;
  case Begin:{if(files_.uploading()){respond(uint16_t(UserFileError::Busy));return;}staged_bytes_=0;const char* name=text(40,n);if(name)ok=files_.begin(name,u64(p),p+8);else {respond(Invalid);return;}break;}
  case Data:{if(n<=8 || n>4096 || !files_.uploading() || u64(p)!=files_.received()+staged_bytes_ || n-8>files_.length()-files_.received()-staged_bytes_){invalid(3);staged_bytes_=0;files_.cancel();return;}ok=true;for(unsigned at=8;at<n && ok;){unsigned count=n-at;if(count>sizeof staged_-staged_bytes_)count=sizeof staged_-staged_bytes_;__builtin_memcpy(staged_+staged_bytes_,p+at,count);staged_bytes_+=count;at+=count;if(staged_bytes_==sizeof staged_){ok=files_.chunk(files_.received(),staged_,staged_bytes_);staged_bytes_=0;}}break;}
  case Commit:if(!n){ok=!staged_bytes_ || files_.chunk(files_.received(),staged_,staged_bytes_);staged_bytes_=0;if(ok)ok=files_.commit();if(ok){size=unsigned(__builtin_strlen(files_.destination()))+1;__builtin_memcpy(out,files_.destination(),size);}}else {respond(Invalid);return;}break;
  case Delete:{const char* path=text(0,n);if(path)ok=files_.erase(path);else {respond(Invalid);return;}break;}
  case Get:{const char* path=text(12,n);if(!path){respond(Invalid);return;}unsigned wanted=u32(p+8);if(!wanted || wanted>32752){respond(Invalid);return;}unsigned got=0;uint64_t at=u64(p),length=files_.size(path);if(at>length){respond(Invalid);return;}ok=files_.get(path,at,out+16,wanted,got);if(ok){put64(out,length);put64(out+8,at);size=16+got;}break;}
  case List:{if(n!=5 || p[0]>=7){respond(Invalid);return;}auto* entries=alloc_array<UserFileEntry>(1024);if(!entries){respond(uint16_t(UserFileError::Io));return;}unsigned count=0,first=u32(p+1);ok=files_.list(UserShelf(p[0]),entries,1024,count);if(ok && first<=count){put32(out,count);size=8;unsigned i=first;for(;i<count;++i){unsigned length=unsigned(__builtin_strlen(entries[i].path));if(size+9+length>kPayload)break;put64(out+size,entries[i].bytes);out[size+8]=uint8_t(length);__builtin_memcpy(out+size+9,entries[i].path,length);size+=9+length;}put32(out+4,i);}else if(ok){ok=false;delete[] entries;respond(Invalid);return;}delete[] entries;break;}
  default:respond(Invalid);return;
  }
  respond(ok?0:uint16_t(files_.error()),ok?size:0);
 }
public:
 explicit Service(UserFiles& files):files_(files){}
 void callbacks(bool (*open)(void*),void (*close)(void*),void* owner){open_=open;close_=close;owner_=owner;}
 bool active()const{return active_;}
 void tick(uint64_t now)override{if(expired(now))disconnected();}
 // Caller must call activity on actual I/O, not on every poll.
 void activity(uint64_t now)override{activity_=now;}
 bool expired(uint64_t now)const{return (active_ || framing()) && activity_ && now-activity_>30000000;}
 bool framing()const override{return used_ || out_;}
 unsigned wanted()const override{return out_?0:need_-used_;}
 void feed(const uint8_t* bytes,unsigned n)override{if(out_ || n>wanted()){disconnected();return;}__builtin_memcpy(input_+used_,bytes,n);used_+=n;if(used_==kHeader && need_==kHeader){if(__builtin_memcmp(input_,"MGF1",4) || u32(input_+20)!=checksum(input_,20) || u32(input_+12)>4096){invalid(1,u32(input_+20),checksum(input_,20));return;}need_=kHeader+u32(input_+12);}if(used_==need_)execute();}
 const uint8_t* output(unsigned& n)const override{n=out_-sent_;return reply_+sent_;}
 void sent(unsigned n)override{if(n>out_-sent_)return;sent_+=n;if(sent_==out_){sent_=out_=0;if(closing_)disconnected();}}
 void disconnected()override{bool close=active_;active_=closing_=false;staged_bytes_=used_=out_=sent_=0;need_=kHeader;if(close){files_.close();if(close_)close_(owner_);}}
};
}
}
