// W1: shared-pool voice streams must be a function of the note event
// (project seed, track, musical position, pitch, same-position ordinal),
// never of the pool slot a voice landed in, that slot's history, or
// processing time. Adapter-level checks are bit-exact; engine-level checks
// render whole projects on the host.
#include "voice_seed.h"
#include "plaits_voice.h"
#include "granular_dsp.h"
#include "texture_dsp.h"
#include "resonator_dsp.h"
#include "engine.h"
#include "project.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace groove;

static uint32_t lcg(uint32_t& s){s=s*1664525u+1013904223u;return s>>16;}

static void test_seed_function(){
 // Deterministic and never zero (several consumers treat 0 as "reseed to 1",
 // which would silently merge distinct events).
 for(uint32_t ps:{0u,1u,0x13579bdfu,0xffffffffu})
  for(unsigned t=0;t<16;++t)
   for(uint64_t tick:{0ull,1ull,63ull,1ull<<40})
    for(unsigned pitch:{0u,60u,127u})
     for(unsigned ord=0;ord<4;++ord){
      uint32_t s=note_event_seed(ps,t,tick,pitch,ord);
      assert(s!=0);
      assert(s==note_event_seed(ps,t,tick,pitch,ord));
     }
 // Sensitive to every ingredient.
 uint32_t base=note_event_seed(777,3,100,60,0);
 assert(note_event_seed(778,3,100,60,0)!=base);
 assert(note_event_seed(777,4,100,60,0)!=base);
 assert(note_event_seed(777,3,101,60,0)!=base);
 assert(note_event_seed(777,3,100,61,0)!=base);
 assert(note_event_seed(777,3,100,60,1)!=base);
 std::puts("seed function: deterministic, nonzero, sensitive to all inputs");
}

// Render one triggered note on a Plaits voice and capture its first samples.
static std::vector<float> plaits_note(PlaitsVoice& v,unsigned model,float note,unsigned samples){
 v.set_model(model);v.set_note_float(note);v.trigger(100);
 std::vector<float> out(samples);
 for(unsigned i=0;i<samples;++i)out[i]=v.next();
 return out;
}
static bool any_sound(const std::vector<float>& a){for(float x:a)if(x!=0.0f)return true;return false;}

static void test_plaits_adapter(){
 const uint32_t event_seed=note_event_seed(4242,0,8,60,0);
 auto a=std::make_unique<PlaitsVoice>();a->init();
 a->seed(event_seed);
 auto cap_a=plaits_note(*a,18,60,960);
 assert(any_sound(cap_a));
 // A different slot whose RNG was left in an unrelated state by earlier
 // draws: after the same canonical init, the event seed alone must decide
 // the stream.
 auto b=std::make_unique<PlaitsVoice>();b->seed(0xdeadbeefu);b->init();
 b->seed(event_seed);
 auto cap_b=plaits_note(*b,18,60,960);
 assert(cap_a==cap_b);
 // Control: without the event seed the stream differs, so the equality
 // above is not vacuous.
 auto c=std::make_unique<PlaitsVoice>();c->seed(0xdeadbeefu);c->init();
 auto cap_c=plaits_note(*c,18,60,960);
 assert(cap_a!=cap_c);
 std::puts("plaits adapter: event seed owns the stream after canonical init");
}

static const float kCloudParams[15]={0,0.25f,0.3f,0,0.7f,0.6f,1.0f,0.5f,0,0,0,0,0,0,1.0f};
static void fill_frames(TextureFrame* f,unsigned n,uint32_t& s){
 for(unsigned i=0;i<n;++i){f[i].l=int16_t(int(lcg(s))-32768);f[i].r=int16_t(int(lcg(s))-32768);}
}
template<class D> static std::vector<TextureFrame> cloud_capture(D& dsp,unsigned blocks,uint32_t& s,bool strum_first){
 std::vector<TextureFrame> out;
 TextureFrame in[32],o[32];
 for(unsigned b=0;b<blocks;++b){fill_frames(in,32,s);dsp.process(kCloudParams,in,o,strum_first && b==0);out.insert(out.end(),o,o+32);}
 return out;
}
static bool frames_sound(const std::vector<TextureFrame>& a){for(auto& f:a)if(f.l||f.r)return true;return false;}
static bool frames_equal(const std::vector<TextureFrame>& a,const std::vector<TextureFrame>& b){
 return a.size()==b.size() && !std::memcmp(a.data(),b.data(),a.size()*sizeof(TextureFrame));
}

static void test_granular_adapter(){
 const uint32_t event_seed=note_event_seed(4242,2,4,60,0);
 GranularDsp a;assert(a.ready());a.init(event_seed);
 uint32_t sa=12345;auto cap_a=cloud_capture(a,16,sa,true);
 assert(frames_sound(cap_a));
 // Permuted history: unrelated seed and input first, then the same
 // per-note init the engine performs.
 GranularDsp b;assert(b.ready());b.init(0x0badf00du);
 uint32_t sb=999;cloud_capture(b,16,sb,true);
 b.init(event_seed);
 uint32_t sb2=12345;auto cap_b=cloud_capture(b,16,sb2,true);
 assert(frames_equal(cap_a,cap_b));
 std::puts("granular adapter: per-note init seed owns the stream, history-free");
}

static void test_texture_adapter(){
 const uint32_t event_seed=note_event_seed(4242,3,3,60,0);
 { // init() fully resets, so an event-seeded init is history-free.
  TextureDsp a;assert(a.ready());a.init(event_seed);
  uint32_t sa=777;auto cap_a=cloud_capture(a,16,sa,true);
  assert(frames_sound(cap_a));
  TextureDsp b;assert(b.ready());b.init(0x1234abcd);
  uint32_t sb=31337;cloud_capture(b,16,sb,true);
  b.init(event_seed);
  uint32_t sb2=777;auto cap_b=cloud_capture(b,16,sb2,true);
  assert(frames_equal(cap_a,cap_b));
 }
 { // reseed(): the note-on mechanism itself. Two instances in identical
  // processor state must produce identical output after reseeding to the
  // event seed, and a different event seed must change the stream (so the
  // check is not vacuous). Cross-history convergence is not claimed for
  // Texture: the processor's grain-scheduler phase is per-voice state
  // carriage, which the design leaves untouched.
  const uint32_t other_seed=note_event_seed(4242,3,3,61,0);
  auto run=[&](uint32_t seed){
   TextureDsp d;assert(d.ready());d.init(0xaaaa);
   uint32_t h=555;cloud_capture(d,48,h,true);
   d.reseed(seed);
   uint32_t c=777;return cloud_capture(d,300,c,false); // long: grain onset latency is stream-state dependent
  };
  auto cap_a=run(event_seed);
  auto cap_b=run(event_seed);
  auto cap_c=run(other_seed);
  assert(frames_sound(cap_a));
  assert(frames_equal(cap_a,cap_b));
  assert(!frames_equal(cap_a,cap_c));
 }
 std::puts("texture adapter: init and reseed both make the stream event-owned");
}

static std::vector<float> resonator_capture(ResonatorDsp& dsp,unsigned blocks){
 std::vector<float> out;
 float in[24]{},l[24],r[24];
 rings::Patch patch{0.5f,0.5f,0.7f,0.5f};
 for(unsigned b=0;b<blocks;++b){
  rings::PerformanceState st{};st.internal_exciter=true;st.strum=(b==0);st.tonic=12.0f;st.note=48.0f;st.fm=0.0f;
  dsp.process(0,1,patch,st,in,l,r);
  out.insert(out.end(),l,l+24);out.insert(out.end(),r,r+24);
 }
 return out;
}
static void resonator_history(ResonatorDsp& dsp,unsigned blocks){
 float in[24]{},l[24],r[24];
 rings::Patch patch{0.5f,0.5f,0.7f,0.5f};
 for(unsigned b=0;b<blocks;++b){
  rings::PerformanceState st{};st.internal_exciter=true;st.strum=false;st.tonic=12.0f;st.note=48.0f;st.fm=0.0f;
  dsp.process(0,1,patch,st,in,l,r);
 }
}
static void test_resonator_adapter(){
 const uint32_t event_seed=note_event_seed(4242,4,3,62,0);
 ResonatorDsp a;assert(a.ready());a.init(event_seed);
 auto cap_a=resonator_capture(a,24);
 assert(any_sound(cap_a));
 ResonatorDsp b;assert(b.ready());b.init(0xfeedfaceu);resonator_history(b,8);
 b.init(event_seed);
 auto cap_b=resonator_capture(b,24);
 assert(cap_a==cap_b);
 // reseed(): silent histories converge once the stream is reseeded.
 ResonatorDsp c;assert(c.ready());c.init(0xaaaa);resonator_history(c,8);c.reseed(event_seed);
 auto cap_c=resonator_capture(c,24);
 ResonatorDsp d;assert(d.ready());d.init(0xbbbb);resonator_history(d,8);d.reseed(event_seed);
 auto cap_d=resonator_capture(d,24);
 assert(cap_c==cap_d);
 assert(any_sound(cap_c));
 std::puts("resonator adapter: init and reseed both make the stream event-owned");
}

// ---- Engine level -------------------------------------------------------

static std::vector<float> render_project(ProjectState& p,WavView* samples,unsigned frames){
 auto e=std::make_unique<Engine>(48000);
 assert(e->stage_project(p,samples));
 std::vector<float> out;out.reserve(size_t(frames)*2);
 float block[960]{};
 e->render(block,0);
 assert(e->submit({CommandType::Start,0,0,0}));
 unsigned done=0;
 while(done<frames){unsigned n=frames-done<480?frames-done:480;e->render(block,n);out.insert(out.end(),block,block+2*n);done+=n;}
 return out;
}
static uint64_t hash_pcm(const std::vector<float>& a){
 uint64_t h=1469598103934665603ull;
 for(float x:a){uint32_t u;std::memcpy(&u,&x,4);h^=u;h*=1099511628211ull;}
 return h;
}
static void setup_determinism_project(ProjectState& p,uint32_t seed){
 make_empty_project(p);p.tempo=120;p.random_seed=seed;
 auto t0=track_at(p,0,0);t0.instrument=Instrument::PlaitsMacro;t0.plaits_model=18;
 t0.steps[0].enabled=true;t0.steps[0].note=60;t0.steps[0].velocity=100;t0.steps[0].chord[0]=64;t0.steps[0].chord[1]=67;
 t0.steps[4].enabled=true;t0.steps[4].note=62;t0.steps[4].velocity=100;t0.steps[4].chord[0]=65;t0.steps[4].chord[1]=69;
 t0.steps[8].enabled=true;t0.steps[8].note=60;t0.steps[8].velocity=100;t0.steps[8].chord[0]=64;t0.steps[8].chord[1]=67;
 auto t1=track_at(p,0,1);t1.instrument=Instrument::Granular;t1.sample_slot=0;
 t1.granular.values[2]=0.3f;t1.granular.values[4]=0.6f;t1.granular.values[5]=0.5f;t1.granular.values[6]=1.0f;t1.granular.values[9]=0.0f;
 t1.steps[2].enabled=true;t1.steps[2].note=60;t1.steps[2].velocity=100;
 t1.steps[6].enabled=true;t1.steps[6].note=65;t1.steps[6].velocity=100;
 auto t2=track_at(p,0,2);t2.instrument=Instrument::VirtualAnalog;
 t2.steps[1].enabled=true;t2.steps[1].note=60;t2.steps[1].velocity=100;
 t2.steps[5].enabled=true;t2.steps[5].note=67;t2.steps[5].velocity=100;
 auto t3=track_at(p,0,3);t3.instrument=Instrument::Texture;
 t3.steps[3].enabled=true;t3.steps[3].note=60;t3.steps[3].velocity=100;
 auto t4=track_at(p,0,4);t4.instrument=Instrument::Resonator;
 t4.steps[3].enabled=true;t4.steps[3].note=62;t4.steps[3].velocity=100;
 t4.steps[7].enabled=true;t4.steps[7].note=65;t4.steps[7].velocity=100;
 p.assets[0].metadata.root=60;
}
static void test_engine_determinism(){
 std::vector<uint8_t> pcm(96000);
 uint32_t s=24680;for(auto& byte:pcm)byte=uint8_t(lcg(s));
 WavView samples[kSampleAssets]{};samples[0]={pcm.data(),48000,48000,1,16,1};
 ProjectState p1,p2,p3;
 setup_determinism_project(p1,20261115u);
 setup_determinism_project(p2,20261115u);
 setup_determinism_project(p3,777u);
 auto a=render_project(p1,samples,102000);
 auto b=render_project(p2,samples,102000);
 auto c=render_project(p3,samples,102000);
 assert(any_sound(a));
 assert(hash_pcm(a)==hash_pcm(b)); // same song, same operations -> same PCM
 assert(hash_pcm(a)!=hash_pcm(c)); // project seed actually reaches the voices
 std::puts("engine: identical projects render bit-identical PCM; project seed matters");
}

static void test_engine_slot_permutation(){
 // Chord tones live in the shared pool and take slots in trigger order.
 // Swapping the chord order permutes the slot assignment (and with it the
 // order the two voices are summed) while every event keeps its identity:
 // the mix must agree to within summation ulps. A different chord pitch is
 // the sensitivity control and must change the mix audibly. (A filler-note
 // variant is not used at engine level: a stolen voice's continuous DSP
 // state and the master dynamics are history, not stream, and cannot be
 // bit-equal by construction; stream history-independence is proven at
 // adapter level above.)
 auto build=[&](uint8_t first,uint8_t second){
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=120;p->random_seed=1357;
  auto t0=track_at(*p,0,0);t0.instrument=Instrument::PlaitsMacro;t0.plaits_model=18;t0.engine_parameters[3]=0.15f;
  t0.steps[8].enabled=true;t0.steps[8].note=60;t0.steps[8].velocity=100;t0.steps[8].gate=50;t0.steps[8].chord[0]=first;t0.steps[8].chord[1]=second;
  return p;
 };
 auto pa=build(64,67), pb=build(67,64), pc=build(64,69);
 auto a=render_project(*pa,nullptr,96000);
 auto b=render_project(*pb,nullptr,96000);
 auto c=render_project(*pc,nullptr,96000);
 assert(any_sound(a));
 float max_ab=0,max_ac=0;
 for(size_t i=0;i<a.size();++i){
  float dab=std::fabs(a[i]-b[i]);if(dab>max_ab)max_ab=dab;
  float dac=std::fabs(a[i]-c[i]);if(dac>max_ac)max_ac=dac;
 }
 assert(max_ab<=2e-6f);
 assert(max_ac>0.01f);
 std::puts("engine: plaits chord mix is slot-assignment invariant (ulp tolerance)");
}

int main(){
 test_seed_function();
 test_plaits_adapter();
 test_granular_adapter();
 test_texture_adapter();
 test_resonator_adapter();
 test_engine_determinism();
 test_engine_slot_permutation();
 std::puts("Voice Seed: all W1 seed-ownership checks passed");
}
