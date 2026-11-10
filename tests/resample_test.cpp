#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cstring>
#include <unistd.h>
using namespace groove;
struct Files:DesktopFiles {bool fail_audio=false;bool write(const char* p,uint64_t at,const void* b,unsigned n,bool trunc)override{return !(fail_audio && at>=44) && DesktopFiles::write(p,at,b,n,trunc);}};
int main(){
 auto previous=std::filesystem::current_path();char root[]="/tmp/mk3-resample-XXXXXX";assert(mkdtemp(root) && chdir(root)==0);
 Files fs;assert(fs.directory("RECORDINGS"));DesktopSampleAssets library;auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->muted_tracks=0xffff;p->routing.monitor=1;p->routing.monitor_bus=1;
 auto engine=std::make_unique<Engine>(8000);Performance ui(*engine,*p);SessionFiles files(fs,library,*engine,*p,ui,"RECORDINGS");float out[128]{},input[128];for(unsigned i=0;i<128;++i)input[i]=0.1f;
 assert(engine->stage_project(*p));ui.project_loaded();
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;engine->render_ports(out,nullptr,input,n);ui.poll();files.service();frames-=n;}};
 auto until=[&](auto done,unsigned limit=100000){for(unsigned i=0;i<limit;i+=64){if(done())return;run(64);}assert(false);};
 run(1);ui.set_mode(UiMode::Routing);ui.set_shift(true);assert(!ui.soft_enabled(5) && !ui.soft_key(5));assert(ui.play());run(1);ui.set_mode(UiMode::Routing);ui.set_shift(true);assert(ui.soft_key(3) && ui.resample_bars()==2);assert(ui.soft_key(5));assert(!ui.soft_enabled(3) && !ui.knob(6,1000) && !ui.select_capture_source(1));assert(ui.select_track(1));assert(files.service());assert(ui.resampling() && ui.resample_view().target.track==0 && ui.resample_view().bars==2 && ui.live_record_source()==0);assert(!ui.knob(6,1000) && !ui.soft_key(3));
 RecordingResult result;until([&]{return files.take_recording_result(result);});assert(result.saved && result.verified && result.assigned && result.frames==32000 && result.peak_l_u>90000);run(1);
 assert(track_at(*p,0,0).sample_slot==result.slot);assert(ui.select_track(0));unsigned first_slot=result.slot;const char* first_path=p->assets[first_slot].path;assert(fs.exists(first_path));auto old=library.lookup(first_path);assert(old.data && wav_sample(old,0,0)>0.09f);
 // Cancellation before the next bar never replaces a destination or leaves a dummy take.
 assert(ui.knob(6,32000) && ui.knob(6,32000) && p->routing.capture_bus==60 && ui.knob_normalized(6)==1);assert(ui.knob(6,-32000) && ui.knob(6,-32000) && p->routing.capture_bus==0);run(1);
 auto original=track_at(*p,0,0).sample_slot;assert(ui.soft_key(5));assert(files.service());
 while(engine->command_space())assert(engine->submit({CommandType::SetTempo,0,0,p->tempo}));
 assert(ui.request_file(FileAction::Refresh));assert(ui.soft_key(5) && ui.pending_file_action()==FileAction::CancelResample);assert(!std::strcmp(ui.soft_label(5),"CANCELLING"));assert(files.service());assert(ui.live_recording() && fs.exists("RECORDINGS/Live-002.part"));assert(!ui.soft_enabled(5));until([&]{return !ui.live_recording();});assert(track_at(*p,0,0).sample_slot==original);assert(!fs.exists("RECORDINGS/Live-002.part"));assert(files.take_recording_result(result) && !result.assigned && !result.saved);
 // Even a complete DSP take is still cancellable before the file owner
 // finalizes its header and publishes a sample assignment.
 assert(ui.soft_key(5) && files.service());bool complete=false;
 for(unsigned i=0;i<100000;i+=64){engine->render_ports(out,nullptr,input,64);ui.poll();if(engine->live_record_done()){complete=true;break;}files.service();}
 assert(complete && engine->live_record_bars_complete() && ui.resampling());assert(ui.soft_key(5) && files.service());assert(!ui.resampling());assert(files.take_recording_result(result) && !result.saved && !result.assigned);assert(track_at(*p,0,0).sample_slot==original && !fs.exists("RECORDINGS/Live-002.part"));
 // Occupancy published by a real save job disables new recording/configuration.
 ProjectEncodeSlot worker;files.set_project_save_worker(&worker);assert(files.start_project_save("Test.mgxs"));assert(ui.recording_file_busy() && !ui.soft_enabled(5) && !ui.knob(6,1000));assert(worker.process_one());until([&]{return !files.project_save_busy();});assert(!ui.recording_file_busy() && ui.soft_enabled(5));
 // A destination assignment changed between queueing and service is
 // rejected with its actual reason, before opening a new take.
 assert(ui.soft_key(5));track_at(*p,0,0).sample_slot=uint8_t((original+1)%kSampleAssets);assert(!files.service());assert(!ui.live_recording() && std::strstr(ui.browser().status,"target changed"));track_at(*p,0,0).sample_slot=original;
 // Failed audio writes keep the old immutable sample and its assignment.
 assert(ui.soft_key(5));assert(files.service());fs.fail_audio=true;until([&]{return !ui.live_recording();});fs.fail_audio=false;assert(files.take_recording_result(result) && !result.assigned);assert(track_at(*p,0,0).sample_slot==original && wav_sample(old,0,0)>0.09f);
 // A successful wet take can become the existing exact 1-4 bar looper base.
 p->assets[first_slot].metadata.root=36;ui.set_shift(false);assert(ui.loop_take());assert(ui.loop_take());run(64);assert(overdub_track_compatible(track_at(*p,0,0),p->assets[first_slot],32000,8000,p->tempo,0));
 p->routing.capture_bus=19;assert(ui.request_file(FileAction::Overdub));assert(!files.service());assert(!ui.live_recording());
 p->routing.capture_bus=20;assert(ui.request_file(FileAction::Overdub));assert(ui.select_track(1));assert(ui.recording_target().track==0 && ui.recording_source()==20);assert(files.service());assert(ui.recording_target().track==0);until([&]{return !ui.live_recording();},150000);run(1);assert(track_at(*p,0,0).sample_slot!=first_slot && fs.exists(first_path) && wav_sample(old,0,0)>0.09f);
 assert(overdub_source_safe(p->routing,1,0) && !overdub_source_safe(p->routing,19,0));p->routing.groups[0].members=1;assert(!overdub_source_safe(p->routing,53,0));p->routing.groups[0].members=2;assert(overdub_source_safe(p->routing,53,0));
 // Group capture is post insert/gain: with only this group on Main its samples
 // match the rendered Main bus, independent of task-side WAV assignment.
 assert(ui.stop());run(1);make_empty_project(*p);p->routing.groups[0].members=1;p->routing.groups[0].gain=0.5f;p->routing.groups[0].insert_fx[0]=1;
 auto t=track_at(*p,0,0);t.instrument=Instrument::Sampler;t.sample_loop=true;t.steps[0].enabled=true;t.steps[0].velocity=127;t.level=1;
 uint8_t pcm[128];for(unsigned i=0;i<64;++i){pcm[2*i]=0xcc;pcm[2*i+1]=0x0c;}WavView sample{};sample.data=pcm;sample.frames=64;sample.rate=8000;sample.bits=16;sample.channels=1;sample.format=1;WavView bindings[kSampleAssets]{};bindings[0]=sample;
 assert(engine->stage_project(*p,bindings));ui.project_loaded();run(1);assert(ui.play());run(128);assert(engine->start_live_record(false,64,53));engine->render_ports(out,nullptr,input,64);unsigned captured=0;LiveRecordFrame f;float peak=0;while(engine->pop_live_record(f)){assert(fabsf(f.l-out[2*captured])<0.00001f);peak=fmaxf(peak,fabsf(f.l));++captured;}assert(captured==64 && peak>0.001f && engine->live_record_done());engine->finish_live_record();
 std::filesystem::current_path(previous);std::filesystem::remove_all(root);std::puts("Wet resample: frozen request/source/destination, exact two-bar WAV/assignment, full-queue/late cancel, busy save, write failure, immutable old take, loop conversion, other-track overdub and post-insert Group capture passed");
}
