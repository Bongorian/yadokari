#pragma once
#include <stdint.h>
#include <string.h>
#include "spectral_voices.h"
#include "formant_voice.h"
#include "wavetable_voice.h"
#include "virtual_analog_voice.h"
namespace groove {
// W2 render-ahead job bundles (DESIGN_MULTICORE_ARCH.md section 4).
// The canonical DSP owner builds a bundle at the render control boundary
// for an already-sounding voice whose inputs are provably constant over
// the coming 192-frame window: the parameter set is resolved once by the
// owner (only when resolution reduces to clamped bases), the trigger list
// is empty by construction (any note event changes the voice instance or
// epoch and invalidates the bundle), and the worker advances only the
// bundle's private copy of the voice state. The owner consumes whole
// 24-frame sub-blocks in window order and adopts the worker's checkpointed
// state at each consumed sub-block boundary. Canonical voice state is only
// ever overwritten by an adopted checkpoint, so a late or invalid bundle
// always falls back to the canonical render of the same samples.
struct BundleVoiceState {
 union {
  AdditiveVoice additive;
  PhaseDistVoice phase_dist;
  FormantVoice formant;
  WavetableVoice wavetable;
  VirtualAnalogVoice virtual_analog; // trigger bank detached
 };
 BundleVoiceState(){}
 ~BundleVoiceState(){}
};
struct RenderBundleJob {
 static constexpr unsigned kFrames=192,kSubFrames=24,kSubBlocks=kFrames/kSubFrames,kMaxParams=25,kMaxRefresh=6;
 // Written by the owner before publish(); immutable while queued/running.
 const void* key=nullptr;
 unsigned voice_key=0,track=0,instrument=0;
 uint64_t start_sample=0,automation_tick=0;
 unsigned generation=0;uint32_t epoch=0,instance=0;
 float frequency=0,env0=0;bool released=false;
 unsigned frames=kFrames;
 uint8_t param_count=0,refresh_count=0;
 uint16_t refresh_offset[kMaxRefresh]{};
 float values[kMaxParams]{};
 // Written by the worker after publish (live state is its private copy).
 BundleVoiceState live{};
 BundleVoiceState checkpoint[kSubBlocks]{};
 float out[kFrames]{};float env[kFrames]{};
 float env_live=0;unsigned next_sub=0;
 alignas(64) unsigned progress=0; // completed sub-blocks, release-published
 unsigned state=0; // RenderBundleSlot::JobState, atomic
};
struct BundleCounts {
 uint64_t submitted=0,adopted_subblocks=0,adopted_samples=0,late=0,invalidated=0,replayed_samples=0,checked=0,mismatch=0;
};
// Worker entry point (engine_synth.cpp): renders one sub-block of the job
// into out/env, checkpoints the private state, publishes progress.
// Returns false when the job has no sub-blocks left.
bool render_bundle_job(RenderBundleJob& job);
class RenderBundleSlot {
public:
 static constexpr unsigned capacity=48; // 16 main voices + 32 poly voices
 using Render=bool(*)(RenderBundleJob&);
 enum JobState:unsigned {Empty,Building,Queued,Running,Complete,Dead};
 void set_render(Render render){render_=render;}
 void worker_ready(){worker_fpcr_=fpcr();__atomic_store_n(&worker_ready_,1u,__ATOMIC_RELEASE);}
 bool disabled()const{return disabled_;}
 void disable(){disabled_=true;}
 RenderBundleJob* claim(){
  if(disabled_ || !__atomic_load_n(&worker_ready_,__ATOMIC_ACQUIRE))return nullptr;
  if(!fpcr_checked_){compatible_=fpcr()==worker_fpcr_;fpcr_checked_=true;}
  if(!compatible_)return nullptr;
  for(auto& job:jobs_){
   if(__atomic_load_n(&job.state,__ATOMIC_ACQUIRE)==Empty){
    __atomic_store_n(&job.state,unsigned(Building),__ATOMIC_RELEASE);
    job.progress=0;job.next_sub=0;
    return &job;
   }
  }
  return nullptr;
 }
 void publish(RenderBundleJob& job){++counts_.submitted;__atomic_store_n(&job.state,unsigned(Queued),__ATOMIC_RELEASE);wake();}
 // Retire a job the owner no longer tracks. Queued/running jobs go through
 // Dead so their storage is never rewritten under the worker; the worker
 // frees Dead jobs in process_one().
 void retire(RenderBundleJob& job){
  unsigned state=__atomic_load_n(&job.state,__ATOMIC_ACQUIRE);
  while(state==Building || state==Complete || state==Queued || state==Running){
   unsigned next=(state==Building || state==Complete)?Empty:Dead;
   if(__atomic_compare_exchange_n(&job.state,&state,next,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
  }
 }
 bool process_one(){
  if(!render_)return false;
  for(auto& job:jobs_){
   unsigned state=__atomic_load_n(&job.state,__ATOMIC_ACQUIRE);
   if(state==Dead){__atomic_store_n(&job.state,unsigned(Empty),__ATOMIC_RELEASE);wake();return true;}
   if(state!=Queued && state!=Running)continue;
   if(state==Queued && !__atomic_compare_exchange_n(&job.state,&state,unsigned(Running),false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))continue;
   bool more=render_(job);
   unsigned now=Running;
   if(more)now=__atomic_load_n(&job.state,__ATOMIC_ACQUIRE);
   else __atomic_compare_exchange_n(&job.state,&now,unsigned(Complete),false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
   if(now==Dead)__atomic_store_n(&job.state,unsigned(Empty),__ATOMIC_RELEASE);
   wake();return true;
  }
  return false;
 }
 unsigned progress(const RenderBundleJob& job)const{return __atomic_load_n(&job.progress,__ATOMIC_ACQUIRE);}
 unsigned index(const RenderBundleJob& job)const{return unsigned(&job-jobs_);}
 RenderBundleJob& at(unsigned i){return jobs_[i];}
 BundleCounts counts()const{return counts_;}
 void note_adopted(unsigned subblocks,unsigned samples){counts_.adopted_subblocks+=subblocks;counts_.adopted_samples+=samples;}
 void note_late(){++counts_.late;}
 void note_invalidated(){++counts_.invalidated;}
 void note_replayed(unsigned samples){counts_.replayed_samples+=samples;}
 void note_checked(){++counts_.checked;}
 void note_mismatch(){++counts_.mismatch;}
private:
 RenderBundleJob jobs_[capacity]{};
 Render render_=nullptr;
 BundleCounts counts_{};
 unsigned worker_ready_=0;uint64_t worker_fpcr_=0;
 bool fpcr_checked_=false,compatible_=false,disabled_=false;
 static uint64_t fpcr(){
#if defined(__aarch64__)
  uint64_t value;asm volatile("mrs %0, fpcr":"=r"(value));return value;
#else
  return 0;
#endif
 }
 static void wake(){
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 }
};
}
