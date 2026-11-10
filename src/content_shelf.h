#pragma once
#include "project_files.h"
#include "project_file_header.h"
#include "data_layout.h"
#include "preset_assets.h"
#include "update/sha256.h"
namespace groove {
// File-service owner only. Bounded enumeration never deletes or rewrites files.
struct ShelfScan {bool complete=true,stop=false;unsigned files=0,folders=0;};
template<class Visit> void walk_content_shelf(IFileAccess& fs,const char* directory,Visit& visit,ShelfScan& result,unsigned depth=0){
 if(directory[0] && !fs.exists(directory))return;
 if(depth>8 || ++result.folders>4096){result.complete=false;return;}
 constexpr unsigned capacity=4096;auto* entries=alloc_array<FileEntry>(capacity);if(!entries){result.complete=false;return;}
 unsigned count=0;if(!fs.list_checked(directory,entries,capacity,count)){result.complete=false;if(!fs.browser_file_management_allowed())count=fs.list(directory,entries,capacity);}if(count>=capacity)result.complete=false;
 for(unsigned i=0;i<count && i<capacity && !result.stop;++i){if(entries[i].name[0]=='.')continue;char path[192];if(!join_path(path,sizeof path,directory,entries[i].name)){result.complete=false;continue;}
  if(entries[i].directory){if(!data_equal(path,"SYSTEM") && !data_equal(path,"UPDATES") && !data_equal(path,"GROOVE") && !data_equal(path,"firmware"))walk_content_shelf(fs,path,visit,result,depth+1);}
  else{if(++result.files>32768){result.complete=false;break;}visit(path,entries[i]);}
 }
 delete[] entries;
}
struct LibraryEntry {char path[160]{},name[96]{};PresetIdentity identity{};Instrument instrument=Instrument::Unknown;ContentKind kind=ContentKind::Preset;bool factory=false,preferred=true,original=false;};
// Metadata is a hint until the normal CRC-covered decoder validates a mount.
// Seek by framed lengths: no whole ProjectState or sample data in the index.
inline bool read_library_entry(IFileAccess& fs,const char* path,LibraryEntry& entry){
 FileReadSession reading(fs,path);
 uint8_t h[10];unsigned got=0;uint64_t total=fs.size(path);if(!fs.read(path,0,h,10,got) || got!=10 || !current_project_header(h,total))return false;
 LibraryEntry next;if(!copy_text(next.path,sizeof next.path,path))return false;next.factory=factory_path(path);
 auto bytes=[&](uint64_t at,void* out,unsigned n){unsigned read=0;return at+n<=total-4 && fs.read(path,at,out,n,read) && read==n;};
 auto frame=[&](uint64_t at,uint64_t limit,uint32_t& kind,uint16_t& rev,uint32_t& size){uint8_t b[12];if(at+12>limit || !bytes(at,b,12))return false;kind=wav_u32(b);rev=wav_u16(b+4);size=wav_u32(b+8);return size<=limit-at-12;};
 bool type=false,id=false,engine=false;
 for(uint64_t at=10;at<total-4;){uint32_t kind,size;uint16_t rev;if(!frame(at,total-4,kind,rev,size))return false;uint64_t body=at+12,end=body+size;
  if(kind==kChunkType && rev==1){uint8_t value;if(type || size!=1 || !bytes(body,&value,1) || value<2 || value>3)return false;next.kind=ContentKind(value);type=true;}
  else if(kind==kChunkIden && rev==1){uint8_t raw[41];if(id || size!=41 || !bytes(body,raw,41))return false;ChunkSource src{raw,41};ChunkReader<ChunkSource> r(src);if(!read_preset_identity(r,next.identity) || !next.identity.assigned())return false;id=true;}
  else if(kind==kChunkTrks && rev==1){
   for(uint64_t tr=body;tr<end;){uint32_t tk,ts;uint16_t tv;if(!frame(tr,end,tk,tv,ts))return false;uint8_t index;if(tk==kChunkTrck && tv==1 && ts && bytes(tr+12,&index,1) && !index){
     for(uint64_t item=tr+13;item<tr+12+ts;){uint32_t ck,cs;uint16_t cv;if(!frame(item,tr+12+ts,ck,cv,cs))return false;
      if(ck==kChunkEngn && cv==1){uint8_t n;if(engine || !cs || !bytes(item+12,&n,1) || n>64 || cs<unsigned(n)+3)return false;char guid[65];uint8_t schema[2];if(!bytes(item+13,guid,n) || !bytes(item+13+n,schema,2) || wav_u16(schema)!=1)return false;guid[n]=0;engine_from_guid(guid,n,next.instrument);engine=true;
       for(uint64_t sub=item+15+n;sub<item+12+cs;){uint32_t sk,ss;uint16_t sv;if(!frame(sub,item+12+cs,sk,sv,ss))return false;if(sk==kChunkName && sv==1){if(ss>=96 || !bytes(sub+12,next.name,ss))return false;next.name[ss]=0;if(!valid_preset_name(next.name))return false;}sub+=12+ss;}
      }item+=12+cs;
     }
    }tr+=12+ts;
   }
  }at=end;
 }
 if(!type || !id || !engine || (next.kind==ContentKind::Kit && next.instrument!=Instrument::DrumKit))return false;
 if(!next.name[0]){copy_text(next.name,sizeof next.name,filename(path));data_strip_mgx_suffix(next.name);}
 entry=next;return true;
}
class PresetLibrary {
 static constexpr unsigned capacity=2048;LibraryEntry* rows_=nullptr;unsigned count_=0;bool complete_=true;
public:
 ~PresetLibrary(){delete[] rows_;}
 PresetLibrary()=default;PresetLibrary(const PresetLibrary&)=delete;PresetLibrary& operator=(const PresetLibrary&)=delete;
 bool refresh(IFileAccess& fs){if(!rows_)rows_=alloc_array<LibraryEntry>(capacity);if(!rows_)return false;
  count_=0;complete_=true;
  auto visit=[&](const char* path,const FileEntry& file){if(!data_mgx_file(file.name))return;LibraryEntry e;if(!read_library_entry(fs,path,e))return;for(unsigned i=0;i<count_;++i)if(data_equal(rows_[i].path,e.path))return;if(count_==capacity){complete_=false;return;}rows_[count_++]=e;};
  const char* roots[]={"PRESETS","KITS","FACTORY/PRESETS","FACTORY/KITS"};ShelfScan result;for(const char* root:roots)walk_content_shelf(fs,root,visit,result);complete_=complete_ && result.complete;
  for(unsigned i=1;i<count_;++i){auto value=rows_[i];unsigned j=i;while(j && __builtin_strcmp(value.path,rows_[j-1].path)<0){rows_[j]=rows_[j-1];--j;}rows_[j]=value;}
  // Resolve lineage visibility once per shelf refresh, never per knob movement.
  for(unsigned i=0;i<count_;++i){auto& row=rows_[i];row.preferred=true;row.original=row.factory;for(unsigned j=0;j<count_;++j){if(i==j || rows_[j].identity.guid!=row.identity.guid)continue;const auto& other=rows_[j];bool later=other.identity.revision>row.identity.revision || (other.identity.revision==row.identity.revision && j<i);if(row.factory && !other.factory)row.preferred=false;if(row.factory==other.factory && later)row.preferred=false;if(row.factory && other.factory && later)row.original=false;}}
  return true;
 }
 unsigned size()const{return count_;}bool complete()const{return complete_;}const LibraryEntry& operator[](unsigned i)const{return rows_[i];}
 int exact(const PresetIdentity& id)const{int factory=-1;for(unsigned i=0;i<count_;++i)if(same_preset_revision(id,rows_[i].identity)){if(!rows_[i].factory)return int(i);if(factory<0)factory=int(i);}return factory;}
 uint32_t latest(const PresetGuid& id)const{uint32_t rev=0;for(unsigned i=0;i<count_;++i)if(rows_[i].identity.guid==id && rows_[i].identity.revision>rev)rev=rows_[i].identity.revision;return rev;}
 bool guid_exists(const PresetGuid& id)const{for(unsigned i=0;i<count_;++i)if(rows_[i].identity.guid==id)return true;return false;}
 bool visible(unsigned at,Instrument filter,bool kits,bool originals=false)const{
  if(at>=count_)return false;
  const auto& row=rows_[at];if((row.kind==ContentKind::Kit)!=kits || (!kits && row.instrument!=filter))return false;
  return originals?row.original:row.preferred;
 }
};
inline bool allocate_user_identity(IFileAccess& fs,const PresetLibrary& library,PresetIdentity& id,const PresetIdentity* parent=nullptr){
 for(unsigned attempt=0;attempt<8;++attempt){PresetIdentity next;if(!fs.random_bytes(next.guid.bytes,16))return false;next.guid.bytes[0]=0x80;next.revision=1;if(parent && parent->assigned()){next.parent=parent->guid;next.parent_revision=parent->revision;}if(library.guid_exists(next.guid))continue;id=next;return true;}return false;
}
inline bool capture_preset_extensions(const PreservedStore& source,PreservedTrackEntry& out){
 if(!out.copy_from(source.tracks[0]))return false;
 if(!out.auxiliary.empty())out.preset_track.steal_from(out.auxiliary);
 ChunkBuffer root;if(!root.write(out.preset_root.data,out.preset_root.size))return false;
 for(unsigned i=0;i<source.chunk_count;++i){const auto& chunk=source.chunks[i];uint32_t at;if(!root.begin_frame(chunk.kind,chunk.version,at,chunk.flags) || !root.write(chunk.payload.data,chunk.payload.size) || !root.end_frame(at))return false;}
 return root.size<=PreservedStore::kMaxEntryBytes && out.preset_root.set(root.data,root.size);
}
inline bool same_preset_extensions(const PreservedTrackEntry& a,const PreservedTrackEntry& b){return same_definition_extensions(a,b);}
// Canonical content excludes identity/display names and storage hints. Resource
// addressing is normalized by CRC/root/BPM (or by ring rotation for wave banks).
inline bool preset_content_digest(const SoundState& sound,const SampleAssetState* assets,uint8_t (&digest)[32]){
 auto* copy=alloc_new<SoundState>(sound);if(!copy || !assets){delete copy;return false;}copy->preset_name[0]=0;for(unsigned i=0;i<copy->layer.count;++i)copy->layer.parts[i].sound.preset_name[0]=0;
 uint64_t needed=sound_asset_mask(sound);uint8_t map[kSampleAssets]{},sources[kSampleAssets]{};unsigned count=0;
 bool ring=engine_asset_mode(sound.instrument)==EngineAssetMode::WaveRing;
 for(unsigned slot=0;slot<kSampleAssets;++slot)if(needed&(uint64_t(1)<<slot))sources[count++]=uint8_t(slot);
 if(!ring)for(unsigned i=1;i<count;++i){auto value=sources[i];unsigned j=i;while(j && (assets[value].metadata.content_crc<assets[sources[j-1]].metadata.content_crc || (assets[value].metadata.content_crc==assets[sources[j-1]].metadata.content_crc && (assets[value].metadata.root<assets[sources[j-1]].metadata.root || (assets[value].metadata.root==assets[sources[j-1]].metadata.root && assets[value].metadata.bpm<assets[sources[j-1]].metadata.bpm))))){sources[j]=sources[j-1];--j;}sources[j]=value;}
 if(ring){for(unsigned i=0;i<count;++i)map[sources[i]]=uint8_t((sources[i]+64-sound.sample_slot)%64);}
 else{unsigned unique=0;for(unsigned i=0;i<count;++i){unsigned slot=sources[i];bool duplicate=unique && assets[slot].metadata.content_crc==assets[sources[unique-1]].metadata.content_crc && assets[slot].metadata.root==assets[sources[unique-1]].metadata.root && assets[slot].metadata.bpm==assets[sources[unique-1]].metadata.bpm && bool(assets[slot].path[0])==bool(assets[sources[unique-1]].path[0]);if(duplicate)map[slot]=uint8_t(unique-1);else{map[slot]=uint8_t(unique);sources[unique++]=uint8_t(slot);}}count=unique;}
 if(ring){copy->sample_slot=0;for(unsigned i=1;i<count;++i){auto value=sources[i];unsigned j=i;while(j && map[value]<map[sources[j-1]]){sources[j]=sources[j-1];--j;}sources[j]=value;}}
 else remap_sound_assets(*copy,map);
 ChunkBuffer data;ChunkWriter<ChunkBuffer> w(data);bool ok=write_sound_engn(data,*copy,nullptr) && write_fx_frame(data,copy->fx) && write_output_trim_frame(data,copy->output_trim) && w.u8(uint8_t(count));
 for(unsigned i=0;i<count && ok;++i){unsigned slot=sources[i];ok=w.u8(map[slot]) && w.u8(assets[slot].path[0]!=0) && w.u32(assets[slot].metadata.content_crc) && w.u8(assets[slot].metadata.root) && w.u16(assets[slot].metadata.bpm);}
 if(ok)update::Sha256::hash(data.data,data.size,digest);
 delete copy;return ok;
}
inline bool same_preset_content(const SoundState& a,const SampleAssetState* aa,const SoundState& b,const SampleAssetState* ba){uint8_t x[32],y[32];return preset_content_digest(a,aa,x) && preset_content_digest(b,ba,y) && !__builtin_memcmp(x,y,32);}
// CRC is identity; paths are validated hints. Cached rows are reverified before
// use, so a moved/replaced file cannot silently reuse an old checksum.
class AssetCatalog {
 struct Row {char path[96]{};uint32_t crc=0;};static constexpr unsigned capacity=4096;Row* rows_=nullptr;unsigned count_=0;bool complete_=true;
public:
 AssetCatalog()=default;~AssetCatalog(){delete[] rows_;}AssetCatalog(const AssetCatalog&)=delete;AssetCatalog& operator=(const AssetCatalog&)=delete;
 void invalidate(){count_=0;}
 bool resolve(IFileAccess& fs,SampleAssetState& asset){if(!asset.path[0])return true;char canonical[192];uint32_t actual=0;
  if(canonical_path(canonical,sizeof canonical,"",asset.path) && !__builtin_strcmp(canonical,asset.path) && file_checksum(fs,canonical,actual) && actual==asset.metadata.content_crc)return true;
  for(unsigned i=0;i<count_;++i)if(rows_[i].crc==asset.metadata.content_crc && file_checksum(fs,rows_[i].path,actual) && actual==asset.metadata.content_crc)return copy_text(asset.path,sizeof asset.path,rows_[i].path);
  if(!rows_)rows_=alloc_array<Row>(capacity);
  if(!rows_)return false;
  complete_=true;bool found=false,skipped=false;char resolved[96]{};
  auto sweep=[&](bool fresh){ShelfScan scan;
   auto visit=[&](const char* path,const FileEntry&){
    uint8_t header[12];unsigned got=0;if(!fs.read(path,0,header,12,got) || got!=12 || !wav_tag(header,"RIFF") || (!wav_tag(header+8,"WAVE") && !wav_tag(header+8,"sfbk")))return;
    unsigned row=count_;for(unsigned i=0;i<count_;++i)if(!__builtin_strcmp(path,rows_[i].path)){row=i;break;}
    // A cached non-match is only a first-pass hint. A failed search always
    // rehashes it, including same-size/in-place edits with unchanged timestamps.
    if(!fresh && row<count_){skipped=true;return;}
    uint32_t crc=0;if(!file_checksum(fs,path,crc)){complete_=false;return;}
    if(row<count_)rows_[row].crc=crc;
    else if(count_<capacity && copy_text(rows_[count_].path,sizeof rows_[count_].path,path)){rows_[count_++].crc=crc;}
    else complete_=false;
    if(crc==asset.metadata.content_crc && copy_text(resolved,sizeof resolved,path)){found=true;scan.stop=true;}
   };
   walk_content_shelf(fs,"",visit,scan);complete_=complete_ && scan.complete && !scan.stop;
  };
  sweep(false);if(!found && skipped){complete_=true;sweep(true);}
  return found && copy_text(asset.path,sizeof asset.path,resolved);
 }
 bool complete()const{return complete_;}
 bool resolve_project(IFileAccess& fs,ProjectState& p,char* error,unsigned size){for(auto& asset:p.assets)if(asset.path[0] && !resolve(fs,asset)){char text[]="Missing CRC 00000000";const char* hex="0123456789abcdef";for(unsigned i=0;i<8;++i)text[12+i]=hex[(asset.metadata.content_crc>>(28-i*4))&15];copy_text(error,size,text);return false;}return true;}
};
inline bool resolve_pinned_content(IFileAccess& fs,const PresetLibrary& library,AssetCatalog& assets,ProjectState& p,char* error,unsigned capacity,const PreservedStore* embedded_store=nullptr){
 PreservedOwner external_store;if(!external_store)return false;
 auto* external=alloc_new<ProjectState>();auto* sound=alloc_new<SoundState>();if(!external || !sound){delete external;delete sound;return false;}
 bool ok=true;for(unsigned i=0;i<kPresetSlots && ok;++i){const auto& embedded=p.sounds[i];if(!embedded.identity.assigned())continue;int index=library.exact(embedded.identity);if(index<0)continue;const auto& row=library[unsigned(index)];
  if(!read_project_file(fs,row.path,*external,nullptr,external_store.get()) || !project_paths(*external,row.path,true,true) || !assets.resolve_project(fs,*external,error,capacity))continue;
  snapshot_sound(*sound,track_at(*external,0,0));if(!same_preset_revision(sound->identity,embedded.identity))continue;
  PreservedTrackEntry extensions;bool same=!embedded_store || (capture_preset_extensions(*external_store,extensions) && same_preset_extensions(embedded_store->sounds[i],extensions));extensions.clear();if(!same || !same_preset_content(embedded,p.assets,*sound,external->assets)){copy_text(error,capacity,"GUID/revision content conflict / song kept");ok=false;}
 }
 delete sound;delete external;return ok;
}

}
