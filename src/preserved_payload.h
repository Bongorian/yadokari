#pragma once
#include <stdint.h>
#include <stddef.h>
#include "alloc_policy.h"
#include "project.h"
namespace groove {
using PreservedCopy=bool (*)(void*,void*,const void*,size_t);
// Sidecar storage for the MGXC unknown-engine / unknown-chunk fallback
// (docs/project-format.md, SPEC section 2.9). Preserved payloads live
// here, never inside ProjectState, which must stay trivially copyable
// for the async decode slot. All buffers are owned and released with
// clear()/steal semantics; allocation follows alloc_policy.h.
struct PreservedBytes {
 uint8_t* data=nullptr;
 uint32_t size=0;
 void clear(){alloc_delete_array(data);size=0;}
 // Replace contents; on allocation/copy failure the old contents are kept
 // and false is returned.
 bool set(const uint8_t* source,uint32_t count,PreservedCopy copy=nullptr,void* context=nullptr){
  if(!count){clear();return true;}
  uint8_t* next=alloc_uninitialized_bytes(count);
  if(!next)return false;
  if(copy){if(!copy(context,next,source,count)){delete[] next;return false;}}
  else __builtin_memcpy(next,source,count);
  clear();data=next;size=count;return true;
 }
 void steal_from(PreservedBytes& other){clear();data=other.data;size=other.size;other.data=nullptr;other.size=0;}
 bool empty()const{return size==0;}
};
struct PreservedTrackEntry {
 PreservedBytes engn,extra_sub,auxiliary,opaque,preset_root,preset_track,preset_sound;
 int32_t extra_for=-1;
 uint32_t cookie=0; // runtime association only; not part of any MGXC payload
 void clear(){engn.clear();extra_sub.clear();auxiliary.clear();opaque.clear();preset_root.clear();preset_track.clear();preset_sound.clear();extra_for=-1;cookie=0;}
 void steal_from(PreservedTrackEntry& other){
  engn.steal_from(other.engn);extra_sub.steal_from(other.extra_sub);auxiliary.steal_from(other.auxiliary);opaque.steal_from(other.opaque);preset_root.steal_from(other.preset_root);preset_track.steal_from(other.preset_track);preset_sound.steal_from(other.preset_sound);extra_for=other.extra_for;cookie=other.cookie;other.extra_for=-1;other.cookie=0;
 }
 bool copy_from(const PreservedTrackEntry& other,PreservedCopy copy=nullptr,void* context=nullptr){
  PreservedTrackEntry temp;
  bool ok=temp.engn.set(other.engn.data,other.engn.size,copy,context) && temp.extra_sub.set(other.extra_sub.data,other.extra_sub.size,copy,context) && temp.auxiliary.set(other.auxiliary.data,other.auxiliary.size,copy,context) && temp.opaque.set(other.opaque.data,other.opaque.size,copy,context) && temp.preset_root.set(other.preset_root.data,other.preset_root.size,copy,context) && temp.preset_track.set(other.preset_track.data,other.preset_track.size,copy,context) && temp.preset_sound.set(other.preset_sound.data,other.preset_sound.size,copy,context);
  if(ok){temp.extra_for=other.extra_for;temp.cookie=other.cookie;steal_from(temp);}temp.clear();return ok;
 }
 bool copy_definition_from(const PreservedTrackEntry& other){
  PreservedTrackEntry temp;if(!temp.copy_from(other))return false;
  temp.auxiliary.clear();temp.auxiliary.steal_from(auxiliary);steal_from(temp);return true;
 }
 void clear_definition(){engn.clear();extra_sub.clear();opaque.clear();preset_root.clear();preset_track.clear();preset_sound.clear();extra_for=-1;cookie=0;}
 uint32_t bytes()const{return engn.size+extra_sub.size+auxiliary.size+opaque.size+preset_root.size+preset_track.size+preset_sound.size;}
 bool empty()const{return !bytes();}
};
inline uint32_t next_definition_cookie(){static uint32_t serial=0;uint32_t value=__atomic_load_n(&serial,__ATOMIC_RELAXED);while(value!=0xffffffffu){uint32_t next=value+1;if(__atomic_compare_exchange_n(&serial,&value,next,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED))return next;}return 0;}
inline bool same_definition_extensions(const PreservedTrackEntry& a,const PreservedTrackEntry& b){
 auto eq=[](const PreservedBytes& x,const PreservedBytes& y){return x.size==y.size && (!x.size || !__builtin_memcmp(x.data,y.data,x.size));};
 return a.extra_for==b.extra_for && eq(a.opaque,b.opaque) && eq(a.engn,b.engn) && eq(a.extra_sub,b.extra_sub) && eq(a.preset_root,b.preset_root) && eq(a.preset_track,b.preset_track) && eq(a.preset_sound,b.preset_sound);
}
// Immutable, file-owner-only records. Undo and pool replacement carry cookies,
// not mutable slot ownership. DSP copies integers and never touches this cache.
class DefinitionExtensions {
 struct Node {uint32_t cookie=0;PreservedTrackEntry value;Node* next=nullptr;};
 Node* first_=nullptr;unsigned count_=0;uint32_t bytes_=0;
public:
 void clear(){while(first_){auto* old=first_;first_=old->next;old->value.clear();delete old;}count_=0;bytes_=0;}
 DefinitionExtensions()=default;~DefinitionExtensions(){clear();}
 DefinitionExtensions(const DefinitionExtensions&)=delete;DefinitionExtensions& operator=(const DefinitionExtensions&)=delete;
 void steal_from(DefinitionExtensions& other){clear();first_=other.first_;count_=other.count_;bytes_=other.bytes_;other.first_=nullptr;other.count_=0;other.bytes_=0;}
 const PreservedTrackEntry* find(uint32_t cookie)const{for(auto* n=first_;n;n=n->next)if(n->cookie==cookie)return &n->value;return nullptr;}
 bool retain(PreservedTrackEntry& value,uint32_t& cookie,bool* inserted=nullptr){
  if(inserted)*inserted=false;
  if(value.empty()){cookie=0;return true;}
  if(cookie){const auto* old=find(cookie);if(old)return same_definition_extensions(*old,value);}
  for(auto* n=first_;!cookie && n;n=n->next)if(same_definition_extensions(n->value,value)){cookie=n->cookie;return true;}
  if(count_>=2048 || uint64_t(bytes_)+value.bytes()>(8u<<20))return false;
  auto* n=alloc_new<Node>();if(!n)return false;if(!n->value.copy_from(value)){delete n;return false;}
  if(!cookie)cookie=next_definition_cookie();
  if(!cookie){n->value.clear();delete n;return false;}
  n->cookie=cookie;n->value.cookie=cookie;n->next=first_;first_=n;++count_;bytes_+=value.bytes();if(inserted)*inserted=true;return true;
 }
 // Only a newly retained, never-published record may be discarded by a caller.
 void discard_unpublished(uint32_t cookie){Node** link=&first_;while(*link){auto* n=*link;if(n->cookie==cookie){*link=n->next;bytes_-=n->value.bytes();--count_;n->value.clear();delete n;return;}link=&n->next;}}
 bool project(PreservedTrackEntry& out,uint32_t cookie)const{if(!cookie){out.clear_definition();return true;}const auto* value=find(cookie);return value && out.copy_definition_from(*value);}
};
struct PreservedChunk {
 uint32_t kind=0;
 uint16_t version=0;
 uint16_t flags=0;
 PreservedBytes payload;
 void clear(){kind=0;version=0;flags=0;payload.clear();}
 void steal_from(PreservedChunk& other){clear();kind=other.kind;version=other.version;flags=other.flags;payload.steal_from(other.payload);}
 bool copy_from(const PreservedChunk& other,PreservedCopy copier=nullptr,void* context=nullptr){
  PreservedBytes copy;if(!other.payload.empty() && !copy.set(other.payload.data,other.payload.size,copier,context))return false;
  clear();kind=other.kind;version=other.version;flags=other.flags;payload.steal_from(copy);return true;
 }
};
// File-scoped hierarchy extensions do not belong to a track's sound identity.
struct PreservedPattern {
 PreservedBytes children,phrases[kTracks];
 ~PreservedPattern(){clear();}
 void clear(){children.clear();for(auto& p:phrases)p.clear();}
 uint32_t bytes()const{uint32_t n=children.size;for(const auto& p:phrases)n+=p.size;return n;}
 void steal_from(PreservedPattern& other){children.steal_from(other.children);for(unsigned i=0;i<kTracks;++i)phrases[i].steal_from(other.phrases[i]);}
 bool copy_from(const PreservedPattern& other,PreservedCopy copy=nullptr,void* context=nullptr){PreservedPattern temp;if(!temp.children.set(other.children.data,other.children.size,copy,context))return false;for(unsigned i=0;i<kTracks;++i)if(!temp.phrases[i].set(other.phrases[i].data,other.phrases[i].size,copy,context))return false;steal_from(temp);return true;}
};
class PreservedStore {
public:
 static constexpr uint32_t kMaxEntryBytes=2u<<20;   // per payload/chunk
 static constexpr uint32_t kMaxTotalBytes=8u<<20;   // per store
 static constexpr unsigned kMaxChunks=64;
 PreservedStore()=default;
 ~PreservedStore(){clear();}
 PreservedStore(const PreservedStore&)=delete;
 PreservedStore& operator=(const PreservedStore&)=delete;
 PreservedTrackEntry tracks[kTracks];
 PreservedTrackEntry sounds[kPresetSlots];
 PreservedChunk chunks[kMaxChunks];
 PreservedBytes trks,snds;PreservedPattern patterns[kPatterns];
 uint32_t container_bytes()const{uint32_t n=trks.size+snds.size;for(const auto& p:patterns)n+=p.bytes();return n;}
 unsigned chunk_count=0;
 // Set when a payload exceeded the preservation caps and was skipped:
 // a save from this store loses that data (SPEC 2.9 clause 5 hook).
 bool dropped_unknown=false;
 void clear(){
  for(auto& entry:tracks)entry.clear();
  for(auto& entry:sounds)entry.clear();
  for(unsigned i=0;i<chunk_count;++i)chunks[i].clear();
  trks.clear();snds.clear();for(auto& p:patterns)p.clear();
  chunk_count=0;dropped_unknown=false;
 }
 void clear_track(unsigned track){if(track<kTracks)tracks[track].clear();}
 void clear_sound(unsigned index){if(index<kPresetSlots)sounds[index].clear();}
 uint32_t used_bytes()const{
  uint32_t total=container_bytes();
  for(const auto& entry:tracks)total+=entry.bytes();
  for(const auto& entry:sounds)total+=entry.bytes();
  for(unsigned i=0;i<chunk_count;++i)total+=chunks[i].payload.size;
  return total;
 }
 // Append an unknown top-level chunk. False (and dropped_unknown set)
 // when a cap rejects it; the caller still skips the chunk in the
 // stream, so decoding never fails over preservation.
 bool add_chunk(uint32_t kind,uint16_t version,uint16_t flags,const uint8_t* payload,uint32_t size){
  if(size>kMaxEntryBytes || chunk_count>=kMaxChunks || used_bytes()+size>kMaxTotalBytes){dropped_unknown=true;return false;}
  PreservedChunk& chunk=chunks[chunk_count];
  chunk.clear();chunk.kind=kind;chunk.version=version;chunk.flags=flags;
  if(!chunk.payload.set(payload,size)){dropped_unknown=true;return false;}
  ++chunk_count;return true;
 }
 void steal_from(PreservedStore& other){
  clear();
  for(unsigned t=0;t<kTracks;++t)tracks[t].steal_from(other.tracks[t]);
  for(unsigned i=0;i<kPresetSlots;++i)sounds[i].steal_from(other.sounds[i]);
  trks.steal_from(other.trks);snds.steal_from(other.snds);for(unsigned i=0;i<kPatterns;++i)patterns[i].steal_from(other.patterns[i]);
  chunk_count=other.chunk_count;
  for(unsigned i=0;i<chunk_count;++i)chunks[i].steal_from(other.chunks[i]);
  dropped_unknown=other.dropped_unknown;
  other.chunk_count=0;other.dropped_unknown=false;
 }
 bool copy_from(const PreservedStore& other,PreservedCopy copy=nullptr,void* context=nullptr){
  PreservedStore temp;
  if(!temp.trks.set(other.trks.data,other.trks.size,copy,context) || !temp.snds.set(other.snds.data,other.snds.size,copy,context))return false;
  for(unsigned i=0;i<kPatterns;++i)if(!temp.patterns[i].copy_from(other.patterns[i],copy,context))return false;
  for(unsigned t=0;t<kTracks;++t)if(!temp.tracks[t].copy_from(other.tracks[t],copy,context))return false;
  for(unsigned i=0;i<kPresetSlots;++i)if(!temp.sounds[i].copy_from(other.sounds[i],copy,context))return false;
  // Count each owned chunk immediately, so a later allocation/copy failure
  // releases the already-copied chunks through temp's destructor.
  for(unsigned i=0;i<other.chunk_count;++i){if(!temp.chunks[i].copy_from(other.chunks[i],copy,context))return false;++temp.chunk_count;}
  temp.dropped_unknown=other.dropped_unknown;
  steal_from(temp);return true;
 }
};
// Evaluate the complete save graph before copying payloads or publishing a mount.
// Repeated references are counted because every emitted TRCK/SND owns its bytes.
inline bool definition_projection_fits(const DefinitionExtensions& owners,const ProjectState& p,const PreservedStore& store,unsigned target=kTracks,unsigned slot=kPresetSlots,uint32_t replacement=0){
 if(store.dropped_unknown)return false;
 uint64_t total=store.container_bytes();for(unsigned i=0;i<store.chunk_count;++i)total+=store.chunks[i].payload.size;
 auto add=[&](uint32_t cookie){if(!cookie)return true;const auto* node=owners.find(cookie);if(!node)return false;total+=node->bytes();return total<=PreservedStore::kMaxTotalBytes;};
 for(unsigned t=0;t<kTracks;++t){total+=store.tracks[t].auxiliary.size;if(!add(t==target?replacement:p.tracks[t].definition_cookie))return false;}
 for(unsigned i=0;i<kPresetSlots;++i)if(!add(i==slot?replacement:p.sounds[i].definition_cookie))return false;
 return total<=PreservedStore::kMaxTotalBytes;
}
// File/worker scope only. A full sidecar table must not sit on Circle's caller stack.
class PreservedOwner {
 PreservedStore* value_=nullptr;
public:
 PreservedOwner():value_(alloc_new<PreservedStore>()){}~PreservedOwner(){delete value_;}
 PreservedOwner(const PreservedOwner&)=delete;PreservedOwner& operator=(const PreservedOwner&)=delete;
 explicit operator bool()const{return value_!=nullptr;}
 PreservedStore& operator*()const{return *value_;}PreservedStore* operator->()const{return value_;}PreservedStore* get()const{return value_;}
};

}
