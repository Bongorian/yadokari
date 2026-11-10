#include "engine.h"
#include "project_codec.h"
#include "layer_controls.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();auto q=std::make_unique<ProjectState>();auto e=std::make_unique<Engine>();float audio[256]{};
 Command on{CommandType::MidiInput,0,60,0x90};on.scalar=100;Command off{CommandType::MidiInput,0,60,0x80};
 for(Instrument inst:{Instrument::WaveSequence,Instrument::ChipClock,Instrument::TapeMicrosound,Instrument::WestCoast,Instrument::SpectralFreeze}){
  make_empty_project(*p);auto& track=p->tracks[0];track.instrument=Instrument::Layer;track.layer.count=1;auto& part=track.layer.parts[0];part.sound.instrument=inst;part.sound.parameters[0]=.8f;
  assert(layer_leaf_supported(inst) && !live_poly_capable(inst));unsigned cost=0;assert(layer_status(track.layer,&cost)==LayerReason::Ready && cost<=12);
  ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));{ChunkSource input{bytes.data,bytes.size,0};assert(decode_project(input,*q));}assert(q->tracks[0].layer.parts[0].sound.instrument==inst);
  assert(e->stage_project(*p));e->render(audio,0);on.step=60;assert(e->submit(on));e->render(audio,128);on.step=67;assert(e->submit(on));double energy=0;
  for(unsigned block=0;block<20;++block){e->render(audio,128);for(float x:audio){assert(std::isfinite(x));energy+=double(x)*x;}}
  assert(energy>1e-7 && e->layer_note_voices(0)==2);off.step=60;assert(e->submit(off));off.step=67;assert(e->submit(off));for(unsigned block=0;block<100;++block)e->render(audio,128);assert(e->layer_note_voices(0)==0);
 }
 make_empty_project(*p);auto& layer=p->tracks[0].layer;p->tracks[0].instrument=Instrument::Layer;layer.count=2;
 for(auto& part:layer.parts){part.sound.instrument=Instrument::Additive;part.sound.additive.values[8]=.005f;}
 auto& a=layer.parts[0];auto& b=layer.parts[1];a.key_high=59;b.key_low=60;a.velocity_high=80;b.velocity_low=81;
 assert(layer_part_matches(a,59,80) && !layer_part_matches(a,60,80) && !layer_part_matches(a,59,81));assert(layer_part_matches(b,60,81));
 uint8_t mask=0;assert(layer_note_cost(layer,59,80,&mask)==3 && mask==1);assert(layer_note_cost(layer,60,81,&mask)==3 && mask==2);assert(layer_note_cost(layer,60,80,&mask)==0 && mask==0);
 assert(e->stage_project(*p));e->render(audio,0);on.step=60;on.scalar=80;assert(e->submit(on));e->render(audio,128);assert(e->layer_note_voices(0)==0);on.scalar=81;assert(e->submit(on));e->render(audio,128);assert(e->layer_note_voices(0)==1);
 // A dormant leaf with a missing asset does not refuse an unrelated split.
 a.sound.instrument=Instrument::Sampler;a.key_high=59;assert(e->stage_project(*p));e->render(audio,0);assert(e->submit(on));e->render(audio,128);assert(e->layer_note_voices(0)==1);a.sound.instrument=Instrument::Additive;
 a.fx.present=b.fx.present=true;a.fx.inserts[0]=b.fx.inserts[0]=19;a.fx.delay=.3f;a.fx.reverb=.4f;a.fx.inserts[4]=5;
 unsigned expected=3+fx_descriptor(19)->cost+fx_descriptor(5)->cost;assert(layer_part_cost(a)==expected);unsigned peak=0;assert(layer_status(layer,&peak)==(expected>12?LayerReason::Cost:LayerReason::Ready));
 // Overlapping leaves exceeding 12 are refused atomically, with no partial note.
 a.key_high=127;a.velocity_high=127;b.key_low=0;b.velocity_low=1;assert(layer_status(layer)==LayerReason::Cost);assert(e->stage_project(*p));e->render(audio,0);assert(e->submit(on));e->render(audio,128);assert(e->layer_note_voices(0)==0 && e->layer_last_reason(0)==LayerReason::Cost);
 a.fx.disabled=1;b.fx.present=false;assert(layer_part_cost(a)==3u+fx_descriptor(5)->cost);assert(layer_status(layer)==LayerReason::Ready);
 // Roundtrip ranges and leaf+root FX together; GUID identity is not regenerated.
 a.key_low=24;a.key_high=72;a.velocity_low=12;a.velocity_high=115;p->tracks[0].preset_fx.present=true;p->tracks[0].preset_fx.inserts[0]=7;
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));{ChunkSource input{bytes.data,bytes.size,0};assert(decode_project(input,*q));}const auto& restored=q->tracks[0].layer.parts[0];assert(restored.key_low==24 && restored.key_high==72 && restored.velocity_low==12 && restored.velocity_high==115 && restored.fx.present && restored.fx.disabled==1 && restored.fx.inserts[0]==19 && restored.fx.delay==.3f && q->tracks[0].preset_fx.present);
 // Leaf FX actually process PCM before the stack's track inserts.
 auto sound_hash=[&](){auto renderer=std::make_unique<Engine>();assert(renderer->stage_project(*p));renderer->render(audio,0);assert(renderer->submit(on));uint64_t hash=1469598103934665603ull;for(unsigned n=0;n<16;++n){renderer->render(audio,128);for(float sample:audio){assert(std::isfinite(sample));uint32_t bits;__builtin_memcpy(&bits,&sample,4);hash=(hash^bits)*1099511628211ull;}}return hash;};
 layer.count=1;a.fx.present=false;auto dry=sound_hash();a.fx.present=true;a.fx.disabled=0;a.fx.inserts[0]=7;a.fx.inserts[1]=.5f;a.fx.inserts[2]=1;a.fx.inserts[3]=1;auto leaf_fx=sound_hash();p->tracks[0].insert_fx[0]=5;p->tracks[0].insert_fx[1]=.9f;auto combined=sound_hash();assert(dry!=leaf_fx && leaf_fx!=combined);
 // The same range editor clamps endpoints and keeps Undo's payload valid.
 assert(edit_layer_knob(1,3,a,1000,false) && a.key_low==a.key_high);assert(edit_layer_knob(1,6,a,-1000,false) && a.velocity_high==a.velocity_low);assert(valid_layer_shape(layer));
 for(unsigned inst=0;inst<256;++inst){if(!engine_descriptor(inst))continue;if(!layer_leaf_supported(Instrument(inst)))assert(layer_leaf_exclusion(Instrument(inst))!=nullptr);}
 printf("Layer components: private Creative voices/release, ranges, active-leaf admission, leaf/root PFXS roundtrip and cost refusal passed\n");
}
