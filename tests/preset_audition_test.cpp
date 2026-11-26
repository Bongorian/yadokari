#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <cstdio>
#include <unistd.h>
using namespace groove;
static float source[4096],preview[4096];
static AudioVisualFrame visual(Engine& e){float out[512];for(unsigned i=0;i<4;++i)e.render(out,256);AudioVisualFrame f,last;while(e.pop_visual(f))last=f;return last;}
static void external_retrigger(){
 auto song=std::make_unique<ProjectState>();make_empty_project(*song);
 song->tracks[0].instrument=Instrument::ExternalMidi;song->tracks[0].midi_channel=3;
 auto player=std::make_unique<Engine>(8000);assert(player->stage_project(*song));
 float pcm[16];player->render(pcm,8);
 assert(player->submit({CommandType::Trigger,0,0,100}));player->render(pcm,8);
 MidiEvent message;int key=-1;
 while(player->pop_midi(message))if(message.bytes[0]==0x93)key=message.bytes[1];
 assert(key>=0);
 auto preset=std::make_unique<SoundState>();preset->instrument=Instrument::ExternalMidi;
 WavView views[kSampleAssets]{};
 assert(player->preview_preset(0,*preset,views,song->assets,0,1));player->render(pcm,8);
 unsigned gates=0;
 while(player->pop_midi(message)){
  if(message.bytes[1]!=key || (message.bytes[0]!=0x83 && message.bytes[0]!=0x93))continue;
  assert(message.bytes[0]==(gates?0x93:0x83));++gates;
 }
 assert(gates==2 && player->preview_state()==3);
}
static void browser_refusal(DesktopFiles& disk){
 const auto previous=hardware::active_profile;
 hardware::set_active_profile(hardware::decode((1u<<23)|(0x17u<<4)));
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 project->tracks[0].instrument=Instrument::Chimera;
 project->tracks[0].chimera.values[ChimeraUnison-kChimeraMacroCount]=2;
 auto engine=std::make_unique<Engine>(48000);
 assert(engine->stage_project(*project));float frame[48];engine->render(frame,0);
 auto ui=std::make_unique<Performance>(*engine,*project);
 DesktopSampleAssets library;library.files(&disk);
 SessionFiles files(disk,library,*engine,*project,*ui);
 assert(disk.directory("AUDITIONS") && files.save_preset("AUDITIONS/Choir.mgxp"));
 assert(ui->set_instrument(Instrument::Acid));
 engine->render(frame,24);ui->poll();ui->set_mode(UiMode::Browser);
 copy_text(ui->browser().directory,160,"AUDITIONS");files.refresh();files.service();
 assert(ui->request_file(FileAction::Preview) && files.service());
 engine->render(frame,24);ui->poll();files.service();
 assert(!ui->browser().previewing && std::strstr(ui->browser().status,"UNISON 1"));
 assert(project->tracks[0].instrument==Instrument::Acid);
 hardware::set_active_profile(previous);
}
int main(){
 external_retrigger();
 for(unsigned i=0;i<4096;++i){source[i]=.4f*sinf(i*.11f);preview[i]=.4f*cosf(i*.07f);}
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Sampler;p->tracks[0].level=.37f;p->tracks[0].midi_channel=9;p->tracks[0].midi_program=17;
 auto e=std::make_unique<Engine>(8000);WavView views[kSampleAssets]{};views[0]={reinterpret_cast<uint8_t*>(source),4096,8000,1,32,3};views[1]={reinterpret_cast<uint8_t*>(preview),4096,8000,1,32,3};assert(e->stage_project(*p,views));float scratch[2]{};e->render(scratch,0);
 auto s=std::make_unique<SoundState>();s->instrument=Instrument::Sampler;s->sample_slot=1;auto assets=std::make_unique<SampleAssetState[]>(kSampleAssets);assets[1].metadata.root=60;
 assert(e->preview_preset(0,*s,views,assets.get(),2,1));assert(!e->preview_preset(0,*s,views,assets.get(),2,2));e->render(scratch,0);assert(e->preview_state()==3);assert(e->receive_midi({0,0,1,{0xf8,0,0}},0));e->render(scratch,0);assert(e->preview_state()==3);assert(visual(*e).sample_slot[0]==1);
 assert(e->submit({CommandType::StopPreview,0,0,0}));assert(e->submit({CommandType::Trigger,0,0,100}));e->render(scratch,0);assert(e->preview_state()==2);assert(visual(*e).sample_slot[0]==0);
 assert(e->preview_preset(0,*s,views,assets.get(),2,2));e->render(scratch,0);assert(e->submit({CommandType::SetInstrument,0,0,uint16_t(Instrument::FM)}));assert(e->submit({CommandType::Trigger,0,0,100}));e->render(scratch,0);assert(e->preview_state()==4 && visual(*e).instrument[0]==unsigned(Instrument::FM));
 assert(e->preview_preset(0,*s,views,assets.get(),2,3));e->render(scratch,0);p->tracks[0].instrument=Instrument::PolySaw;assert(e->stage_project(*p,views));e->render(scratch,0);assert(e->preview_state()==6);assert(e->submit({CommandType::Trigger,0,0,100}));assert(visual(*e).instrument[0]==unsigned(Instrument::PolySaw));
 s->instrument=Instrument::DrumKit;s->kit.pads[0].sample[0]=1;assert(e->preview_preset(0,*s,views,assets.get(),2,4));e->render(scratch,0);assert(e->preview_state()==9);assert(visual(*e).peak[0]>0 && e->active_sample_voices()>0);assert(e->submit({CommandType::StopPreview,0,0,0}));e->render(scratch,0);views[1].frames=0;assert(e->preview_preset(0,*s,views,assets.get(),2,5));e->render(scratch,0);assert(e->preview_state()==10);
 // A scene owns the musical boundary. A lone audition follows on the next
 // short callback, while a queued STOP behind an audition stays in order.
 s->instrument=Instrument::Sampler;s->sample_slot=0;
 float short_block[16];assert(e->submit({CommandType::LaunchScene,0,0,0}));
 assert(e->preview_preset(0,*s,views,assets.get(),0,6));
 e->render(short_block,8);assert((e->preview_state()>>1)!=6);
 e->render(short_block,8);assert(e->preview_state()==13);
 assert(e->submit({CommandType::LaunchScene,0,0,0}));
 assert(e->preview_preset(0,*s,views,assets.get(),0,7));
 assert(e->submit({CommandType::StopPreview,0,0,0}));
 e->render(short_block,8);assert(e->preview_state()==14);
 // Real file workflow: temporary mount is absent from saves, pool and Undo.
 char dir[]="build/preset-audition-XXXXXX";assert(mkdtemp(dir) && chdir(dir)==0);DesktopFiles fs;assert(fs.directory("PRESETS"));DesktopSampleAssets library;library.files(&fs);make_empty_project(*p);p->tracks[0].instrument=Instrument::FM;p->tracks[0].level=.37f;p->tracks[0].midi_program=17;auto ui=std::make_unique<Performance>(*e,*p);SessionFiles files(fs,library,*e,*p,*ui);assert(e->stage_project(*p));ui->project_loaded();e->render(scratch,0);ui->poll();assert(files.save_preset("PRESETS/Bell.mgx"));assert(ui->set_instrument(Instrument::PolySaw));e->render(scratch,0);ui->poll();ui->set_mode(UiMode::Browser);copy_text(ui->browser().directory,160,"PRESETS");files.refresh();
 assert(files.service());auto snap=std::make_unique<TrackState>(track_at(*p,0,0));assert(same_track_snapshot(*snap,track_at(*p,0,0)));ProjectDecodeSlot worker;files.set_project_load_worker(&worker);assert(ui->request_file(FileAction::Preview));assert(files.service());for(unsigned i=0;i<1024 && files.kit_load_busy();++i){worker.process_one();assert(files.service());}assert(!files.kit_load_busy());bool ready=false,probe=false;assert(files.take_kit_load_result(ready,probe) && ready && !probe);e->render(scratch,0);assert(e->preview_state()&1);assert(p->tracks[0].instrument==Instrument::PolySaw && p->tracks[0].level==.37f && p->tracks[0].midi_program==17);unsigned registered=0;for(const auto& sound:p->sounds)registered+=sound.identity.assigned();assert(registered==1);assert(files.save_preset("PRESETS/While-previewing.mgx"));auto saved=std::make_unique<ProjectState>();assert(read_project_file(fs,"PRESETS/While-previewing.mgx",*saved));assert(saved->tracks[0].instrument==Instrument::PolySaw);
 ui->set_mode(UiMode::Pad);assert(files.service());e->render(scratch,0);assert(!(e->preview_state()&1));assert(ui->undo());e->render(scratch,0);ui->poll();assert(p->tracks[0].instrument==Instrument::FM);
 // A read completed after a target change cannot mount on the newly selected track.
 ui->set_mode(UiMode::Browser);copy_text(ui->browser().directory,160,"PRESETS");files.refresh();assert(ui->request_file(FileAction::Preview));assert(files.service());assert(ui->select_track(1));for(unsigned i=0;i<1024 && files.kit_load_busy();++i){worker.process_one();files.service();}assert(!files.kit_load_busy());e->render(scratch,0);assert(!(e->preview_state()&1) && p->tracks[1].instrument==Instrument::Empty);
 assert(ui->select_track(0));files.refresh();assert(ui->request_file(FileAction::Preview));assert(files.service() && files.kit_load_busy());assert(!__builtin_strcmp(ui->soft_label(2),"STOP PREVIEW"));assert(ui->soft_key(2));for(unsigned i=0;i<1024 && files.kit_load_busy();++i){worker.process_one();files.service();}assert(!files.kit_load_busy());e->render(scratch,0);assert(!(e->preview_state()&1) && !ui->browser().previewing);
 // Browser OPEN also uses the existing decode/asset job and commits only to
 // its captured target; direct load_preset remains synchronous for host callers.
 assert(ui->select_track(0));assert(ui->set_instrument(Instrument::PolySaw));e->render(scratch,0);ui->poll();
 assert(files.start_kit_load("PRESETS/Bell.mgx",false,false,true));
 for(unsigned i=0;i<1024 && files.kit_load_busy();++i){worker.process_one();files.service();}
 assert(!files.kit_load_busy() && files.take_kit_load_result(ready,probe) && ready);e->render(scratch,0);ui->poll();assert(p->tracks[0].instrument==Instrument::FM && p->tracks[0].level==.37f && p->tracks[0].midi_program==17);
 assert(files.start_kit_load("PRESETS/Bell.mgx",false,false,true) && ui->select_track(1));
 for(unsigned i=0;i<1024 && files.kit_load_busy();++i){worker.process_one();files.service();}
 assert(!files.kit_load_busy() && files.take_kit_load_result(ready,probe) && !ready && p->tracks[1].instrument==Instrument::Empty);
 p->patterns[0].tracks[0].mutation.frozen=true;p->patterns[0].tracks[0].mutation_source[0].locks[0]={300,7,true};assert(project_asset_mask(*p)&(uint64_t(1)<<7));
 browser_refusal(fs);
 std::puts("preset audition: restore, edit/load cancellation, async target guard, unchanged saves/pool/Undo passed");
}
