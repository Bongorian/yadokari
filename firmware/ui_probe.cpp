#include "ui_probe.h"
#include "circle_display.h"
#include "project_storage.h"
#include "mk3_audio.h"
#include "../src/ui_probe_workload.h"
#include "../src/session_files.h"
#include <circle/timer.h>
#include <circle/logger.h>
using namespace groove;
struct CUiProbe::State {
 ProjectState* original=nullptr;ProjectState* score=nullptr;WavView bindings[64]{},probe_views[64]{};
 NavigationLocation navigation{};CMk3Audio* audio=nullptr;unsigned stage=0,generation=0,missing=0;bool steady=false,restoring=false;
 uint64_t ready_after=0,window=0,until=0;
 ~State(){delete original;delete score;}
};
CUiProbe::~CUiProbe(){delete state_;}
bool CUiProbe::start(Engine& engine,ProjectState& project,Performance& ui,SessionFiles& files,CProjectStorage& storage,CMk3Audio* audio){
 if(busy() || files.recording_busy() || !audio || !audio->operational() || ui.visual_settings().theme!=ui::Theme::kDefault)return false;
 completed_=false;auto* s=alloc_new<State>();if(!s)return false;s->original=alloc_new<ProjectState>(project);s->score=alloc_new<ProjectState>();
 if(!s->original || !s->score){delete s;return false;}
 uint32_t crc=0;bool ok=storage.load(kUiProbeSample,s->probe_views[0]) && file_checksum(storage,kUiProbeSample,crc) && crc==kUiProbeSampleCrc;
 if(!ok){delete s;return false;}make_ui_probe_project(*s->score);files.copy_bindings(s->bindings);s->navigation=ui.navigation();s->audio=audio;
 if(!ui.stop() || !engine.stage_project(*s->score,s->probe_views)){delete s;return false;}
 project=*s->score;files.set_bindings(s->probe_views);ui.project_loaded();
 // Pin every page/cursor, not the user's previous navigation. Browser content
 // belongs to the immutable factory shelf, never a variable user directory.
 NavigationLocation fixed{};fixed.mode=uint8_t(UiMode::Synth);fixed.value[45]=unsigned(Instrument::FM);copy_text(fixed.directory,sizeof fixed.directory,"FACTORY/DEBUG");ui.restore_navigation(fixed);
 auto& browser=ui.browser();browser.count=browser.cursor=0;browser.library_list=browser.favorites_only=browser.previewing=false;browser.preview={};browser.analysis={};browser.metadata={};browser.preview_path[0]=browser.audition_path[0]=browser.status[0]=0;
 ui.set_shift(false);ui.select_track(0);ui.set_mode(UiMode::Synth);ui.soft_key(0);ui.play();
 s->generation=engine.staged_generation();s->ready_after=CTimer::GetClockTicks64()+2000000;state_=s;
 CLogger::Get()->Write("groove",LogNotice,"UI workload revision %u seed %08x theme %u stages %u sample-crc %08x transition-us 200000 stage-us 800000",kUiProbeRevision,kUiProbeSeed,ui.visual_settings().theme,kUiProbeStages,kUiProbeSampleCrc);return true;
}
static void select_stage(Performance& ui,unsigned stage){
 ui.cancel_browser_edit();ui.set_shift(false);
 switch(stage){
 case 1:ui.select_track(0);ui.set_mode(UiMode::Synth);ui.soft_key(0);break;
 case 2:ui.soft_key(1);break;case 3:ui.soft_key(4);break;
 case 4:ui.set_mode(UiMode::Settings);ui.soft_key(0);break;
 case 5:ui.set_mode(UiMode::Routing);ui.soft_key(0);break;
 case 6:ui.set_mode(UiMode::Project);ui.begin_project_save_as();break;
 case 7:ui.set_mode(UiMode::Step);while(ui.tracker_view() || ui.step_overview())ui.cycle_step_view();break;
 case 8:while(!ui.tracker_view())ui.cycle_step_view();break;
 case 9:ui.set_mode(UiMode::Mixer);break;
 case 10:ui.select_track(1);ui.set_mode(UiMode::Sampler);break;
 default:{static constexpr UiMode modes[]={UiMode::Pad,UiMode::Clip,UiMode::Scene,UiMode::Effects,UiMode::Perform,UiMode::Generate,UiMode::Keyboard,UiMode::Modulation,UiMode::Browser,UiMode::Diagnostics,UiMode::Chord,UiMode::Song};ui.select_track(0);ui.set_mode(modes[stage-11]);break;}
 }
}
void CUiProbe::service(Engine& engine,ProjectState& project,Performance& ui,SessionFiles& files,CircleDisplay& display,bool connected,CMk3Audio* current){
 if(!state_){return;}auto& s=*state_;uint64_t now=CTimer::GetClockTicks64();
 // The previous audio binding may already have been destroyed by hotplug.
 // Its pointer is only compared, never dereferenced on this cancellation path.
 if(current!=s.audio || !current || !current->operational()){
  ui.cancel_browser_edit();ui.stop();
  if(s.restoring || engine.stage_project(*s.original,s.bindings)){
   if(!s.restoring){project=*s.original;files.set_bindings(s.bindings);ui.project_loaded();}
   ui.restore_navigation(s.navigation);delete state_;state_=nullptr;completed_=false;
   CLogger::Get()->Write("groove",LogWarning,"UI probe aborted / audio binding changed / original restaged");
  }
  return;
 }
 if(s.restoring){if(ui.visual().generation==s.generation){ui.restore_navigation(s.navigation);delete state_;state_=nullptr;completed_=true;CLogger::Get()->Write("groove",LogNotice,"UI probe completed / fixed revision 2 restored / physical buttons and appearance not observed");}return;}
 if(!s.stage){if(ui.visual().generation!=s.generation || now<s.ready_after || (connected && !display.FrameComplete()))return;s.stage=1;select_stage(ui,1);s.window=now;s.until=now+800000;display.BeginTrace();display.TraceWindow(1,false);display.ResetFrameStats(now);s.audio->BeginWorkerWindow();s.audio->BeginTimingWindow();s.missing=s.audio->worker_missing();}
 bool boundary=!s.steady?now-s.window>=200000:now>=s.until;
 if(!boundary || (connected && !display.FrameComplete()))return;
 auto f=display.FrameStats();CLogger::Get()->Write("groove",LogNotice,"UI probe stage %u window %s us %u frames %u/%u gap-us %u/%u over100ms %u/%u discarded %u/%u",s.stage,s.steady?"steady":"transition",unsigned(now-s.window),f.complete[0],f.complete[1],f.max_gap_us[0],f.max_gap_us[1],f.over100ms[0],f.over100ms[1],f.discarded[0],f.discarded[1]);
 const auto transport=display.Transport();
 CLogger::Get()->Write("groove",LogNotice,"UI transport %u %s usb-us %u bytes %u reap-us %u prep-us %u draw-us %u packets %u total-bytes %u",s.stage,s.steady?"steady":"transition",transport.usb_us,transport.usb_bytes,transport.reap_us,transport.prepare_us,transport.draw_us,transport.packets,transport.bytes);
 CLogger::Get()->Write("groove",LogNotice,"UI class %u %s revision 2 mode %u synth-page %u nav %u",s.stage,ui_probe_class(s.stage),unsigned(ui.mode()),ui.instrument_view(),ui.navigation_key());
 if(!s.steady){s.steady=true;s.window=CTimer::GetClockTicks64();display.TraceWindow(s.stage,s.steady);display.ResetFrameStats(s.window);return;}
 if(++s.stage<=kUiProbeStages){select_stage(ui,s.stage);s.steady=false;s.window=CTimer::GetClockTicks64();s.until=s.window+800000;display.TraceWindow(s.stage,s.steady);display.ResetFrameStats(s.window);return;}
 display.TraceWindow(0,false);
 auto w=s.audio->EndWorkerWindow();auto a=s.audio->EndTimingWindow();CLogger::Get()->Write("groove",LogNotice,"UI fixed audio blocks %u avg-us %u max-us %u over500 %u missing %u callback-max %u",w.blocks,w.blocks?unsigned(w.total_us/w.blocks):0,w.max_us,w.over500,s.audio->worker_missing()-s.missing,a.max_us);
 s.audio->SetTimingPhase(AudioPhaseTiming::Restore);ui.cancel_browser_edit();ui.stop();if(engine.stage_project(*s.original,s.bindings)){project=*s.original;files.set_bindings(s.bindings);ui.project_loaded();s.generation=engine.staged_generation();s.restoring=true;}else {CLogger::Get()->Write("groove",LogError,"UI probe restore pending");--s.stage;}
}
