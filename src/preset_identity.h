#pragma once
#include <stdint.h>
namespace groove {
// Opaque 128-bit IDs. The byte order is the printed hexadecimal order.
struct PresetGuid {
 uint8_t bytes[16]{};
 bool empty()const{for(auto b:bytes)if(b)return false;return true;}
};
inline bool operator==(const PresetGuid& a,const PresetGuid& b){return !__builtin_memcmp(a.bytes,b.bytes,16);}
inline bool operator!=(const PresetGuid& a,const PresetGuid& b){return !(a==b);}
enum class PresetOrigin:uint8_t {User=0,Factory=1};
struct PresetIdentity {
 PresetGuid guid{};uint32_t revision=0;PresetOrigin origin=PresetOrigin::User;
 PresetGuid parent{};uint32_t parent_revision=0;
 bool assigned()const{return !guid.empty() && revision;}
 bool valid()const{
  if(unsigned(origin)>1 || parent.empty()!=(parent_revision==0))return false;
  if(guid.empty())return !revision && origin==PresetOrigin::User && parent.empty();
  if(!revision || (origin==PresetOrigin::Factory && !parent.empty()))return false;
  return parent!=guid || parent_revision<revision;
 }
};
inline bool same_preset_revision(const PresetIdentity& a,const PresetIdentity& b){return a.assigned() && b.assigned() && a.guid==b.guid && a.revision==b.revision;}
inline bool same_preset_identity(const PresetIdentity& a,const PresetIdentity& b){return a.guid==b.guid && a.revision==b.revision && a.origin==b.origin && a.parent==b.parent && a.parent_revision==b.parent_revision;}
inline void preset_guid_text(const PresetGuid& id,char (&out)[33]){const char* hex="0123456789abcdef";for(unsigned i=0;i<16;++i){out[2*i]=hex[id.bytes[i]>>4];out[2*i+1]=hex[id.bytes[i]&15];}out[32]=0;}
inline bool parse_preset_guid(const char* text,PresetGuid& out){PresetGuid next;for(unsigned i=0;i<32;++i){unsigned char c=text[i];if(!c)return false;unsigned v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(v==16)return false;next.bytes[i/2]|=uint8_t(v<<((i&1)?0:4));}if(text[32])return false;out=next;return true;}
// Reserved immutable built-in IDs, allocated once by source key. The f0 42 55
// family is disjoint from imported-master IDs recorded in identities.json.
inline PresetIdentity builtin_factory_identity(uint32_t key){PresetIdentity id;id.guid.bytes[0]=0xf0;id.guid.bytes[1]=0x42;id.guid.bytes[2]=0x55;id.guid.bytes[3]=0x49;id.guid.bytes[4]=0x4c;id.guid.bytes[5]=0x54;for(unsigned i=0;i<4;++i)id.guid.bytes[12+i]=uint8_t(key>>(24-8*i));id.revision=1;id.origin=PresetOrigin::Factory;return id;}
// Seeded by the file/platform owner once; use on the UI producer only. No I/O
// in an edit or DSP render. The 64-bit counter is never allowed to wrap.
class PresetGuidIssuer {
 PresetGuid seed_{};uint64_t counter_=0;bool ready_=false;
public:
 void seed(const uint8_t (&bytes)[16]){__builtin_memcpy(seed_.bytes,bytes,16);counter_=0;ready_=true;}
 bool ready()const{return ready_;}
 bool issue(PresetIdentity& out,const PresetIdentity* parent=nullptr){if(!ready_ || counter_==~uint64_t(0))return false;PresetIdentity id;id.guid=seed_;id.guid.bytes[0]=0x80;uint64_t n=++counter_;for(unsigned i=0;i<8;++i)id.guid.bytes[8+i]^=uint8_t(n>>(8*i));id.revision=1;if(parent && parent->assigned()){id.parent=parent->guid;id.parent_revision=parent->revision;}out=id;return true;}
};

}
