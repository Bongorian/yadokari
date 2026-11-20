// W2: render-ahead job bundles must never change the rendered PCM. The
// same project is rendered serially (no helper) and with the bundle slot
// under several worker schedules -- instant, absent, double-rate and a
// real thread -- and every mode must produce bit-identical output. The
// cross-check inside the engine additionally verifies worker results
// against the canonical render before adoption.
#include "render_bundle_slot.h"
#include "engine.h"
#include "project.h"
#include "future_factory_presets.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>
using namespace groove;

static uint64_t hash_pcm(const std::vector<float>& a){
 uint64_t h=1469598103934665603ull;
 for(float x:a){uint32_t u;std::memcpy(&u,&x,4);h^=u;h*=1099511628211ull;}
 return h;
}
static bool any_sound(const std::vector<float>& a){for(float x:a)if(x!=0.0f)return true;return false;}

static void setup_project(ProjectState& p,bool dense){
 make_empty_project(p);p.tempo=120;p.random_seed=20261116u;
 auto t0=track_at(p,0,0);t0.instrument=Instrument::Additive;
 t0.steps[0].enabled=true;t0.steps[0].note=60;t0.steps[0].velocity=100;t0.steps[0].gate=100;t0.steps[0].chord[0]=64;t0.steps[0].chord[1]=67;
 t0.steps[8].enabled=true;t0.steps[8].note=62;t0.steps[8].velocity=100;t0.steps[8].gate=100;t0.steps[8].chord[0]=65;t0.steps[8].chord[1]=69;
 auto t1=track_at(p,0,1);t1.instrument=Instrument::Formant;
 t1.steps[2].enabled=true;t1.steps[2].note=57;t1.steps[2].velocity=100;t1.steps[2].gate=100;
 t1.steps[10].enabled=true;t1.steps[10].note=60;t1.steps[10].velocity=100;t1.steps[10].gate=100;
 auto t2=track_at(p,0,2);t2.instrument=Instrument::Wavetable;
 t2.steps[0].enabled=true;t2.steps[0].note=48;t2.steps[0].velocity=100;t2.steps[0].gate=100;
 t2.steps[8].enabled=true;t2.steps[8].note=50;t2.steps[8].velocity=100;t2.steps[8].gate=100;
 auto t3=track_at(p,0,3);t3.instrument=Instrument::PhaseDist;
 t3.steps[4].enabled=true;t3.steps[4].note=65;t3.steps[4].velocity=100;t3.steps[4].gate=100;
 t3.steps[12].enabled=true;t3.steps[12].note=64;t3.steps[12].velocity=100;t3.steps[12].gate=100;
 auto t4=track_at(p,0,4);t4.instrument=Instrument::PlaitsMacro;t4.plaits_model=18;
 t4.steps[0].enabled=true;t4.steps[0].note=60;t4.steps[0].velocity=100;t4.steps[0].chord[0]=64;t4.steps[0].chord[1]=67;
 t4.steps[8].enabled=true;t4.steps[8].note=62;t4.steps[8].velocity=100;t4.steps[8].chord[0]=65;t4.steps[8].chord[1]=69;
 if(dense){
  auto t5=track_at(p,0,5);t5.instrument=Instrument::Additive;
  for(unsigned s=0;s<16;s+=2){t5.steps[s].enabled=true;t5.steps[s].note=uint8_t(55+(s%5));t5.steps[s].velocity=100;t5.steps[s].gate=25;t5.steps[s].chord[0]=uint8_t(59+(s%5));}
  auto t6=track_at(p,0,6);t6.instrument=Instrument::Wavetable;
  for(unsigned s=0;s<16;s+=4){t6.steps[s].enabled=true;t6.steps[s].note=uint8_t(43+s/4);t6.steps[s].velocity=100;t6.steps[s].gate=40;}
 }
}

enum class Pump { None, Instant, Double, Thread };
struct RenderResult { std::vector<float> pcm; BundleCounts counts; double ms=0; };

static RenderResult render_project(ProjectState& p,unsigned frames,Pump pump){
 auto e=std::make_unique<Engine>(48000);
 assert(e->stage_project(p,nullptr));
 RenderBundleSlot slot;slot.set_render(&render_bundle_job);
 if(pump!=Pump::None){e->set_bundle_helper(&slot);}
 std::atomic<bool> stop{false};
 std::thread worker;
 if(pump==Pump::Thread){
  worker=std::thread([&]{
   slot.worker_ready();
   while(!stop.load(std::memory_order_acquire)){
    if(!slot.process_one())std::this_thread::yield();
   }
  });
 } else if(pump!=Pump::None){
  slot.worker_ready();
 }
 std::vector<float> out;out.reserve(size_t(frames)*2);
 float block[48]{};
 e->render(block,0);
 assert(e->submit({CommandType::Start,0,0,0}));
 auto t_begin=std::chrono::steady_clock::now();
 unsigned done=0;
 while(done<frames){
  unsigned n=frames-done<24?frames-done:24;
  e->render(block,n);
  out.insert(out.end(),block,block+2*n);
  done+=n;
  if(pump==Pump::Instant){while(slot.process_one()){}}
  else if(pump==Pump::Double){slot.process_one();slot.process_one();}
 }
 auto t_end=std::chrono::steady_clock::now();
 if(worker.joinable()){stop.store(true,std::memory_order_release);worker.join();}
 RenderResult result;
 result.pcm=std::move(out);
 result.counts=e->bundle_counts();
 result.ms=std::chrono::duration<double,std::milli>(t_end-t_begin).count();
 return result;
}

static void test_identity(){
 ProjectState p;setup_project(p,false);
 auto serial=render_project(p,192000,Pump::None);
 assert(any_sound(serial.pcm));
 auto instant=render_project(p,192000,Pump::Instant);
 auto nopump=render_project(p,192000,Pump::None);
 auto dbl=render_project(p,192000,Pump::Double);
 auto threaded=render_project(p,192000,Pump::Thread);
 auto instant2=render_project(p,192000,Pump::Instant);
 uint64_t h=hash_pcm(serial.pcm);
 std::printf("serial hash %016llx\n",(unsigned long long)h);
 assert(hash_pcm(instant.pcm)==h);
 assert(hash_pcm(nopump.pcm)==h);
 assert(hash_pcm(dbl.pcm)==h);
 assert(hash_pcm(threaded.pcm)==h);
 assert(hash_pcm(instant2.pcm)==h);
 std::printf("instant: submitted %llu adopted-samples %llu late %llu invalidated %llu replayed %llu checked %llu mismatch %llu\n",
  (unsigned long long)instant.counts.submitted,(unsigned long long)instant.counts.adopted_samples,
  (unsigned long long)instant.counts.late,(unsigned long long)instant.counts.invalidated,
  (unsigned long long)instant.counts.replayed_samples,(unsigned long long)instant.counts.checked,
  (unsigned long long)instant.counts.mismatch);
 assert(instant.counts.adopted_samples>10000);
 assert(instant.counts.checked>0); // the cross-check must demonstrably run
 assert(instant.counts.mismatch==0);
 assert(threaded.counts.adopted_samples>0);
 assert(threaded.counts.mismatch==0);
 std::puts("identity: serial == instant == double == threaded, bundles adopted, no mismatch");
}

static void test_no_worker_still_canonical(){
 // Helper attached but the worker never runs: every bundle must miss and
 // the canonical path must produce the serial result on its own.
 ProjectState p;setup_project(p,false);
 auto e=std::make_unique<Engine>(48000);
 assert(e->stage_project(p,nullptr));
 RenderBundleSlot slot;slot.set_render(&render_bundle_job);slot.worker_ready();
 e->set_bundle_helper(&slot);
 std::vector<float> out;float block[48]{};
 e->render(block,0);assert(e->submit({CommandType::Start,0,0,0}));
 unsigned done=0;const unsigned frames=96000;
 while(done<frames){unsigned n=frames-done<24?frames-done:24;e->render(block,n);out.insert(out.end(),block,block+2*n);done+=n;}
 auto serial=render_project(p,frames,Pump::None);
 assert(hash_pcm(out)==hash_pcm(serial.pcm));
 auto counts=e->bundle_counts();
 std::printf("no-worker: submitted %llu adopted %llu late %llu\n",(unsigned long long)counts.submitted,
  (unsigned long long)counts.adopted_samples,(unsigned long long)counts.late);
 assert(counts.submitted>0);
 assert(counts.adopted_samples==0);
 std::puts("no-worker: canonical fallback is bit-identical, zero adoptions");
}

static void test_dense_events(){
 ProjectState p;setup_project(p,true);
 auto serial=render_project(p,192000,Pump::None);
 auto instant=render_project(p,192000,Pump::Instant);
 auto threaded=render_project(p,192000,Pump::Thread);
 assert(any_sound(serial.pcm));
 assert(hash_pcm(instant.pcm)==hash_pcm(serial.pcm));
 assert(hash_pcm(threaded.pcm)==hash_pcm(serial.pcm));
 std::printf("dense: invalidated %llu replayed %llu adopted %llu mismatch %llu\n",
  (unsigned long long)instant.counts.invalidated,(unsigned long long)instant.counts.replayed_samples,
  (unsigned long long)instant.counts.adopted_samples,(unsigned long long)instant.counts.mismatch);
 assert(instant.counts.mismatch==0);
 std::puts("dense events: assisted render keeps bit-identity");
}

static void test_release_inside_adopted_subblock(){
 auto project=std::make_unique<ProjectState>();
 make_empty_project(*project);project->tracks[0].instrument=Instrument::Additive;
 auto run=[&](RenderBundleSlot* helper,float* pcm){
  auto engine=std::make_unique<Engine>(48000);
  engine->set_bundle_helper(helper);
  assert(engine->stage_project(*project));
  engine->render(pcm,0);
  assert(engine->submit({CommandType::Trigger,0,0,100}));
  engine->render(pcm,24);engine->render(pcm+48,24);
  if(helper)while(helper->process_one()){}
  engine->render(pcm+96,5);
  assert(engine->submit({CommandType::NoteOff,0,0,0}));
  engine->render(pcm+106,24);
  return engine->bundle_counts();
 };
 float serial[154]{},assisted[154]{};
 run(nullptr,serial);
 RenderBundleSlot slot;slot.set_render(&render_bundle_job);slot.worker_ready();
 const auto counts=run(&slot,assisted);
 assert(counts.invalidated==1 && counts.replayed_samples==5);
 assert(std::memcmp(serial,assisted,sizeof serial)==0);
}

static void test_heavy_timing(){
 ProjectState p;make_empty_project(p);p.tempo=120;p.random_seed=99;
 for(unsigned t=0;t<8;++t){
  auto tr=track_at(p,0,t);tr.instrument=Instrument::Additive;
  for(unsigned s=0;s<16;s+=2){tr.steps[s].enabled=true;tr.steps[s].note=uint8_t(48+t+(s%4));tr.steps[s].velocity=100;tr.steps[s].gate=100;tr.steps[s].chord[0]=uint8_t(52+t);tr.steps[s].chord[1]=uint8_t(55+t);tr.steps[s].chord[2]=uint8_t(59+t);}
 }
 auto serial=render_project(p,96000,Pump::None);
 auto threaded=render_project(p,96000,Pump::Thread);
 assert(any_sound(serial.pcm));
 assert(hash_pcm(threaded.pcm)==hash_pcm(serial.pcm));
 std::printf("heavy additive scene (8 tracks, chords): serial %.1f ms, threaded %.1f ms for 96000 frames (%.2f us per 24-frame block serial, %.2f threaded); adopted %llu samples\n",
  serial.ms,threaded.ms,serial.ms*1000.0/4000.0,threaded.ms*1000.0/4000.0,
  (unsigned long long)threaded.counts.adopted_samples);
 std::puts("heavy timing: threaded render is bit-identical (times above)");
}

static std::atomic<bool> worker_entered{false}, finish_job{false};

static bool complete_on_signal(RenderBundleJob&){
 worker_entered.store(true,std::memory_order_release);
 while(!finish_job.load(std::memory_order_acquire))std::this_thread::yield();
 return false;
}

static void test_retirement_racing_completion(){
 RenderBundleSlot slot;
 slot.set_render(&complete_on_signal);
 std::atomic<bool> ready{false},stop{false};
 std::thread worker([&]{
  slot.worker_ready();
  ready.store(true,std::memory_order_release);
  while(!stop.load(std::memory_order_acquire))if(!slot.process_one())std::this_thread::yield();
 });
 while(!ready.load(std::memory_order_acquire))std::this_thread::yield();
 for(unsigned i=0;i<100000;++i){
  auto* job=slot.claim();assert(job && slot.index(*job)==0);
  worker_entered.store(false,std::memory_order_relaxed);
  finish_job.store(false,std::memory_order_relaxed);
  slot.publish(*job);
  while(!worker_entered.load(std::memory_order_acquire))std::this_thread::yield();
  finish_job.store(true,std::memory_order_release);
  slot.retire(*job);
  unsigned state;
  do {state=__atomic_load_n(&job->state,__ATOMIC_ACQUIRE);} while(state==RenderBundleSlot::Dead);
  assert(state==RenderBundleSlot::Empty);
 }
 stop.store(true,std::memory_order_release);
 worker.join();
}

// VA voices, main and Layer children (a Jupiter-style two-VA pad), are bundled
// too; the owner keeps Layer gain/pan/detune and the trigger-bank memo.
static void setup_va_project(ProjectState& p){
 auto q=std::make_unique<ProjectState>();assert(make_future_factory_preset(*q,7));
 make_empty_project(p);p.tempo=120;p.random_seed=20261119u;
 // Exercise the last VA parameter at its actual array index.
 q->tracks[0].virtual_analog.values[23]=2;
 q->tracks[0].virtual_analog.values[25]=0.73f;
 auto t0=track_at(p,0,0);t0.instrument=Instrument::VirtualAnalog;t0.virtual_analog=q->tracks[0].virtual_analog;
 t0.steps[0].enabled=true;t0.steps[0].note=48;t0.steps[0].velocity=100;t0.steps[0].gate=100;
 t0.steps[8].enabled=true;t0.steps[8].note=50;t0.steps[8].velocity=100;t0.steps[8].gate=100;
 auto t1=track_at(p,0,1);t1.instrument=Instrument::Layer;t1.layer.count=2;
 for(unsigned i=0;i<2;++i){auto& part=t1.layer.parts[i];part.sound.instrument=Instrument::VirtualAnalog;part.sound.virtual_analog=q->tracks[0].virtual_analog;part.level=.55f;part.pan=i?.4f:-.4f;part.detune=i?9:-9;}
 t1.steps[0].enabled=true;t1.steps[0].note=60;t1.steps[0].velocity=100;t1.steps[0].gate=100;t1.steps[0].chord[0]=64;t1.steps[0].chord[1]=67;
 t1.steps[8].enabled=true;t1.steps[8].note=62;t1.steps[8].velocity=100;t1.steps[8].gate=100;t1.steps[8].chord[0]=65;t1.steps[8].chord[1]=69;
}
static void test_va_layer(){
 auto p=std::make_unique<ProjectState>();setup_va_project(*p);
 auto serial=render_project(*p,192000,Pump::None);
 auto instant=render_project(*p,192000,Pump::Instant);
 auto dbl=render_project(*p,192000,Pump::Double);
 auto threaded=render_project(*p,192000,Pump::Thread);
 assert(any_sound(serial.pcm));
 const uint64_t h=hash_pcm(serial.pcm);
 assert(hash_pcm(instant.pcm)==h && hash_pcm(dbl.pcm)==h && hash_pcm(threaded.pcm)==h);
 std::printf("va/layer: submitted %llu adopted-samples %llu late %llu invalidated %llu checked %llu mismatch %llu; serial %.1f ms threaded %.1f ms\n",
  (unsigned long long)instant.counts.submitted,(unsigned long long)instant.counts.adopted_samples,(unsigned long long)instant.counts.late,
  (unsigned long long)instant.counts.invalidated,(unsigned long long)instant.counts.checked,(unsigned long long)instant.counts.mismatch,serial.ms,threaded.ms);
 assert(instant.counts.adopted_samples>100000 && instant.counts.checked>0 && instant.counts.mismatch==0 && threaded.counts.mismatch==0);
 std::puts("va/layer: VA and Layer-child VA bundles are bit-identical to the serial render");
}

int main(){
 test_retirement_racing_completion();
 std::printf("sizeof RenderBundleJob %zu, slot %zu\n",sizeof(RenderBundleJob),sizeof(RenderBundleSlot));
 test_identity();
 test_no_worker_still_canonical();
 test_dense_events();
 test_release_inside_adopted_subblock();
 test_heavy_timing();
 test_va_layer();
 std::puts("Render Bundle: all W2 host checks passed");
}
