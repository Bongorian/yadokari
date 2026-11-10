#pragma once
#include "soundfont.h"
namespace groove {
// One file-service producer and one decode worker. The worker requests bounded
// reads; only the producer touches IFileAccess/FatFs. Buffers remain worker-owned
// until the producer acknowledges, including on cancellation. No DSP access.
class SoundFontLoadSlot {
 unsigned state_=0,read_state_=0,cancel_=0; // idle, queued, running, complete
 SoundFontCache* cache_=nullptr;char path_[96]{};uint64_t size_=0,offset_=0;
 void* destination_=nullptr;unsigned wanted_=0,got_=0;bool read_ok_=false,ok_=false;
 uint32_t expected_crc_=0;WavView result_{};
 static void wake(){
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 }
 bool cancelled()const{return __atomic_load_n(&cancel_,__ATOMIC_ACQUIRE)!=0;}
 bool request(uint64_t offset,void* data,unsigned wanted,unsigned& got){
  got=0;if(cancelled() || wanted>65536 || offset>size_ || wanted>size_-offset)return false;
  offset_=offset;destination_=data;wanted_=wanted;
  __atomic_store_n(&read_state_,1u,__ATOMIC_RELEASE);wake();
  // The file owner must acknowledge even a cancelled read before its buffer
  // can be released. The file owner continues servicing stale jobs to join.
  while(__atomic_load_n(&read_state_,__ATOMIC_ACQUIRE)!=2){
#if defined(__aarch64__)
   asm volatile("yield" ::: "memory");
#endif
  }
  got=got_;bool ok=read_ok_ && !cancelled();__atomic_store_n(&read_state_,0u,__ATOMIC_RELEASE);return ok;
 }
 class Files:public IFileAccess {
  SoundFontLoadSlot& slot_;
 public:
  explicit Files(SoundFontLoadSlot& slot):slot_(slot){}
  unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
  bool read(const char*,uint64_t offset,void* data,unsigned wanted,unsigned& got)override{return slot_.request(offset,data,wanted,got);}
  uint64_t size(const char*)override{return slot_.size_;}
  bool exists(const char*)override{return true;}
  bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}
  bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 };
public:
 bool submit(SoundFontCache& cache,const char* path,uint64_t bytes,uint32_t crc){
  if(busy() || !path || bytes<12 || !copy_text(path_,sizeof path_,path))return false;
  cache_=&cache;size_=bytes;expected_crc_=crc;result_={};ok_=false;
  __atomic_store_n(&cancel_,0u,__ATOMIC_RELAXED);__atomic_store_n(&read_state_,0u,__ATOMIC_RELAXED);
  __atomic_store_n(&state_,1u,__ATOMIC_RELEASE);wake();return true;
 }
 bool process_one(){
  if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;
  __atomic_store_n(&state_,2u,__ATOMIC_RELEASE);Files files(*this);
  ok_=!cancelled() && cache_->load(files,path_,result_);
  ok_=ok_ && !cancelled() && (!expected_crc_ || soundfont_crc(result_.soundfont)==expected_crc_);
  __atomic_store_n(&state_,3u,__ATOMIC_RELEASE);return true;
 }
 bool service(IFileAccess& files){
  if(__atomic_load_n(&read_state_,__ATOMIC_ACQUIRE)!=1)return false;
  got_=0;read_ok_=!cancelled() && files.read(path_,offset_,destination_,wanted_,got_) && got_==wanted_;
  __atomic_store_n(&read_state_,2u,__ATOMIC_RELEASE);wake();return true;
 }
 void cancel(){__atomic_store_n(&cancel_,1u,__ATOMIC_RELEASE);wake();}
 bool collect(WavView& out,bool& ok){
  if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=3)return false;
  ok=ok_ && !cancelled();if(ok)out=result_;
  __atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;
 }
 bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
};
}
