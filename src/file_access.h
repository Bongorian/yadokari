#pragma once
#if defined(__linux__)
#include <sys/random.h>
#include <errno.h>
#endif
#include "project.h"
#include "wav_sampler.h"
#include "alloc_policy.h"
#include <stdint.h>
#include <stddef.h>
namespace groove {
struct FileEntry {char name[96]{};bool directory=false;uint64_t bytes=0;uint8_t content_kind=0;uint16_t format_version=0;bool unsupported=false;};
struct FileRevision {uint64_t size=0,identity=0,modified=0,detail=0;};
inline bool same_file_revision(const FileRevision& a,const FileRevision& b){return a.size==b.size && a.identity==b.identity && a.modified==b.modified && a.detail==b.detail;}
class IFileAccess {public:
 virtual bool random_bytes(uint8_t* bytes,unsigned count){
#if defined(__linux__)
  for(unsigned at=0;at<count;){auto n=getrandom(bytes+at,count-at,0);if(n<0 && errno==EINTR)continue;if(n<=0)return false;at+=unsigned(n);}return true;
#else
  (void)bytes;(void)count;return false;
#endif
 }
virtual ~IFileAccess(){};virtual bool begin_read_session(const char*){return false;}virtual void end_read_session(){};virtual bool space(uint64_t& free,uint64_t& total){free=total=0;return false;}virtual unsigned list(const char* directory,FileEntry* entries,unsigned capacity)=0;virtual bool read(const char* path,uint64_t offset,void* data,unsigned wanted,unsigned& got)=0;virtual bool write(const char* path,uint64_t offset,const void* data,unsigned size,bool truncate)=0;virtual bool begin_recording(const char* path,const void* header,unsigned bytes,uint64_t reserve){(void)reserve;return write(path,0,header,bytes,true);}virtual bool finish_recording(const char* path,const void* header,unsigned bytes,uint64_t length){(void)length;return write(path,0,header,bytes,false);}virtual bool move(const char* from,const char* to)=0;virtual bool remove(const char* path)=0;virtual bool directory(const char* path)=0;virtual uint64_t size(const char* path)=0;virtual bool exists(const char* path)=0;virtual bool revision(const char*,FileRevision&){return false;}virtual bool browser_file_management_allowed()const{return false;}virtual bool list_checked(const char*,FileEntry*,unsigned,unsigned& count){count=0;return false;}virtual bool list_browser(const char* dir,FileEntry* out,unsigned cap,unsigned& count){return list_checked(dir,out,cap,count);}};
// Optional task-owned cursor reservation. Default backends keep ordinary reads;
// a successful owner is closed on every success/failure/early-return path.
class FileReadSession {
 IFileAccess& fs_;bool owned_;
public:
 explicit FileReadSession(IFileAccess& fs):fs_(fs),owned_(false){}
 FileReadSession(IFileAccess& fs,const char* path):fs_(fs),owned_(fs.begin_read_session(path)){}
 void begin(const char* path){if(!owned_)owned_=fs_.begin_read_session(path);}
 ~FileReadSession(){if(owned_)fs_.end_read_session();}
 FileReadSession(const FileReadSession&)=delete;FileReadSession& operator=(const FileReadSession&)=delete;
};
// Fixed-size heap storage for file-service scans. These operations never run
// in the audio callback; a shared size avoids excessive FatFs open/seek calls.
constexpr unsigned kFileScanBufferBytes=32768;
class FileScanBuffer {
 uint8_t* data_;
public:
 FileScanBuffer():data_(alloc_array<uint8_t>(kFileScanBufferBytes)){}
 ~FileScanBuffer(){delete[] data_;}
 FileScanBuffer(const FileScanBuffer&)=delete;FileScanBuffer& operator=(const FileScanBuffer&)=delete;
 uint8_t* data()const{return data_;}
};
// Called only by the file-service owner, never by the audio callback.
inline bool cached_file_matches(IFileAccess& fs,const char* path,const uint8_t* cached,uint64_t length){if(!cached || fs.size(path)!=length)return false;FileScanBuffer storage;uint8_t* buffer=storage.data();if(!buffer)return false;for(uint64_t offset=0;offset<length;){unsigned count=unsigned(length-offset>kFileScanBufferBytes?kFileScanBufferBytes:length-offset),got=0;if(!fs.read(path,offset,buffer,count,got) || got!=count || __builtin_memcmp(buffer,cached+offset,count))return false;offset+=count;}return true;}
inline bool copy_text(char* out,size_t size,const char* in){size_t i=0;while(in[i]){if(i+1>=size)return false;out[i]=in[i];++i;}out[i]=0;return true;}
inline const char* filename(const char* path){const char* result=path;for(;*path;++path)if(*path=='/' || *path=='\\')result=path+1;return result;}
inline bool join_path(char* out,size_t size,const char* folder,const char* name){if(!copy_text(out,size,folder))return false;size_t i=0;while(out[i])++i;if(i && out[i-1]!='/'){if(i+1>=size)return false;out[i++]='/';out[i]=0;}return copy_text(out+i,size-i,name);}
inline bool parent_path(char* path){size_t n=0;while(path[n])++n;while(n && path[n-1]!='/')--n;if(!n){path[0]=0;return true;}path[n-1]=0;return true;}
enum class SampleLoadProgress:uint8_t {Working,Ready,Failed};
class ISampleLibrary {public:virtual ~ISampleLibrary(){}virtual bool load(const char* path,WavView& view)=0;virtual bool load_twin(const char* path,WavView& view){return load(path,view);}
virtual WavView lookup(const char* path)const=0;virtual bool prepare(const ProjectState& project,WavView* views)=0;virtual void service(){};virtual const char* failure_reason()const{return "Assets unavailable / sound kept";};virtual bool supports_incremental_load()const{return false;}virtual bool incremental_validates_crc(const char*,uint64_t){return false;}virtual bool begin_incremental_load(const char*,uint32_t){return false;}virtual SampleLoadProgress poll_incremental_load(WavView&){return SampleLoadProgress::Failed;}virtual void cancel_incremental_load(){};};
// Optional RIFF metadata is scanned on the file-service thread, including
// chunks after audio data. Never read sample bytes or allocate in this walk.
inline bool sample_file_root(IFileAccess& fs,const char* path,uint8_t& root){
 uint8_t top[12],chunk[8],data[16];unsigned got=0;uint64_t size=fs.size(path);if(!fs.read(path,0,top,12,got) || got!=12 || !wav_tag(top,"RIFF") || !wav_tag(top+8,"WAVE"))return false;
 uint64_t total=uint64_t(wav_u32(top+4))+8;if(total<12 || total>size)return false;bool found=false;uint8_t fallback=60;
 for(uint64_t at=12,budget=0;at+8<=total && budget<128;++budget){if(!fs.read(path,at,chunk,8,got) || got!=8)break;uint32_t length=wav_u32(chunk+4);uint64_t end=at+8+uint64_t(length);if(end>total)break;
  if(wav_tag(chunk,"smpl") && length>=36 && fs.read(path,at+8,data,16,got) && got==16){uint32_t note=wav_u32(data+12);if(note<128){root=uint8_t(note);return true;}}
  if(wav_tag(chunk,"inst") && length>=7 && fs.read(path,at+8,data,1,got) && got==1 && data[0]<128){fallback=data[0];found=true;}
  if(end+(length&1)>total)break;
  at=end+(length&1);
 }
 if(found)root=fallback;
 return found;
}
// smpl loop end is inclusive. Only ordinary, indefinite forward loops can be
// represented by the sampler; unsupported modes/counts are left untouched.
inline bool sample_file_loop(IFileAccess& fs,const char* path,uint32_t frames,uint32_t& first,uint32_t& last){
 uint8_t top[12],chunk[8],header[36],loop[24];unsigned got=0;uint64_t size=fs.size(path);if(!frames || !fs.read(path,0,top,12,got) || got!=12 || !wav_tag(top,"RIFF") || !wav_tag(top+8,"WAVE"))return false;
 uint64_t total=uint64_t(wav_u32(top+4))+8;if(total<12 || total>size)return false;
 for(uint64_t at=12,budget=0;at+8<=total && budget<128;++budget){if(!fs.read(path,at,chunk,8,got) || got!=8)return false;uint32_t length=wav_u32(chunk+4);uint64_t end=at+8+uint64_t(length);if(end>total)return false;
  if(wav_tag(chunk,"smpl") && length>=36 && fs.read(path,at+8,header,36,got) && got==36){uint32_t count=wav_u32(header+28),available=(length-36)/24;if(count>available)count=available;if(count>32)count=32;for(unsigned i=0;i<count;++i){if(!fs.read(path,at+44+uint64_t(i)*24,loop,24,got) || got!=24)return false;uint32_t type=wav_u32(loop+4),start=wav_u32(loop+8),finish=wav_u32(loop+12),fraction=wav_u32(loop+16),plays=wav_u32(loop+20);if(type==0 && !fraction && !plays && start<=finish && finish<frames){first=start;last=finish+1;return true;}}}
  if(end+(length&1)>total)break;
  at=end+(length&1);
 }
 return false;
}
// Match WavVoice's float multiplication exactly; reject boundaries that the
// current normalized representation cannot express for very long samples.
inline bool sample_frame_fraction(uint32_t frame,uint32_t frames,float& value){
 if(!frames || frame>frames)return false;
 value=float(frame)/float(frames);
 for(unsigned i=0;i<4;++i){uint64_t mapped=uint64_t(value*float(frames));if(mapped==frame)return true;float next=nextafterf(value,mapped<frame?1.0f:0.0f);if(next==value)break;value=next;}
 return false;
}
// Canonical DATA-root paths. Resolve relative package references without escaping DATA.
inline bool canonical_path(char* out,unsigned capacity,const char* base,const char* relative){char joined[320];if(!join_path(joined,sizeof joined,base,relative))return false;unsigned starts[80]{},depth=0,used=0;const char* p=joined;while(*p){while(*p=='/')++p;if(!*p)break;const char* q=p;while(*q && *q!='/'){if(*q==':' || *q=='\\')return false;++q;}unsigned n=unsigned(q-p);if(n==1 && p[0]=='.'){}else if(n==2 && p[0]=='.' && p[1]=='.'){if(!depth)return false;used=starts[--depth];}else{if(depth==80 || used+n+2>capacity)return false;starts[depth++]=used;if(used)out[used++]='/';for(unsigned j=0;j<n;++j)out[used++]=p[j];}p=q;}out[used]=0;return true;}
inline bool project_paths(ProjectState& project,const char* filename,bool resolve,bool recover_hints=false){char base[192];if(!copy_text(base,sizeof base,filename))return false;parent_path(base);for(auto& a:project.assets)if(a.path[0]){char path[192];if(resolve){if(!canonical_path(path,sizeof path,base,a.path)){if(recover_hints)continue;return false;}}else{unsigned depth=0;for(const char* p=base;*p;++p)if(*p=='/')++depth;if(base[0])++depth;unsigned n=0;for(unsigned i=0;i<depth;++i){if(n+3>=sizeof path)return false;path[n++]='.';path[n++]='.';path[n++]='/';}if(!copy_text(path+n,sizeof path-n,a.path))return false;}if(!copy_text(a.path,sizeof a.path,path))return false;}return true;}
// Walk metadata chunks by file offset; a large JUNK/BEXT chunk never has to
// fit in a header prefix or in the real-time page cache.
inline bool sample_file_header(IFileAccess& fs,const char* path,WavView& view,uint64_t& offset){uint8_t top[12],chunk[8],fmt[40]{};unsigned got,format_size=0;uint64_t file_size=fs.size(path);if(!fs.read(path,0,top,12,got)||got!=12||!wav_tag(top,"RIFF")||!wav_tag(top+8,"WAVE"))return false;uint64_t total=uint64_t(wav_u32(top+4))+8;if(total>file_size)return false;for(uint64_t at=12,budget=0;at+8<=total && budget<4096;++budget){if(!fs.read(path,at,chunk,8,got)||got!=8)return false;unsigned size=wav_u32(chunk+4);uint64_t end=at+8+size;if(end>total)return false;if(wav_tag(chunk,"fmt ")){format_size=size>40?40:size;if(format_size<16||!fs.read(path,at+8,fmt,format_size,got)||got!=format_size)return false;}if(wav_tag(chunk,"data")){if(!format_size)return false;uint8_t header[80]{};for(unsigned i=0;i<12;++i)header[i]=top[i];__builtin_memcpy(header+12,"fmt ",4);header[16]=uint8_t(format_size);for(unsigned i=0;i<format_size;++i)header[20+i]=fmt[i];unsigned pos=20+format_size+(format_size&1);for(unsigned i=0;i<8;++i)header[pos+i]=chunk[i];uint64_t synthetic=uint64_t(pos)+8+size+(size&1);if(synthetic>0xffffffffu)return false;for(unsigned i=0;i<4;++i)header[4+i]=uint8_t((synthetic-8)>>(8*i));uint64_t ignored;if(!parse_wav_header(header,pos+8,synthetic,view,ignored))return false;offset=at+8;return true;}at=end+(size&1);}return false;}
}
