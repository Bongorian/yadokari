#pragma once
#include "file_access.h"
#include "soundfont.h"
#include "project_codec.h"
#include "recovery_source.h"
namespace groove {
// File-service helpers only: no dependency on Performance, Engine or UI state.
// Larger sequential buffers reduce FatFs open/seek/sync cycles for project
// files. They are allocated only by the non-audio file service, not on the
// relatively small bare-metal stack or inside the audio worker.
constexpr unsigned kProjectIoBufferBytes=kFileScanBufferBytes;
class FileWriter {
 IFileAccess& fs_;const char* path_;uint8_t* buffer_;unsigned used_=0;uint64_t offset_=0;
public:
 FileWriter(IFileAccess& fs,const char* path):fs_(fs),path_(path),buffer_(alloc_array<uint8_t>(kProjectIoBufferBytes)){}
 ~FileWriter(){delete[] buffer_;}
 FileWriter(const FileWriter&)=delete;FileWriter& operator=(const FileWriter&)=delete;
 bool write(const uint8_t* p,size_t n){if(!buffer_)return false;if(n==1){if(used_==kProjectIoBufferBytes && !flush())return false;buffer_[used_++]=*p;return used_!=kProjectIoBufferBytes || flush();}while(n){if(used_==kProjectIoBufferBytes && !flush())return false;size_t count=n<kProjectIoBufferBytes-used_?n:kProjectIoBufferBytes-used_;__builtin_memcpy(buffer_+used_,p,count);used_+=unsigned(count);p+=count;n-=count;}return true;}
 bool flush(){if(!buffer_ || !used_)return false;if(!fs_.write(path_,offset_,buffer_,used_,offset_==0))return false;offset_+=used_;used_=0;return true;}
 bool finish(){return buffer_ && (!used_ || flush());}
};
class FileReader {
 IFileAccess& fs_;const char* path_;FileReadSession session_;uint8_t* buffer_;unsigned used_=0,pos_=0;uint64_t offset_=0;
public:
 FileReader(IFileAccess& fs,const char* path):fs_(fs),path_(path),session_(fs,path),buffer_(alloc_array<uint8_t>(kProjectIoBufferBytes)){}
 ~FileReader(){delete[] buffer_;}
 FileReader(const FileReader&)=delete;FileReader& operator=(const FileReader&)=delete;
 bool read(uint8_t* p,size_t n){if(!buffer_)return false;while(n){if(pos_==used_){if(!fs_.read(path_,offset_,buffer_,kProjectIoBufferBytes,used_) || !used_)return false;offset_+=used_;pos_=0;}size_t count=n<used_-pos_?n:used_-pos_;__builtin_memcpy(p,buffer_+pos_,count);pos_+=unsigned(count);p+=count;n-=count;}return true;}
 bool at_end(){return buffer_ && pos_==used_ && offset_==fs_.size(path_);}
};
// Validators use 100 for the current chunk format. Older files are rejected.
inline bool read_project_candidate(IFileAccess& fs,const char* name,ProjectState& candidate,unsigned* version=nullptr,PreservedStore* store=nullptr){uint8_t magic[4]{};unsigned got=0;if(version){*version=0;if(fs.read(name,0,magic,4,got) && got==4)*version=magic[0]=='M' && magic[1]=='G' && magic[2]=='X' && magic[3]=='C'?100:0;}FileReader reader(fs,name);return decode_project(reader,candidate,store);}
inline bool read_project_file(IFileAccess& fs,const char* name,ProjectState& destination,unsigned* version=nullptr,PreservedStore* store=nullptr){auto* candidate=alloc_new<ProjectState>();bool ok=candidate && read_project_candidate(fs,name,*candidate,version,store);if(ok)destination=*candidate;delete candidate;return ok;}
// A candidate validator may inspect the sidecar from this exact decode. Never
// consult the previously mounted project's sidecar while validating a new file.
template<class V> auto validate_file_candidate(V& validate,ProjectState& p,unsigned version,const PreservedStore& store,int)->decltype(validate(p,version,store)){return validate(p,version,store);}
template<class V> bool validate_file_candidate(V& validate,ProjectState& p,unsigned version,const PreservedStore&,long){return validate(p,version);}
// Validate a complete candidate before committing it; rejected generations
// must not modify the caller's current project or recovery source.
template<class Validator> bool load_recoverable_candidate_checked(IFileAccess& fs,const char* name,ProjectState*& candidate,unsigned* version,RecoverySource* source,Validator validate,PreservedStore* accepted_store=nullptr){
 if(source){*source={};}if(version){*version=0;}candidate=nullptr;unsigned selected_version=0;
 auto accept=[&](const char* path,RecoveryKind kind){if(!fs.exists(path))return false;unsigned v=0;delete candidate;candidate=alloc_new<ProjectState>();PreservedStore attempt_store;if(!candidate || !read_project_candidate(fs,path,*candidate,&v,&attempt_store) || !validate_file_candidate(validate,*candidate,v,attempt_store,0))return false;selected_version=v;if(source)*source={kind};if(accepted_store)accepted_store->steal_from(attempt_store);return true;};
 bool ok=accept(name,RecoveryKind::Main);char temporary[192];
 if(!ok && copy_text(temporary,sizeof temporary,name)){size_t n=0;while(temporary[n])++n;if(copy_text(temporary+n,sizeof temporary-n,".tmp"))ok=accept(temporary,RecoveryKind::Temporary);}

 if(!ok){delete candidate;candidate=nullptr;}if(ok && version)*version=selected_version;return ok;
}
template<class Validator> bool load_recoverable_checked(IFileAccess& fs,const char* name,ProjectState& destination,unsigned* version,RecoverySource* source,Validator validate,PreservedStore* accepted_store=nullptr){ProjectState* candidate=nullptr;bool ok=load_recoverable_candidate_checked(fs,name,candidate,version,source,validate,accepted_store);if(ok)destination=*candidate;delete candidate;return ok;}
inline bool load_recoverable(IFileAccess& fs,const char* name,ProjectState& destination,unsigned* version=nullptr,RecoverySource* source=nullptr,PreservedStore* accepted_store=nullptr){return load_recoverable_checked(fs,name,destination,version,source,[](ProjectState&,unsigned){return true;},accepted_store);}
// Only current MGXC generations are recoverable. A completed temporary may be
// the sole good generation after promotion failed. Never promote unchecked
// bytes or destroy that generation when the visible main is corrupt.
inline bool valid_project_file(IFileAccess& fs,const char* path){FileReader reader(fs,path);return decode_project_impl(reader,nullptr,nullptr);}
inline bool prepare_project_save(IFileAccess& fs,const char* main,const char* temporary){
 if(!fs.exists(temporary))return true;
 if(fs.exists(main))return valid_project_file(fs,main);
 return valid_project_file(fs,temporary) && fs.move(temporary,main);
}
// On the physical DATA filesystem, keep only the current project. A temporary
// file validates the candidate; failure never creates a historical backup.
template<class Guard> bool save_clean_checked(IFileAccess& fs,const char* name,const ProjectState& project,Guard guard,const PreservedStore* store=nullptr){
 char temp[192];if(!copy_text(temp,sizeof temp,name))return false;size_t n=0;while(temp[n])++n;if(!copy_text(temp+n,sizeof temp-n,".tmp"))return false;
 if(!prepare_project_save(fs,name,temp))return false;
 if(!fs.remove(temp))return false;
 FileWriter writer(fs,temp);if(!encode_project(writer,project,store)){fs.remove(temp);return false;}if(!writer.finish()){fs.remove(temp);return false;}
 // Validation owns its fresh parser state; no destination or second
 // transactional copy is needed for a private, unpublished save candidate.
 FileReader reader(fs,temp);bool valid=decode_project_impl(reader,nullptr,nullptr);
 if(!valid || !guard()){fs.remove(temp);return false;}
 if(fs.exists(name) && !fs.remove(name)){fs.remove(temp);return false;}
 if(!fs.move(temp,name))return false;
 // An event loss changes the reported outcome, not the validity of the
 // bytes already verified on disk. Never delete the sole saved generation.
 return guard();
}
template<class Guard> bool save_recoverable_checked(IFileAccess& fs,const char* name,const ProjectState& project,Guard guard,const PreservedStore* store=nullptr){
 return save_clean_checked(fs,name,project,guard,store);
}

inline bool save_recoverable(IFileAccess& fs,const char* name,const ProjectState& project,const PreservedStore* store=nullptr){return save_recoverable_checked(fs,name,project,[]{return true;},store);}
inline bool file_checksum(IFileAccess& fs,const char* name,uint32_t& crc){FileReadSession session(fs,name);uint64_t size=fs.size(name),at=0;FileScanBuffer storage;uint8_t* bytes=storage.data();if(!bytes)return false;ProjectCrc c;while(at<size){unsigned want=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),got;if(!fs.read(name,at,bytes,want,got)||got!=want)return false;for(unsigned i=0;i<got;++i)c.add(bytes[i]);at+=got;}crc=c.value();return size>0;}
inline bool copy_file(IFileAccess& fs,const char* from,const char* to){uint64_t size=fs.size(from),at=0;if(!size)return false;FileScanBuffer storage;uint8_t* bytes=storage.data();if(!bytes)return false;ProjectCrc before;while(at<size){unsigned want=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),got;if(!fs.read(from,at,bytes,want,got) || got!=want || !fs.write(to,at,bytes,got,at==0))return false;for(unsigned i=0;i<got;++i)before.add(bytes[i]);at+=got;}uint32_t after;return fs.size(to)==size && file_checksum(fs,to,after) && before.value()==after;}
inline bool copy_file_prefix(IFileAccess& fs,const char* from,const char* to,uint64_t size){if(!size || fs.size(from)<size)return false;FileScanBuffer storage;uint8_t* bytes=storage.data();if(!bytes)return false;ProjectCrc before;for(uint64_t at=0;at<size;){unsigned want=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),got=0;if(!fs.read(from,at,bytes,want,got) || got!=want || !fs.write(to,at,bytes,got,at==0))return false;for(unsigned i=0;i<got;++i)before.add(bytes[i]);at+=got;}uint32_t after=0;return fs.size(to)==size && file_checksum(fs,to,after) && before.value()==after;}
// Compare content as well as its CRC: collisions must never overwrite a prior generation.
inline bool same_file(IFileAccess& fs,const char* a,const char* b){uint64_t size=fs.size(a);if(!size || fs.size(b)!=size)return false;FileScanBuffer left_storage,right_storage;uint8_t* left=left_storage.data();uint8_t* right=right_storage.data();if(!left || !right)return false;for(uint64_t at=0;at<size;){unsigned n=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),x=0,y=0;if(!fs.read(a,at,left,n,x)||!fs.read(b,at,right,n,y)||x!=n||y!=n||__builtin_memcmp(left,right,n))return false;at+=n;}return true;}
inline bool verify_asset_checksums(IFileAccess& fs,const ProjectState& project){for(const auto& asset:project.assets){if(!asset.path[0] || !asset.metadata.content_crc)continue;uint32_t actual=0;if(!file_checksum(fs,asset.path,actual) || actual!=asset.metadata.content_crc)return false;}return true;}
// Check files without populating the immutable audio cache for rejected generations.
inline bool verify_project_assets(IFileAccess& fs,const ProjectState& project){if(!verify_asset_checksums(fs,project))return false;for(const auto& asset:project.assets){if(!asset.path[0])continue;WavView wav;uint64_t offset=0;if(!sample_file_header(fs,asset.path,wav,offset) && !soundfont_file_header(fs,asset.path))return false;}return true;}
}
