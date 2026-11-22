#include "wave4_workload.h"
#include "../firmware/load_sf2_generator.h"
#include <cassert>
#include <memory>
#include <cstdio>
using namespace groove;
class CollisionFile:public IFileAccess {
public:
 unsigned writes=0,reads=0;uint64_t bytes=123;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t,void* p,unsigned n,unsigned& got)override{++reads;__builtin_memset(p,0,n);got=n;return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{++writes;return false;}
 bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}
 bool directory(const char*)override{return true;}uint64_t size(const char*)override{return bytes;}
 bool exists(const char*)override{return true;}
};
static void stream_checksums(){
 struct StreamFiles final:CollisionFile {
  bool exists(const char*)override{return false;}
  bool write(const char*,uint64_t,const void*,unsigned,bool)override{return true;}
 } files;
 struct Reader final:ISampleReader {
  bool read_frame(uint32_t,unsigned,float&)override{return false;}
  uint32_t underruns()const override{return 0;}
 } reader;
 struct Library final:ISampleLibrary {
  explicit Library(ISampleReader& source):source_(source){}
  bool load(const char*,WavView& view)override{view.reader=&source_;return true;}
  WavView lookup(const char*)const override{return {};}
  bool prepare(const ProjectState&,WavView*)override{return false;}
  ISampleReader& source_;
 } library(reader);
 AudioStressAssets streams(files);assert(streams.start());
 for(unsigned i=0;i<2400 && !streams.ready();++i)streams.service(library);
 assert(streams.ready());constexpr uint32_t expected[]={0x5f9f81bf,0xe0875c91,0x5eb218d7,0x6a032d26};
 for(unsigned i=0;i<4;++i)assert(streams.assets[i].metadata.content_crc==expected[i]);
}
int main(){stream_checksums();CollisionFile fs;LoadSf2Generator generator;assert(!generator.service(fs) && generator.failed() && !fs.writes && !fs.reads);
 generator.reset();fs.bytes=33557548;assert(!generator.service(fs) && generator.failed() && !fs.writes && fs.reads);
auto p=std::make_unique<ProjectState>();for(unsigned c=0;c<5;++c){make_wave4_workload(*p,c);assert(valid_project(*p));for(const auto& s:p->sounds)assert(s.instrument==Instrument::Empty);for(unsigned t=0;t<16;++t){auto v=track_at(*p,0,t);unsigned hits=0;for(unsigned i=0;i<16;++i)hits+=v.steps[i].enabled;assert(hits==4);}if(c==1 || c==2)assert(p->tracks[0].layer.count==(c==1?2:4));}puts("Wave4 workload: valid fixed attached state, four triggers/track and empty pool passed");}
