#include "engine.h"
#include "performance.h"
#include "ui_renderer.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <new>
#include <atomic>
static std::atomic<unsigned> allocations{0};
[[gnu::noinline]] void* operator new(std::size_t size) { allocations.fetch_add(1); if(void* p=std::malloc(size?size:1))return p; throw std::bad_alloc(); }
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
// alloc_policy uses nothrow new. Replace that family too: ASan's default
// nothrow allocator bypasses the counting replacements above, and mixing it
// with the fixture's free-based delete is itself an allocator mismatch.
[[gnu::noinline]] void* operator new(std::size_t size,const std::nothrow_t&) noexcept {allocations.fetch_add(1);return std::malloc(size?size:1);}
[[gnu::noinline]] void* operator new[](std::size_t size,const std::nothrow_t& tag) noexcept {return ::operator new(size,tag);}
[[gnu::noinline]] void operator delete(void* p,const std::nothrow_t&) noexcept {std::free(p);}
[[gnu::noinline]] void operator delete[](void* p,const std::nothrow_t&) noexcept {std::free(p);}

[[gnu::noinline]] void operator delete(void* p) noexcept {std::free(p);} [[gnu::noinline]] void operator delete[](void* p) noexcept {std::free(p);}
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept {std::free(p);} [[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

using namespace groove;
static void render(Engine& e,unsigned frames=0,std::vector<float>* output=nullptr){
 std::vector<float> scratch(frames*2+2);unsigned before=allocations.load();e.render(scratch.data(),frames);assert(allocations.load()==before);for(float x:scratch)assert(std::isfinite(x));if(output)*output=std::move(scratch);
}
static void on(Engine& e,unsigned note,unsigned track=0,unsigned channel=0){Command c{CommandType::MidiInput,uint8_t(track),uint8_t(note),uint16_t(0x90|channel)};c.scalar=100;assert(e.submit(c));}
static void off(Engine& e,unsigned note,unsigned track=0,unsigned channel=0){Command c{CommandType::MidiInput,uint8_t(track),uint8_t(note),uint16_t(0x80|channel)};c.scalar=0;assert(e.submit(c));}
static double component(const std::vector<float>& audio,unsigned note){double re=0,im=0;unsigned count=unsigned(audio.size()/2);double f=440*std::pow(2.0,(double(note)-69)/12);for(unsigned i=0;i<count;++i){double w=0.5-0.5*std::cos(6.283185307*i/count),p=6.283185307*f*i/48000;re+=audio[2*i]*w*std::cos(p);im+=audio[2*i]*w*std::sin(p);}return std::sqrt(re*re+im*im)/count;}
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=120;
 for(auto& t:p->tracks){t.level=0.3f;t.instrument=Instrument::PlaitsMacro;t.plaits_model=8;t.engine_parameters[3]=float(16383)/16383.0f;}
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));render(*e);
 const unsigned notes[]={60,64};for(unsigned n:notes)on(*e,n);render(*e,2400);assert(e->plaits_note_count(0)==2 && e->poly_owner_masks_valid());
 std::vector<float> chord;render(*e,9600,&chord);for(unsigned n:notes){double level=component(chord,n);std::printf("note %u component %.6f\n",n,level);assert(level>0.00001);assert(e->plaits_note_held(0,n));}
 off(*e,64);render(*e,7200);assert(!e->plaits_note_held(0,64) && e->plaits_note_count(0)==1);for(unsigned n:{60u})assert(e->plaits_note_held(0,n));
 on(*e,74);render(*e,24);on(*e,76);render(*e,24);assert(e->plaits_note_count(0)==2 && !e->plaits_note_held(0,60));
 // A different MIDI channel must not release another channel's note.
 off(*e,76,0,1);render(*e);assert(e->plaits_note_held(0,76));off(*e,76);render(*e,7200);assert(!e->plaits_note_held(0,76));
 assert(e->submit({CommandType::Panic,0,0,0}));render(*e,24);assert(e->plaits_note_count(0)==0 && e->poly_owner_masks_valid());
 // The four-note request obeys the calibrated two-note weighted budget.
 auto t=track_at(*p,0,0);t.steps[0].enabled=true;t.steps[0].chord[0]=64;t.steps[0].chord[1]=67;t.steps[0].chord[2]=71;
 assert(e->stage_project(*p));render(*e);assert(e->submit({CommandType::Start,0,0,0}));render(*e,100);assert(e->plaits_note_count(0)==2);
 // Sound Lock chooses a separate model and obeys its two-note budget.
 p->sounds[0].instrument=Instrument::PlaitsMacro;p->sounds[0].model=2;t.steps[0].sound=0;
 assert(e->stage_project(*p));render(*e);assert(e->submit({CommandType::Start,0,0,0}));render(*e,100);assert(e->plaits_note_count(0)==2 && e->poly_owner_masks_valid());
 assert(e->submit({CommandType::Panic,0,0,0}));render(*e,24);
 // Cross-track steals stay inside the bounded extra DSP pool.
 t.steps[0].enabled=false;t.steps[0].sound=kNoPreset;assert(e->stage_project(*p));render(*e);
 for(unsigned track=0;track<4;++track){on(*e,60+track,track);render(*e,24);}on(*e,72,4);render(*e,24);assert(e->plaits_note_count(0)==0 && e->plaits_note_count(4)==1 && e->poly_owner_masks_valid());
 // UI exposes the model limit, rather than the old mono warning.
 assert(instrument_voice_limit(Instrument::PlaitsMacro,2)==2 && instrument_voice_limit(Instrument::PlaitsMacro,8)==2);
 Performance ui(*e,*p);ui.set_mode(UiMode::Chord);assert(ui.pad(12,0x10,4095));render(*e,24);assert(ui.pad(12,0x20,0));render(*e,7200);assert(e->poly_owner_masks_valid());
 std::puts("Plaits polyphony: spectral chord, independent release, steal, MIDI channel, Sound Lock, project reset, UI and allocation-free render passed");
}
