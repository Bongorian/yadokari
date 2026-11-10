#include "project_save_slot.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace groove;
struct FaultFiles final:IFileAccess {
 enum Fault {None,Write,ShortWrite,Read,ShortRead,Size,Remove,Move};Fault fault=None;
 std::map<std::string,std::vector<uint8_t>> files;unsigned moves=0,probes=0;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char* p,uint64_t off,void* dst,unsigned n,unsigned& got)override{
  got=0;auto i=files.find(p);if(i==files.end() || off>i->second.size() || (fault==Read && std::strstr(p,".tmp")))return false;
  got=unsigned(std::min<uint64_t>(n,i->second.size()-off));if(got && fault==ShortRead && std::strstr(p,".tmp"))--got;
  if(got){std::memcpy(dst,i->second.data()+off,got);}return true;
 }
 bool write(const char* p,uint64_t off,const void* src,unsigned n,bool truncate)override{
  if(fault==Write){return false;}auto& out=files[p];if(truncate)out.clear();
  if(fault==ShortWrite && n){--n;}auto* bytes=static_cast<const uint8_t*>(src);out.resize(size_t(off)+n);if(n){std::memcpy(out.data()+off,bytes,n);}return true;
 }
 bool move(const char* a,const char* b)override{++moves;if(fault==Move || !files.count(a) || files.count(b))return false;files[b]=std::move(files[a]);files.erase(a);return true;}
 bool remove(const char* p)override{if(fault==Remove && !std::strstr(p,".tmp"))return false;files.erase(p);return true;}
 bool directory(const char*)override{return true;}
 uint64_t size(const char* p)override{auto i=files.find(p);return i==files.end()?0:i->second.size()+(fault==Size && std::strstr(p,".tmp")?1:0);}
 bool exists(const char* p)override{++probes;return files.count(p);}
};
static bool refuse_copy(void*,void*,const void*,size_t){return false;}
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);project->tempo=137;
 ChunkBuffer encoded;assert(encode_project_buffer(encoded,*project));std::vector<uint8_t> valid(encoded.data,encoded.data+encoded.size);
 const char* main="song.mgxs";const char* temporary="song.mgxs.tmp";
 FaultFiles fs;fs.files[temporary]={1,2,3};auto partial=fs.files[temporary];
 assert(!prepare_project_save(fs,main,temporary) && !fs.moves && fs.files[temporary]==partial && !fs.files.count(main));
 fs.files[main]={4,5,6};fs.files[temporary]=valid;
 assert(!prepare_project_save(fs,main,temporary) && fs.files[temporary]==valid && fs.files[main]==std::vector<uint8_t>({4,5,6}));
 auto loaded=std::make_unique<ProjectState>();loaded->tempo=191;RecoverySource source;
 assert(load_recoverable(fs,main,*loaded,nullptr,&source) && source.kind==RecoveryKind::Temporary && loaded->tempo==137);
 fs.files.clear();fs.files["song.mgxs.bak"]=valid;fs.files["song.mgxs.new000"]=valid;fs.probes=0;
 assert(!load_recoverable(fs,main,*loaded) && loaded->tempo==137 && fs.probes<=2);
 fs.files.clear();fs.files[temporary]=valid;fs.fault=FaultFiles::Move;
 assert(!prepare_project_save(fs,main,temporary) && fs.files[temporary]==valid);
 fs.fault=FaultFiles::None;ProjectEncodeSlot worker;worker.set_snapshot_copy(refuse_copy,nullptr);AsyncProjectSave save(fs,worker);
 assert(!save.start(*project,main) && fs.files[main]==valid && !fs.files.count(temporary));worker.set_snapshot_copy(nullptr,nullptr);
 project->tempo=183;unsigned cases=0;
 for(auto fault:{FaultFiles::Write,FaultFiles::ShortWrite,FaultFiles::Read,FaultFiles::ShortRead,FaultFiles::Size,FaultFiles::Remove,FaultFiles::Move}){
  fs.files.clear();fs.files[main]=valid;fs.fault=FaultFiles::None;assert(save.start(*project,main) && worker.process_one());fs.fault=fault;
  unsigned calls=0;while(save.busy()){save.service(true,false);assert(++calls<10000);}
  assert(!save.start(*project,main));assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed);fs.fault=FaultFiles::None;
  assert(load_recoverable(fs,main,*loaded,nullptr,&source));
  if(fault==FaultFiles::Move){assert(loaded->tempo==183 && source.kind==RecoveryKind::Temporary);worker.set_snapshot_copy(refuse_copy,nullptr);assert(!save.start(*project,main) && fs.files.count(main) && !fs.files.count(temporary));worker.set_snapshot_copy(nullptr,nullptr);}
  else assert(loaded->tempo==137 && source.kind==RecoveryKind::Main);
  ++cases;
 }
 fs.files.clear();fs.files[main]=valid;assert(save.start(*project,main) && worker.process_one());while(save.busy())save.service(true,true);
 assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed && fs.files[main]==valid && !fs.files.count(temporary));
 std::printf("Current save: %u I/O faults, invalid/pending generation protection, outcome ownership and bounded main/tmp recovery passed\n",cases);
}
