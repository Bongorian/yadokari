#pragma once
#include "session_files.h"
#include "../../platform/desktop/file_access.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
using namespace groove;
// Rebuild reproducible factory assets atomically. The generated file is the
// source of truth; old factory output and candidate copies are not retained.
inline bool save_factory_wav(DesktopFiles& files,const char* path,const float* samples,unsigned frames){
 char staged[192];int length=std::snprintf(staged,sizeof staged,"%s.tmp-XXXXXX",path);
 if(length<0 || unsigned(length)>=sizeof staged)return false;
 int fd=::mkstemp(staged);if(fd<0)return false;
 if(::close(fd)!=0){files.remove(staged);return false;}
 FileWriter writer(files,staged);if(!write_wav(writer,samples,frames) || !writer.finish()){files.remove(staged);return false;}
 if(files.exists(path) && same_file(files,path,staged))return files.remove(staged);
 if(files.move(staged,path))return true;
 std::fprintf(stderr,"Cannot install factory WAV %s\n",path);files.remove(staged);return false;
}
