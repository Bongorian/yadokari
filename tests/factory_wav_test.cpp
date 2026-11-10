#include "../tools/content/factory_wav.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
int main(){
 char dir[]="/tmp/groove-factory-XXXXXX";assert(mkdtemp(dir));char path[160];std::snprintf(path,sizeof path,"%s/kit.wav",dir);
 DesktopFiles files;float audio[8]={0,0,0.1f,0.1f,0.2f,0.2f,0,0};assert(save_factory_wav(files,path,audio,4));uint32_t old=0,after=0;assert(file_checksum(files,path,old));assert(save_factory_wav(files,path,audio,4));audio[2]=0.8f;assert(save_factory_wav(files,path,audio,4));assert(file_checksum(files,path,after) && old!=after);
 char candidate[192];std::snprintf(candidate,sizeof candidate,"%s.factory-000",path);assert(!files.exists(candidate));assert(files.remove(path));assert(::rmdir(dir)==0);
}
