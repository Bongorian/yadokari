#pragma once
#include "../../src/file_access.h"
#include "../../src/data_layout.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
namespace groove {
class DesktopFiles:public IFileAccess {
 bool data_root_=false;
 static bool directory_path(const char* path,char (&checked)[192]){
  if(*path=='/' || !canonical_path(checked,sizeof checked,"",path))return false;
  for(char* c=checked;*c;++c)if(*c=='/'){*c=0;struct stat part{};bool valid=lstat(checked,&part)==0 && S_ISDIR(part.st_mode);*c='/';if(!valid)return false;}
  struct stat parent{};return lstat(*checked?checked:".",&parent)==0 && S_ISDIR(parent.st_mode);
 }
public:
 // Development cwd is not a mounted DATA volume. Only its owner can opt in
 // when it has selected a dedicated DATA root (e.g. a temporary test volume).
 explicit DesktopFiles(bool data_root=false):data_root_(data_root){}

 bool space(uint64_t& free,uint64_t& total)override{struct statvfs v{};if(statvfs(".",&v))return false;free=uint64_t(v.f_bavail)*v.f_frsize;total=uint64_t(v.f_blocks)*v.f_frsize;return true;}
 bool browser_file_management_allowed()const override{return data_root_;}
 unsigned list(const char* path,FileEntry* out,unsigned max)override{char checked[192];if(!directory_path(path,checked))return 0;DIR* dir=opendir(*checked?checked:".");if(!dir)return 0;unsigned n=0;while(n<max){auto* e=readdir(dir);if(!e)break;if(e->d_name[0]=='.')continue;char name[256];if(!join_path(name,sizeof name,checked,e->d_name))continue;struct stat s{};if(lstat(name,&s)!=0 || (!S_ISDIR(s.st_mode) && !S_ISREG(s.st_mode)))continue;if(!data_browser_entry(checked,e->d_name,S_ISDIR(s.st_mode)))continue;if(!copy_text(out[n].name,sizeof out[n].name,e->d_name))continue;out[n].directory=S_ISDIR(s.st_mode);out[n].bytes=s.st_size;++n;}closedir(dir);for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j)if((out[j].directory && !out[i].directory) || (out[i].directory==out[j].directory && std::strcmp(out[j].name,out[i].name)<0)){auto tmp=out[i];out[i]=out[j];out[j]=tmp;}return n;}
 bool list_checked(const char* path,FileEntry* out,unsigned capacity,unsigned& count)override{
  count=0;char checked[192];if(!directory_path(path,checked))return false;
  DIR* dir=opendir(*checked?checked:".");if(!dir)return false;bool ok=true;
  for(;;){errno=0;auto* e=readdir(dir);if(!e){ok=errno==0;break;}if(!std::strcmp(e->d_name,".") || !std::strcmp(e->d_name,".."))continue;
   char full[256];struct stat info{};if(count==capacity || !join_path(full,sizeof full,checked,e->d_name) || lstat(full,&info)!=0 || (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode)) || !copy_text(out[count].name,sizeof out[count].name,e->d_name)){ok=false;break;}
   out[count].directory=S_ISDIR(info.st_mode);out[count++].bytes=uint64_t(info.st_size);
  }
  return closedir(dir)==0 && ok;
 }
 bool list_browser(const char* path,FileEntry* out,unsigned capacity,unsigned& count)override{
  count=0;char checked[192];if(!directory_path(path,checked))return false;
  DIR* dir=opendir(*checked?checked:".");if(!dir)return false;bool ok=true;
  for(;;){errno=0;auto* e=readdir(dir);if(!e){ok=errno==0;break;}if(e->d_name[0]=='.')continue;
   char full[256];struct stat info{};if(!join_path(full,sizeof full,checked,e->d_name) || lstat(full,&info)!=0){ok=false;break;}
   if((!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode)) || !data_browser_entry(checked,e->d_name,S_ISDIR(info.st_mode)))continue;
   if(count==capacity){ok=false;break;}if(!copy_text(out[count].name,sizeof out[count].name,e->d_name)){ok=false;break;}
   out[count].directory=S_ISDIR(info.st_mode);out[count++].bytes=uint64_t(info.st_size);
  }
  return closedir(dir)==0 && ok;
 }
 bool read(const char* path,uint64_t offset,void* data,unsigned wanted,unsigned& got)override{got=0;FILE* f=std::fopen(path,"rb");if(!f)return false;bool ok=fseeko(f,off_t(offset),SEEK_SET)==0;if(ok){got=std::fread(data,1,wanted,f);ok=!std::ferror(f);}return std::fclose(f)==0 && ok;}
 bool write(const char* path,uint64_t offset,const void* data,unsigned n,bool truncate)override{FILE* f=std::fopen(path,truncate?"wb":"r+b");if(!f)return false;bool ok=fseeko(f,off_t(offset),SEEK_SET)==0 && std::fwrite(data,1,n,f)==n && std::fflush(f)==0 && fsync(fileno(f))==0;return std::fclose(f)==0 && ok;}
 bool move(const char* a,const char* b)override{return std::rename(a,b)==0;}bool remove(const char* p)override{return std::remove(p)==0 || access(p,F_OK)!=0;}
 bool directory(const char* p)override{struct stat s{};if(stat(p,&s)==0)return S_ISDIR(s.st_mode);return mkdir(p,0755)==0;}
 bool exists(const char* p)override{struct stat s{};return stat(p,&s)==0;}
 uint64_t size(const char* p)override{struct stat s{};return stat(p,&s)==0?uint64_t(s.st_size):0;}
 bool revision(const char* p,FileRevision& r)override{struct stat s{};if(stat(p,&s)!=0 || !S_ISREG(s.st_mode))return false;r.size=uint64_t(s.st_size);r.identity=uint64_t(s.st_ino);r.modified=uint64_t(s.st_mtim.tv_sec);r.detail=uint64_t(s.st_mtim.tv_nsec);return true;}
};
}
