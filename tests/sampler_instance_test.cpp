#include "engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
namespace {
constexpr unsigned Rate=8000;
float wave[2048],other[2048];
struct Rig {
 float scratch[2]{};
 std::unique_ptr<ProjectState> project=std::make_unique<ProjectState>();
 std::unique_ptr<Engine> engine=std::make_unique<Engine>(Rate);
 Rig(bool sound_lock=false){
  make_empty_project(*project);auto t=track_at(*project,0,0);
  t.instrument=Instrument::Sampler;t.sample_edit.fade_ms=0;t.sample_edit.crossfade_ms=0;
  if(sound_lock){t.length=1;t.steps[0].enabled=true;t.steps[0].sound=3;
   auto& s=project->sounds[3];s.instrument=Instrument::Sampler;s.sample_edit=t.sample_edit;s.sample_loop=true;s.sample_reverse=true;s.sample_slices=4;s.sample_slice=1;
  }
 }
 void load(){assert(engine->stage_project(*project));engine->render(scratch,0);
  for(unsigned slot=0;slot<2;++slot){Command c{CommandType::BindSample,0,0,uint16_t(slot)};c.sample={reinterpret_cast<const uint8_t*>(slot?other:wave),2048,Rate,1,32,3};assert(engine->submit(c));}engine->render(scratch,0);
 }
 void send(Command c){assert(engine->submit(c));engine->render(scratch,0);}
 void trigger(bool poly){Command c{poly?CommandType::MidiInput:CommandType::Trigger,0,60,uint16_t(poly?0x90:100)};if(poly)c.scalar=100;send(c);}
 void speed(float value){Command c{CommandType::SetParameter,0,0,0};c.parameter=80;c.scalar=value;send(c);}
 std::vector<float> render(unsigned frames){std::vector<float> out(frames*2);engine->render(out.data(),frames);return out;}
};
void equal(const std::vector<float>& a,const std::vector<float>& b){assert(a.size()==b.size());double peak=0;for(unsigned i=0;i<a.size();++i){assert(std::isfinite(a[i]));assert(a[i]==b[i]);peak=fmax(peak,fabs(a[i]));}assert(peak>1e-4);}
void different(const std::vector<float>& a,const std::vector<float>& b){double delta=0;for(unsigned i=0;i<a.size();++i)delta=fmax(delta,fabs(a[i]-b[i]));assert(delta>1e-3);}
}
int main(){
 for(unsigned i=0;i<2048;++i){wave[i]=.3f*sinf(float(i)*.17f)+.1f*cosf(float(i)*.031f);other[i]=.25f*cosf(float(i)*.057f);}
 // ApplySound must not retune/re-region an already sounding instance. Check
 // both the sequenced/mono voice and the live polyphonic pool.
 for(bool poly:{false,true}){Rig control,changed;control.load();changed.load();control.trigger(poly);changed.trigger(poly);equal(control.render(128),changed.render(128));
  Command c{CommandType::ApplySound,0,0,0};c.sound().instrument=Instrument::Sampler;c.sound().sample_edit.speed=2;c.sound().sample_edit.start=.5f;c.sound().sample_slot=1;changed.send(c);
  equal(control.render(128),changed.render(128));
 }
 // LIVE speed remains live while the source instance has not been replaced,
 // including MIDI-created polyphonic sampler voices.
 for(bool poly:{false,true}){Rig control,changed;control.load();changed.load();control.trigger(poly);changed.trigger(poly);equal(control.render(128),changed.render(128));changed.speed(2);different(control.render(128),changed.render(128));}
 // Selecting a different file retires the old instance; later edits belong
 // to the newly selected source, whose next note uses the new sample.
 {Rig control,changed;control.load();changed.load();control.trigger(false);changed.trigger(false);equal(control.render(128),changed.render(128));changed.send({CommandType::SetSampleSlot,0,0,1});changed.speed(2);equal(control.render(128),changed.render(128));
  control.send({CommandType::SetSampleSlot,0,0,1});control.speed(2);control.trigger(false);changed.trigger(false);equal(control.render(128),changed.render(128));}
 // A step-selected sound supplies the entire playback selection. Its reverse,
 // slice grid, selected slice and loop flag must override different track flags.
 {Rig reference,locked(true);auto t=track_at(*reference.project,0,0);t.length=1;t.steps[0].enabled=true;t.sample_loop=true;t.sample_reverse=true;t.sample_slices=4;t.sample_slice=1;reference.load();locked.load();reference.send({CommandType::Start,0,0,0});locked.send({CommandType::Start,0,0,0});equal(reference.render(800),locked.render(800));}
 // Leaving and returning to the source pattern must not reattach a surviving
 // one-shot voice to the newly active instance.
 for(bool poly:{false,true}){Rig control,changed;changed.project->patterns[1]=changed.project->patterns[0];changed.project->sounds[0]=snapshot_sound(track_at(*changed.project,0,0));changed.project->sounds[0].sample_edit.speed=2;changed.project->patterns[1].tracks[0].sound_override=0;control.load();changed.load();control.trigger(poly);changed.trigger(poly);equal(control.render(128),changed.render(128));
  changed.send({CommandType::LaunchClip,0,0,1});equal(control.render(128),changed.render(128));changed.send({CommandType::LaunchClip,0,0,0});changed.speed(3);equal(control.render(128),changed.render(128));}
 // Transactional preset/Undo replacement uses the same retirement contract.
 {Rig control,changed;control.load();changed.load();control.trigger(false);changed.trigger(false);equal(control.render(128),changed.render(128));track_at(*changed.project,0,0).sample_edit.speed=2;auto transaction=std::make_unique<PatternState>(*changed.project,0);transaction->include_settings=true;assert(changed.engine->replace_pattern(0,*transaction,1,true));changed.engine->render(changed.scratch,0);equal(control.render(128),changed.render(128));}
 std::puts("sampler instances: source replacement isolation, live poly speed, next-note sample and complete step sound selection passed");
}
