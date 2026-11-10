#include "project_load_slot.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>
using namespace groove;
struct Sink {std::vector<uint8_t> data;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}};
struct Files final:IFileAccess {
 std::vector<uint8_t> bytes;unsigned reads=0,largest=0;bool fail=false;uint64_t rev=1;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t offset,void* data,unsigned n,unsigned& got)override{++reads;largest=std::max(largest,n);if(fail || offset>bytes.size())return false;got=unsigned(std::min<uint64_t>(n,bytes.size()-offset));std::memcpy(data,bytes.data()+offset,got);return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}
 bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 uint64_t size(const char*)override{return bytes.size();}bool exists(const char*)override{return true;}bool revision(const char*,FileRevision& r)override{r.size=bytes.size();r.modified=rev;return true;}
};
int main(){auto original=std::make_unique<ProjectState>();make_empty_project(*original);original->tempo=173;Sink encoded;assert(encode_project(encoded,*original));Files fs;fs.bytes=std::move(encoded.data);ProjectDecodeSlot worker;assert(worker.initialize(nullptr) && worker.pooled());AsyncProjectRead reader(fs,worker);assert(reader.start("kit.mgx"));
 std::thread thread([&]{while(!worker.process_one())std::this_thread::yield();});unsigned calls=0;while(reader.busy()){unsigned before=fs.reads;reader.service();assert(fs.reads-before<=1);++calls;std::this_thread::yield();}thread.join();ProjectState* loaded=nullptr;unsigned version=0;assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Ready && loaded && loaded->tempo==173 && version==100);assert(worker.recycle(loaded));loaded=nullptr;assert(fs.largest<=kFileScanBufferBytes && fs.reads>1 && calls>=fs.reads);
 fs.bytes[10]^=1;assert(reader.start("bad.mgx"));std::thread corrupt([&]{while(!worker.process_one())std::this_thread::yield();});while(reader.busy()){reader.service();std::this_thread::yield();}corrupt.join();assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Failed && !loaded);
 fs.bytes[10]^=1;assert(reader.start("again.mgx"));std::thread again([&]{while(!worker.process_one())std::this_thread::yield();});while(reader.busy()){reader.service();std::this_thread::yield();}again.join();assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Ready && loaded && loaded->tempo==173);assert(worker.recycle(loaded));loaded=nullptr;
 fs.fail=true;assert(!reader.start("bad.mgx") && !reader.busy() && !worker.busy());fs.fail=false;assert(reader.start("changed.mgx"));reader.service();++fs.rev;while(reader.busy())reader.service();assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Failed && !loaded && !worker.busy());// Cancellation at read, queued decode, ready decode, and uncollected outcome.
 fs.fail=false;
 auto to_decode=[&]{unsigned budget=0;while(!reader.decoding()){reader.service();assert(reader.busy() && ++budget<10000);}};
 auto cancelled=[&]{assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Cancelled && !loaded && !version && !worker.busy());};
 unsigned before_cancel_reads=fs.reads;assert(reader.start("cancel-read.mgxs") && reader.cancel());cancelled();assert(fs.reads==before_cancel_reads+1);
 assert(reader.start("cancel-queued.mgxs"));to_decode();assert(reader.cancel() && worker.process_one());reader.service();cancelled();
 assert(reader.start("cancel-decoded.mgxs"));to_decode();assert(worker.process_one() && reader.cancel());reader.service();cancelled();
 assert(reader.start("cancel-result.mgxs"));to_decode();assert(worker.process_one());reader.service();assert(!reader.busy() && reader.cancel());cancelled();
 assert(!reader.cancel());assert(reader.start("after-cancel.mgxs"));to_decode();assert(worker.process_one());reader.service();assert(reader.take(loaded,version)==AsyncProjectRead::Outcome::Ready && loaded && loaded->tempo==173 && worker.recycle(loaded));loaded=nullptr;
 puts("incremental read, worker decode and failure handling passed");}
