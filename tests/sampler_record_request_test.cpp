#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <unistd.h>
using namespace groove;
struct Files:DesktopFiles {unsigned writes=0;bool write(const char* p,uint64_t at,const void* b,unsigned n,bool trunc)override{++writes;return DesktopFiles::write(p,at,b,n,trunc);}};
int main(){
 char root[]="/tmp/mk3-record-request-XXXXXX";assert(mkdtemp(root));Files fs;DesktopSampleAssets library;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto engine=std::make_unique<Engine>(8000);
 Performance ui(*engine,*p);SessionFiles files(fs,library,*engine,*p,ui,root);auto renderer=std::make_unique<UiRenderer>();
 float audio[128]{},input[128];for(float& x:input)x=0.2f;
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;engine->render_ports(audio,nullptr,input,n);ui.poll();assert(files.service());frames-=n;}};
 auto finish=[&]{for(unsigned i=0;i<20 && ui.live_recording();++i)run(64);assert(!ui.live_recording());};
 run(1);ui.set_mode(UiMode::Sampler);ui.set_shift(true);copy_text(ui.browser().directory,sizeof ui.browser().directory,root);
 // Queue on T1/P1, then select another destination before file service.
 auto other=track_at(*p,1,1).instrument;
 assert(ui.soft_key(4) && ui.soft_selected(4) && !ui.soft_selected(6));assert(ui.recording_source()==1 && !ui.select_capture_source(1));
 assert(ui.select_pattern(1) && ui.select_track(1));assert(files.service() && ui.live_recording());assert(ui.recording_target().track==0 && ui.recording_target().pattern==0);
 run(128);
 // STOP bypasses an ordinary pending request and retries a full DSP queue.
 assert(ui.request_file(FileAction::Refresh));while(engine->command_space())assert(engine->submit({CommandType::SetFill,0,0,0}));
 assert(ui.soft_key(6) && ui.pending_file_action()==FileAction::RecordToggle);assert(!std::strcmp(ui.soft_label(4),"STOPPING") && !ui.soft_enabled(6));
 assert(files.service() && ui.live_recording());finish();RecordingResult result;assert(files.take_recording_result(result) && result.saved && result.verified && result.assigned && result.peak_l_u>190000);
 assert(track_at(*p,0,0).instrument==Instrument::Sampler && track_at(*p,1,1).instrument==other);
 // REC SOURCE selecting Input is still the Source shortcut, not REC INPUT.
 assert(ui.select_pattern(0) && ui.select_track(0) && ui.select_capture_source(1));run(1);
 assert(ui.soft_key(6) && ui.soft_selected(6) && !ui.soft_selected(4));
 assert(ui.undo());assert(p->routing.capture_bus==0 && ui.recording_source()==1);assert(files.service());run(128);
 assert(ui.live_record_source()==1 && ui.soft_selected(6) && !ui.soft_selected(4));assert(ui.soft_key(4) && files.service());finish();assert(files.take_recording_result(result) && result.saved && result.assigned && result.peak_l_u>190000);
 // Reject reassignment before starting, with no new file write.
 assert(ui.soft_key(4));unsigned slot=track_at(*p,0,0).sample_slot,before=fs.writes;track_at(*p,0,0).sample_slot=uint8_t((slot+1)%kSampleAssets);
 assert(!files.service() && !ui.live_recording() && fs.writes==before && std::strstr(ui.browser().status,"target changed"));track_at(*p,0,0).sample_slot=uint8_t(slot);
 // A late STOP consumes a stop request; it must not start another take.
 assert(files.start_sample_workflow_probe());engine->render_ports(audio,nullptr,input,0);
 for(unsigned i=0;i<8000;i+=64){engine->render_ports(audio,nullptr,input,64);ui.poll();}
 assert(engine->live_record_done() && ui.live_recording());assert(ui.soft_key(4));
 for(unsigned i=0;i<20 && ui.live_recording();++i)assert(files.service());
 assert(!ui.live_recording() && !ui.file_action_pending() && files.take_recording_result(result) && result.saved && result.frames==8000);
 // Quantize belongs to REC MODE; SHIFT's RESAMPLE highlight is independent.
 ui.set_mode(UiMode::Routing);ui.set_shift(false);assert(ui.soft_key(3) && ui.soft_selected(3));ui.set_shift(true);assert(!ui.soft_selected(5));
 ui.set_mode(UiMode::Sampler);assert(ui.soft_key(6) && files.service());run(1);assert(ui.record_state()==2 && !ui.recording_quantized());assert(ui.soft_key(4) && files.service());finish();assert(files.take_recording_result(result) && result.saved && result.frames>0);
 assert(ui.play());run(1);assert(ui.soft_key(6) && files.service());run(1);assert(ui.record_state()==4 && ui.recording_quantized() && std::strstr(ui.sampler_recording_status(),"ARMED"));assert(ui.soft_key(4) && files.service());finish();assert(files.take_recording_result(result) && !result.saved && result.frames==0 && ui.notice()==UiNotice::None);assert(ui.stop());run(1);
 for(auto& entry:std::filesystem::directory_iterator(root))assert(entry.path().extension()!=".part");
 // Stopped-transport recording metadata must invalidate the Sampler body.
 ui.animate(1000);renderer->draw(*p,ui);renderer->pixels[1][190*480+30]=0xffdebeef;ui.recording_file_busy(true);ui.animate(1200);renderer->draw(*p,ui);assert(renderer->pixels[1][190*480+30]!=0xffdebeef);ui.recording_file_busy(false);
 // Browser auto-refresh must wait behind a queued operation, including recording.
 ui.set_shift(false);ui.set_mode(UiMode::Sampler);ui.browser().count=0;
 assert(ui.request_file(FileAction::SampleRecordToggle));ui.set_mode(UiMode::Browser);
 assert(ui.pending_file_action()==FileAction::SampleRecordToggle);assert(files.service() && ui.live_recording());
 assert(ui.pending_file_action()==FileAction::Refresh);run(64);assert(ui.request_file(FileAction::RecordToggle));assert(files.service());finish();
 // Capture uses the accepted target, and rejects a changed source before writing.
 ui.set_mode(UiMode::Sampler);assert(ui.request_file(FileAction::Capture));auto captured=ui.take_request();
 assert(ui.select_track(1));assert(ui.take_request().target.track==captured.target.track);
 p->routing.capture_bus=(captured.source+1)%3;before=fs.writes;
 assert(!files.service() && fs.writes==before && std::strstr(ui.browser().status,"source changed"));
 // Settings pages contain disjoint controls; all nine device themes are reachable.
 ui.set_mode(UiMode::Settings);assert(!std::strcmp(ui.knob_label(0),"MOTION"));
 assert(ui.soft_key(1) && ui.encoder(1) && !std::strcmp(ui.knob_label(0),"THEME"));
 assert(ui.knob(0,8000));assert(ui.visual_settings().theme==ui::Theme::kCount-1 && ui.knob_normalized(0)==1 && ui.device_settings().theme==ui.visual_settings().theme);
 assert(ui.encoder(1) && !std::strcmp(ui.knob_label(0),"PIXEL ORDER"));assert(ui.encoder(1) && !std::strcmp(ui.knob_label(0),"16-BIT VIEW"));
 assert(ui.soft_key(2) && ui.encoder(1) && !std::strcmp(ui.knob_label(0),"STRIP PROFILE"));for(unsigned i=1;i<8;++i)assert(!*ui.knob_label(i) && !ui.knob(i,1000));
 assert(ui.soft_enabled(3) && ui.soft_key(3));assert(!ui.knob(1,10000));for(unsigned n=0;n<6 && ui.settings_page();++n)assert(ui.encoder_direction(0x20));assert(ui.settings_page()==0);assert(ui.knob(0,-1000) && ui.visual_settings().motion==ui::Motion::Reduced);

 ui.set_mode(UiMode::Perform);float morph=ui.morph_position();ui.control(5,0x40);ui.control(5,0);
 assert(ui.strip_input(14000,16383) && ui.morph_position()==morph);
 // Background waveform completion invalidates a stopped WAVE view and uses the
 // analysis envelope for the zoom, without reading the resident sample again.
 ui.set_mode(UiMode::Sampler);ui.select_track(0);ui.set_shift(false);ui.soft_key(2);ui.encoder_direction(0x08);assert(ui.sampler_page()==3);
 auto& bs=ui.browser();uint8_t pcm[128]{};WavView view{};view.data=pcm;view.frames=64;view.channels=1;view.rate=8000;view.bits=16;view.format=1;
 bs.analysis_background=true;bs.analysis_source=nullptr;ui.animate(2000);renderer->draw(*p,ui,0,0,view);ui.animate(2400);renderer->draw(*p,ui,0,0,view);
 uint32_t before_wave[380];for(unsigned x=40;x<420;++x)before_wave[x-40]=renderer->pixels[1][122*480+x];
 renderer->pixels[1][120*480+100]=0xffdebeef;
 for(unsigned n=0;n<kSampleWaveBins;++n){bs.analysis.high[n]=0.75f;bs.analysis.low[n]=-0.5f;}bs.analysis_source=pcm;
 ui.animate(2600);renderer->draw(*p,ui,0,0,view);assert(renderer->pixels[1][120*480+100]!=0xffdebeef);
 unsigned wave_pixels=0;for(unsigned x=40;x<420;++x)if(renderer->pixels[1][122*480+x]!=before_wave[x-40])++wave_pixels;assert(wave_pixels>200);
 std::filesystem::remove_all(root);std::puts("Sampler recording: frozen destination/source, origin highlights, priority/full-queue/late STOP, stale-target rejection, empty armed stop and redraw passed");
}
