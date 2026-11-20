#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <filesystem>
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace groove;
// A seek starts partway through a checkpoint. Measured positions remain
// exact across clock changes, low-word rollovers and control-side page growth.
static void timeline_clock_positions(){
 for(unsigned rate:{48000u,768000u}){
  AudioTimelineClock clock(rate);SongState song{};song.length=1;song.sections[0].bars=1;
  assert(clock.prepare(song));
  constexpr unsigned origin=19,last=16003;
  std::vector<uint64_t> stamps(last+1);
  uint64_t frame=(uint64_t(1)<<32)-100;
  for(unsigned tick=origin;tick<=last;++tick){
   if(tick==3000){song.sections[0].bars=8;assert(clock.prepare_section(0,song.sections[0]));}
   stamps[tick]=frame;clock.observe(tick,frame,120u<<16);
   frame+=rate/(tick<3000?1920:960)+(tick%3);
  }
  for(unsigned tick=origin;tick<=last;++tick)assert(clock.elapsed(tick,rate)==stamps[last]-stamps[tick]);
  assert(clock.elapsed(0,rate)==stamps[last]-stamps[origin]+uint64_t(origin)*rate/1920);
  clock.reset();clock.observe(0,frame,60u<<16);assert(clock.elapsed(0,rate)==0);
 }
}
static void edit_clip_after_tempo_change(){
 constexpr unsigned rate=8000,duration=10;
 std::vector<int16_t> data(rate*duration);
 uint32_t seed=42;
 for(unsigned i=0;i<rate*4;++i){seed=1664525*seed+1013904223;data[i]=(seed>>31)?10000:-10000;}
 enum Edit { Future,Trim,Replace,Delete,Move,ChangeSlot,Rebind };
 for(auto edit:{Future,Trim,Replace,Delete,Move,ChangeSlot,Rebind}){
  std::vector<int16_t> reference;
  for(bool live:{false,true}){
   auto p=std::make_unique<ProjectState>();make_empty_project(*p);
   p->tempo=120;p->song.enabled=true;p->song.length=1;p->song.sections[0].bars=16;
   auto& lane=p->audio[0];lane.kind=TrackKind::Audio;
   lane.clips[0].asset=0;lane.clips[0].frames=unsigned(data.size())-(edit==Replace || edit==Rebind);
   if(edit==Delete){lane.clips[1]=lane.clips[0];lane.clips[1].tick=2300;}
   AudioLane changed=lane;
   switch(edit){
    case Future:changed.clips[1]=lane.clips[0];changed.clips[1].tick=30720;break;
    case Trim:changed.clips[0].first=rate/4;changed.clips[0].frames-=rate/4;break;
    case Replace:changed.clips[0].asset=1;break;
    case Rebind:break;
    case Delete:changed.clips[1]=AudioClip{};break;
    case Move:changed.clips[0].tick=321;break;
    case ChangeSlot:changed.clips[1]=lane.clips[0];changed.clips[1].gain=0.6f;
     changed.clips[0]=AudioClip{};break;
   }
   if(!live)lane=changed;
   WavView assets[kSampleAssets]{};
   for(unsigned slot=0;slot<2;++slot){
    const unsigned frames=unsigned(data.size())-slot;
    assets[slot]={reinterpret_cast<const uint8_t*>(data.data()+slot),frames,rate,1,16,1,nullptr};
    auto& asset=p->assets[slot];copy_text(asset.path,sizeof asset.path,slot?"ASSETS/SAMPLES/B.wav":"ASSETS/SAMPLES/A.wav");
    asset.metadata.rate=rate;asset.metadata.frames=frames;asset.metadata.bits=16;asset.metadata.channels=1;
   }
   if(edit==Rebind && !live){assets[0]=assets[1];p->assets[0].metadata=p->assets[1].metadata;}
   auto e=std::make_unique<Engine>(rate);assert(e->stage_project(*p,assets));
   float block[128]{};e->render(block,0);assert(e->submit({CommandType::Start,0,0,0}));
   auto render=[&](unsigned n){while(n){unsigned k=n>64?64:n;e->render(block,k);n-=k;}};
   render(2*rate);assert(e->submit({CommandType::SetTempo,0,0,60}));render(rate);
   if(live){
    Command c{CommandType::SetAudioLane,0,0,255};c.audio_lane=changed;
    if(edit==Rebind){c.value=0;c.sample=assets[1];c.metadata=p->assets[1].metadata;}
    assert(e->submit(c));
   }
   render(rate/10);
   std::vector<int16_t> pcm;
   for(unsigned n=0;n<16;++n){render(64);for(float sample:block)pcm.push_back(pcm16(sample));}
   if(live){if(pcm!=reference){std::fprintf(stderr,"clip edit %d: first difference at %zu, reference %d actual %d\n",
    int(edit),size_t(std::mismatch(pcm.begin(),pcm.end(),reference.begin()).first-pcm.begin()),reference[0],pcm[0]);}assert(pcm==reference);}
   else{assert(std::any_of(pcm.begin(),pcm.end(),[](int16_t v){return v>1000;}));reference=pcm;}
  }
 }
}

int main(){
 timeline_clock_positions();
 edit_clip_after_tempo_change();
 char root[]="/tmp/mk3-audio-lane-XXXXXX";assert(mkdtemp(root));
 DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 auto e=std::make_unique<Engine>(8000);auto ui=std::make_unique<Performance>(*e,*p);SessionFiles files(fs,library,*e,*p,*ui,root);float output[128]{},input[128];for(float& v:input)v=0.1f;
 assert(e->stage_project(*p));ui->project_loaded();
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;e->render_ports(output,nullptr,input,n);ui->poll();files.service();library.service();frames-=n;}};
 auto until=[&](auto done,unsigned limit=160000){for(unsigned i=0;i<limit;i+=64){if(done())return;run(64);}assert(false);};
 run(1);ui->set_mode(UiMode::Perform);
 assert(ui->soft_key(7) && ui->audio_page());
 assert(!ui->playing() && p->audio[0].kind==TrackKind::Instrument);
 assert(ui->soft_key(4) && p->audio[0].kind==TrackKind::Looper && ui->playing());
 assert(files.service());RecordingResult result;until([&]{return files.take_recording_result(result);});assert(result.saved && result.assigned && result.frames==16000);run(16800);
 ui->set_mode(UiMode::Pad);assert(ui->select_track(1));assert(ui->control(6,1) && ui->control(6,0));assert(ui->soft_key(7) && ui->selected_track()==0);
 auto original=p->audio[0].clips[0];assert(original.asset<64 && original.frames==16000);assert(track_at(*p,0,0).instrument==Instrument::Empty);assert(ui->visual().sample_active[0]);
 auto a=p->audio[0];a.feedback=0.5f;assert(ui->set_audio_lane(0,a));run(1);assert(ui->soft_key(5));assert(files.service());until([&]{return !ui->live_recording();});assert(p->audio[0].clips[0].asset!=original.asset);auto mixed=library.lookup(p->assets[p->audio[0].clips[0].asset].path);assert(fabsf(wav_sample(mixed,100,0)-0.15f)<0.0002f);
 ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);run(1);
 // Clear has its own take undo; step history is not consumed.
 ui->set_shift(true);assert(ui->soft_key(6) && ui->soft_key(0));ui->set_shift(false);assert(p->audio[0].clips[0].asset==255);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);
 // Explicit upper bound rejects a fifth audio role without changing it.
 for(unsigned t=1;t<4;++t){AudioLane lane;lane.kind=TrackKind::Audio;assert(ui->set_audio_lane(t,lane));}AudioLane extra;extra.kind=TrackKind::Audio;assert(!ui->set_audio_lane(4,extra));
 // Punch into the running Song and place the take at the actual DSP bar.
 assert(ui->stop());run(1);p->song.length=1;p->song.enabled=true;p->song.sections[0].bars=8;Command song{CommandType::SetSong,0,0,0};song.song_state=p->song;assert(e->submit(song));assert(ui->soft_key(1) && ui->on_perform_page(PerformTimeline));assert(ui->select_track(1));assert(ui->play());run(1);assert(ui->soft_key(4));assert(files.service());until([&]{return e->live_record_state()==2;});run(1000);assert(ui->soft_key(4));assert(files.service());until([&]{return files.take_recording_result(result);});assert(result.saved && result.assigned && result.frames==16000);assert(p->audio[1].clips[0].tick==3840);assert(ui->soft_key(5));assert(p->audio[1].clips[1].tick==7680);assert(ui->knob(2,1000));assert(p->audio[1].clips[1].tick==11520);assert(ui->soft_key(6) && ui->soft_key(0));assert(p->audio[1].clips[1].asset==255);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[1].clips[1].asset<64);
 // Free takes stop immediately and round to the nearest bar on the service owner.
 assert(ui->stop());run(1);assert(ui->soft_key(0) && ui->on_perform_page(PerformLooper));assert(ui->select_track(0));a=p->audio[0];a.length=LoopLength::Free;assert(ui->set_audio_lane(0,a));assert(ui->play());run(1);assert(ui->soft_key(4));assert(files.service());until([&]{return e->live_record_state()==2;});run(17000);assert(ui->soft_key(4));assert(files.service());until([&]{return files.take_recording_result(result);});assert(result.assigned && result.frames==16000);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);assert(p->audio[0].clips[0].asset==original.asset);
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
