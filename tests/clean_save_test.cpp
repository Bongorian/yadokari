#include "project_files.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <unistd.h>
struct CleanFiles:groove::DesktopFiles { bool fail_move=false;bool move(const char* a,const char* b)override{return !fail_move && DesktopFiles::move(a,b);} };
int main(){
 char dir[]="build/clean-save-XXXXXX";assert(mkdtemp(dir));char path[160],bak[180],tmp[180];
 std::snprintf(path,sizeof path,"%s/song.mgx",dir);std::snprintf(bak,sizeof bak,"%s.bak",path);std::snprintf(tmp,sizeof tmp,"%s.tmp",path);
 CleanFiles fs;auto p=std::make_unique<groove::ProjectState>();groove::make_empty_project(*p);p->tempo=111;
 assert(groove::save_recoverable(fs,path,*p));p->tempo=122;assert(groove::save_recoverable(fs,path,*p));
 assert(!fs.exists(bak) && !fs.exists(tmp));auto result=std::make_unique<groove::ProjectState>();assert(groove::read_project_file(fs,path,*result) && result->tempo==122);
 p->tempo=133;assert(!groove::save_recoverable_checked(fs,path,*p,[]{return false;}));
 assert(!fs.exists(bak) && !fs.exists(tmp) && groove::read_project_file(fs,path,*result) && result->tempo==122);
 // No-backup mode still retains the verified replacement if rename fails.
 fs.fail_move=true;p->tempo=133;assert(!groove::save_recoverable(fs,path,*p));
 assert(!fs.exists(path) && fs.exists(tmp) && groove::read_project_file(fs,tmp,*result) && result->tempo==133);
 assert(!groove::save_recoverable(fs,path,*p) && fs.exists(tmp));
 fs.fail_move=false;assert(!groove::save_recoverable_checked(fs,path,*p,[]{return false;}));
 assert(fs.exists(path) && !fs.exists(tmp) && groove::read_project_file(fs,path,*result) && result->tempo==133);
 unsigned guards=0;p->tempo=139;assert(!groove::save_recoverable_checked(fs,path,*p,[&](){return ++guards==1;}));
 assert(fs.exists(path) && groove::read_project_file(fs,path,*result) && result->tempo==139);
 // Recovery candidates are decoded into a fresh object; rejected input must
 // not mutate the caller, and a valid temporary candidate can still recover.
 p->tempo=144;assert(groove::save_recoverable(fs,tmp,*p));uint8_t bad[4]={0,0,0,0};assert(fs.write(path,0,bad,4,false));groove::RecoverySource source{};assert(groove::load_recoverable(fs,path,*result,nullptr,&source) && result->tempo==144 && source.kind==groove::RecoveryKind::Temporary);assert(fs.write(tmp,0,bad,4,false));result->tempo=177;assert(!groove::load_recoverable(fs,path,*result) && result->tempo==177);
}
