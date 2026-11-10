#pragma once
#include "sha256.h"
#include "monocypher-ed25519.h"
namespace groove { namespace update {
constexpr unsigned HeaderSize=192,MaxImage=8*1024*1024,JournalSize=512;
enum class Phase:uint8_t { Stable, Installing, Ready, Trial, Booting, Recovery };
enum class Event:uint8_t { Installed=1, Trial, Confirmed, RolledBack, Rejected, BootFailed };
enum class Error:uint8_t { None, NotArmed, NoJournal, NoKey, Header, Signature, Version, Storage, Hash, Busy, BootUnsupported, Health };
inline const char* error_name(Error e){static const char* names[]={"OK","ENTER UPDATE MODE","NO VALID JOURNAL","NO TRUST KEY","INVALID HEADER","BAD SIGNATURE","OLD VERSION","STORAGE FAILURE","HASH MISMATCH","BUSY","TRYBOOT NOT APPROVED","HEALTH NOT CONFIRMED"};return names[unsigned(e)];}
struct History {uint32_t sequence=0,version=0;Event event=Event::Installed;uint8_t slot=0;};
struct Slot {uint32_t version=0,size=0;uint8_t hash[32]{};};
struct State {uint32_t sequence=0,highest=0;uint8_t active=0,pending=1,count=0;Phase phase=Phase::Stable;Slot slots[2]{};History history[16]{};};
// Storage implementation has no arbitrary path API. Only inactive image and two
// reserved journal sectors are writable. User assets and recovery are excluded.
class Storage {
public:
 virtual ~Storage(){}
 virtual bool read_journal(unsigned copy,uint8_t out[512])=0;
 virtual bool write_journal(unsigned copy,const uint8_t data[512])=0;
 virtual bool open_image(unsigned slot,bool write)=0;
 virtual bool write_image(const uint8_t* p,unsigned n)=0;
 virtual bool read_image(uint8_t* p,unsigned n,unsigned& got)=0;
 virtual bool close_image()=0; // flush durable data before success
};
class Rebooter {public:virtual ~Rebooter(){};virtual bool supported()const=0;virtual bool restart(unsigned partition,bool trial)=0;};
inline void encode_state(const State& s,uint8_t b[512]){
 for(unsigned i=0;i<512;++i)b[i]=0;
 b[0]='G';b[1]='J';b[2]='R';b[3]='1';put32(b+4,s.sequence);put32(b+8,s.highest);b[12]=s.active;b[13]=s.pending;b[14]=uint8_t(s.phase);b[15]=s.count;
 for(unsigned i=0;i<2;++i){unsigned o=16+i*40;put32(b+o,s.slots[i].version);put32(b+o+4,s.slots[i].size);for(unsigned j=0;j<32;++j)b[o+8+j]=s.slots[i].hash[j];}
 for(unsigned i=0;i<16;++i){unsigned o=96+i*12;put32(b+o,s.history[i].sequence);put32(b+o+4,s.history[i].version);b[o+8]=uint8_t(s.history[i].event);b[o+9]=s.history[i].slot;}
 Sha256::hash(b,480,b+480);
}
inline bool decode_state(const uint8_t b[512],State& s){
 uint8_t hash[32];Sha256::hash(b,480,hash);if(b[0]!='G'||b[1]!='J'||b[2]!='R'||b[3]!='1'||!equal(hash,b+480,32)||b[12]>1||b[13]>1||b[14]>uint8_t(Phase::Recovery)||b[15]>16)return false;
 s=State{};s.sequence=get32(b+4);s.highest=get32(b+8);s.active=b[12];s.pending=b[13];s.phase=Phase(b[14]);s.count=b[15];
 for(unsigned i=0;i<2;++i){unsigned o=16+i*40;s.slots[i].version=get32(b+o);s.slots[i].size=get32(b+o+4);if(s.slots[i].size>MaxImage)return false;for(unsigned j=0;j<32;++j)s.slots[i].hash[j]=b[o+8+j];}
 for(unsigned i=0;i<16;++i){unsigned o=96+i*12;s.history[i]={get32(b+o),get32(b+o+4),Event(b[o+8]),b[o+9]};}return true;
}
class Manager {
 Storage& storage_;Rebooter& reboot_;uint8_t key_[32]{};State state_{};bool loaded_=false,armed_=false,receiving_=false;Error error_=Error::None;uint8_t header_[HeaderSize]{};unsigned received_=0;Sha256 hash_;
 bool fail(Error e){error_=e;return false;}
 void record(Event e,unsigned slot,uint32_t version){if(state_.count==16){for(unsigned i=1;i<16;++i)state_.history[i-1]=state_.history[i];--state_.count;}state_.history[state_.count++]={state_.sequence+1,version,e,uint8_t(slot)};}
 bool persist(){if(state_.sequence==0xffffffff)return fail(Error::Storage);++state_.sequence;uint8_t b[512],check[512];encode_state(state_,b);unsigned copy=state_.sequence&1;if(!storage_.write_journal(copy,b)||!storage_.read_journal(copy,check)||!equal(b,check,512)){load();return fail(Error::Storage);}return true;}
 bool image_hash(unsigned slot,uint32_t size,const uint8_t expected[32]){if(!storage_.open_image(slot,false))return false;Sha256 h;uint8_t buffer[4096];unsigned total=0,got=0;bool ok=true;while(total<size){unsigned n=size-total;if(n>sizeof buffer)n=sizeof buffer;if(!storage_.read_image(buffer,n,got)||got!=n){ok=false;break;}h.add(buffer,n);total+=n;}if(ok){ok=storage_.read_image(buffer,1,got)&&got==0;}if(!storage_.close_image())ok=false;uint8_t out[32];h.finish(out);return ok&&equal(out,expected,32);}
public:
 Manager(Storage& storage,Rebooter& reboot,const uint8_t public_key[32]):storage_(storage),reboot_(reboot){for(unsigned i=0;i<32;++i)key_[i]=public_key[i];}
 const State& state()const{return state_;}Error error()const{return error_;}unsigned received()const{return received_;}unsigned total()const{return get32(header_+16);}bool receiving()const{return receiving_;}bool loaded()const{return loaded_;}bool armed()const{return armed_;}
 bool load(){State a,b;uint8_t bytes[512];bool va=storage_.read_journal(0,bytes)&&decode_state(bytes,a),vb=storage_.read_journal(1,bytes)&&decode_state(bytes,b);if(!va&&!vb){loaded_=false;return fail(Error::NoJournal);}state_=!vb||(va&&a.sequence>b.sequence)?a:b;loaded_=true;error_=Error::None;return true;}
 bool rejected_trial(unsigned partition)const{return loaded_ && state_.phase==Phase::Trial && error_==Error::Health && partition>=2 && partition<=3;}
 void arm(bool on){if(!on&&receiving_)abort();armed_=on;}
 bool begin(const uint8_t header[HeaderSize]){
  error_=Error::None;if(!armed_)return fail(Error::NotArmed);if(!loaded_)return fail(Error::NoJournal);if(receiving_||state_.phase==Phase::Trial||state_.phase==Phase::Booting)return fail(Error::Busy);
  uint8_t nonzero=0;for(auto b:key_)nonzero|=b;if(!nonzero)return fail(Error::NoKey);
  if(header[0]!='G'||header[1]!='U'||header[2]!='P'||header[3]!='1'||get32(header+4)!=1||get32(header+8)!=0x34414152||get32(header+20)!=1||!get32(header+16)||get32(header+16)>MaxImage)return fail(Error::Header);
  for(unsigned i=104;i<128;++i)if(header[i])return fail(Error::Header);
  uint8_t fingerprint[32];Sha256::hash(key_,32,fingerprint);if(!equal(fingerprint,header+56,16)||crypto_ed25519_check(header+128,key_,header,128)!=0)return fail(Error::Signature);
  if(get32(header+12)<=state_.highest)return fail(Error::Version);
  for(unsigned i=0;i<HeaderSize;++i)header_[i]=header[i];
  state_.pending=1-state_.active;state_.slots[state_.pending]=Slot{};state_.phase=Phase::Installing;if(!persist())return false;
  if(!storage_.open_image(state_.pending,true))return fail(Error::Storage);
  receiving_=true;received_=0;hash_=Sha256{};return true;
 }
 bool append(const uint8_t* p,unsigned n){if(!receiving_)return fail(Error::Busy);if(!n||n>4096||n>total()-received_)return fail(Error::Header);if(!storage_.write_image(p,n)){abort();return fail(Error::Storage);}hash_.add(p,n);received_+=n;return true;}
 bool finish(){
  if(!receiving_||received_!=total())return fail(Error::Header);
  receiving_=false;bool closed=storage_.close_image();uint8_t digest[32];hash_.finish(digest);if(!closed)return fail(Error::Storage);if(!equal(digest,header_+24,32))return fail(Error::Hash);
  if(!image_hash(state_.pending,total(),digest))return fail(Error::Hash);
  auto& slot=state_.slots[state_.pending];slot.version=get32(header_+12);slot.size=total();for(unsigned i=0;i<32;++i)slot.hash[i]=digest[i];state_.phase=Phase::Ready;record(Event::Installed,state_.pending,slot.version);return persist();
 }
 void abort(){if(receiving_)storage_.close_image();receiving_=false;error_=Error::Storage;}
 bool cancel(){if(!armed_||!loaded_)return fail(Error::NotArmed);if(receiving_)abort();if(state_.phase==Phase::Ready||state_.phase==Phase::Installing||state_.phase==Phase::Trial){record(Event::RolledBack,state_.pending,state_.slots[state_.pending].version);state_.phase=Phase::Stable;if(!persist())return false;}error_=Error::None;return true;}
 bool trial(){error_=Error::None;if(!armed_||!loaded_||state_.phase!=Phase::Ready)return fail(Error::Busy);if(!reboot_.supported())return fail(Error::BootUnsupported);if(!image_hash(state_.pending,state_.slots[state_.pending].size,state_.slots[state_.pending].hash))return fail(Error::Hash);state_.phase=Phase::Trial;record(Event::Trial,state_.pending,state_.slots[state_.pending].version);if(!persist())return false;if(reboot_.restart(unsigned(state_.pending)+2,true))return true;state_.phase=Phase::Ready;if(!persist())return false;return fail(Error::BootUnsupported);}
 // Recovery always boots from partition 1. Its journal decides a bounded next
 // handoff. A failed candidate never replaces the confirmed active slot.
 bool on_boot(unsigned partition,bool tryboot,uint32_t running_version){
  if(!loaded_)return false;
  if(partition==1){if(state_.phase==Phase::Trial||state_.phase==Phase::Installing){record(Event::RolledBack,state_.pending,state_.slots[state_.pending].version);state_.phase=Phase::Stable;return persist();}if(state_.phase==Phase::Booting){record(Event::BootFailed,state_.active,state_.slots[state_.active].version);state_.phase=Phase::Recovery;return persist();}return true;}
  if(state_.phase==Phase::Trial)return partition==unsigned(state_.pending)+2&&running_version==state_.slots[state_.pending].version?true:fail(Error::Health);
  if(state_.phase==Phase::Booting&&partition==unsigned(state_.active)+2&&!tryboot){state_.phase=Phase::Stable;return persist();}return fail(Error::Health);
 }
 bool confirm(unsigned partition,bool trial_identity,bool healthy){error_=Error::None;if(!armed_||!loaded_||!healthy||!trial_identity||partition!=unsigned(state_.pending)+2||state_.phase!=Phase::Trial)return fail(Error::Health);state_.active=state_.pending;state_.highest=state_.slots[state_.active].version;state_.phase=Phase::Stable;record(Event::Confirmed,state_.active,state_.highest);return persist();}
 bool rollback_confirmed(){if(!armed_||!loaded_||(state_.phase!=Phase::Stable&&state_.phase!=Phase::Recovery))return fail(Error::Busy);unsigned previous=1-state_.active;const auto& slot=state_.slots[previous];if(!slot.version||slot.version>state_.highest||!image_hash(previous,slot.size,slot.hash))return fail(Error::Hash);state_.active=uint8_t(previous);state_.pending=uint8_t(1-previous);state_.phase=Phase::Stable;record(Event::RolledBack,previous,slot.version);error_=Error::None;return persist();}
 bool boot_active(){error_=Error::None;if(!armed_||!loaded_||(state_.phase!=Phase::Stable&&state_.phase!=Phase::Recovery)||!state_.slots[state_.active].size)return fail(Error::Busy);if(!image_hash(state_.active,state_.slots[state_.active].size,state_.slots[state_.active].hash))return fail(Error::Hash);state_.phase=Phase::Booting;if(!persist())return false;if(reboot_.restart(unsigned(state_.active)+2,false))return true;state_.phase=Phase::Stable;if(!persist())return false;return fail(Error::BootUnsupported);}
};
}}
