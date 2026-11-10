#pragma once
#include "file_access.h"
namespace groove {
// Optional runtime preference; a damaged pointer never makes the project invalid.
// The project itself is committed by the existing recoverable save protocol first.
inline bool read_project_location(IFileAccess& fs,const char* marker,char* out,unsigned capacity){
 char raw[160]{},canonical[160]{};unsigned got=0;uint64_t n=fs.size(marker);
 if(!n || n>=sizeof raw || !fs.read(marker,0,raw,unsigned(n),got) || got!=n)return false;
 for(unsigned i=0;i<got;++i)if(uint8_t(raw[i])<32 || uint8_t(raw[i])>126)return false;
 if(raw[0]=='/' || !canonical_path(canonical,sizeof canonical,"",raw) || __builtin_strcmp(raw,canonical))return false;
 return copy_text(out,capacity,raw);
}
inline bool write_project_location(IFileAccess& fs,const char* marker,const char* path){
 char canonical[160];if(path[0]=='/' || !canonical_path(canonical,sizeof canonical,"",path))return false;
 unsigned n=0;while(canonical[n])++n;
 return n && fs.write(marker,0,canonical,n,true);
}
}
