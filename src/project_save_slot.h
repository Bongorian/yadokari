#pragma once
#include "project_codec.h"
#include "project_files.h"
#include "alloc_policy.h"
#include <stddef.h>
#include <stdint.h>
namespace groove {
using ProjectSaveCopy=PreservedCopy;
// One UI-side producer and one core-2 consumer. The producer hands ownership
// of an immutable snapshot to the slot and retains the slot until collection.
class ProjectEncodeSlot {
 static_assert(__atomic_always_lock_free(sizeof(unsigned),nullptr),"project worker requires lock-free state");
 static_assert(__is_trivially_copyable(ProjectState),"snapshot pacing copies object representation");

public:
 using SnapshotCopy=ProjectSaveCopy;
 using SnapshotBegin=void (*)(void*,size_t);
 using SnapshotEnd=void (*)(void*,bool);
 // Reserve before audio starts. The producer can pace the copy without
 // dispatching edits, retaining point-in-time semantics and a single owner.
 ProjectEncodeSlot():cached_snapshot_(alloc_new<ProjectState>()){}
 // The task must stop/join the consumer before destroying the slot.
 ~ProjectEncodeSlot(){unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);if(state==1 && owns_snapshot_)delete snapshot_;if(state==2)delete[] result_.bytes;delete cached_snapshot_;}
 ProjectEncodeSlot(const ProjectEncodeSlot&)=delete;ProjectEncodeSlot& operator=(const ProjectEncodeSlot&)=delete;
 void set_snapshot_copy(SnapshotCopy copy,void* context,SnapshotBegin begin=nullptr,SnapshotEnd end=nullptr){copy_=copy;copy_context_=context;snapshot_begin_=begin;snapshot_end_=end;}
 void set_buffer_copy(ProjectSaveCopy copy,void* context){buffer_copy_=copy;buffer_context_=context;}
 struct Result {uint8_t* bytes=nullptr;unsigned size=0;bool ok=false;};
 bool submit(ProjectState* snapshot,const char* path,const PreservedStore* store=nullptr){
  if(!snapshot || !prepare(path))return false;
  if(snapshot_begin_)snapshot_begin_(copy_context_,store?store->used_bytes():0);
  bool ok=copy_store(store);
  if(snapshot_end_)snapshot_end_(copy_context_,ok);
  if(ok)publish(snapshot,true);
  return ok;
 }
 bool submit_copy(const ProjectState& project,const char* path,const PreservedStore* store=nullptr){
  if(!cached_snapshot_ || !prepare(path))return false;
  // Copy the split POD state through the existing paced callback. Phrase
  // KIT events also belong to inactive/overridden instruments and must survive.
  size_t bytes=sizeof project;if(store)bytes+=store->used_bytes();
  if(snapshot_begin_)snapshot_begin_(copy_context_,bytes);
  auto* dst=reinterpret_cast<uint8_t*>(cached_snapshot_);const auto* src=reinterpret_cast<const uint8_t*>(&project);
  bool ok=true;for(size_t at=0;at<sizeof project;at+=16384){size_t n=sizeof project-at;if(n>16384)n=16384;if(copy_){if(!copy_(copy_context_,dst+at,src+at,n)){ok=false;break;}}else __builtin_memcpy(dst+at,src+at,n);}
  if(ok)ok=copy_store(store);
  if(snapshot_end_)snapshot_end_(copy_context_,ok);
  if(!ok)return false;
  publish(cached_snapshot_,false);return true;
 }
 bool process_one(){
  if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;
  ChunkBuffer encoded(buffer_copy_,buffer_context_);
  bool ok=project_paths(*snapshot_,path_,false) && encode_project_buffer(encoded,*snapshot_,&store_);
  if(ok){ChunkSource source{encoded.data,encoded.size,0};ok=validate_snapshot_encoding(source,*snapshot_);}
  if(owns_snapshot_)delete snapshot_;
  snapshot_=nullptr;unsigned size=ok?encoded.size:0;
  result_={ok?encoded.release():nullptr,size,ok};
  __atomic_store_n(&state_,2u,__ATOMIC_RELEASE);wake();return true;
 }
 bool collect(Result& result){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=2)return false;result=result_;result_={};__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;}
 bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
private:
 bool prepare(const char* path){return path && __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==0 && copy_text(path_,sizeof path_,path);}
 bool copy_store(const PreservedStore* store){if(store)return store_.copy_from(*store,copy_,copy_context_);store_.clear();return true;}
 void publish(ProjectState* snapshot,bool own){
  // End the entire producer copy batch before releasing the job. Worker
  // buffer pacing must never overlap producer-side m_CopyBatch updates.
  snapshot_=snapshot;owns_snapshot_=own;__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);wake();
 }

 static void wake(){
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 }
 ProjectState* snapshot_=nullptr;ProjectState* cached_snapshot_=nullptr;SnapshotCopy copy_=nullptr;void* copy_context_=nullptr;SnapshotBegin snapshot_begin_=nullptr;SnapshotEnd snapshot_end_=nullptr;ProjectSaveCopy buffer_copy_=nullptr;void* buffer_context_=nullptr;bool owns_snapshot_=true;char path_[192]{};Result result_{};unsigned state_=0;PreservedStore store_;
};

// Main-loop-owned SD state machine. Each service call issues at most one
// 8 KiB read or write, apart from the final remove/rename promotion. The
// snapshot has already been encoded and decoded on the worker.
class AsyncProjectSave {
public:
 enum class Outcome:uint8_t { None,Saved,Failed };
 AsyncProjectSave(IFileAccess& fs,ProjectEncodeSlot& worker):fs_(fs),worker_(worker){}
 ~AsyncProjectSave(){delete[] bytes_;}
 AsyncProjectSave(const AsyncProjectSave&)=delete;AsyncProjectSave& operator=(const AsyncProjectSave&)=delete;
 bool start(const ProjectState& project,const char* path,bool remove_after=false,const PreservedStore* store=nullptr){
  if(!path || busy() || outcome_!=Outcome::None || !copy_text(path_,sizeof path_,path) || !copy_text(temp_,sizeof temp_,path))return false;
  unsigned n=0;while(temp_[n])++n;if(!copy_text(temp_+n,sizeof temp_-n,".tmp"))return false;
  // A failed promotion may have left the only complete generation here.
  // Restore its name before reusing the temporary path; never truncate it.
  if(!prepare_project_save(fs_,path_,temp_))return false;
  if(!worker_.submit_copy(project,path_,store))return false;
  remove_after_=remove_after;outcome_=Outcome::None;phase_=Phase::Encoding;offset_=0;size_=0;return true;
 }
 unsigned service_phase()const{return unsigned(phase_);}
 bool busy()const{return phase_!=Phase::Idle;}
 Outcome take_outcome(){Outcome o=outcome_;outcome_=Outcome::None;return o;}
 void service(bool snapshot_safe,bool event_loss){
  if(phase_==Phase::Idle)return;
  if(phase_==Phase::Encoding){ProjectEncodeSlot::Result result;if(!worker_.collect(result))return;if(!result.ok || !result.bytes || !result.size){delete[] result.bytes;fs_.remove(temp_);finish(false);return;}bytes_=result.bytes;size_=result.size;phase_=Phase::Prepare;return;}
  if(event_loss){fail();return;}
  if(phase_==Phase::Prepare){char parent[192];if(!copy_text(parent,sizeof parent,path_)){fail();return;}parent_path(parent);if((parent[0] && !fs_.directory(parent)) || !fs_.remove(temp_)){fail();return;}phase_=Phase::Write;return;}
  if(phase_==Phase::Write){unsigned count=size_-offset_>8192?8192:size_-offset_;if(!fs_.write(temp_,offset_,bytes_+offset_,count,offset_==0)){fail();return;}offset_+=count;if(offset_==size_){offset_=0;phase_=Phase::CheckSize;}return;}
  if(phase_==Phase::CheckSize){if(fs_.size(temp_)!=size_ || !scratch_.data()){fail();return;}phase_=Phase::Verify;return;}
  if(phase_==Phase::Verify){unsigned count=size_-offset_>8192?8192:size_-offset_,got=0;if(!fs_.read(temp_,offset_,scratch_.data(),count,got) || got!=count || __builtin_memcmp(scratch_.data(),bytes_+offset_,count)){fail();return;}offset_+=count;if(offset_==size_)phase_=Phase::Promote;return;}
  if(!snapshot_safe)return;
  // From this point the temporary file is verified. Keep it on failure so
  // normal recovery can load it even when the old main was already removed.
  if((fs_.exists(path_) && !fs_.remove(path_)) || !fs_.move(temp_,path_)){finish(false);return;}
  if(remove_after_ && !fs_.remove(path_)){finish(false);return;}finish(true);
 }
private:
 enum class Phase:uint8_t {Idle,Encoding,Prepare,Write,CheckSize,Verify,Promote};
 void finish(bool success){delete[] bytes_;bytes_=nullptr;size_=offset_=0;phase_=Phase::Idle;outcome_=success?Outcome::Saved:Outcome::Failed;}
 void fail(){fs_.remove(temp_);finish(false);}
 IFileAccess& fs_;ProjectEncodeSlot& worker_;FileScanBuffer scratch_;char path_[192]{},temp_[192]{};uint8_t* bytes_=nullptr;unsigned size_=0,offset_=0;bool remove_after_=false;Phase phase_=Phase::Idle;Outcome outcome_=Outcome::None;
};
}
