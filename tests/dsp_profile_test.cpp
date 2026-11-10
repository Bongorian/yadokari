#include "engine.h"
#include "dsp_probe_workload.h"
#include "update/serial_control.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <cstring>
static uint32_t clock_tick=0;static uint32_t clock_now(){return clock_tick+=3;}
static void reset_identity(){
 auto texture_a=std::make_unique<groove::TextureVoice>(),texture_b=std::make_unique<groove::TextureVoice>();
 auto resonator_a=std::make_unique<groove::ResonatorVoice>(),resonator_b=std::make_unique<groove::ResonatorVoice>();
 float texture[15]={0,.5f,.8f,0,.8f,.4f,1,.7f,.5f,.6f,0,0,52,0,1};
 float resonator[8]={1,2,.8f,.7f,.6f,.5f,0,36};
 texture_a->init(123);texture_b->init(123);resonator_a->init(123);resonator_b->init(123);
 texture_a->trigger();texture_b->trigger();assert(resonator_a->trigger(60) && resonator_b->trigger(60));
 float a,b,c,d;for(unsigned i=0;i<256;++i){texture_a->next(texture,.2f,-.1f,a,b);texture_b->next(texture,.2f,-.1f,c,d);resonator_a->next(resonator,0,a,b);resonator_b->next(resonator,0,c,d);}
 texture_a->init(456);texture_b->reset(456);resonator_a->init(456);resonator_b->reset(456);
 assert(!texture_b->frozen() && texture_b->boundary() && resonator_b->boundary());
 resonator_b->next(resonator,0,a,b);assert(a==0 && b==0);
 texture_a->trigger();texture_b->trigger();assert(resonator_a->trigger(63) && resonator_b->trigger(63));
 for(unsigned i=0;i<4096;++i){texture_a->next(texture,.2f,-.1f,a,b);texture_b->next(texture,.2f,-.1f,c,d);assert(a==c && b==d);resonator_a->next(resonator,0,a,b);resonator_b->next(resonator,0,c,d);assert(a==c && b==d);}
}
int main(){
 reset_identity();
 groove::update::SerialControl serial;groove::update::SerialCommand command=groove::update::SerialCommand::None;for(char c: "!GBX1 DSPTEST\n")if(c)command=serial.feed(uint8_t(c));assert(command==groove::update::SerialCommand::DspTest);for(char c: "!GBX1 DSPSTRESS\n")if(c)command=serial.feed(uint8_t(c));assert(command==groove::update::SerialCommand::DspStress);
 auto profiler=std::make_unique<groove::DspProfile>();assert(profiler->quiescent());assert(!profiler->begin(nullptr));assert(profiler->begin(clock_now,4));assert(!profiler->begin(clock_now));profiler->block_begin();profiler->control_end(4,true);{groove::DspTimingScope timer(*profiler,groove::DspProfile::voice(2,0));}profiler->cut(groove::DspProfile::Control,24);profiler->block_end(24);profiler->stop();assert(!profiler->ready());profiler->block_begin();assert(profiler->ready());assert(profiler->snapshot().sampled==1 && profiler->snapshot().control_calls==1);assert(profiler->counter(64).frames==1 && profiler->counter(64).ticks==3 && profiler->counter(64).max_voice_q16==3*65536);
 assert(profiler->begin(clock_now,5));profiler->block_begin();profiler->control_end(4,false);{groove::DspTimingScope timer(*profiler,64);}profiler->block_end(24);profiler->stop();profiler->block_begin();assert(profiler->ready() && !profiler->counter(64).calls && profiler->snapshot().control_calls==1);
 auto project=std::make_unique<groove::ProjectState>();for(unsigned i=0;i<groove::dsp_case_count();++i){groove::make_dsp_probe_project(*project,groove::dsp_condition(i));assert(groove::valid_project(*project));assert(project->random_seed==0x57313001);for(auto& sound:project->sounds)assert(sound.instrument==groove::Instrument::Empty);}
 auto a=std::make_unique<groove::Engine>(),b=std::make_unique<groove::Engine>();groove::make_dsp_probe_project(*project,{groove::DspCondition::Leaf,groove::Instrument::Additive,0,1,1});assert(a->stage_project(*project) && b->stage_project(*project));float x[48],y[48];a->render(x,24);b->render(y,24);assert(b->dsp_profile().begin(clock_now));b->dsp_profile().begin_measurement();groove::Command c{groove::CommandType::Trigger,0,0,100};c.parameter=1;c.scalar=60;assert(a->submit(c) && b->submit(c));for(unsigned n=0;n<300;++n){a->render(x,24);b->render(y,24);assert(!std::memcmp(x,y,sizeof x));}assert(b->dsp_profile().last_block().ticks>0);b->dsp_profile().stop();b->render(y,24);assert(!b->dsp_profile().last_block().ticks);assert(b->dsp_profile().ready());
 groove::make_empty_project(*project);auto& track=project->tracks[0];track.instrument=groove::Instrument::Layer;track.layer.count=2;track.layer.parts[0].sound.instrument=groove::Instrument::Additive;track.layer.parts[1].sound.instrument=groove::Instrument::Resonator;assert(groove::layer_status(track.layer)==groove::LayerReason::Unsupported);assert(a->stage_project(*project));a->render(x,24);assert(a->submit(c));a->render(x,24);assert(!a->poly_note_count(0) && a->layer_last_reason(0)==groove::LayerReason::Unsupported);
 assert(a->submit({groove::CommandType::Panic,0,0,0}));a->render(x,24);assert(a->layer_last_reason(0)==groove::LayerReason::Ready);assert(a->submit(c));a->render(x,24);assert(a->layer_last_reason(0)==groove::LayerReason::Unsupported);
 groove::make_empty_project(*project);assert(a->stage_project(*project));a->render(x,24);assert(a->layer_last_reason(0)==groove::LayerReason::Ready);
 std::printf("DSP profile handoff, generation isolation, PCM identity, %u fixed conditions and atomic track-backed Layer rejection passed\n",groove::dsp_case_count());
}
