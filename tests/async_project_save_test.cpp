#include "project_save_slot.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
struct MemoryFiles final:groove::IFileAccess {
 std::map<std::string,std::vector<uint8_t>> files;unsigned reads=0,writes=0,fail_write=0;bool corrupt=false,fail_move=false;
 unsigned list(const char*,groove::FileEntry*,unsigned)override{return 0;}
 bool read(const char* path,uint64_t offset,void* out,unsigned count,unsigned& got)override{++reads;auto it=files.find(path);if(it==files.end() || offset>it->second.size())return false;got=unsigned(std::min<uint64_t>(count,it->second.size()-offset));if(got)std::memcpy(out,it->second.data()+offset,got);if(got && corrupt)static_cast<uint8_t*>(out)[0]^=1;return true;}
 bool write(const char* path,uint64_t offset,const void* data,unsigned count,bool truncate)override{++writes;if(writes==fail_write)return false;auto& dest=files[path];if(truncate)dest.clear();if(offset!=dest.size())return false;auto* p=static_cast<const uint8_t*>(data);dest.insert(dest.end(),p,p+count);return true;}
 bool move(const char* from,const char* to)override{if(fail_move)return false;auto it=files.find(from);if(it==files.end() || files.count(to))return false;files[to]=std::move(it->second);files.erase(it);return true;}
 bool remove(const char* path)override{files.erase(path);return true;}bool directory(const char*)override{return true;}
 uint64_t size(const char* path)override{auto it=files.find(path);return it==files.end()?0:it->second.size();}bool exists(const char* path)override{return files.count(path)!=0;}
};
struct Source {const std::vector<uint8_t>& bytes;size_t at=0;bool read(uint8_t* out,size_t n){if(n>bytes.size()-at)return false;std::memcpy(out,bytes.data()+at,n);at+=n;return true;}bool at_end()const{return at==bytes.size();}};
struct SnapshotCopies {
 void* storage=nullptr;unsigned calls=0,begin_calls=0,end_calls=0,fail_at=0;size_t bytes=0,planned=0;bool reject=true,last_ok=false;
 static void begin(void* context,size_t n){auto& self=*static_cast<SnapshotCopies*>(context);++self.begin_calls;self.planned=n;}
 static void end(void* context,bool ok){auto& self=*static_cast<SnapshotCopies*>(context);++self.end_calls;self.last_ok=ok;}
 static bool copy(void* context,void* dst,const void* src,size_t size){
  auto& self=*static_cast<SnapshotCopies*>(context);++self.calls;
  if(!self.storage){self.storage=dst;}
  assert(static_cast<uint8_t*>(dst)>=static_cast<uint8_t*>(self.storage) && static_cast<uint8_t*>(dst)+size<=static_cast<uint8_t*>(self.storage)+sizeof(groove::ProjectState));
  if(self.reject || (self.fail_at && self.calls==self.fail_at))return false;
  self.bytes+=size;std::memcpy(dst,src,size);return true;
 }
};
struct BufferCopies {
 bool reject=true;unsigned calls=0;
 static bool copy(void* context,void* dst,const void* src,size_t n){auto& self=*static_cast<BufferCopies*>(context);++self.calls;if(self.reject)return false;std::memcpy(dst,src,n);return true;}
};
struct CompositeCopies {
 groove::ProjectEncodeSlot* slot;const void* payload=nullptr;size_t planned=0,bytes=0;unsigned payload_calls=0;bool active=false,reject=false,ok=false;
 static void begin(void* p,size_t n){auto& c=*static_cast<CompositeCopies*>(p);assert(!c.active && !c.slot->busy());c.active=true;c.planned=n;c.bytes=0;}
 static bool copy(void* p,void* dst,const void* src,size_t n){auto& c=*static_cast<CompositeCopies*>(p);assert(c.active && !c.slot->busy());if(src==c.payload){++c.payload_calls;if(c.reject)return false;}c.bytes+=n;std::memcpy(dst,src,n);return true;}
 static void end(void* p,bool ok){auto& c=*static_cast<CompositeCopies*>(p);assert(c.active && !c.slot->busy() && !c.slot->process_one());c.active=false;c.ok=ok;}
};
int main(){using namespace groove;auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=137;MemoryFiles fs;fs.files["GROOVE/SESS.MGX"]={1,2,3};ProjectEncodeSlot worker;AsyncProjectSave save(fs,worker);
 SnapshotCopies copies;worker.set_snapshot_copy(SnapshotCopies::copy,&copies,SnapshotCopies::begin,SnapshotCopies::end);
 assert(!save.start(*p,"GROOVE/SESS.MGX") && !save.busy() && !worker.busy());
 assert(fs.files["GROOVE/SESS.MGX"]==std::vector<uint8_t>({1,2,3}));assert(copies.begin_calls==1 && copies.end_calls==1 && !copies.last_ok);copies.reject=false;
 assert(save.start(*p,"GROOVE/SESS.MGX"));p->tempo=180;std::thread thread([&](){assert(worker.process_one());});thread.join();unsigned ticks=0;do {unsigned before_w=fs.writes,before_r=fs.reads;save.service(false,false);assert(fs.writes-before_w<=1 && fs.reads-before_r<=1);assert(++ticks<10000);}while(fs.reads==0 || fs.reads<fs.writes);for(unsigned i=0;i<3;++i)save.service(false,false);assert(save.busy() && fs.files["GROOVE/SESS.MGX"]==std::vector<uint8_t>({1,2,3}));while(save.busy()){save.service(true,false);std::this_thread::yield();}assert(save.take_outcome()==AsyncProjectSave::Outcome::Saved && ticks>0 && !fs.exists("GROOVE/SESS.MGX.tmp"));auto restored=std::make_unique<ProjectState>();Source source{fs.files["GROOVE/SESS.MGX"]};assert(decode_project(source,*restored) && restored->tempo==137);
 assert(copies.planned==sizeof(ProjectState) && copies.bytes==copies.planned && copies.last_ok && copies.end_calls==copies.begin_calls);
 // Phrase KIT events survive even while the persistent track uses another engine.
 track_at(*p,3,5).instrument=Instrument::DrumKit;track_at(*p,3,5).kit.steps[2][17].enabled=true;track_at(*p,3,5).kit.steps[2][17].velocity=111;
 size_t copied_before=copies.bytes;assert(worker.submit_copy(*p,"GROOVE/K.MGX"));assert(copies.bytes-copied_before==copies.planned && copies.planned==sizeof(ProjectState));assert(worker.process_one());ProjectEncodeSlot::Result result;assert(worker.collect(result) && result.ok);std::vector<uint8_t> active(result.bytes,result.bytes+result.size);delete[] result.bytes;Source active_source{active};assert(decode_project(active_source,*restored) && track_at(*restored,3,5).kit.steps[2][17].velocity==111);
 track_at(*p,3,5).instrument=Instrument::DaisyAnalogDrums;assert(worker.submit_copy(*p,"GROOVE/K.MGX") && worker.process_one());assert(worker.collect(result) && result.ok);std::vector<uint8_t> inactive(result.bytes,result.bytes+result.size);delete[] result.bytes;Source inactive_source{inactive};assert(decode_project(inactive_source,*restored) && track_at(*restored,3,5).kit.steps[2][17].enabled);assert(track_at(*p,3,5).kit.steps[2][17].enabled);
 std::printf("Snapshot payload: %zu / %zu bytes (all phrase events retained)\n",copies.planned,sizeof(ProjectState));
 copies.fail_at=copies.calls+3;auto before_partial_failure=fs.files["GROOVE/SESS.MGX"];assert(!worker.submit_copy(*p,"GROOVE/SESS.MGX") && !worker.busy() && !copies.last_ok && copies.begin_calls==copies.end_calls);assert(fs.files["GROOVE/SESS.MGX"]==before_partial_failure);copies.fail_at=0;
 BufferCopies buffers;worker.set_buffer_copy(BufferCopies::copy,&buffers);auto before_buffer_failure=fs.files["GROOVE/SESS.MGX"];
 assert(save.start(*p,"GROOVE/SESS.MGX") && worker.process_one());while(save.busy())save.service(true,false);
 assert(buffers.calls && save.take_outcome()==AsyncProjectSave::Outcome::Failed && fs.files["GROOVE/SESS.MGX"]==before_buffer_failure);buffers.reject=false;
 fs.fail_write=fs.writes+2;auto original=fs.files["GROOVE/SESS.MGX"];assert(save.start(*p,"GROOVE/SESS.MGX"));std::thread failure([&](){while(!worker.process_one())std::this_thread::yield();});while(save.busy()){save.service(true,false);std::this_thread::yield();}failure.join();assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed && fs.files["GROOVE/SESS.MGX"]==original && !fs.exists("GROOVE/SESS.MGX.tmp"));
 fs.fail_write=0;fs.corrupt=true;assert(save.start(*p,"GROOVE/SESS.MGX"));std::thread corrupt([&](){while(!worker.process_one())std::this_thread::yield();});while(save.busy()){save.service(true,false);std::this_thread::yield();}corrupt.join();assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed && fs.files["GROOVE/SESS.MGX"]==original && !fs.exists("GROOVE/SESS.MGX.tmp"));
 // Promotion failures must retain the complete new generation, including
 // a second save attempted while rename is still unavailable.
 fs.corrupt=false;fs.fail_move=true;assert(save.start(*p,"GROOVE/SESS.MGX"));assert(worker.process_one());
 while(save.busy())save.service(true,false);
 assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed && !fs.exists("GROOVE/SESS.MGX") && fs.exists("GROOVE/SESS.MGX.tmp"));
 auto retained=fs.files["GROOVE/SESS.MGX.tmp"];Source retained_source{retained};assert(decode_project(retained_source,*restored) && restored->tempo==180);
 assert(!save.start(*p,"GROOVE/SESS.MGX") && fs.files["GROOVE/SESS.MGX.tmp"]==retained);
 fs.fail_move=false;fs.fail_write=fs.writes+1;p->tempo=190;
 assert(save.start(*p,"GROOVE/SESS.MGX") && fs.files["GROOVE/SESS.MGX"]==retained);assert(worker.process_one());
 while(save.busy())save.service(true,false);
 assert(save.take_outcome()==AsyncProjectSave::Outcome::Failed && fs.files["GROOVE/SESS.MGX"]==retained && !fs.exists("GROOVE/SESS.MGX.tmp"));
 fs.fail_write=0;assert(save.start(*p,"GROOVE/PROBE.MGXS",true));std::thread probe([&](){while(!worker.process_one())std::this_thread::yield();});while(save.busy()){save.service(true,false);std::this_thread::yield();}probe.join();assert(save.take_outcome()==AsyncProjectSave::Outcome::Saved && !fs.exists("GROOVE/PROBE.MGXS") && !fs.exists("GROOVE/PROBE.MGXS.tmp"));// Project and foreign bytes share one Begin/End budget; publication is
 // after End, and a failed sidecar copy cannot expose a half-snapshot.
 PreservedStore foreign;std::vector<uint8_t> payload(65536,0x6d);assert(foreign.add_chunk(0x51515151,7,19,payload.data(),payload.size()));
 CompositeCopies batch{&worker,foreign.chunks[0].payload.data};worker.set_snapshot_copy(CompositeCopies::copy,&batch,CompositeCopies::begin,CompositeCopies::end);
 batch.reject=true;assert(!save.start(*p,"GROOVE/PACED.MGX",false,&foreign) && !worker.busy() && !batch.ok && !batch.active && batch.payload_calls==1 && !fs.exists("GROOVE/PACED.MGX"));
 batch.reject=false;assert(worker.submit_copy(*p,"GROOVE/PACED.MGX",&foreign));assert(batch.ok && batch.bytes==batch.planned && batch.planned==sizeof(ProjectState)+payload.size());
 foreign.chunks[0].payload.data[0]=0;assert(worker.process_one() && worker.collect(result) && result.ok);
 ChunkSource composite{result.bytes,result.size,0};PreservedStore roundtrip;assert(decode_project(composite,*restored,&roundtrip) && roundtrip.chunk_count==1 && roundtrip.chunks[0].flags==19 && roundtrip.chunks[0].payload.data[0]==0x6d);delete[] result.bytes;
 auto owned=std::make_unique<ProjectState>(*p);assert(worker.submit(owned.get(),"GROOVE/PACED.MGX",&foreign));owned.release();assert(batch.ok && batch.planned==payload.size() && batch.bytes==batch.planned && worker.process_one() && worker.collect(result) && result.ok);delete[] result.bytes;
 puts("async save snapshot, bounded I/O, guard, failure, verification and cleanup passed");}
