#pragma once
#include "dx7_bank.h"
#include "project_files.h"
#include "data_layout.h"
namespace groove {
// File-service only: bank bytes become portable preset state, never pool entries.
inline bool import_dx7_bank(IFileAccess& fs,const char* source,char* folder,unsigned capacity,const char* bank_name=nullptr,bool (*keep_going)(void*)=nullptr,void* context=nullptr){
 uint8_t bytes[4104];unsigned got=0;Dx7BankView bank;
 if(fs.size(source)!=sizeof bytes || !fs.read(source,0,bytes,sizeof bytes,got) || got!=sizeof bytes || !parse_dx7_bulk(bytes,got,bank))return false;
 auto* p=alloc_new<ProjectState>();if(!p)return false;
 bool ok=fs.directory("PRESETS") && fs.directory("PRESETS/DX7"),available=false;char target[160],paths[32][192]{},stem[88]{};
 const char* source_name=bank_name?bank_name:filename(source);unsigned used=0;for(;source_name[used] && used<80;++used)stem[used]=source_name[used];if(used>4 && data_suffix(stem,".syx"))stem[used-=4]=0;
 if(!used){copy_text(stem,sizeof stem,"Bank");}
 for(unsigned j=0;stem[j];++j)if(uint8_t(stem[j])<32 || stem[j]=='/' || stem[j]=='\\' || stem[j]==':' || stem[j]=='*' || stem[j]=='?' || stem[j]=='"' || stem[j]=='<' || stem[j]=='>' || stem[j]=='|')stem[j]='-';
 for(char* c=stem;*c;++c)if(*c>='a' && *c<='z')*c=char(*c-'a'+'A');
 for(unsigned i=0;ok && i<1000;++i){char name[96];copy_text(name,sizeof name,stem);if(i){unsigned n=unsigned(__builtin_strlen(name));char suffix[]={ '-',char('0'+i/100),char('0'+i/10%10),char('0'+i%10),0};copy_text(name+n,sizeof name-n,suffix);}ok=join_path(target,sizeof target,"PRESETS/DX7",name);if(ok && !fs.exists(target)){available=true;break;}}
 ok=ok && available && copy_text(folder,capacity,target) && fs.directory(target);
 unsigned written=0;
 if(ok){make_empty_project(*p);p->content_kind=ContentKind::Preset;auto t=track_at(*p,0,0);t.instrument=Instrument::Fm6;t.fm6.values[0]=-1;
  for(unsigned i=0;i<32 && ok;++i){if(keep_going && !keep_going(context)){ok=false;break;}__builtin_memcpy(t.fm6.patch,bank.data+i*128,128);char name[24]{};name[0]=char('0'+(i+1)/10);name[1]=char('0'+(i+1)%10);name[2]='-';
   for(unsigned j=0;j<10;++j){char c=char(t.fm6.patch[118+j]);t.preset_name[j]=c>=32 && c<127?c:' ';name[j+3]=(c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9')?c:'-';}t.preset_name[10]=0;copy_text(name+13,sizeof name-13,".mgxp");
   t.preset_identity={};ok=fs.random_bytes(t.preset_identity.guid.bytes,16);t.preset_identity.guid.bytes[0]=0x80;t.preset_identity.revision=1;ok=ok && join_path(paths[i],sizeof paths[i],target,name) && !fs.exists(paths[i]);
   if(ok){written=i+1;ok=save_recoverable_checked(fs,paths[i],*p,[&](){return !keep_going || keep_going(context);});}
  }
 }
 if(!ok && available){for(unsigned i=0;i<written;++i){fs.remove(paths[i]);char tmp[200];if(copy_text(tmp,sizeof tmp,paths[i])){unsigned n=0;while(tmp[n])++n;copy_text(tmp+n,sizeof tmp-n,".tmp");fs.remove(tmp);}}fs.remove(target);}
 delete p;return ok;
}
}
