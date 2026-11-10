#include "project_codec.h"
#include "update/sha256.h"
#include "review_wave2_writer_fixtures.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>
#include <cstdio>
using namespace groove;
static uint16_t u16(const uint8_t* p){return uint16_t(p[0]|unsigned(p[1])<<8);}
static uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|uint32_t(u16(p+2))<<16;}
struct Input {const uint8_t* data;size_t n,at=0;bool read(uint8_t* p,size_t size){if(size>n-at)return false;std::memcpy(p,data+at,size);at+=size;return true;}bool at_end()const{return at==n;}};
struct Node {uint32_t kind;uint16_t version,flags;std::vector<uint8_t> prefix;std::vector<Node> children;bool branch=false;};
static std::vector<Node> parse(const uint8_t* data,unsigned size){std::vector<Node> out;for(unsigned at=0;at<size;){assert(size-at>=12);unsigned n=u32(data+at+8);assert(n<=size-at-12);Node node{u32(data+at),u16(data+at+4),u16(data+at+6),{}, {},false};auto* p=data+at+12;unsigned pre=0;switch(node.kind){case kChunkTrks:case kChunkSnds:node.branch=true;break;case kChunkTrck:case kChunkPhrs:node.branch=true;pre=1;break;case kChunkPtrn:case kChunkSnd:node.branch=true;pre=2;break;case kChunkEngn:node.branch=true;pre=3+p[0];break;default:break;}assert(pre<=n);node.prefix.assign(p,p+(node.branch?pre:n));if(node.branch)node.children=parse(p+pre,n-pre);out.push_back(std::move(node));at+=12+n;}return out;}
static void emit(ChunkBuffer& out,const std::vector<Node>& nodes){ChunkWriter<ChunkBuffer> w(out);for(const auto& n:nodes){uint32_t frame;assert(out.begin_frame(n.kind,n.version,frame,n.flags) && w.block(n.prefix.data(),n.prefix.size()));emit(out,n.children);assert(out.end_frame(frame));}}
static std::vector<Node> tree(const ChunkBuffer& b){return parse(b.data+10,b.size-14);}
static Node& node(std::vector<Node>& n,uint32_t kind){for(auto& child:n)if(child.kind==kind)return child;assert(false);return n[0];}
static bool decode(const std::vector<Node>& nodes,ProjectState& p,PreservedStore* store=nullptr){ChunkBuffer b;const uint8_t h[10]={'M','G','X','C',13,0,0,0,0,0};assert(b.write(h,10));emit(b,nodes);b.patch_u32(6,b.size-10);ProjectCrc crc;for(unsigned i=4;i<b.size;++i)crc.add(b.data[i]);ChunkWriter<ChunkBuffer> w(b);assert(w.u32(crc.value()));Input input{b.data,b.size};return decode_project(input,p,store);}
static bool equal(const PreservedBytes& a,const PreservedBytes& b){return a.size==b.size && (!a.size || !std::memcmp(a.data,b.data,a.size));}
int main(){
 auto p=std::make_unique<ProjectState>(),q=std::make_unique<ProjectState>();review_writer_fixture(*p,0);
 // SHA256 from v557/1ee9e4a3's encode_project_buffer, not recaptured
 // from the implementation under test. Covers Song, Preset and Kit bytes.
 const char* hashes[]={"af856d4cf502bbba8e63c41604db87747f2140b05f4ab07d2525ee6c893b16ed","1d733059f2d13d90afca8b069db31314e71159f5341e1174fdc3ec591a2c9acc","171f5508f1facea12358788f53de11cb5a9eb27256c6de263d96c808a4eba38c"};const unsigned sizes[]={364001,7631,13179};
 for(unsigned kind=0;kind<3;++kind){review_writer_fixture(*q,kind);ChunkBuffer data;assert(encode_project_buffer(data,*q) && data.size==sizes[kind]);uint8_t digest[32];update::Sha256::hash(data.data,data.size,digest);char hex[65];for(unsigned i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",digest[i]);assert(!std::strcmp(hex,hashes[kind]));}
 ChunkBuffer original;assert(encode_project_buffer(original,*p));Input input{original.data,original.size};PreservedStore empty;assert(decode_project(input,*q,&empty));ChunkBuffer round;assert(encode_project_buffer(round,*q,&empty));assert(original.size==round.size && !std::memcmp(original.data,round.data,round.size));
 // Every newly covered scope owns a complete future TLV (version and flags too).
 PreservedStore foreign,read,copy;ChunkBuffer unknown;uint32_t frame;assert(unknown.begin_frame(0x32545845,7,frame,13) && unknown.write(reinterpret_cast<const uint8_t*>("future"),6) && unknown.end_frame(frame));
 for(auto* target:{&foreign.trks,&foreign.snds,&foreign.patterns[0].children,&foreign.patterns[0].phrases[0]}){assert(target->set(unknown.data,unknown.size));}assert(foreign.patterns[1].copy_from(foreign.patterns[0]));
 ChunkBuffer extended;assert(encode_project_buffer(extended,*p,&foreign));Input ext{extended.data,extended.size};assert(decode_project(ext,*q,&read) && !read.dropped_unknown);assert(equal(read.trks,foreign.trks) && equal(read.snds,foreign.snds));assert(equal(read.patterns[1].children,foreign.patterns[1].children) && equal(read.patterns[1].phrases[0],foreign.patterns[1].phrases[0]));assert(copy.copy_from(read) && copy.container_bytes()==read.container_bytes());PreservedStore moved;moved.steal_from(copy);assert(!copy.container_bytes() && moved.container_bytes()==read.container_bytes());
 ChunkBuffer again;assert(encode_project_buffer(again,*q,&read) && extended.size==again.size && !std::memcmp(extended.data,again.data,again.size));
 // Copy failure must leave the old hierarchy intact.
 struct Fault {unsigned calls=0;static bool copy(void* context,void* out,const void* in,size_t n){auto& self=*static_cast<Fault*>(context);if(++self.calls==3)return false;std::memcpy(out,in,n);return true;}} fault;
 assert(copy.trks.set(unknown.data,unknown.size));assert(!copy.copy_from(read,Fault::copy,&fault));assert(copy.trks.size==unknown.size && !copy.snds.size && copy.container_bytes()==unknown.size);
 // A compact PREF fan-out cannot amplify opaque data past the store budget.
 auto capped=tree(original);for(auto it=capped.begin();it!=capped.end();){if(it->kind==kChunkPref || (it->kind==kChunkPtrn && it->prefix[0]!=0))it=capped.erase(it);else ++it;}
 node(capped,kChunkPtrn).children.push_back(Node{0x32545845,7,13,std::vector<uint8_t>(PreservedStore::kMaxEntryBytes-12,0x5a),{},false});for(unsigned i=1;i<kPatterns;++i)capped.push_back(Node{kChunkPref,1,0,{uint8_t(i),0},{},false});PreservedStore limited;assert(decode(capped,*q,&limited) && limited.dropped_unknown && limited.used_bytes()<=PreservedStore::kMaxTotalBytes);
 // Required masks apply to every registered engine, for both track and sound.
 unsigned checked=0;auto sound=std::make_unique<SoundState>();
 for(unsigned id=0;id<instrument_count();++id){const auto* d=engine_descriptor(id);for(unsigned missing=0;missing<d->state.count;++missing)if(d->state.required&(1u<<missing)){
  *sound=SoundState{};sound->instrument=Instrument(id);ChunkBuffer payload;assert(write_sound_engn(payload,*sound,nullptr));unsigned pre=payload.data[0]+3;auto parts=parse(payload.data+pre,payload.size-pre);for(auto it=parts.begin();it!=parts.end();++it)if(it->kind==d->state.chunks[missing].fourcc){parts.erase(it);break;}ChunkBuffer broken;assert(broken.write(payload.data,pre));emit(broken,parts);ChunkSource src{broken.data,broken.size,0};PreservedStore keep;assert(parse_sound_engn(src,*sound,&keep,&keep.sounds[0]) && sound->instrument==Instrument::Unknown && keep.sounds[0].engn.size==broken.size);
  auto track=track_at(*q,0,0);ChunkSource track_src{broken.data,broken.size,0};assert(parse_track_engn(track_src,track,&keep,&keep.tracks[0]) && track.instrument==Instrument::Unknown);++checked;
 }}assert(checked>20);
 auto base=tree(original);
 auto& engn=node(node(node(base,kChunkTrks).children,kChunkTrck).children,kChunkEngn);assert(!engn.children.empty());auto known=engn.children.back();engn.children.push_back(known);assert(decode(base,*q) && q->tracks[0].instrument==Instrument::Unknown);
 base=tree(original);node(node(node(base,kChunkTrks).children,kChunkTrck).children,kChunkEngn).children.back().flags=1;assert(decode(base,*q) && q->tracks[0].instrument==Instrument::Unknown);
 base=tree(original);node(node(node(base,kChunkSnds).children,kChunkSnd).children,kChunkEngn).children.back().flags=1;assert(decode(base,*q) && q->sounds[0].instrument==Instrument::Unknown);
 // Optional name sections must not use last-wins either.
 base=tree(original);auto& named=node(node(node(base,kChunkTrks).children,kChunkTrck).children,kChunkEngn);Node name{kChunkName,1,0,{'A'},{},false};named.children.push_back(name);named.children.push_back(name);assert(decode(base,*q) && q->tracks[0].instrument==Instrument::Unknown);
 // All interpreted container/state flags and singleton duplicates reject.
 for(uint32_t target:{kChunkTcom,kChunkPseq,kChunkKseq,kChunkClfa,kChunkPmut,kChunkAsst}){
  base=tree(original);Node* selected=nullptr;if(target==kChunkTcom)selected=&node(node(node(base,kChunkTrks).children,kChunkTrck).children,target);else if(target==kChunkAsst)selected=&node(base,target);else if(target==kChunkPmut)selected=&node(node(base,kChunkPtrn).children,target);else selected=&node(node(node(base,kChunkPtrn).children,kChunkPhrs).children,target);
  selected->flags=1;assert(!decode(base,*q));selected->flags=0;
  if(target==kChunkTcom){auto& list=node(node(base,kChunkTrks).children,kChunkTrck).children;list.push_back(*selected);}else if(target==kChunkAsst)base.push_back(*selected);else if(target==kChunkPmut)node(base,kChunkPtrn).children.push_back(*selected);else node(node(base,kChunkPtrn).children,kChunkPhrs).children.push_back(*selected);
  assert(!decode(base,*q));
 }
 base=tree(original);node(node(base,kChunkPtrn).children,kChunkPmut).prefix.push_back(0);assert(!decode(base,*q));
 for(unsigned variant=0;variant<2;++variant){LayerPart part;part.sound.instrument=Instrument::VirtualAnalog;ChunkBuffer child;assert(layer_write_child(child,part));unsigned pre=child.data[0]+3;auto parts=parse(child.data+pre,child.size-pre);if(variant)parts.push_back(parts.back());else parts.back().flags=1;ChunkBuffer altered;assert(altered.write(child.data,pre));emit(altered,parts);ChunkSource leaf{altered.data,altered.size,0};assert(layer_read_child(leaf,part) && part.sound.instrument==Instrument::Unknown && part.raw_size==altered.size);}
 SampleEdit edit;for(auto& v:edit.slice_warp)v=true;uint8_t bytes[]={2,1,0};ChunkSource source{bytes,3,0};ChunkReader<ChunkSource> reader(source);assert(read_warp(reader,edit) && edit.slice_warp[0] && !edit.slice_warp[1]);for(unsigned i=2;i<64;++i)assert(!edit.slice_warp[i]);ChunkSource absent{};ChunkReader<ChunkSource> absent_reader(absent);assert(read_warp(absent_reader,edit));for(bool v:edit.slice_warp)assert(!v);
 std::printf("Wave2 codec: four container scopes/PREF/copy/steal, v557 Song/Preset/Kit byte hashes, %u required track+sound cases, flags/duplicates and warp tail passed\n",checked);
}
