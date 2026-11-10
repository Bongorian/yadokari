#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 VirtualAnalogState defaults;VirtualAnalogVoice voice;voice.init(48000);double energy=0;
 for(unsigned wave=0;wave<4;++wave){auto state=defaults;state.values[0]=float(wave);state.values[1]=float((wave+1)%4);state.values[9]=.3f;state.values[14]=.005f;voice.trigger(1,state.values,1234);for(unsigned i=0;i<4000;++i){float sample=voice.next(220,i<1000);assert(__builtin_isfinite(sample));energy+=sample*sample;}assert(!voice.active());}
 assert(energy>.01);assert(engine_voice_cost(Instrument::VirtualAnalog,0)==4 && instrument_voice_limit(Instrument::VirtualAnalog)==8);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::VirtualAnalog;t.virtual_analog.values[11]=.02f;t.virtual_analog.values[7]=.17f;snapshot_sound(p->sounds[255],t);t.virtual_analog.values[11]=.1f;assert(p->sounds[255].virtual_analog.values[11]==.02f);mount_sound(t,p->sounds[255]);
 assert(!set_engine_parameter(t,0,.5f));assert(set_engine_parameter(t,5,.7f));assert(engine_parameter_value(t,5)==.7f);
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource source{bytes.data,bytes.size,0};auto out=std::make_unique<ProjectState>();assert(decode_project(source,*out));assert(out->tracks[0].virtual_analog.values[11]==.02f && out->sounds[255].virtual_analog.values[7]==.17f);ChunkBuffer again;assert(encode_project_buffer(again,*out));assert(bytes.size==again.size && !__builtin_memcmp(bytes.data,again.data,bytes.size));
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[128]{};e->render(audio,0);Command note{CommandType::MidiInput,0,60,0x90};note.scalar=100;assert(e->submit(note));double rendered=0;for(unsigned n=0;n<20;++n){e->render(audio,64);for(float x:audio){assert(__builtin_isfinite(x));rendered+=x*x;}}assert(rendered>.01);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->knob_parameter(0)==engine_target(Instrument::VirtualAnalog,0));assert(ui->soft_key(2));assert(ui->knob_parameter(2)==engine_target(Instrument::VirtualAnalog,11));assert(ui->knob(2,1000));e->render(audio,0);assert(t.virtual_analog.values[11]>.02f);assert(ui->undo());e->render(audio,0);assert(t.virtual_analog.values[11]==.02f);
 std::printf("VA: four waveforms, PWM/sub/noise/Ladder finite render and release, declared cost, codec, independent mount and editable ADSR/Undo passed; energy %.6f\n",rendered);
}
