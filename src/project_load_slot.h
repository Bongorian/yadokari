#pragma once
#include "project_codec.h"
#include "file_access.h"
#include "project_file_header.h"
#include "audio_render_slot.h"
#include "alloc_policy.h"
#include <stdint.h>

namespace groove {
// One producer and one core-2 consumer. The submitted byte buffer becomes
// worker-owned until collection. No storage operations occur on the worker.
class ProjectDecodeSlot {
 static_assert(__atomic_always_lock_free(sizeof(unsigned),nullptr),"project worker requires lock-free state");
 struct MemorySource {
  const uint8_t* bytes;size_t size,position=0,next_gate=0;ProjectDecodeSlot* owner;
  bool read(uint8_t* out,size_t count){if(count>size-position)return false;if(position>=next_gate){if(!owner->yield_for_audio())return false;next_gate=position+4096;}__builtin_memcpy(out,bytes+position,count);position+=count;return true;}
  bool at_end()const{return position==size;}
 };

public:
 struct Result {ProjectState* project=nullptr;unsigned version=0;PreservedStore store;};
 // Called before audio starts. The template and reusable candidate avoid a
 // large default-construction burst while the instrument is performing.
 bool initialize(AudioRenderSlot* audio){static_assert(__is_trivially_copyable(ProjectState),"project pool requires byte-copyable state");if(template_ || busy())return false;audio_=audio;template_=alloc_new<ProjectState>();auto* spare=alloc_new<ProjectState>();if(!template_ || !spare){delete template_;template_=nullptr;delete spare;return false;}spare_pristine_=true;__atomic_store_n(&spare_,spare,__ATOMIC_RELEASE);return true;}
 // No consumer may be running during destruction. Pending buffers and results
 // still belong to the slot until their explicit handoff to a caller.
 ~ProjectDecodeSlot(){delete template_;delete __atomic_exchange_n(&spare_,static_cast<ProjectState*>(nullptr),__ATOMIC_ACQ_REL);unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);if(state==1)delete[] bytes_;if(state==2)delete result_.project;}
 bool recycle(ProjectState* project){if(!project || !template_)return false;ProjectState* expected=nullptr;return __atomic_compare_exchange_n(&spare_,&expected,project,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED);}
 bool pooled()const{return template_!=nullptr;}

 bool submit(uint8_t* bytes,unsigned size){if(!bytes || size<14 || !current_project_header(bytes,size) || __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0)return false;bytes_=bytes;size_=size;__atomic_store_n(&cancelled_,0u,__ATOMIC_RELAXED);__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);wake();return true;}
 bool process_one(){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;unsigned char v=bytes_[3];unsigned version=bytes_[0]=='M' && bytes_[1]=='G' && bytes_[2]=='X' && v=='C'?100:0;auto* project=__atomic_exchange_n(&spare_,static_cast<ProjectState*>(nullptr),__ATOMIC_ACQ_REL);bool copy_failed=false;if(project && template_ && !spare_pristine_){auto* out=reinterpret_cast<uint8_t*>(project);auto* in=reinterpret_cast<const uint8_t*>(template_);for(size_t at=0;at<sizeof(ProjectState);at+=16384){if(!yield_for_audio()){copy_failed=true;break;}size_t count=sizeof(ProjectState)-at>16384?16384:sizeof(ProjectState)-at;__builtin_memcpy(out+at,in+at,count);}}spare_pristine_=false;if(!project)project=alloc_new<ProjectState>();// This is a private candidate with fresh template defaults, never a live destination.
  decode_store_.clear();MemorySource source{bytes_,size_,0,0,this};bool ok=!copy_failed && project && decode_project_impl(source,nullptr,&decode_store_,project);delete[] bytes_;bytes_=nullptr;if(!ok){if(!recycle(project))delete project;project=nullptr;version=0;decode_store_.clear();}result_.project=project;result_.version=version;result_.store.steal_from(decode_store_);__atomic_store_n(&state_,2u,__ATOMIC_RELEASE);wake();return true;}
 bool collect(Result& result){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=2)return false;result.project=result_.project;result.version=result_.version;result.store.steal_from(result_.store);result_.project=nullptr;result_.version=0;__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;}
 bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
 void cancel(){__atomic_store_n(&cancelled_,1u,__ATOMIC_RELEASE);wake();}
private:
 // Bounded wait (SPEC section 8, item m1): the caller-visible contract is
 // unchanged (wait while the audio slot is starved), but the wait can no
 // longer spin forever if the audio side wedges. The cap is orders of
 // magnitude beyond any legitimate prefill wait; on expiry false is
 // propagated and the read/copy in flight fails cleanly.
 static constexpr unsigned kYieldSpinLimit=1u<<22;
 bool yield_for_audio(){if(__atomic_load_n(&cancelled_,__ATOMIC_ACQUIRE))return false;unsigned spins=0;while(audio_ && audio_->active() && audio_->available()<72){if(__atomic_load_n(&cancelled_,__ATOMIC_ACQUIRE) || ++spins>=kYieldSpinLimit)return false;
#if defined(__aarch64__)
  asm volatile("wfe" ::: "memory");
#endif
 }return true;}
 static void wake(){
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 }
 uint8_t* bytes_=nullptr;unsigned size_=0,state_=0,cancelled_=0;Result result_{};PreservedStore decode_store_;AudioRenderSlot* audio_=nullptr;ProjectState* template_=nullptr;ProjectState* spare_=nullptr;bool spare_pristine_=false;
};

// The main loop reads at most one bounded chunk per call. A result project is
// private to the caller until it has validated assets and submitted a pattern.
class AsyncProjectRead {
public:
 enum class Outcome:uint8_t {None,Ready,Failed,Cancelled};
 AsyncProjectRead(IFileAccess& fs,ProjectDecodeSlot& worker):fs_(fs),worker_(worker){}
 ~AsyncProjectRead(){delete[] bytes_;if(project_ && !worker_.recycle(project_))delete project_;}
 AsyncProjectRead(const AsyncProjectRead&)=delete;AsyncProjectRead& operator=(const AsyncProjectRead&)=delete;
 bool start(const char* path){if(!path || busy() || outcome_!=Outcome::None || worker_.busy() || !copy_text(path_,sizeof path_,path))return false;uint64_t size=fs_.size(path_);if(size<14 || size>uint64_t(kMaxFormatBytes)+14)return false;uint8_t header[10];unsigned got=0;if(!fs_.read(path_,0,header,sizeof header,got) || got!=sizeof header || !current_project_header(header,size))return false;bytes_=alloc_array<uint8_t>(size);if(!bytes_)return false;size_=unsigned(size);offset_=0;has_revision_=fs_.revision(path_,revision_);cancelled_=false;outcome_=Outcome::None;phase_=Phase::Read;return true;}
 bool busy()const{return phase_!=Phase::Idle;}
 bool decoding()const{return phase_==Phase::Decode;}
 bool recycle(ProjectState* project){return worker_.recycle(project);}
 bool cancel(){if(!busy() && outcome_!=Outcome::Ready)return false;cancelled_=true;if(phase_==Phase::Decode){worker_.cancel();return true;}if(project_ && !worker_.recycle(project_))delete project_;project_=nullptr;version_=0;store_.clear();finish(false);outcome_=Outcome::Cancelled;return true;}
 void service(){if(phase_==Phase::Idle)return;if(phase_==Phase::Read){unsigned count=size_-offset_>kFileScanBufferBytes?kFileScanBufferBytes:size_-offset_,got=0;if(!fs_.read(path_,offset_,bytes_+offset_,count,got) || got!=count){finish(false);return;}offset_+=count;if(offset_==size_){FileRevision current;if(fs_.size(path_)!=size_ || (has_revision_ && (!fs_.revision(path_,current) || !same_file_revision(revision_,current))) || !worker_.submit(bytes_,size_)){finish(false);return;}bytes_=nullptr;phase_=Phase::Decode;}return;}ProjectDecodeSlot::Result result;if(!worker_.collect(result))return;project_=result.project;version_=result.version;if(cancelled_){if(project_ && !worker_.recycle(project_))delete project_;project_=nullptr;version_=0;result.store.clear();finish(false);outcome_=Outcome::Cancelled;}else{store_.steal_from(result.store);finish(project_!=nullptr);}}
 Outcome take(ProjectState*& project,unsigned& version,PreservedStore* store=nullptr){Outcome out=outcome_;outcome_=Outcome::None;project=project_;version=version_;project_=nullptr;version_=0;if(store)store->steal_from(store_);else store_.clear();return out;}
private:
 enum class Phase:uint8_t {Idle,Read,Decode};
 void finish(bool ok){delete[] bytes_;bytes_=nullptr;phase_=Phase::Idle;outcome_=ok?Outcome::Ready:Outcome::Failed;}
 IFileAccess& fs_;ProjectDecodeSlot& worker_;char path_[192]{};uint8_t* bytes_=nullptr;ProjectState* project_=nullptr;PreservedStore store_;unsigned size_=0,offset_=0,version_=0;FileRevision revision_{};bool has_revision_=false,cancelled_=false;Phase phase_=Phase::Idle;Outcome outcome_=Outcome::None;
};
}
