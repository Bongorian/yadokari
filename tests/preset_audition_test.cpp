#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
#include <unistd.h>
using namespace groove;
static float source[4096],preview[4096];
static AudioVisualFrame visual(Engine& e){float out[512];for(unsigned i=0;i<4;++i)e.render(out,256);AudioVisualFrame f,last;while(e.pop_visual(f))last=f;return last;}
int main(){
 for(unsigned i=0;i<4096;++i){source[i]=.4f*sinf(i*.11f);preview[i]=.4f*cosf(i*.07f);}
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Sampler;p->tracks[0].level=.37f;p->tracks[0].midi_channel=9;p->tracks[0].midi_program=17;
 auto e=std::make_unique<Engine>(8000);WavView views[kSampleAssets]{};views[0]={reinterpret_cast<uint8_t*>(source),4096,8000,1,32,3};views[1]={reinterpret_cast<uint8_t*>(preview),4096,8000,1,32,3};assert(e->stage_project(*p,views));float scratch[2]{};e->render(scratch,0);
 auto s=std::make_unique<SoundState>();s->instrument=Instrument::Sampler;s->sample_slot=1;auto assets=std::make_unique<SampleAssetState[]>(kSampleAssets);assets[1].metadata.root=60;
 assert(e->preview_preset(0,*s,views,assets.get(),2,1));assert(!e->preview_preset(0,*s,views,assets.get(),2,2));e->render(scratch,0);assert(e->preview_state()==3);assert(e->receive_midi({0,0,1,{0xf8,0,0}},0));e->render(scratch,0);assert(e->preview_state()==3);assert(visual(*e).sample_slot[0]==1);
 assert(e->submit({CommandType::StopPreview,0,0,0}));assert(e->submit({CommandType::Trigger,0,0,100}));e->render(scratch,0);assert(e->preview_state()==2);assert(visual(*e).sample_slot[0]==0);
 assert(e->preview_preset(0,*s,views,assets.get(),2,2));e->render(scratch,0);assert(e->submit({CommandType::SetInstrument,0,0,uint16_t(Instrument::FM)}));assert(e->submit({CommandType::Trigger,0,0,100}));e->render(scratch,0);assert(e->preview_state()==4 && visual(*e).instrument[0]==unsigned(Instrument::FM));
 assert(e->preview_preset(0,*s,views,assets.get(),2,3));e->render(scratch,0);p->tracks[0].instrument=Instrument::PolySaw;assert(e->stage_project(*p,views));e->render(scratch,0);assert(e->preview_state()==6);assert(e->submit({CommandType::Trigger,0,0,100}));assert(visual(*e).instrument[0]==unsigned(Instrument::PolySaw));
 s->instrument=Instrument::DrumKit;s->kit.pads[0].sample[0]=1;assert(e->preview_preset(0,*s,views,assets.get(),2,4));e->render(scratch,0);assert(e->preview_state()==9);assert(visual(*e).peak[0]>0 && e->active_sample_voices()>0);assert(e->submit({CommandType::StopPreview,0,0,0}));e->render(scratch,0);views[1].frames=0;assert(e->preview_preset(0,*s,views,assets.get(),2,5));e->render(scratch,0);assert(e->preview_state()==10);
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
 std::puts("preset audition: restore, edit/load cancellation, async target guard, unchanged saves/pool/Undo passed");
}
