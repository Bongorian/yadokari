#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include "future_factory_presets.h"
#include "ui_renderer.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 // Every registered non-composite payload must be portable, including engines
 // deliberately excluded from note-driven layers. Eligibility cannot hide a
 // missing state pointer (FORMANT / GRANULAR regression).
 for(unsigned i=0;i<256;++i){const auto* d=engine_descriptor(i);if(!d || Instrument(i)==Instrument::Layer)continue;
  auto part=std::make_unique<LayerPart>();part->sound.instrument=Instrument(i);
  ChunkBuffer encoded;assert(layer_write_child(encoded,*part));auto restored=std::make_unique<LayerPart>();
  assert(layer_read_child({encoded.data,encoded.size,0},*restored));assert(restored->sound.instrument==Instrument(i));
  ChunkBuffer again;assert(layer_write_child(again,*restored));assert(again.size==encoded.size && !__builtin_memcmp(again.data,encoded.data,encoded.size));
 }

 auto p=std::make_unique<ProjectState>();auto q=std::make_unique<ProjectState>();assert(make_future_factory_preset(*p,15));auto t=track_at(*p,0,0);snapshot_sound(p->sounds[255],t);t.layer.parts[0].detune=-12;assert(p->sounds[255].layer.parts[0].detune==-7);mount_sound(t,p->sounds[255]);ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource source{bytes.data,bytes.size,0};assert(decode_project(source,*q));assert(q->tracks[0].layer.count==2 && q->tracks[0].layer.parts[0].detune==-7);ChunkBuffer again;assert(encode_project_buffer(again,*q));assert(bytes.size==again.size && !__builtin_memcmp(bytes.data,again.data,bytes.size));
 // Unknown child and unknown subchunks survive independently of typed state.
 auto& part=t.layer.parts[0];ChunkBuffer raw;assert(layer_write_child(raw,part));raw.data[1]='X';ChunkSource child{raw.data,raw.size,0};assert(layer_read_child(child,part));assert(part.sound.instrument==Instrument::Unknown && layer_status(t.layer)==LayerReason::Unknown);ChunkBuffer unknown;assert(encode_project_buffer(unknown,*p));ChunkSource input{unknown.data,unknown.size,0};assert(decode_project(input,*q));ChunkBuffer round;assert(encode_project_buffer(round,*q));assert(round.size==unknown.size && !__builtin_memcmp(round.data,unknown.data,round.size));assert(make_future_factory_preset(*p,15));
 for(auto& part:t.layer.parts){part.sound.additive.values[8]=.005f;part.sound.phase_dist.values[8]=.005f;}
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[256]{};e->render(audio,0);Command on{CommandType::MidiInput,0,60,0x90};on.scalar=100;assert(e->submit(on));double energy=0;for(unsigned n=0;n<20;++n){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.01 && e->layer_note_voices(0)==2);Command off{CommandType::MidiInput,0,60,0x80};assert(e->submit(off));for(unsigned n=0;n<100;++n)e->render(audio,128);assert(e->layer_note_voices(0)==0);
 // Four cost-3 layers consume 12 units/note: notes beyond two are wholly refused.
 t.layer.count=4;for(auto& part:t.layer.parts){part.sound.instrument=Instrument::Additive;part.sound.additive.values[8]=.005f;}assert(e->stage_project(*p));e->render(audio,0);for(unsigned n=0;n<4;++n){on.step=uint8_t(60+n);assert(e->submit(on));e->render(audio,64);}assert(e->layer_note_voices(0)==8 && e->layer_last_reason(0)==LayerReason::Cost && e->poly_owner_masks_valid());
 // A missing sampler refuses every child before any voice starts.
 t.layer.parts[3].sound.instrument=Instrument::Sampler;assert(e->stage_project(*p));e->render(audio,0);assert(e->submit(on));e->render(audio,64);assert(e->layer_note_voices(0)==0 && e->layer_last_reason(0)==LayerReason::Assets);
 make_empty_project(*p);p->sounds[7].instrument=Instrument::Additive;assert(e->stage_project(*p));e->render(audio,0);auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->set_instrument(Instrument::Layer));e->render(audio,0);assert(ui->layer_page());assert(ui->knob(2,7000));assert(ui->soft_presentation(2).label && !__builtin_strcmp(ui->soft_presentation(2).label,"USE CHILD"));assert(!__builtin_strcmp(ui->soft_presentation(4).label,"ENGINE"));assert(!__builtin_strcmp(ui->soft_presentation(5).label,"MODULATION"));assert(ui->soft_key(2));e->render(audio,0);assert(t.layer.count==1 && t.layer.parts[0].source==7);assert(ui->knob(5,12000));e->render(audio,0);assert(t.layer.parts[0].detune==12);assert(ui->undo());e->render(audio,0);assert(t.layer.parts[0].detune==0);assert(ui->knob(7,1000));assert(ui->knob(3,1000));e->render(audio,0);assert(t.layer.parts[0].key_low==1);assert(ui->undo());e->render(audio,0);assert(t.layer.parts[0].key_low==0);assert(ui->knob(7,1000));assert(ui->knob(3,1000));e->render(audio,0);assert(t.layer.parts[0].fx.present && t.layer.parts[0].fx.inserts[0]==1);assert(ui->soft_key(3) && ui->soft_key(0));e->render(audio,0);assert(t.layer.count==0);for(unsigned i=0;i<256;++i)assert(i==7 || p->sounds[i].instrument==Instrument::Empty);
 printf("Layer: GUID/unknown roundtrip, independent snapshot, whole-note cost/assets admission, release and UI Undo passed (Project %zu bytes)\n",sizeof(ProjectState));
}
