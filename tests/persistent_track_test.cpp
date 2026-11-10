#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdio>
using namespace groove;
struct Sink {std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
struct Source {const std::vector<uint8_t>& bytes;size_t pos=0;bool read(uint8_t* p,size_t n){if(n>bytes.size()-pos)return false;__builtin_memcpy(p,bytes.data()+pos,n);pos+=n;return true;}bool at_end()const{return pos==bytes.size();}};
static float wave[4096],other[4096];
struct Rig {
 float scratch[2]{};
 std::unique_ptr<ProjectState> p=std::make_unique<ProjectState>();
 std::unique_ptr<Engine> e=std::make_unique<Engine>(8000);
 std::unique_ptr<Performance> ui;
 Rig(){make_empty_project(*p);p->tracks[0].instrument=Instrument::Sampler;p->tracks[0].sample_edit.fade_ms=0;p->tracks[0].sample_edit.crossfade_ms=0;p->tracks[0].level=.37f;p->tracks[0].midi_channel=9;p->patterns[1].tracks[0].sound_override=255;auto& s=p->sounds[255];s=snapshot_sound(track_at(*p,0,0));s.sample_slot=1;s.sample_edit.speed=2;ui=std::make_unique<Performance>(*e,*p);}
 void load(){assert(e->stage_project(*p));ui->project_loaded();e->render(scratch,0);ui->poll();for(unsigned slot=0;slot<2;++slot){Command c{CommandType::BindSample,0,0,uint16_t(slot)};c.sample={reinterpret_cast<uint8_t*>(slot?other:wave),4096,8000,1,32,3};send(c);}}
 void send(Command c){assert(e->submit(c));e->render(scratch,0);ui->poll();}
 std::vector<float> audio(){std::vector<float> a(256);e->render(a.data(),128);ui->poll();return a;}
};
static void equal(const std::vector<float>& a,const std::vector<float>& b){float peak=0;for(unsigned i=0;i<a.size();++i){assert(a[i]==b[i]);peak=fmaxf(peak,fabsf(a[i]));}assert(peak>1e-5f);}
int main(){
 static_assert(__is_trivially_copyable(ProjectState),"snapshot ownership");
 for(unsigned i=0;i<4096;++i){wave[i]=.3f*sinf(i*.11f);other[i]=.2f*cosf(i*.09f);}
 {Rig r;r.load();assert(r.ui->select_pattern(3));assert(r.ui->set_parameter(1,.43f));r.e->render(r.scratch,0);r.ui->poll();r.send({CommandType::LaunchClip,0,0,2});assert(r.p->tracks[0].level==.43f && r.p->tracks[0].midi_channel==9);assert(track_at(*r.p,0,0).level==track_at(*r.p,15,0).level);}
 {Rig a,b;a.load();b.load();a.send({CommandType::LaunchClip,0,0,1});b.send({CommandType::LaunchClip,0,0,1});assert(a.p->tracks[0].sample_edit.speed==2 && a.p->tracks[0].level==.37f && a.p->tracks[0].midi_channel==9);
  Command pool{CommandType::SetSound,0,0,255};pool.sound()=a.p->sounds[255];pool.sound().sample_edit.speed=3;a.p->sounds[255]=pool.sound();a.send(pool);
  assert(a.p->tracks[0].sample_edit.speed==2);a.send({CommandType::Trigger,0,0,100});b.send({CommandType::Trigger,0,0,100});equal(a.audio(),b.audio());
  a.send({CommandType::LaunchClip,0,0,2});assert(a.p->tracks[0].sample_edit.speed==2);a.send({CommandType::LaunchClip,0,0,1});assert(a.p->tracks[0].sample_edit.speed==3);
  // Payload is consumed with its own event, not re-resolved from an edited pool.
  pool.sound().sample_edit.speed=4;a.send(pool);assert(a.p->tracks[0].sample_edit.speed==3);
 }
 {Rig r;r.load();assert(r.ui->copy_pattern());assert(r.ui->select_pattern(4));assert(r.ui->set_parameter(1,.61f));assert(r.ui->paste_pattern());r.e->render(r.scratch,0);r.ui->poll();assert(r.p->tracks[0].level==.61f);assert(r.ui->undo());r.e->render(r.scratch,0);r.ui->poll();assert(r.p->tracks[0].level==.61f);}
 {Rig r;r.p->clips[0].pattern=1;r.load();r.send({CommandType::Start,0,0,0});assert(r.p->tracks[0].sample_edit.speed==2);r.send({CommandType::Stop,0,0,0});}
 {Rig r;r.load();r.send({CommandType::LaunchClip,0,0,1});Command pool{CommandType::SetSound,0,0,255};pool.sound()=r.p->sounds[255];pool.sound().sample_edit.speed=3;r.send(pool);r.send({CommandType::StopClip,0,0,0});assert(!r.p->clips[0].playing && r.p->tracks[0].sample_edit.speed==2);}
 {Rig r;r.p->patterns[2].tracks[0].sound_override=254;r.p->scenes[1].pattern_for_track[0]=2;r.load();r.send({CommandType::LaunchScene,0,0,1});assert(r.p->clips[0].pattern==0 && r.p->tracks[0].sample_edit.speed==1);}
 {Rig r;r.p->patterns[15].tracks[0].steps[255].sound=255;r.p->patterns[15].tracks[0].sound_override=255;r.p->patterns[15].tracks[0].length=256;Sink out;assert(encode_project(out,*r.p));auto decoded=std::make_unique<ProjectState>();Source input{out.bytes};assert(decode_project(input,*decoded));assert(decoded->tracks[0].level==.37f && decoded->tracks[0].midi_channel==9);assert(decoded->patterns[15].tracks[0].steps[255].sound==255 && decoded->patterns[15].tracks[0].sound_override==255);assert(decoded->sounds[255].sample_edit.speed==2);assert(decoded->patterns[0].tracks[0].steps[0].sound==kNoPreset);out.bytes[4]=2;Source legacy{out.bytes};assert(!decode_project(legacy,*decoded));}
 {Rig r;auto& sound=r.p->sounds[255];sound.instrument=Instrument::DrumKit;sound.kit.pads[0].sample[0]=0;sound.kit.pads[0].edit.fade_ms=0;sound.kit.pads[0].edit.crossfade_ms=0;r.p->tracks[0].instrument=Instrument::Empty;r.p->patterns[0].tracks[0].steps[0].enabled=true;r.p->patterns[0].tracks[0].steps[0].sound=255;r.load();r.send({CommandType::Start,0,0,0});auto output=r.audio();float peak=0;for(float v:output)peak=fmaxf(peak,fabsf(v));assert(peak>1e-5f);assert(r.p->tracks[0].instrument==Instrument::Empty);}
 {Rig r;r.p->tracks[0].instrument=Instrument::Empty;auto& sound=r.p->sounds[255];sound.instrument=Instrument::ExternalMidi;sound.midi_program=42;sound.midi_cc_used[0]=1;sound.midi_cc[0]=2;sound.midi_cc_used[4]=1;sound.midi_cc[32]=3;r.p->patterns[0].tracks[0].steps[0].enabled=true;r.p->patterns[0].tracks[0].steps[0].sound=255;r.load();r.send({CommandType::Start,0,0,0});r.audio();MidiEvent event;unsigned phase=0;while(r.e->pop_midi(event)){if(event.bytes[0]>=0xf0)continue;if(phase==0)assert(event.bytes[0]==0xb9 && event.bytes[1]==0 && event.bytes[2]==2);if(phase==1)assert(event.bytes[0]==0xb9 && event.bytes[1]==32 && event.bytes[2]==3);if(phase==2)assert(event.bytes[0]==0xc9 && event.bytes[1]==42);if(phase==3)assert(event.bytes[0]==0x99 && event.bytes[1]==60);++phase;}assert(phase>=4 && r.p->tracks[0].midi_program==128 && r.p->tracks[0].instrument==Instrument::Empty);}
 {auto e=std::make_unique<Engine>(8000);Command c{CommandType::SetSound,0,0,255};c.sound().instrument=Instrument::FM;for(unsigned i=0;i<64;++i){c.value=uint16_t(i);assert(e->submit(c));}assert(!e->submit(c));assert(e->submit({CommandType::SetTempo,0,0,137}));float out[2]{};e->render(out,0);assert(e->submit(c));e->render(out,0);}
 {Rig r;r.load();assert(r.ui->store_sound(3));r.e->render(r.scratch,0);Command c{CommandType::SetSound,0,0,250};c.sound().instrument=Instrument::FM;for(unsigned i=0;i<64;++i)assert(r.e->submit(c));assert(!r.ui->undo() && r.p->sounds[3].instrument==Instrument::Sampler);r.e->render(r.scratch,0);assert(r.ui->undo() && r.p->sounds[3].instrument==Instrument::Empty);r.e->render(r.scratch,0);}
 std::puts("persistent tracks: shared settings, fixed launch copy, phrase-only copy/Undo, slot 255 and old-format rejection passed");
}
