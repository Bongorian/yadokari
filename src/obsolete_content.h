#pragma once
#include "file_access.h"
#include "data_layout.h"
#include "update/sha256.h"
namespace groove {
// Explicit user authorization2026-11-09. Classification only; no legacy decoder.
// Frozen at version12: a future version bump does not broaden deletion rights.
inline constexpr const char* kRetirementLog="GROOVE/RETIRED-CONTENT.jsonl";
enum class RetirementResult {Preserved,ProtectedContainer,Removed,Failed};
inline unsigned obsolete_container_version(const uint8_t* bytes,unsigned count){
 if(count>=6 && !__builtin_memcmp(bytes,"MGXC",4)){unsigned version=unsigned(bytes[4])|(unsigned(bytes[5])<<8);return version>0 && version<=12?version:0;}
 if(count>=4 && !__builtin_memcmp(bytes,"MGX",3) && bytes[3]>='1' && bytes[3]<='9')return 0x100u+unsigned(bytes[3]-'0');
 return 0;
}
inline bool append_retirement_record(IFileAccess& fs,const char* path,unsigned version,uint64_t size,const uint8_t (&sha)[32],bool removed){
 char line[768];unsigned n=0;bool ok=true;
 auto text=[&](const char* p){while(*p){if(n+1>=sizeof line){ok=false;return;}line[n++]=*p++;}};
 auto number=[&](uint64_t v){char digits[21];unsigned length=0;do{digits[length++]=char('0'+v%10);v/=10;}while(v);while(length){char c[2]={digits[--length],0};text(c);}};
 const char* hex="0123456789abcdef";auto byte=[&](unsigned v){char value[3]={hex[(v>>4)&15],hex[v&15],0};text(value);};
 text("{\"action\":\"");text(removed?"removed":"intent");text("\",\"path_hex\":\"");for(const char* p=path;*p;++p)byte(uint8_t(*p));text("\",\"version_tag\":");number(version);text(",\"bytes\":");number(size);text(",\"sha256\":\"");for(unsigned i=0;i<32;++i)byte(sha[i]);text("\"}\n");
 if(!ok)return false;
 uint64_t offset=fs.size(kRetirementLog);return fs.write(kRetirementLog,offset,line,n,offset==0);
}
inline RetirementResult retire_obsolete_container(IFileAccess& fs,const char* path){
 if(data_under(path,"firmware") || data_equal(path,kRetirementLog))return RetirementResult::Preserved;
 uint8_t header[6]{};unsigned got=0;
 if(!fs.read(path,0,header,sizeof header,got))return RetirementResult::Failed;
 unsigned version=obsolete_container_version(header,got);
 // The retired .mgp/.mgk aliases are explicitly obsolete; a modern/future
 // MGXC body is protected even when someone gave it an obsolete suffix.
 bool mgxc=got>=4 && !__builtin_memcmp(header,"MGXC",4);
 if(!version && !mgxc && (data_suffix(path,".mgp") || data_suffix(path,".mgk")))version=0x200;
 if(!version)return mgxc?RetirementResult::ProtectedContainer:RetirementResult::Preserved;
 uint64_t size=fs.size(path),offset=0;FileScanBuffer buffer;if(!buffer.data())return RetirementResult::Failed;
 update::Sha256 digest;{
  FileReadSession session(fs,path);
  while(offset<size){unsigned n=unsigned(size-offset>kFileScanBufferBytes?kFileScanBufferBytes:size-offset),read=0;if(!fs.read(path,offset,buffer.data(),n,read) || read!=n)return RetirementResult::Failed;digest.add(buffer.data(),n);offset+=n;}
 }
 uint8_t sha[32];digest.finish(sha);
 // The durable intent precedes unlink. A failed log write leaves the original.
 // If a post-unlink write fails, the intent still records path, hash and size.
 if(!fs.directory("GROOVE") || !append_retirement_record(fs,path,version,size,sha,false) || !fs.remove(path))return RetirementResult::Failed;
 return append_retirement_record(fs,path,version,size,sha,true)?RetirementResult::Removed:RetirementResult::Failed;
}
}
