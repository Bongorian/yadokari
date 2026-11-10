#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <memory>
#include <filesystem>
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace groove;
int main(){
 char root[]="/tmp/mk3-audio-lane-XXXXXX";assert(mkdtemp(root));
 DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 auto e=std::make_unique<Engine>(8000);auto ui=std::make_unique<Performance>(*e,*p);SessionFiles files(fs,library,*e,*p,*ui,root);float output[128]{},input[128];for(float& v:input)v=0.1f;
 assert(e->stage_project(*p));ui->project_loaded();
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;e->render_ports(output,nullptr,input,n);ui->poll();files.service();library.service();frames-=n;}};
 auto until=[&](auto done,unsigned limit=160000){for(unsigned i=0;i<limit;i+=64){if(done())return;run(64);}assert(false);};
 run(1);ui->set_mode(UiMode::Perform);assert(ui->soft_key(7));assert(ui->audio_page());assert(ui->soft_key(0));assert(p->audio[0].kind==TrackKind::Looper);assert(ui->dispatch({ControllerInput::Type::MuteModifier,0,1}));assert(ui->pad(12,0x10,1000));assert(p->muted_tracks&1);ui->pad(12,0x20,0);assert(ui->pad(12,0x10,1000));assert(!(p->muted_tracks&1));ui->pad(12,0x20,0);ui->dispatch({ControllerInput::Type::MuteModifier,0,0});assert(ui->play());run(1);
 assert(ui->soft_key(4));assert(files.service());RecordingResult result;until([&]{return files.take_recording_result(result);});assert(result.saved && result.assigned && result.frames==16000);run(16800);
 auto original=p->audio[0].clips[0];assert(original.asset<64 && original.frames==16000);assert(track_at(*p,0,0).instrument==Instrument::Empty);assert(ui->visual().sample_active[0]);
 auto a=p->audio[0];a.feedback=0.5f;assert(ui->set_audio_lane(0,a));run(1);assert(ui->soft_key(5));assert(files.service());until([&]{return !ui->live_recording();});assert(p->audio[0].clips[0].asset!=original.asset);auto mixed=library.lookup(p->assets[p->audio[0].clips[0].asset].path);assert(fabsf(wav_sample(mixed,100,0)-0.15f)<0.0002f);
 ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);run(1);
 // Clear has its own take undo; step history is not consumed.
 ui->set_shift(true);assert(ui->soft_key(6) && ui->soft_key(0));ui->set_shift(false);assert(p->audio[0].clips[0].asset==255);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);
 // Explicit upper bound rejects a fifth audio role without changing it.
 for(unsigned t=1;t<4;++t){AudioLane lane;lane.kind=TrackKind::Audio;assert(ui->set_audio_lane(t,lane));}AudioLane extra;extra.kind=TrackKind::Audio;assert(!ui->set_audio_lane(4,extra));
 // Punch into the running Song and place the take at the actual DSP bar.
 assert(ui->stop());run(1);p->song.length=1;p->song.enabled=true;p->song.sections[0].bars=8;Command song{CommandType::SetSong,0,0,0};song.song_state=p->song;assert(e->submit(song));assert(ui->select_track(1));assert(ui->play());run(1);assert(ui->soft_key(4));assert(files.service());until([&]{return e->live_record_state()==2;});run(1000);assert(ui->soft_key(4));assert(files.service());until([&]{return files.take_recording_result(result);});assert(result.saved && result.assigned && result.frames==16000);assert(p->audio[1].clips[0].tick==3840);assert(ui->soft_key(5));assert(p->audio[1].clips[1].tick==7680);assert(ui->knob(2,1000));assert(p->audio[1].clips[1].tick==11520);assert(ui->soft_key(6) && ui->soft_key(0));assert(p->audio[1].clips[1].asset==255);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[1].clips[1].asset<64);
 // Free takes stop immediately and round to the nearest bar on the service owner.
 assert(ui->stop());run(1);assert(ui->select_track(0));a=p->audio[0];a.length=LoopLength::Free;assert(ui->set_audio_lane(0,a));assert(ui->play());run(1);assert(ui->soft_key(4));assert(files.service());until([&]{return e->live_record_state()==2;});run(17000);assert(ui->soft_key(4));assert(files.service());until([&]{return files.take_recording_result(result);});assert(result.assigned && result.frames==16000);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);
 // A contiguous streaming read crosses pages without disk I/O on the reader.
 {SampleStream stream(fs);WavView v;assert(stream.open(p->assets[original.asset].path,v));float block[32];assert(stream.read_block(2040,0,block,32)==32);for(float x:block)assert(fabsf(x-0.1f)<0.0001f);assert(stream.read_block(10000,1,block,32)==0);stream.service();assert(stream.read_block(10000,1,block,32)==32);for(float x:block)assert(fabsf(x-0.1f)<0.0001f);float l,r;assert(stream.read_stereo(10001,l,r) && l==r && fabsf(l-0.1f)<0.0001f);}
 // Serialization uses an independent versioned chunk and validates the lane limit.
 std::string save=std::string(root)+"/song.mgx";assert(save_recoverable(fs,save.c_str(),*p));auto restored=std::make_unique<ProjectState>();assert(read_project_file(fs,save.c_str(),*restored));assert(restored->audio[0].clips[0].asset==original.asset);assert(restored->audio[1].clips[1].tick==11520);assert(restored->audio[0].feedback==0.5f);
 assert(quantized_loop_frames(17000,8000,120)==16000);assert(quantized_loop_frames(25000,8000,120)==32000);
 // Loading a kit must retain assets referenced only by audio lanes.
 char loop_path[96];assert(copy_text(loop_path,sizeof loop_path,p->assets[original.asset].path));
 auto empty_kit=std::make_unique<ProjectState>();make_empty_project(*empty_kit);empty_kit->content_kind=ContentKind::Kit;empty_kit->tracks[0].preset_identity=builtin_factory_identity(3);empty_kit->tracks[0].instrument=Instrument::DrumKit;const auto kit_path=std::string(root)+"/retention.mgxk";assert(save_recoverable(fs,kit_path.c_str(),*empty_kit));assert(files.load(kit_path.c_str(),true));run(1);
 assert(p->audio[0].clips[0].asset==original.asset);
 assert(!__builtin_strcmp(p->assets[original.asset].path,loop_path));
 assert(fs.exists(loop_path) && library.lookup(loop_path).frames==original.frames);
 std::filesystem::remove_all(root);std::puts("audio lanes: loop / overdub / undo / punch / placement / codec passed");
}
