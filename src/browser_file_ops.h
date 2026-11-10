#pragma once
#include "file_access.h"
#include "project_files.h"
#include "benchmark_cleanup_policy.h"
#include "data_layout.h"
namespace groove {
enum class BrowserOperation:uint8_t {None,Rename,Delete,NewFolder,SaveProject,SavePreset,OverwritePreset,RenameTrack};
struct BrowserFileEdit {bool duplicate=false;BrowserOperation operation=BrowserOperation::None;char source[160]{},name[96]{};unsigned cursor=0;bool directory=false,include_fx=false;};
inline bool browser_path_equal(const char* a,const char* b){return benchmark_path_equal(a,b);}
inline bool browser_name_valid(const char* name){unsigned n=0;for(;name[n];++n){unsigned c=uint8_t(name[n]);if(c<32 || c>126 || c=='/' || c=='\\' || c==':' || c=='*' || c=='?' || c=='"' || c=='<' || c=='>' || c=='|')return false;}return n && n<96 && name[0]!='.' && name[n-1]!='.' && name[n-1]!=' ';}
inline bool browser_system_path(const char* path){char first[160];if(!copy_text(first,sizeof first,path))return true;for(char* p=first;*p;++p)if(*p=='/'){*p=0;break;}return !*path || factory_path(path) || benchmark_keep_subtree(first) || benchmark_keep_file(path) || benchmark_keep_directory(path) || browser_path_equal(path,"GROOVE/SESS.MGX") || browser_path_equal(path,"GROOVE/SESS.MGX.tmp");}
enum class FileReferences:uint8_t {Clear,Used,Unknown};
// Complete, bounded DATA walk. Incomplete enumeration or unreadable/corrupt
// projects is uncertainty, never evidence that a file is unused. All storage
// access stays with the task owner. Buffers and the walk queue are off-stack.
class BrowserFileGuard {
 struct Directory {
  Directory* parent=nullptr;char path[160]{};FileEntry* entries=nullptr;unsigned count=0,index=0;
  ~Directory(){delete[] entries;}
  bool open(IFileAccess& fs){for(unsigned capacity=64;;){delete[] entries;entries=alloc_array<FileEntry>(capacity);if(!entries)return false;
   count=0;if(fs.list_checked(path,entries,capacity,count))return true;
   if(count!=capacity || capacity>unsigned(-1)/2)return false;
   capacity*=2;
  }}
 };
 static bool project_extension(const char* path){return data_mgx_file(path); }
 struct Source {IFileAccess& fs;const char* path;uint8_t* buffer;uint64_t at=0;unsigned pos=0,used=0;bool read(uint8_t* dst,size_t n){while(n){if(pos==used){if(!fs.read(path,at,buffer,kFileScanBufferBytes,used) || !used)return false;at+=used;pos=0;}unsigned count=n<used-pos?unsigned(n):used-pos;__builtin_memcpy(dst,buffer+pos,count);pos+=count;dst+=count;n-=count;}return true;}bool at_end()const{return pos==used && at==fs.size(path);}};
 static FileReferences project_refs(IFileAccess& fs,const char* file,const char* target,uint8_t* buffer,uint32_t target_crc,bool crc_valid){
  uint8_t magic[4];unsigned got=0;if(!fs.read(file,0,magic,4,got))return FileReferences::Unknown;
  bool mgxc=got==4 && magic[0]=='M' && magic[1]=='G' && magic[2]=='X' && magic[3]=='C';
  if(!mgxc)return project_extension(file)?FileReferences::Unknown:FileReferences::Clear;
  uint64_t bytes=fs.size(file);if(bytes>uint64_t(kMaxFormatBytes)+14)return FileReferences::Unknown;
  Source source{fs,file,buffer};FormatReader<Source> r(source);for(unsigned i=0;i<4;++i)if(!r.raw(magic[i]))return FileReferences::Unknown;
  uint16_t version;uint32_t total;if(!r.u16(version) || version!=kFormatVersion || !r.u32(total) || total>kMaxFormatBytes)return FileReferences::Unknown;
  uint32_t consumed=0;bool saw_assets=false,used=false;char base[160];if(!copy_text(base,sizeof base,file))return FileReferences::Unknown;parent_path(base);
  // Separate source and payload storage: the reader may refill its cache.
  FileScanBuffer payload;if(!payload.data())return FileReferences::Unknown;
  while(consumed<total){uint32_t kind,n;uint16_t revision,flags;
   if(total-consumed<12 || !r.u32(kind) || !r.u16(revision) || !r.u16(flags) || !r.u32(n) || n>total-consumed-12)return FileReferences::Unknown;
   consumed+=12+n;
   bool known=kind==kChunkTwin || kind==kChunkPtrm || kind==kChunkIden || kind==kChunkPfxs || kind==kChunkTrks || kind==kChunkType || kind==kChunkPref || kind==kChunkAudio || kind==kChunkProj || kind==kChunkAsst || kind==kChunkScen || kind==kChunkSong || kind==kChunkMacr || kind==kChunkMfx || kind==kChunkPtrn || kind==kChunkSnds;
   if(!known || revision!=project_section_revision(kind) || flags)return FileReferences::Unknown;
   if(kind==kChunkAsst){if(saw_assets || revision!=1 || n!=64*(96+19) || !r.block(payload.data(),n))return FileReferences::Unknown;saw_assets=true;
    for(unsigned i=0;i<64;++i){const char* path=reinterpret_cast<const char*>(payload.data()+i*115);bool terminated=false;for(unsigned j=0;j<96;++j)terminated|=path[j]==0;if(!terminated)return FileReferences::Unknown;if(*path){char resolved[192];if(!canonical_path(resolved,sizeof resolved,base,path))return FileReferences::Unknown;used|=browser_path_equal(resolved,target) || (crc_valid && wav_u32(payload.data()+i*115+104)==target_crc);}}
   }else while(n){unsigned count=n>kFileScanBufferBytes?kFileScanBufferBytes:n;if(!r.block(payload.data(),count))return FileReferences::Unknown;n-=count;}
  }
  if(!saw_assets || !r.finish())return FileReferences::Unknown;
  return used?FileReferences::Used:FileReferences::Clear;
 }
public:
 static FileReferences check(IFileAccess& fs,ISampleLibrary& library,const ProjectState& project,const char* path){
  uint32_t target_crc=0;bool crc_valid=(data_suffix(path,".wav") || data_suffix(path,".sf2")) && file_checksum(fs,path,target_crc);
  for(const auto& asset:project.assets)if(asset.path[0] && (browser_path_equal(asset.path,path) || (crc_valid && asset.metadata.content_crc==target_crc)))return FileReferences::Used;
  WavView cached=library.lookup(path);if(cached.data || cached.reader)return FileReferences::Used;
  FileScanBuffer scratch;Directory* current=alloc_new<Directory>();
  if(!scratch.data() || !current){delete current;return FileReferences::Unknown;}
  FileReferences result=current->open(fs)?FileReferences::Clear:FileReferences::Unknown;
  while(current && result==FileReferences::Clear){
   if(current->index==current->count){auto* parent=current->parent;delete current;current=parent;continue;}
   const auto& entry=current->entries[current->index++];char full[160];if(!join_path(full,sizeof full,current->path,entry.name)){result=FileReferences::Unknown;break;}
   if(entry.directory){if(data_under(full,"firmware"))continue;auto* next=alloc_new<Directory>();
    if(!next){result=FileReferences::Unknown;break;}next->parent=current;if(!copy_text(next->path,sizeof next->path,full) || !next->open(fs)){delete next;result=FileReferences::Unknown;break;}current=next;
   }else result=project_refs(fs,full,path,scratch.data(),target_crc,crc_valid);
  }
  while(current){auto* parent=current->parent;delete current;current=parent;}return result;
 }
};
}
