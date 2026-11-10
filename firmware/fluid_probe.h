#pragma once
#include "project_storage.h"
#include "circle_display.h"
#include "mk3_audio.h"
#include "../src/performance.h"
#include <circle/timer.h>
#include <circle/logger.h>
// Optional final diagnostic: exactly FluidR3 GM, never a generated substitute.
// No project/asset assignment or user file writes. Normal preset save/mount is
// checked separately. Its only filesystem operation is the shared async load.
class CFluidProbe {
 bool active_=false;uint64_t began_=0,last_=0;unsigned max_gap_=0,missing_=0;
 groove::NavigationLocation navigation_{};CMk3Audio* audio_=nullptr;
 static constexpr const char* path_="ASSETS/SOUNDFONTS/Optimization-FluidR3_GM.sf2";
 static constexpr uint32_t crc_=0x7886b4e9;
public:
 bool busy()const{return active_;}
 bool start(CProjectStorage& storage,groove::Performance& ui,CircleDisplay& display,CMk3Audio* audio){
  if(active_ || !audio || !audio->operational())return false;
  if(storage.size(path_)!=148398306){CLogger::Get()->Write("groove",LogWarning,"Fluid probe SKIPPED / exact FluidR3 GM 148398306 bytes required");return false;}
  if(!storage.begin_incremental_load(path_,crc_)){CLogger::Get()->Write("groove",LogWarning,"Fluid probe refused / previous sound kept");return false;}
  navigation_=ui.navigation();ui.set_mode(groove::UiMode::Settings);ui.soft_key(0);
  began_=last_=CTimer::GetClockTicks64();max_gap_=0;audio_=audio;missing_=audio->worker_missing();audio->BeginWorkerWindow();display.ResetFrameStats(began_);active_=true;
  CLogger::Get()->Write("groove",LogNotice,"Fluid probe begin bytes 148398306 crc 7886b4e9 async core2 / task-owned IO");return true;
 }
 void service(CProjectStorage& storage,groove::Performance& ui,CircleDisplay& display,CMk3Audio* audio){
  if(!active_)return;
  uint64_t now=CTimer::GetClockTicks64();unsigned gap=unsigned(now-last_);last_=now;if(gap>max_gap_)max_gap_=gap;
  bool bound=audio && audio==audio_ && audio->operational();groove::WavView view;auto progress=groove::SampleLoadProgress::Failed;
  if(bound && now-began_<120000000)progress=storage.poll_incremental_load(view);else storage.cancel_incremental_load();
  if(progress==groove::SampleLoadProgress::Working)return;
  unsigned bank=0,program=0;bool ok=progress==groove::SampleLoadProgress::Ready && groove::soundfont_crc(view.soundfont)==crc_ && groove::soundfont_first(view.soundfont,bank,program) && groove::soundfont_name(view.soundfont,bank,program);
  auto frames=display.FrameStats();CMk3Audio::WorkerWindow w{};if(bound)w=audio->EndWorkerWindow();
  CLogger::Get()->Write("groove",ok?LogNotice:LogWarning,"Fluid load complete ok %u us %u service-gap-us %u resident %u bank %u program %u",unsigned(ok),unsigned(now-began_),max_gap_,unsigned(groove::soundfont_resident_bytes()),bank,program);
  CLogger::Get()->Write("groove",LogNotice,"Fluid UI frames %u/%u gap-us %u/%u",frames.complete[0],frames.complete[1],frames.max_gap_us[0],frames.max_gap_us[1]);
  CLogger::Get()->Write("groove",LogNotice,"Fluid audio max-us %u over500 %u missing %u completed",w.max_us,w.over500,bound?audio->worker_missing()-missing_:0);
  ui.restore_navigation(navigation_);active_=false;
 }
};
