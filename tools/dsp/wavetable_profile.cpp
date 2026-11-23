// Host counterpart to the firmware DSPTEST Wavetable cases. Run an optimized
// build pinned to one core. Wall time includes OS preemption on hosted builds.
#include "engine.h"
#include "dsp_probe_workload.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
#include <sys/resource.h>
using namespace groove;

#if defined(__aarch64__)
static uint32_t stamp(){
 uint64_t ticks;asm volatile("isb\n\tmrs %0, cntvct_el0":"=r"(ticks)::"memory");
 return uint32_t(ticks);
}
static double ticks_per_us(){
 uint64_t hz;asm volatile("mrs %0, cntfrq_el0":"=r"(hz));return double(hz)/1e6;
}
#else
static uint32_t stamp(){
 using namespace std::chrono;
 return uint32_t(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
static double ticks_per_us(){return 1000;}
#endif

static bool measure(ProjectState& project,Engine& engine,DspCondition c){
 make_dsp_probe_project(project,c);
 if(!engine.stage_project(project))return false;
 std::array<float,48> pcm{};
 engine.render(pcm.data(),24);
 for(unsigned n=0;n<c.notes;++n){
  Command note{CommandType::Trigger,0,uint8_t(n),100};note.parameter=1;note.scalar=float(60+3*n);
  if(!engine.submit(note))return false;
 }
 // The firmware uses the same two seconds of warmup and six seconds of audio.
 for(unsigned n=0;n<4000;++n)engine.render(pcm.data(),24);
 if(engine.poly_note_count(0)!=c.notes)return false;
 auto& profile=engine.dsp_profile();
 if(!profile.begin(stamp,engine.staged_generation()))return false;
 profile.begin_measurement();
 const unsigned voice_bin=DspProfile::voice(unsigned(Instrument::Wavetable),0);
 std::vector<unsigned> elapsed;elapsed.reserve(12000);
 // Keep the absolute wall maxima as well as the subset of profiled blocks
 // in which Linux reported no context switch for this audio thread.
 std::vector<double> uninterrupted;
 const double unit=ticks_per_us();
 for(unsigned n=0;n<12000;++n){
  const bool sampled=n%127==0;
  rusage before{},after{};
  uint64_t previous=0;
  if(sampled){
   if(getrusage(RUSAGE_THREAD,&before))return false;
   previous=n?profile.counter(voice_bin).active_ticks:0; // First block clears counters from the preceding case.
  }
  engine.render(pcm.data(),24);
  elapsed.push_back(profile.last_block().ticks);
  if(sampled){
   if(getrusage(RUSAGE_THREAD,&after))return false;
   if(before.ru_nivcsw==after.ru_nivcsw && before.ru_nvcsw==after.ru_nvcsw)
    uninterrupted.push_back((profile.counter(voice_bin).active_ticks-previous)/unit);
  }
 }
 profile.stop();engine.render(pcm.data(),24);
 if(!profile.ready() || profile.snapshot().poly_peak!=c.notes)return false;
 const auto& bin=profile.counter(voice_bin);
 if(bin.frames!=uint64_t(profile.snapshot().sampled)*24*c.notes)return false;
 if(uninterrupted.empty())return false;
 std::sort(uninterrupted.begin(),uninterrupted.end());
 std::sort(elapsed.begin(),elapsed.end());
 std::printf("%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%zu,%.4f,%.4f\n",c.variant,c.notes,profile.snapshot().sampled,
             double(bin.active_ticks)*24/(bin.frames*unit),double(bin.max_voice_q16)*24/(65536*unit),
             double(bin.max_block_ticks)/unit,elapsed[elapsed.size()/2]/unit,
             elapsed[elapsed.size()*99/100]/unit,elapsed.back()/unit,
             uninterrupted.size(),uninterrupted[uninterrupted.size()/2],uninterrupted.back());
 return true;
}
int main(){
 auto project=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>();
 std::puts("variant,notes,sampled_blocks,voice_mean_us,voice_observed_max_us,voices_sum_max_us,worker_p50_us,worker_p99_us,worker_max_us,no_switch_samples,no_switch_voices_p50_us,no_switch_voices_max_us");
 for(unsigned i=dsp_wavetable_start();i<dsp_case_count();++i)
  if(!measure(*project,*engine,dsp_condition(i))){std::fprintf(stderr,"incomplete DSP case %u\n",i);return 1;}
}
