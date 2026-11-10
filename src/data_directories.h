#pragma once
#include "data_layout.h"
namespace groove {
// DATA metadata only: change directory spelling, never relocate files between
// shelves, rename a file, or rewrite its contents. FatFs resolves case equally.
struct DataDirectoryReport {unsigned renamed=0;bool complete=false;};
inline bool normalize_data_directories(IFileAccess& fs,DataDirectoryReport& report){
 struct Work{char queue[256][192]{};FileEntry entries[1024]{};};
 auto* work=alloc_new<Work>();if(!work)return false;unsigned head=0,tail=1;bool ok=true;
 while(ok && head<tail){char parent[192];copy_text(parent,sizeof parent,work->queue[head++]);unsigned count=0;if(!fs.list_checked(parent,work->entries,1024,count)){ok=false;break;}
  for(unsigned i=0;i<count && ok;++i){const auto& entry=work->entries[i];if(!entry.directory)continue;char path[192],upper[96];if(!join_path(path,sizeof path,parent,entry.name) || !copy_text(upper,sizeof upper,entry.name)){ok=false;break;}for(char* c=upper;*c;++c)if(*c>='a' && *c<='z')*c=char(*c-'a'+'A');
   if(__builtin_strcmp(upper,entry.name)){char target[192],temporary[192];if(!join_path(target,sizeof target,parent,upper) || !join_path(temporary,sizeof temporary,parent,".MGX-DIRECTORY-CASE") || fs.exists(temporary)){ok=false;break;}
    if(!fs.move(path,temporary)){ok=false;break;}if(!fs.move(temporary,target)){fs.move(temporary,path);ok=false;break;}copy_text(path,sizeof path,target);++report.renamed;
   }
   if(tail==256 || !copy_text(work->queue[tail++],192,path)){ok=false;break;}
  }
 }
 delete work;report.complete=ok;return ok;
}
}
