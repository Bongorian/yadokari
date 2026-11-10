#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(!prepare_wavetable_bank() && !wavetable_bank_ready());allocation_admission_hook=nullptr;assert(prepare_wavetable_bank() && wavetable_bank_ready());
 ProjectCrc bank_crc;for(unsigned i=0;i<458752;++i)bank_crc.add(reinterpret_cast<const uint8_t*>(kWavetableData)[i]);assert(bank_crc.value()==0x1db3d4c9u);

 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::Wavetable;t.wavetable.values[0]=42.5f;t.wavetable.values[1]=9;t.wavetable.values[4]=.01f;
 snapshot_sound(p->sounds[255],t);t.wavetable.values[0]=80;assert(p->sounds[255].wavetable.values[0]==42.5f);mount_sound(t,p->sounds[255]);assert(t.wavetable.values[0]==42.5f);
 t.steps[0].locks[0]={engine_target(Instrument::Wavetable,0),63,true};capture_parameter_snapshot(p->morph_a[0],*p,0);p->morph_b[0]=p->morph_a[0];p->morph_b[0].engine_values[0]=100;
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource source{bytes.data,bytes.size,0};auto out=std::make_unique<ProjectState>();assert(decode_project(source,*out));assert(out->tracks[0].instrument==Instrument::Wavetable && out->tracks[0].wavetable.values[0]==42.5f && out->sounds[255].wavetable.values[1]==9);assert(out->morph_b[0].engine_values[0]==100);ChunkBuffer again;assert(encode_project_buffer(again,*out));assert(bytes.size==again.size && !__builtin_memcmp(bytes.data,again.data,bytes.size));
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[256]{};e->render(audio,0);Command note{CommandType::MidiInput,0,69,0x90};note.scalar=110;assert(e->submit(note));double energy=0;
 for(unsigned b=0;b<40;++b){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.001);
 note.value=0x80;note.scalar=0;assert(e->submit(note));for(unsigned b=0;b<20;++b)e->render(audio,128);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->knob_parameter(0)==engine_target(Instrument::Wavetable,0));assert(ui->knob(0,1000));e->render(audio,0);assert(t.wavetable.values[0]>42.5f);assert(ui->undo());e->render(audio,0);assert(t.wavetable.values[0]==42.5f);
 // The complete bank and mip interpolation remain finite over note/scan extremes.
 WavetableVoice voice;voice.init(48000);WavetableState state;state.values[1]=63.5f;state.values[0]=127;state.values[4]=.005f;
 for(float hz:{20.f,440.f,12000.f,20000.f}){voice.trigger(hz,1,state.values);for(unsigned n=0;n<2000;++n)assert(__builtin_isfinite(voice.next(hz,n<500)));assert(!voice.active());}
 std::printf("Wavetable: bank/render/release, scoped UI/Undo, pool independence, preset/Track/Morph codec passed; energy %.6f\n",energy);
}
