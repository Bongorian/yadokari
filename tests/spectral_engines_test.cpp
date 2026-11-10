#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include "future_factory_presets.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 AdditiveState a;a.values[8]=.005f;AdditiveVoice av;av.init(48000);av.trigger(1,a.values);double energy=0;for(unsigned i=0;i<8000;++i){float x=av.next(440,i<1000);assert(__builtin_isfinite(x));energy+=x*x;}assert(!av.active() && energy>.01);
 PhaseDistState d;d.values[8]=.005f;PhaseDistVoice dv;dv.init(48000);for(unsigned wave=0;wave<4;++wave){d.values[0]=float(wave);dv.trigger(1,d.values);double sum=0;for(unsigned i=0;i<8000;++i){float x=dv.next(440,i<1000);assert(__builtin_isfinite(x));sum+=x*x;}assert(!dv.active() && sum>.01);}
 d=PhaseDistState{};d.values[1]=.9f;d.values[2]=d.values[3]=0;d.values[7]=1;dv.trigger(1,d.values);double mean=0;for(unsigned i=0;i<72000;++i){float x=dv.next(440,true);if(i>=24000)mean+=x;}assert(fabs(mean/48000)<.001);
 auto p=std::make_unique<ProjectState>();auto out=std::make_unique<ProjectState>();auto e=std::make_unique<Engine>();float audio[256]{};
 for(unsigned k=11;k<15;++k){assert(make_future_factory_preset(*p,k));auto t=track_at(*p,0,0);snapshot_sound(p->sounds[255],t);ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource in{bytes.data,bytes.size,0};assert(decode_project(in,*out));ChunkBuffer again;assert(encode_project_buffer(again,*out));assert(bytes.size==again.size && !__builtin_memcmp(bytes.data,again.data,bytes.size));assert(e->stage_project(*p));e->render(audio,0);Command note{CommandType::MidiInput,0,60,0x90};note.scalar=100;assert(e->submit(note));double sum=0;for(unsigned n=0;n<40;++n){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));sum+=x*x;}}assert(sum>.001);auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->soft_key(2));assert(ui->knob_parameter(2)==engine_target(t.instrument,5));float before=engine_parameter_value(t,5);assert(ui->knob(2,1000));e->render(audio,0);assert(engine_parameter_value(t,5)>before);assert(ui->undo());e->render(audio,0);assert(engine_parameter_value(t,5)==before);}
 puts("ADDITIVE/PHASE DIST: finite audio/release, factory presets, GUID roundtrip, UI ADSR/Undo passed");
}
