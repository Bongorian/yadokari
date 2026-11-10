#pragma once
#include "data_layout.h"
#include "project_files.h"
#include "file_replace.h"
#include "dx7_import.h"
#include "update/sha256.h"

namespace groove {
// File-service owner only. Both transports use this state machine; no DSP I/O.
enum class UserFileError:uint8_t {Ok,Closed,Busy,Path,Type,Protected,Length,Hash,Invalid,Io,Exists,Limit};
inline const char* user_file_error(UserFileError e){static const char* names[]={"OK","CLOSED","BUSY","INVALID_PATH","UNSUPPORTED_TYPE","IN_USE","LENGTH_MISMATCH","HASH_MISMATCH","INVALID_CONTENT","IO_ERROR","EXISTS","LIMIT"};return names[unsigned(e)];}
enum class UserShelf:uint8_t {Projects,Presets,Kits,Samples,Banks,Midi,Recordings,Count};
inline const char* user_shelf(UserShelf s){return unsigned(s)<7?kUserShelves[unsigned(s)]:"";}
inline bool user_filename(const char* name){unsigned n=0;for(;name[n];++n){unsigned c=uint8_t(name[n]);if(c<32 || c=='/' || c=='\\' || c==':' || c=='*' || c=='?' || c=='"' || c=='<' || c=='>' || c=='|')return false;}return n && n<88 && name[0]!='.' && name[n-1]!='.' && name[n-1]!=' ';}
inline bool user_extension(const char* name){const char* ext[]={".mgxs",".mgxk",".mgxp",".wav",".mid",".syx",".sf2"};for(const char* e:ext)if(data_suffix(name,e))return true;return false;}
inline bool user_file_path(const char* path){if(!path || !*path || *path=='/' || *path=='\\')return false;unsigned depth=0;for(const char* at=path;*at;){const char* end=at;while(*end && *end!='/'){if(*end=='\\' || *end==':' || uint8_t(*end)<32)return false;++end;}unsigned n=unsigned(end-at);if(!n || (n==1 && at[0]=='.') || (n==2 && at[0]=='.' && at[1]=='.') || ++depth>6)return false;at=*end?end+1:end;if(*end && !*at)return false;}bool shelf=false;for(unsigned i=0;i<7;++i){const char* root=user_shelf(UserShelf(i));if(data_under(path,root) && !data_equal(path,root))shelf=true;}return shelf && !factory_path(path) && user_extension(path) && !data_suffix(path,".syx");}
struct UserFileEntry {char path[192]{};uint64_t bytes=0;};
class UserFiles {
 IFileAccess& fs_;const ProjectState& project_;const char* current_=nullptr;
 bool open_=false,busy_=false,upload_=false;bool (*continue_)(void*)=nullptr;void* continue_context_=nullptr;UserFileError error_=UserFileError::Ok;
 char name_[96]{},temporary_[192]{},destination_[192]{};
 bool (*reference_identity_)(void*,const char*,const char*)=nullptr;void* reference_context_=nullptr;
 uint64_t length_=0,received_=0;uint8_t expected_[32]{};update::Sha256 hash_;
 bool fail(UserFileError e){error_=e;return false;}
 bool read(const char* p,uint64_t at,void* b,unsigned n){unsigned got=0;return fs_.read(p,at,b,n,got) && got==n;}
 bool matches_reference(const char* path,const char* reference)const{if(data_equal(path,reference))return true;if(reference_identity_)return reference_identity_(reference_context_,path,reference);if(!fs_.exists(path) || !fs_.exists(reference))return false;FileRevision a,b;if(!fs_.revision(path,a) || !fs_.revision(reference,b) || !a.identity || !b.identity)return true;return a.identity==b.identity;}
 bool in_use(const char* path)const{if(factory_path(path) || (current_ && matches_reference(path,current_)))return true;for(const auto& a:project_.assets)if(a.path[0] && matches_reference(path,a.path))return true;return false;}
 bool available(){if(!open_)return fail(UserFileError::Closed);if(busy_)return fail(UserFileError::Busy);if(continue_ && !continue_(continue_context_)){cancel();return fail(UserFileError::Closed);}error_=UserFileError::Ok;return true;}
 bool dirs(const char* path){char dir[192];if(!copy_text(dir,sizeof dir,path))return false;for(char* c=dir;*c;++c)if(*c=='/'){*c=0;bool ok=fs_.directory(dir);*c='/';if(!ok)return false;}return true;}
 bool sf2_list(uint64_t at,uint64_t end,bool table){
  static const char* ids[]={"phdr","pbag","pmod","pgen","inst","ibag","imod","igen","shdr"};static const unsigned stride[]={38,4,10,4,22,4,10,4,46};unsigned mask=0;bool sample=false;unsigned count=0;
  while(at<end){uint8_t h[8];if(++count>4096 || end-at<8 || !read(temporary_,at,h,8))return false;unsigned n=wav_u32(h+4);uint64_t next=at+8+uint64_t(n)+(n&1);if(next>end)return false;
   if(table){for(unsigned i=0;i<9;++i)if(wav_tag(h,ids[i])){if((mask&(1u<<i)) || n<stride[i] || n%stride[i])return false;if(i==0 || i==4 || i==8){uint8_t terminal[3];if(n<2*stride[i] || !read(temporary_,at+8+n-stride[i],terminal,3) || __builtin_memcmp(terminal,i==0?"EOP":i==4?"EOI":"EOS",3))return false;}mask|=1u<<i;}}
   else if(wav_tag(h,"smpl")){if(sample || n<2 || n%2)return false;sample=true;}
   at=next;
  }return table?mask==511:sample;
 }
 bool riff(const char* kind){uint8_t h[12],c[8];if(!read(temporary_,0,h,12) || !wav_tag(h,"RIFF") || !wav_tag(h+8,kind) || uint64_t(wav_u32(h+4))+8!=length_)return false;uint64_t at=12;unsigned count=0;bool data=false,fmt=false,sdta=false,pdta=false;while(at<length_){if(++count>4096 || length_-at<8 || !read(temporary_,at,c,8))return false;uint64_t n=wav_u32(c+4),end=at+8+n+(n&1);if(end>length_ || end<at)return false;if(wav_tag(c,"data"))data=n!=0;if(wav_tag(c,"fmt "))fmt=n>=16;if(wav_tag(c,"LIST") && n>=4){uint8_t tag[4];if(!read(temporary_,at+8,tag,4))return false;if(wav_tag(tag,"sdta")){if(sdta || !sf2_list(at+12,at+8+n,false))return false;sdta=true;}if(wav_tag(tag,"pdta")){if(pdta || !sf2_list(at+12,at+8+n,true))return false;pdta=true;}}at=end;}if(kind[0]=='s')return sdta && pdta;WavView v;uint64_t offset;return fmt && data && sample_file_header(fs_,temporary_,v,offset);}
 bool midi(){uint8_t h[14],c[8];if(!read(temporary_,0,h,14) || !wav_tag(h,"MThd") || h[4] || h[5] || h[6] || h[7]!=6 || h[8] || h[9]>1)return false;unsigned tracks=unsigned(h[10])*256+h[11];if(!tracks || tracks>256 || !(h[12]|h[13]))return false;uint64_t at=14;for(unsigned i=0;i<tracks;++i){if(!read(temporary_,at,c,8) || !wav_tag(c,"MTrk"))return false;uint64_t n=(uint32_t(c[4])<<24)|(uint32_t(c[5])<<16)|(uint32_t(c[6])<<8)|c[7];if(n>length_ || at+8+n>length_)return false;at+=8+n;}return at==length_;}
 bool classify(){uint8_t h[23]{};if(length_<12 || !read(temporary_,0,h,length_<23?unsigned(length_):23))return fail(UserFileError::Invalid);const char* root=nullptr;unsigned kind=0;
  if(data_suffix(name_,".mgxs") || data_suffix(name_,".mgxk") || data_suffix(name_,".mgxp")){kind=data_content_header(h,sizeof h);if(!kind || !valid_project_file(fs_,temporary_))return fail(UserFileError::Invalid);root=data_shelf_for_content(kind);}
  else if(data_suffix(name_,".wav")){if(!riff("WAVE"))return fail(UserFileError::Invalid);root=user_shelf(UserShelf::Samples);}
  else if(data_suffix(name_,".sf2")){if(!riff("sfbk"))return fail(UserFileError::Invalid);root=user_shelf(UserShelf::Banks);}
  else if(data_suffix(name_,".mid")){if(!midi())return fail(UserFileError::Invalid);root=user_shelf(UserShelf::Midi);}
  else if(data_suffix(name_,".syx")){uint8_t bytes[4104];Dx7BankView bank;if(length_!=sizeof bytes || !read(temporary_,0,bytes,sizeof bytes) || !parse_dx7_bulk(bytes,sizeof bytes,bank))return fail(UserFileError::Invalid);return true;}
  if(!root){return fail(UserFileError::Type);}
  char target[96];if(kind){if(!data_content_name(target,sizeof target,name_,kind))return fail(UserFileError::Path);}else if(!copy_text(target,sizeof target,name_))return fail(UserFileError::Path);return join_path(destination_,sizeof destination_,root,target) || fail(UserFileError::Path);
 }
public:
 explicit UserFiles(IFileAccess& fs,const ProjectState& p):fs_(fs),project_(p){}
 ~UserFiles(){close();}
 bool open(const char* current){current_=current;open_=!FileReplace::pending(fs_);error_=open_?UserFileError::Ok:UserFileError::Io;return open_;}
 void continuation(bool (*guard)(void*),void* context){continue_=guard;continue_context_=context;}
 void reference_identity(bool (*match)(void*,const char*,const char*),void* context){reference_identity_=match;reference_context_=context;}
 void busy(bool on){busy_=on;}
 bool active()const{return open_;}bool uploading()const{return upload_;}UserFileError error()const{return error_;}
 const char* destination()const{return destination_;}uint64_t received()const{return received_;}uint64_t length()const{return length_;}
 void cancel(){if(upload_){fs_.remove(temporary_);upload_=false;}received_=length_=0;}
 void close(){cancel();open_=false;}
 bool begin(const char* name,uint64_t length,const uint8_t sha[32]){if(!available())return false;if(upload_)return fail(UserFileError::Busy);if(!name || !user_filename(name))return fail(UserFileError::Path);if(!user_extension(name))return fail(UserFileError::Type);if(!sha)return fail(UserFileError::Hash);if(!length || length>1024ull*1024*1024)return fail(UserFileError::Limit);
  copy_text(name_,sizeof name_,name);const char* root=data_suffix(name,".wav")?user_shelf(UserShelf::Samples):data_suffix(name,".sf2")?user_shelf(UserShelf::Banks):data_suffix(name,".mid")?user_shelf(UserShelf::Midi):data_suffix(name,".syx")?"PRESETS/DX7":data_suffix(name,".mgxs")?"PROJECTS":data_suffix(name,".mgxk")?"KITS":"PRESETS";
  if((data_suffix(name,".wav") || data_suffix(name,".sf2")) && __builtin_strlen(root)+__builtin_strlen(name)+4>=sizeof project_.assets[0].path)return fail(UserFileError::Path);
  if(!join_path(temporary_,sizeof temporary_,root,".MGX-TRANSFER.tmp") || !dirs(temporary_))return fail(UserFileError::Io);
  if(fs_.exists(temporary_))return fail(UserFileError::Exists);
  const uint8_t empty=0;
  upload_=true;
  if(!fs_.write(temporary_,0,&empty,0,true)){cancel();return fail(UserFileError::Io);}
  length_=length;received_=0;destination_[0]=0;__builtin_memcpy(expected_,sha,32);hash_=update::Sha256{};return true;
 }
 bool chunk(uint64_t offset,const uint8_t* bytes,unsigned n){if(!available())return false;if(!upload_)return fail(UserFileError::Closed);if(!bytes || offset!=received_ || !n || n>32768 || n>length_-received_){cancel();return fail(UserFileError::Length);}if(!fs_.write(temporary_,received_,bytes,n,false)){cancel();return fail(UserFileError::Io);}hash_.add(bytes,n);received_+=n;return true;}
 bool commit(){if(!available())return false;if(!upload_)return fail(UserFileError::Closed);if(received_!=length_ || fs_.size(temporary_)!=length_){cancel();return fail(UserFileError::Length);}uint8_t actual[32];hash_.finish(actual);if(!update::equal(actual,expected_,32)){cancel();return fail(UserFileError::Hash);}FileScanBuffer storage;if(!storage.data()){cancel();return fail(UserFileError::Io);}update::Sha256 stored;for(uint64_t at=0;at<length_;){if(!available())return false;unsigned n=unsigned(length_-at>kFileScanBufferBytes?kFileScanBufferBytes:length_-at);if(!read(temporary_,at,storage.data(),n)){cancel();return fail(UserFileError::Io);}stored.add(storage.data(),n);at+=n;}stored.finish(actual);if(!update::equal(actual,expected_,32)){cancel();return fail(UserFileError::Hash);}if(!classify()){UserFileError e=error_;cancel();return fail(e);}
  if(data_suffix(name_,".syx")){bool ok=import_dx7_bank(fs_,temporary_,destination_,sizeof destination_,name_,continue_,continue_context_);fs_.remove(temporary_);upload_=false;return ok || fail(UserFileError::Io);}
  if(!available())return false;
  if(in_use(destination_)){cancel();return fail(UserFileError::Protected);}if(!dirs(destination_)){cancel();return fail(UserFileError::Io);}
  FileReplace replacement(fs_);bool ok=replacement.replace(temporary_,destination_,length_,expected_,continue_,continue_context_);if(!ok){cancel();return fail(UserFileError::Io);}upload_=false;error_=UserFileError::Ok;return true;
 }
 bool erase(const char* path){if(!available())return false;if(upload_)return fail(UserFileError::Busy);if(!user_file_path(path))return fail(UserFileError::Path);if(in_use(path))return fail(UserFileError::Protected);return fs_.remove(path) || fail(UserFileError::Io);}
 bool get(const char* path,uint64_t at,void* bytes,unsigned wanted,unsigned& got){got=0;if(!available())return false;if(upload_)return fail(UserFileError::Busy);if(!user_file_path(path))return fail(UserFileError::Path);return fs_.read(path,at,bytes,wanted,got) || fail(UserFileError::Io);}
 uint64_t size(const char* path){return available() && user_file_path(path)?fs_.size(path):0;}
 bool space(uint64_t& free,uint64_t& total){return available() && (fs_.space(free,total) || fail(UserFileError::Io));}
 bool list(UserShelf shelf,UserFileEntry* out,unsigned capacity,unsigned& count){count=0;if(!available())return false;if(upload_)return fail(UserFileError::Busy);if(unsigned(shelf)>=7)return fail(UserFileError::Path);
  struct Scan{char dirs[96][192]{};FileEntry entries[128]{};};auto* scan=alloc_new<Scan>();if(!scan)return fail(UserFileError::Io);copy_text(scan->dirs[0],192,user_shelf(shelf));unsigned head=0,tail=1;bool ok=true;
  while(ok && head<tail){const char* dir=scan->dirs[head++];if(!fs_.exists(dir))continue;unsigned n=0;if(!fs_.list_checked(dir,scan->entries,128,n)){ok=false;break;}for(unsigned i=0;i<n;++i){const auto& e=scan->entries[i];char path[192];if(!join_path(path,sizeof path,dir,e.name)){ok=false;break;}if(e.directory){if(tail==96 || !copy_text(scan->dirs[tail++],192,path)){ok=false;break;}}else if(user_file_path(path)){if(count==capacity){ok=false;break;}copy_text(out[count].path,sizeof out[count].path,path);out[count++].bytes=e.bytes;}}}
  delete scan;return ok || fail(UserFileError::Limit);
 }
};
}
