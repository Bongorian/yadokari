#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
using namespace groove;
static uint32_t fake_profile_clock(){static uint32_t t=0;return ++t;}
struct Buffer {
 std::vector<uint8_t> data;size_t position=0;
 bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}
 bool read(uint8_t* p,size_t n){if(position+n>data.size())return false;for(size_t i=0;i<n;++i)p[i]=data[position+i];position+=n;return true;}
 bool at_end()const{return position==data.size();}
};
int main(){
 {
  // Live pads and MIDI notes record to independent kit lanes using the
  // shared transport, including non-quantized timing. Track-level locks are
  // not erased when a previously selected melodic Replace mode is active.
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->ui.note_quantized=false;p->ui.note_replace=true;auto t=track_at(*p,0,0);t.instrument=Instrument::DrumKit;t.steps[0].enabled=true;t.steps[0].locks[0]={3,0.5f,true};for(unsigned pad=0;pad<3;++pad)t.kit.pads[pad].sample[0]=0;
  std::vector<uint8_t> pcm(8000,32);WavView samples[kSampleAssets]{};samples[0]={pcm.data(),4000,8000,1,16,1};auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*p,samples));float audio[512]{};e->render(audio,0);Performance ui(*e,*p);ui.set_mode(UiMode::Pad);assert(ui.arm_note_record(true) && ui.play());e->render(audio,200);
  assert(ui.pad(12,0x10,4095) && ui.pad(13,0x10,2048));e->render(audio,0);ui.poll();e->render(audio,0);assert(t.kit.steps[0][0].enabled && t.kit.steps[0][0].velocity==127 && t.kit.steps[0][0].microtiming==19);assert(t.kit.steps[1][0].enabled && t.kit.steps[1][0].microtiming==19);assert(t.steps[0].enabled && t.steps[0].locks[0].enabled);
  assert(e->receive_midi({0,0,3,{0x90,42,100}},0));e->render(audio,0);ui.poll();e->render(audio,0);assert(t.kit.steps[2][0].enabled && t.kit.steps[2][0].velocity==100 && t.kit.steps[2][0].microtiming==19);assert(ui.undo());e->render(audio,0);assert(!t.kit.steps[2][0].enabled && t.kit.steps[0][0].enabled);
  assert(ui.arm_note_record(false));e->render(audio,0);assert(e->watch_kit_hits(0,0,0,0,127,64));for(unsigned i=0;i<64;++i)e->render(audio,256);assert(e->kit_watch_hits(0)>0 && e->kit_watch_hits(1)>0);assert(e->kit_watch_notes(0)==e->kit_watch_velocity(0) && e->kit_watch_notes(1)==e->kit_watch_velocity(1));assert(e->kit_watch_frames(0)>0 && e->kit_watch_frames(1)>0);e->stop_kit_watch();
  ui.set_mode(UiMode::Step);ui.set_shift(true);assert(!ui.soft_enabled(5) && !std::strcmp(ui.soft_label(5),"PAD OVERDUB"));
 }
 {
  // Step 1's early first hit is at 3/4 of the preceding 1000-frame step.
  // Its second ratchet crosses the grid boundary and must not be lost or
  // duplicated when the next step is prepared.
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::DrumKit;t.kit.pads[0].sample[0]=0;t.kit.pads[0].edit.fade_ms=0;auto& k=t.kit.steps[0][1];k.enabled=1;k.microtiming=-24;k.ratchet=2;
  std::vector<uint8_t> pcm(16000,32);WavView wav[kSampleAssets]{};wav[0]={pcm.data(),8000,8000,1,16,1};auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*p,wav));float frame[2200]{};e->render(frame,0);assert(e->submit({CommandType::Start,0,0,0}));e->render(frame,749);assert(e->active_sample_voices()==0);e->render(frame,2);assert(e->active_sample_voices()==1);e->render(frame,250);assert(e->active_sample_voices()==1);e->render(frame,250);assert(e->active_sample_voices()==2);
  assert(e->submit({CommandType::Stop,0,0,0}));e->render(frame,1);assert(e->active_sample_voices()==0);e->render(frame,1000);assert(e->active_sample_voices()==0);
  // Editing the upcoming step before its first hit replaces its lookahead.
  e=std::make_unique<Engine>(8000);assert(e->stage_project(*p,wav));e->render(frame,0);assert(e->submit({CommandType::Start,0,0,0}));e->render(frame,500);Command edit{CommandType::SetKitStep,0,1,0};edit.parameter=0;edit.kit_step=k;edit.kit_step.microtiming=-12;assert(e->submit(edit));e->render(frame,375);assert(e->active_sample_voices()==0);e->render(frame,1);assert(e->active_sample_voices()==1);
  // A faster tempo rescales a not-yet-fired lookahead from sample 750 to 625.
  e=std::make_unique<Engine>(8000);assert(e->stage_project(*p,wav));e->render(frame,0);assert(e->submit({CommandType::Start,0,0,0}));e->render(frame,500);assert(e->submit({CommandType::SetTempo,0,0,240}));e->render(frame,125);assert(e->active_sample_voices()==0);e->render(frame,1);assert(e->active_sample_voices()==1);
  // Muting cancels the pending hit; unmuting before the grid must not revive it.
  e=std::make_unique<Engine>(8000);assert(e->stage_project(*p,wav));e->render(frame,0);assert(e->submit({CommandType::Start,0,0,0}));e->render(frame,500);Command mute{CommandType::SetKitPad,0,0,0};mute.kit_pad=t.kit.pads[0];mute.kit_pad.muted=true;assert(e->submit(mute));e->render(frame,1);mute.kit_pad.muted=false;assert(e->submit(mute));e->render(frame,500);assert(e->active_sample_voices()==0);
 }
 {
  // Copying between a kit lane and a melodic track transfers the common
  // rhythm, never stale clipboard data or the destination's sound locks.
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto drum=track_at(*p,0,0);drum.instrument=Instrument::DrumKit;auto& k=drum.kit.steps[0][0];k.enabled=1;k.velocity=91;k.probability=75;k.ratchet=3;k.microtiming=9;k.sample_lock=5;
  auto& s=track_at(*p,0,1).steps[0];s.note=73;s.gate=55;s.locks[0]={3,0.5f,true};
  auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*p));float frame[2]{};e->render(frame,0);Performance ui(*e,*p);ui.set_mode(UiMode::Step);ui.set_shift(true);
  assert(ui.soft_key(6) && ui.select_track(1) && ui.soft_key(7));assert(s.enabled && s.velocity==91 && s.probability==75 && s.ratchet==3 && s.microtiming==36 && s.note==73 && s.gate==55 && s.locks[0].enabled);e->render(frame,0);assert(ui.undo() && !s.enabled);e->render(frame,0);assert(ui.redo() && s.enabled);e->render(frame,0);
  s.velocity=67;s.microtiming=120;assert(ui.soft_key(6) && ui.select_track(0) && ui.soft_key(7));assert(k.velocity==67 && k.microtiming==30 && k.sample_lock==5);e->render(frame,0);assert(ui.undo() && k.velocity==91 && k.microtiming==9 && k.sample_lock==5);
 }
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 auto track=track_at(*project,0,0);track.instrument=Instrument::DrumKit;track.level=0.35f;
 for(unsigned pad=0;pad<2;++pad){track.kit.pads[pad].sample[0]=uint8_t(pad);track.kit.pads[pad].edit.fade_ms=0;track.kit.steps[pad][0].enabled=1;}
 track.kit.steps[1][0].sample_lock=0;track.kit.steps[1][0].pitch_lock=7;track.kit.steps[1][0].level_lock=96;track.kit.steps[1][0].delay_lock=31;track.kit.steps[1][0].reverb_lock=80;
 project->assets[0].metadata.root=project->assets[1].metadata.root=60;
 Buffer bytes;assert(encode_project(bytes,*project) && bytes.data[0]=='M' && bytes.data[1]=='G' && bytes.data[2]=='X' && bytes.data[3]=='C');
 auto decoded=std::make_unique<ProjectState>();assert(decode_project(bytes,*decoded));
 assert(track_at(*decoded,0,0).kit.steps[1][0].sample_lock==0);
 assert(track_at(*decoded,0,0).kit.steps[1][0].pitch_lock==7);
 assert(track_at(*decoded,0,0).kit.steps[1][0].level_lock==96 && track_at(*decoded,0,0).kit.steps[1][0].delay_lock==31 && track_at(*decoded,0,0).kit.steps[1][0].reverb_lock==80);
 std::vector<uint8_t> pcm(8192);for(unsigned i=0;i<4096;++i){pcm[i*2]=0;pcm[i*2+1]=64;}
 WavView assets[kSampleAssets]{};assets[0]={pcm.data(),4096,8000,1,16,1};assets[1]=assets[0];
 {
  // Pad mixer parameters share the same scene/macro/lock resolver and survive save.
  auto mix=std::make_unique<ProjectState>(*project);auto& pad=track_at(*mix,0,0).kit.pads[0];uint16_t level=kit_mix_parameter_id(0,0),pan=kit_mix_parameter_id(0,1);assert(parameter_index(level)==338 && parameter_at(417)==553);assert(set_track_parameter(track_at(*mix,0,0),pan,-0.5f) && pad.pan==-0.5f);
  mix->morph_a[0].values[morph_index(level)]=1;mix->morph_b[0].values[morph_index(level)]=0;mix->morph_enabled=true;mix->morph_position=0;
  Buffer encoded;assert(encode_project(encoded,*mix));auto copy=std::make_unique<ProjectState>();assert(decode_project(encoded,*copy) && copy->morph_b[0].values[morph_index(level)]==0 && track_at(*copy,0,0).kit.pads[0].pan==-0.5f);
  std::vector<uint8_t> tone(8192);for(unsigned i=0;i<4096;++i){int16_t x=int16_t(15000*sinf(float(i)*0.19f));tone[2*i]=uint8_t(x);tone[2*i+1]=uint8_t(uint16_t(x)>>8);}WavView mix_assets[kSampleAssets]{};mix_assets[0]={tone.data(),4096,8000,1,16,1};
  auto player=std::make_unique<Engine>(8000);assert(player->stage_project(*mix,mix_assets));float audio[512]{};player->render(audio,0);Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(player->submit(hit));player->render(audio,256);float first=0;for(float x:audio)first+=fabsf(x);assert(first>1);
  Command morph{CommandType::SetMorph,0,0,0};morph.scalar=1;assert(player->submit(morph));for(unsigned i=0;i<8;++i)player->render(audio,256);float silent=0;for(float x:audio)silent+=fabsf(x);assert(silent<first*0.01f);
  mix->morph_enabled=false;auto& route=mix->macros[0].routes[0];route.enabled=true;route.parameter=level;route.low=1;route.high=0;mix->macros[0].active=true;mix->macros[0].value=1;ParameterLock locks[kLocks]{};locks[0]={level,0.4f,true};auto value=resolve_parameter(*mix,track_at(*mix,0,0),0,level,locks,0);assert(fabsf(value.value-0.4f)<0.001f && (value.sources&FromMacro) && (value.sources&FromLock));
  Performance ui(*player,*mix);ui.set_mode(UiMode::Pad);assert(ui.knob(2,-1000));assert(fabsf(pad.level-0.98f)<0.001f);assert(ui.undo() && fabsf(pad.level-1)<0.001f);
 }
 {
  // A control refresh must not replace a drum pad's playback speed with the
  // sampler track's unrelated default speed while its voice is sounding.
  auto speed_project=std::make_unique<ProjectState>(*project);track_at(*speed_project,0,0).kit.pads[0].edit.speed=0.5f;
  std::vector<uint8_t> ramp(8192);for(unsigned i=0;i<4096;++i){int16_t x=int16_t((int(i%128)-64)*400);ramp[i*2]=uint8_t(x);ramp[i*2+1]=uint8_t(uint16_t(x)>>8);}
  WavView sample_assets[kSampleAssets]{};sample_assets[0]={ramp.data(),4096,8000,1,16,1};
  auto reference=std::make_unique<Engine>(8000),changed=std::make_unique<Engine>(8000);assert(reference->stage_project(*speed_project,sample_assets) && changed->stage_project(*speed_project,sample_assets));
  float a[512]{},b[512]{};reference->render(a,0);changed->render(b,0);Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(reference->submit(hit) && changed->submit(hit));changed->begin_stage_profile(fake_profile_clock);reference->render(a,256);changed->render(b,256);for(unsigned i=0;i<512;++i)assert(std::abs(a[i]-b[i])<0.00001f);assert(changed->end_stage_profile().calls==1);
  Command morph{CommandType::SetMorph,0,0,0};morph.scalar=0.5f;assert(changed->submit(morph));reference->render(a,256);changed->render(b,256);for(unsigned i=0;i<512;++i)assert(std::abs(a[i]-b[i])<0.00001f);
 }
 {
  auto rhythm=std::make_unique<ProjectState>(*project);auto track=track_at(*rhythm,0,0);track.kit.steps[1][0].enabled=0;track.kit.steps[0][0].ratchet=4;
  auto player=std::make_unique<Engine>(8000);assert(player->stage_project(*rhythm,assets));float audio[250]{};player->render(audio,0);assert(player->submit({CommandType::Start,0,0,0}));
  uint64_t times[4]{};unsigned notes=0;for(unsigned i=0;i<8;++i){player->render(audio,125);EngineEvent event;while(player->pop_event(event))if(event.type==EngineEventType::Note && event.track==0 && notes<4)times[notes++]=event.sample;}
  assert(notes==4 && times[0]==0 && times[1]==250 && times[2]==500 && times[3]==750);
  assert(player->submit({CommandType::Stop,0,0,0}));player->render(audio,0);assert(player->submit({CommandType::Start,0,0,0}));player->render(audio,125);EngineEvent event;while(player->pop_event(event)){}
  Command mute{CommandType::SetKitPad,0,0,0};mute.kit_pad=track.kit.pads[0];mute.kit_pad.muted=true;assert(player->submit(mute));
  notes=0;for(unsigned i=0;i<7;++i){player->render(audio,125);while(player->pop_event(event))if(event.type==EngineEventType::Note && event.track==0)++notes;}assert(notes==0);
 }
 {
  auto with_fx=std::make_unique<ProjectState>(*project);auto& pad=track_at(*with_fx,0,0).kit.pads[0];pad.insert_fx[0]=1;pad.insert_fx[1]=0;pad.insert_fx[3]=1;
  Buffer saved;assert(encode_project(saved,*with_fx));auto restored=std::make_unique<ProjectState>();assert(decode_project(saved,*restored));assert(track_at(*restored,0,0).kit.pads[0].insert_fx[0]==1);
  pad.insert_fx[0]=18;track_at(*with_fx,0,0).kit.steps[0][0].fx_lock[0]=18;assert(valid_drum_kit(track_at(*with_fx,0,0).kit));Buffer extra;assert(encode_project(extra,*with_fx));assert(decode_project(extra,*restored));assert(track_at(*restored,0,0).kit.pads[0].insert_fx[0]==18 && track_at(*restored,0,0).kit.steps[0][0].fx_lock[0]==18);pad.insert_fx[0]=1;track_at(*with_fx,0,0).kit.steps[0][0].fx_lock[0]=255;
  auto wet=std::make_unique<Engine>(8000),dry=std::make_unique<Engine>(8000);assert(wet->stage_project(*with_fx,assets));pad.insert_fx[0]=0;assert(dry->stage_project(*with_fx,assets));
  float a[512]{},b[512]{};wet->render(a,0);dry->render(b,0);Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(wet->submit(hit) && dry->submit(hit));wet->render(a,256);dry->render(b,256);
  float difference=0;for(unsigned i=0;i<512;++i)difference+=std::abs(a[i]-b[i]);assert(difference>1);
  Performance ui(*wet,*with_fx);ui.set_mode(UiMode::Effects);assert(ui.soft_key(6) && ui.fx_page()==4);assert(ui.knob(1,1000));assert(track_at(*with_fx,0,0).kit.pads[0].insert_fx[0]==1);
  assert(ui.encoder(1) && ui.selected_kit_pad()==1);assert(ui.encoder(-1) && ui.selected_kit_pad()==0);assert(ui.encoder_push() && ui.encoder(1));assert(pad.insert_fx[0]==2 && std::abs(ui.knob_normalized(1)-2.0f/18)<0.0001f);assert(ui.encoder_push());ui.set_shift(true);assert(ui.knob(1,1000) && ui.knob(1,1000));assert(track_at(*with_fx,0,0).kit.steps[0][0].fx_lock[0]==4 && ui.knob_value(1)==4);ui.set_shift(false);assert(ui.control(1,0x10) && ui.selected_kit_pad()==1);assert(ui.control(1,0));assert(ui.control(1,0x04) && ui.selected_kit_pad()==0);assert(ui.control(1,0));
  ui.set_shift(true);assert(ui.knob(2,1000));assert(track_at(*with_fx,0,0).kit.steps[0][0].fx_lock[1]!=255);assert(track_at(*with_fx,0,0).kit.steps[1][0].fx_lock[1]==255);assert(ui.clear_step_locks());assert(track_at(*with_fx,0,0).kit.steps[0][0].fx_lock[1]==255);ui.set_shift(false);
 }
 {
  auto route=std::make_unique<ProjectState>(*project);auto t=track_at(*route,0,0);t.level=0;t.delay_send=1;route->effects.delay_mix=1;route->effects.delay_feedback=0;route->effects.delay_ms=10;
  auto post=std::make_unique<Engine>(8000),pre=std::make_unique<Engine>(8000);assert(post->stage_project(*route,assets));t.send_prefader=1;assert(pre->stage_project(*route,assets));
  t.insert_fx[0]=7;t.insert_fx[1]=0.7f;route->morph_b[0].values[morph_index(95)]=0.3f;
  Buffer saved;assert(encode_project(saved,*route));auto restored=std::make_unique<ProjectState>();assert(decode_project(saved,*restored));assert(track_at(*restored,0,0).send_prefader==1);assert(track_at(*restored,0,0).insert_fx[0]==7 && track_at(*restored,0,0).insert_fx[1]==0.7f && restored->morph_b[0].values[morph_index(95)]==0.3f);
  float p[2048]{},q[2048]{};post->render(p,0);pre->render(q,0);post->render(p,1024);pre->render(q,1024);
  Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(post->submit(hit) && pre->submit(hit));post->render(p,1024);pre->render(q,1024);
  float post_peak=0,pre_peak=0;for(unsigned i=0;i<2048;++i){post_peak=fmaxf(post_peak,fabsf(p[i]));pre_peak=fmaxf(pre_peak,fabsf(q[i]));}assert(post_peak<0.01f && pre_peak>0.02f);
 }
 {
  auto locked=std::make_unique<ProjectState>(*project);track_at(*locked,0,0).steps[0].locks[0]={1,0,true};
  auto player=std::make_unique<Engine>(8000);assert(player->stage_project(*locked,assets));float samples[1600]{};player->render(samples,0);player->render(samples,800);player->render(samples,800);assert(player->submit({CommandType::Start,0,0,0}));
  player->render(samples,800);float muted_peak=0;for(unsigned i=1400;i<1600;++i)muted_peak=fmaxf(muted_peak,fabsf(samples[i]));assert(muted_peak<0.01f);
  player->render(samples,800);float restored_peak=0;for(unsigned i=1000;i<1600;++i)restored_peak=fmaxf(restored_peak,fabsf(samples[i]));assert(restored_peak>0.02f);
 }
 {
  auto layers=std::make_unique<ProjectState>(*project);auto& pad=track_at(*layers,0,0).kit.pads[0];
  pad.sample[1]=1;pad.layer_mode=3;pad.edit.mode=SampleMode::Gate;pad.edit.choke=1;pad.edit.release_ms=2;
  Buffer saved;assert(encode_project(saved,*layers));auto restored=std::make_unique<ProjectState>();assert(decode_project(saved,*restored));assert(track_at(*restored,0,0).kit.pads[0].layer_mode==3);
  auto layered=std::make_unique<Engine>(8000);assert(layered->stage_project(*layers,assets));float audio[512]{};layered->render(audio,0);
  Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(layered->submit(hit));layered->render(audio,128);assert(layered->active_sample_voices()==2);
  assert(layered->submit(hit));layered->render(audio,128);assert(layered->active_sample_voices()==2);
  Command off{CommandType::NoteOff,0,0,0};off.parameter=4;assert(layered->submit(off));layered->render(audio,256);assert(layered->active_sample_voices()==0);
  MidiPacket note_on{0,0,3,{0x90,36,100}},note_off{0,0,3,{0x80,36,0}};assert(layered->receive_midi(note_on,0));layered->render(audio,128);assert(layered->active_sample_voices()==2);
  assert(layered->receive_midi(note_off,0));layered->render(audio,256);assert(layered->active_sample_voices()==0);
  assert(layered->submit({CommandType::Start,0,0,0}));layered->render(audio,128);assert(layered->active_sample_voices()==3);
  assert(layered->submit({CommandType::Stop,0,0,0}));layered->render(audio,128);assert(layered->active_sample_voices()==0);
  Command edit_pad{CommandType::SetKitPad,0,0,0};edit_pad.kit_pad=pad;edit_pad.kit_pad.sample[0]=255;assert(layered->submit(edit_pad));assert(layered->submit(hit));layered->render(audio,128);assert(layered->active_sample_voices()==1);
  assert(layered->submit(off));layered->render(audio,256);assert(layered->active_sample_voices()==0);
  assert(layered->poly_owner_masks_valid());
  Performance workflow(*layered,*layers);workflow.set_mode(UiMode::Step);assert(workflow.modifier(0x08,true));assert(workflow.pad(13,0x10,4095));assert(workflow.selected_kit_pad()==1);assert(workflow.pad(13,0x20,0));assert(workflow.modifier(0x08,false));
  assert(workflow.encoder(1) && workflow.selected_step()==1 && workflow.selected_kit_pad()==1);assert(workflow.encoder(-1) && workflow.selected_step()==0);assert(workflow.modifier(0x08,true));assert(workflow.pad(12,0x10,4095));assert(workflow.selected_kit_pad()==0);assert(workflow.pad(12,0x20,0));assert(workflow.pad(13,0x10,4095));assert(workflow.selected_kit_pad()==1);assert(workflow.pad(13,0x20,0));assert(workflow.modifier(0x08,false));assert(workflow.control(1,0x08) && workflow.page()==1);assert(workflow.control(1,0));assert(workflow.control(1,0x20) && workflow.page()==0 && workflow.selected_step()==0);assert(workflow.control(1,0));
  Command hit_other{CommandType::Trigger,0,1,100};hit_other.parameter=4;hit_other.scalar=1;assert(layered->submit(hit_other));layered->render(audio,128);assert(layered->active_sample_voices()==1);
  assert(workflow.modifier(0x10,true));assert(workflow.pad(12,0x10,4095));assert(track_at(*layers,0,0).kit.solo_mask==1 && !track_at(*layers,0,0).kit.pads[0].muted);assert(workflow.pad(12,0x20,0));assert(workflow.modifier(0x10,false));layered->render(audio,128);assert(layered->active_sample_voices()==0);
  assert(layered->submit(hit_other));layered->render(audio,128);assert(layered->active_sample_voices()==0);assert(layered->submit(hit));layered->render(audio,128);assert(layered->active_sample_voices()==1);
  Buffer solo_saved;assert(encode_project(solo_saved,*layers));auto solo_restored=std::make_unique<ProjectState>();assert(decode_project(solo_saved,*solo_restored));assert(track_at(*solo_restored,0,0).kit.solo_mask==1);
  workflow.set_mode(UiMode::Routing);assert(workflow.knob(6,1000) && workflow.capture_source()==1);assert(workflow.knob(6,-1000) && workflow.capture_source()==0);workflow.set_mode(UiMode::Sampler);
  assert(workflow.control(6,0x40));assert(workflow.take_file_action()==FileAction::RecordToggle);assert(workflow.control(6,0));workflow.set_shift(true);assert(workflow.control(6,0x40));assert(workflow.take_file_action()==FileAction::SampleRecordToggle);assert(workflow.control(6,0));workflow.set_shift(false);
 }
 auto engine=std::make_unique<Engine>(8000);assert(engine->stage_project(*project,assets));float frame[2]{};engine->render(frame,0);
 assert(engine->submit({CommandType::Start,0,0,0}));std::vector<float> out(512);engine->render(out.data(),256);
 assert(engine->active_sample_voices()>=2);float peak=0;for(float x:out)peak=fmaxf(peak,fabsf(x));assert(peak>0.01f);
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(out.data(),256);
 assert(engine->active_sample_voices()==0);for(float x:out)assert(fabsf(x)<0.00001f);
 Performance ui(*engine,*project);ui.set_mode(UiMode::Pad);assert(ui.pad(12,0x10,4095));engine->render(out.data(),256);
 assert(ui.selected_kit_pad()==0 && engine->active_sample_voices()>=1);assert(ui.pad(12,0x20,0));engine->render(frame,0);
 ui.set_mode(UiMode::Step);assert(ui.pad(8,0x10,4095));engine->render(frame,0);assert(track_at(*project,0,0).kit.steps[0][4].enabled);
 ui.set_mode(UiMode::Sampler);auto edit=track_at(*project,0,0).kit.pads[0].edit;edit.start=0.1f;edit.end=0.9f;
 assert(ui.sample_edit(edit));engine->render(frame,0);assert(track_at(*project,0,0).kit.pads[0].edit.start==0.1f);
 assert(track_at(*project,0,0).sample_edit.start!=0.1f);
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(frame,0);
 for(unsigned i=0;i<20;++i){Command c{CommandType::Trigger,0,0,100};c.parameter=4;c.scalar=0;assert(engine->submit(c));}
 engine->render(frame,0);assert(engine->active_sample_voices()>=20);
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(out.data(),256);
 assert(engine->active_sample_voices()==0);for(float x:out)assert(fabsf(x)<0.00001f);
 for(unsigned t=0;t<kTracks;++t)project->morph_b[t]=project->morph_a[t];
 ui.set_mode(UiMode::Perform);
 assert(ui.morph(0.25f));
 assert(!morph_snapshots_equal(*project));
 assert(project->morph_b[0].values[morph_index(3)]<project->morph_a[0].values[morph_index(3)]);
 assert(!engine->morph_stage_ready(0) && !engine->morph_stage_ready(1));
 engine->render(frame,0);
 assert(engine->morph_stage_ready(0) && engine->morph_stage_ready(1));
 for(unsigned i=0;i<1000;++i){
  assert(ui.morph(float(i%101)/100));
  engine->render(out.data(),6);
  for(unsigned n=0;n<12;++n)assert(std::isfinite(out[n]));
 }
 ui.animate(1000);assert(ui.dispatch({ControllerInput::Type::Strip,0,8192}));
 ui::LedFrame strip_leds;ui.led_frame(strip_leds);assert(strip_leds.strip_level[12]==3);
 ui.animate(1300);ui.led_frame(strip_leds);
 assert(strip_leds.strip_level[0]==3 && strip_leds.strip_level[12]==0);
 assert(ui.dispatch({ControllerInput::Type::Strip,0,0}));
 assert(ui.set_control_all(true) && ui.control_all_parameter()==3);
 assert(ui.encoder(1) && ui.control_all_parameter()==4);
 assert(ui.control(1,0x10) && ui.control_all_parameter()==5);
 assert(ui.control(1,0));
 assert(ui.select_control_all_target(1,true) && ui.control_all_parameter()==64);
 assert(ui.dispatch({ControllerInput::Type::Strip,0,13106}) && ui.control_all_position()>0.7f);
 assert(ui.control(1,0));
 assert(ui.control(1,0x01) && ui.control_all_parameter()==64);
 assert(std::abs(ui.control_all_position()-0.5f)<0.001f);
 assert(ui.play());engine->render(out.data(),256);engine->render(out.data(),256);ui.poll();
 ui.animate(2000);ui.led_frame(strip_leds);
 unsigned star=0;for(unsigned i=0;i<25;++i)star+=strip_leds.strip_level[i]!=0;
 assert(star==4);
 uint8_t before[25];for(unsigned i=0;i<25;++i)before[i]=strip_leds.strip_level[i];
 assert(ui.encoder(1));ui.led_frame(strip_leds);
 for(unsigned i=0;i<25;++i)assert(strip_leds.strip_level[i]==before[i]);
 assert(ui.soft_key(6) && ui.performance_page()==4);
 assert(ui.knob(0,1000) && ui.performance_fx_amount()>0.6f);
 assert(ui.pad(14,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::Filter);
 assert(ui.strip_input(12000,16383) && ui.performance_fx_amount()>0.7f);
 engine->render(out.data(),256);
 assert(ui.pad(14,0x20,0) && ui.performance_fx_mode()==PerformanceFx::Off);
 ui.set_shift(true);
 assert(ui.pad(15,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::Gate && ui.performance_fx_latched());
 assert(ui.pad(15,0x20,0) && ui.performance_fx_mode()==PerformanceFx::Gate);
 assert(ui.pad(15,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::Off);
 assert(ui.pad(15,0x20,0));
 ui.set_shift(false);
 assert(ui.pad(0,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::ReverbFreeze);assert(ui.pad(0,0x20,0) && ui.performance_fx_mode()==PerformanceFx::Off);
 assert(ui.soft_key(4) && ui.performance_fx_bank()==1);assert(ui.pad(14,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::PitchDive);assert(ui.pad(14,0x20,0) && ui.performance_fx_mode()==PerformanceFx::Off);assert(ui.pad(15,0x10,3500) && ui.performance_fx_mode()==PerformanceFx::PitchRise);assert(ui.pad(15,0x20,0) && ui.performance_fx_mode()==PerformanceFx::Off);assert(ui.soft_key(4) && ui.performance_fx_bank()==0);
 // Quantized momentary FX arms on the audio clock; releasing before the
 // boundary cancels it. A held effect starts at the next bar instead.
 ui.set_shift(true);assert(ui.soft_key(2) && ui.soft_key(2) && ui.performance_fx_quantize()==2);
 assert(ui.performance_fx_tail());assert(ui.soft_key(3) && !ui.performance_fx_tail());assert(ui.soft_key(3) && ui.performance_fx_tail());ui.set_shift(false);
 assert(ui.stop());engine->render(out.data(),256);assert(ui.play());engine->render(out.data(),256);ui.poll();
 assert(ui.pad(14,0x10,3500));for(unsigned n=0;n<4;++n){engine->render(out.data(),256);ui.poll();}assert(ui.visual().performance_fx_pending);
 assert(ui.pad(14,0x20,0));for(unsigned n=0;n<4;++n){engine->render(out.data(),256);ui.poll();}assert(!ui.visual().performance_fx_pending);
 assert(ui.pad(14,0x10,3500));for(unsigned n=0;n<4;++n){engine->render(out.data(),256);ui.poll();}assert(ui.visual().performance_fx_pending);
 for(unsigned n=0;n<70;++n){engine->render(out.data(),256);ui.poll();}assert(!ui.visual().performance_fx_pending);assert(ui.pad(14,0x20,0));
 for(unsigned i=0;i<2048;i+=256)engine->render(out.data(),256);
 for(float v:out)assert(std::isfinite(v));
 for(unsigned hw=0;hw<16;++hw){ui.pad(uint8_t(hw),0x20,0);}
 ui.set_mode(UiMode::Effects);assert(ui.soft_key(3) && ui.fx_page()==3);assert(ui.knob(0,1000));assert(track_at(*project,0,0).insert_fx[0]==1);ui.set_shift(true);assert(ui.knob(1,1000));bool insert_lock=false;for(const auto& lock:track_at(*project,0,0).steps[ui.selected_step()].locks)insert_lock|=lock.enabled && lock.id==95;assert(insert_lock);ui.set_shift(false);
 assert(ui.control(1,0x10) && ui.fx_focus_slot()==1);assert(ui.control(1,0));assert(ui.encoder_push() && ui.encoder(1));assert(track_at(*project,0,0).insert_fx[4]==1);assert(ui.encoder_push());ui.set_shift(true);assert(ui.knob(4,1000) && ui.knob(4,1000));assert(ui.knob_value(4)==3);ui.set_shift(false);assert(ui.control(1,0x04) && ui.fx_focus_slot()==0);assert(ui.control(1,0));
 auto poly_project=std::make_unique<ProjectState>();make_empty_project(*poly_project);track_at(*poly_project,0,0).instrument=Instrument::FM;
 auto crowded=std::make_unique<Engine>(),reference_engine=std::make_unique<Engine>();
 assert(crowded->stage_project(*poly_project) && reference_engine->stage_project(*poly_project));
 crowded->render(frame,0);reference_engine->render(frame,0);
 for(unsigned n=40;n<=40+Engine::kPolyVoiceCapacity;++n){MidiPacket note{0,0,3,{0x90,uint8_t(n),100}};assert(crowded->receive_midi(note,0));if(n>40)assert(reference_engine->receive_midi(note,0));}
 float crowded_audio[256]{},reference_audio[256]{};crowded->render(crowded_audio,128);reference_engine->render(reference_audio,128);
 assert(crowded->poly_owner_masks_valid() && reference_engine->poly_owner_masks_valid());
 float steal_difference=0;for(unsigned i=0;i<256;++i)steal_difference+=std::fabs(crowded_audio[i]-reference_audio[i]);
 assert(steal_difference<0.001f);
}
