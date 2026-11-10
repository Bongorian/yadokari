#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();auto e=std::make_unique<Engine>();
 for(unsigned mode=0;mode<2;++mode){
  FormantState state;state.values[0]=float(mode);state.values[8]=.005f;FormantVoice voice;voice.init(48000);voice.trigger(1,state.values);double energy=0;for(unsigned n=0;n<8000;++n){float x=voice.next(220,n<1000);assert(__builtin_isfinite(x));energy+=x*x;}assert(energy>.01 && !voice.active());
  p->content_kind=ContentKind::Preset;p->tracks[0].preset_identity=builtin_factory_identity(mode+1);auto t=track_at(*p,0,0);t.instrument=Instrument::Formant;t.formant=state;snapshot_sound(p->sounds[0],t);ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource input{bytes.data,bytes.size,0};assert(decode_project(input,*out));auto loaded=track_at(*out,0,0);assert(loaded.instrument==Instrument::Formant && loaded.formant.values[0]==mode);mount_sound(t,p->sounds[0]);assert(t.formant.values[0]==mode);
  assert(e->stage_project(*out));float audio[256]{};e->render(audio,0);Command note{CommandType::MidiInput,0,60,0x90};note.scalar=100;assert(e->submit(note));energy=0;for(unsigned n=0;n<32;++n){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.001);
  auto ui=std::make_unique<Performance>(*e,*out);ui->set_mode(UiMode::Synth);assert(ui->soft_key(2));assert(ui->knob_parameter(2)==engine_target(Instrument::Formant,5));
 }
 assert(instrument_voice_limit(Instrument::Formant)==8);for(unsigned n=0;n<9;++n){auto* d=engine_parameter(Instrument::Formant,n);assert(d && d->value.initial==FormantState{}.values[n]);}
 puts("FORMANT: both modes finite/release, GUID preset mount, DSP route, envelope mapping, schema/8 voice contract passed");
}
