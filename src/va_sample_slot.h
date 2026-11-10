#pragma once
#include "virtual_analog_voice.h"
#include "dsp_assist_counts.h"
namespace groove {
// One canonical DSP producer and one optional helper. The helper advances only
// typed private copies. Late results never block or touch a canonical voice.
class VaSampleSlot {
public:
 static constexpr unsigned capacity=32;
 static constexpr unsigned shadow_capacity=48; // 16 main voices + 32 shared voices
 // Retain the eight-job policy for light scenes. In a saturated scene leave
 // three earlier voices on the owner so the helper can finish private jobs
 // before their canonical render points. No consumer ever waits for a job.
 static unsigned selected_count(unsigned eligible){return eligible<12?(eligible<8?eligible:8):(eligible-3<capacity?eligible-3:capacity);}
 using Counts=DspAssistCounts;
 static bool eligible(const VirtualAnalogVoice& voice){return voice.active_ && (voice.frame_&31);}
 // The selected sample must not update VA coefficients or use its shared math
 // cache. Engine also excludes global parameter-resolution samples.
 void worker_ready(){worker_fpcr_=fpcr();__atomic_store_n(&worker_ready_,1u,__ATOMIC_RELEASE);}
 bool begin(uint64_t sample,unsigned generation=0){
  current_=false;
  if(disabled_ || !__atomic_load_n(&worker_ready_,__ATOMIC_ACQUIRE))return false;
  if(!fpcr_checked_){compatible_=fpcr()==worker_fpcr_;fpcr_checked_=true;}
  if(!compatible_)return false;
  unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);
  if(state==Queued || state==Running){++counts_.busy;return false;}
  // Complete is released only after the worker's last access to every job.
  sample_=sample;generation_=generation;count_=0;selected_mask_=0;current_=true;return true;
 }
 unsigned add(const void* key,const VirtualAnalogVoice& source,float frequency,bool gate,uint64_t age=0,unsigned shadow=shadow_capacity){
  if(!current_ || count_==capacity || !eligible(source))return capacity;
  // The dispatch list may compact/reorder when another voice reaches a
  // coefficient boundary. Retain each voice's shadow independently of its
  // position in that list; the caller's stable hint never grants ownership.
  if(shadow>=shadow_capacity)shadow=count_;
  if(selected_mask_&(uint64_t(1)<<shadow))return capacity;
  unsigned i=count_++;selected_[i]=uint8_t(shadow);selected_mask_|=uint64_t(1)<<shadow;auto& job=jobs_[shadow];
  // A retained shadow is exact only after this same voice's preceding
  // sample was adopted. Any gap, generation, allocation-age or coefficient
  // boundary resynchronizes from the canonical state. Main voices retrigger
  // at frame0 (never eligible); poly allocations also carry their sample age.
  bool reuse=job.reusable && job.key==key && job.generation==generation_ && job.age==age && job.adopted_sample+1==sample_ && source.frame_==job.voice.frame_;
  if(!reuse){copy_input(job.voice,source);++counts_.cloned;}else ++counts_.reused;
  job.reusable=false;job.key=key;job.generation=generation_;job.age=age;
  job.frequency=frequency;job.gate=gate;job.start_frame=source.frame_;
  __atomic_store_n(&job.done,0u,__ATOMIC_RELAXED);return i;
 }
 void publish(){
  if(!current_ || !count_)return;
  counts_.submitted+=count_;
  __atomic_store_n(&state_,unsigned(Queued),__ATOMIC_RELEASE);wake();
 }
 bool take(const void* key,uint64_t sample,unsigned index,VirtualAnalogVoice& voice,float frequency,bool gate,float& output){
  if(disabled_ || !current_ || sample!=sample_ || index>=count_)return false;
  auto& job=jobs_[selected_[index]];if(key!=job.key)return false;
  // These inputs are immutable after publication. A note release/retrigger or
  // changed frequency between dispatch and the original render point falls back.
  if(!same(frequency,job.frequency) || gate!=job.gate || voice.frame_!=job.start_frame){++counts_.late;return false;}
  if(!__atomic_load_n(&job.done,__ATOMIC_ACQUIRE)){++counts_.late;return false;}
  // A sparse real-device check uses the canonical answer and leaves canonical
  // state in place. Any mismatch disables all further adoptions for this boot.
  if((counts_.used&1023)==0){
   float canonical=voice.next(job.frequency,job.gate);++counts_.checked;
   if(!same(canonical,job.output) || voice.active()!=job.voice.active() || !same(voice.level(),job.voice.level())){++counts_.mismatch;disabled_=true;}
   output=canonical;
  }else{
   adopt_running_state(voice,job.voice);output=job.output;
   job.reusable=true;job.adopted_sample=sample_;
  }
  ++counts_.used;return true;
 }
 bool process_one(){
  if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=Queued)return false;
  __atomic_store_n(&state_,unsigned(Running),__ATOMIC_RELEASE);
  for(unsigned i=0;i<count_;++i){
   auto& job=jobs_[selected_[i]];job.output=job.voice.next(job.frequency,job.gate);
   // Each finished job is immutable even while subsequent jobs are running.
   __atomic_store_n(&job.done,1u,__ATOMIC_RELEASE);
  }
  __atomic_store_n(&state_,unsigned(Complete),__ATOMIC_RELEASE);wake();return true;
 }
 Counts counts()const{return counts_;} // canonical DSP owner only
private:
 enum State:unsigned {Empty,Queued,Running,Complete};
 struct alignas(64) Job {
  VirtualAnalogVoice voice;const void* key=nullptr;float frequency=0,output=0;
  bool gate=false,reusable=false;unsigned done=0,start_frame=0,generation=0;uint64_t age=0,adopted_sample=0;
 };
 Job jobs_[shadow_capacity];uint8_t selected_[capacity]{};uint64_t selected_mask_=0;
 alignas(64) unsigned state_=Empty;
 unsigned worker_ready_=0,count_=0,generation_=0;uint64_t worker_fpcr_=0,sample_=0;
 bool current_=false,fpcr_checked_=false,compatible_=false,disabled_=false;
 Counts counts_{};
 // No coefficient update occurs in an eligible sample. Targets, static
 // controls and the inactive filter do not need a round trip each sample.
 static void copy_input(VirtualAnalogVoice& dst,const VirtualAnalogVoice& src){
  dst.trigger_bank_=nullptr;dst.osc1_=src.osc1_;dst.osc2_=src.osc2_;dst.sub_=src.sub_;dst.envelope_=src.envelope_;dst.mod_=src.mod_;
  __builtin_memcpy(dst.values_,src.values_,sizeof dst.values_);
  if(src.values_[23]<.5f)dst.filter_=src.filter_;else dst.svf_=src.svf_;
  dst.hz1_=src.hz1_;dst.hz2_=src.hz2_;dst.width_=src.width_;dst.rate_=src.rate_;dst.velocity_=src.velocity_;dst.level_=src.level_;dst.lfo_=src.lfo_;dst.noise_=src.noise_;dst.frame_=src.frame_;dst.active_=src.active_;
 }
 static void adopt_running_state(VirtualAnalogVoice& dst,const VirtualAnalogVoice& src){
  dst.osc1_=src.osc1_;dst.osc2_=src.osc2_;dst.sub_=src.sub_;dst.envelope_=src.envelope_;dst.mod_=src.mod_;
  if(dst.values_[23]<.5f)dst.filter_=src.filter_;else dst.svf_=src.svf_;
  dst.level_=src.level_;dst.lfo_=src.lfo_;dst.noise_=src.noise_;dst.frame_=src.frame_;dst.active_=src.active_;
 }
 static bool same(float a,float b){uint32_t x,y;__builtin_memcpy(&x,&a,4);__builtin_memcpy(&y,&b,4);return x==y;}
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
