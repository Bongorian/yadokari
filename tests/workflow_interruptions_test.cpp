#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <unistd.h>
using namespace groove;
int main(){
 auto cwd=std::filesystem::current_path();char directory[]="/tmp/mk3-workflow-boundaries-XXXXXX";assert(mkdtemp(directory));std::filesystem::current_path(directory);
 {
  DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);uint8_t wav[44+128]{};assert(wav_header_ch(wav,64,48000,1));
  for(unsigned n=0;n<64;++n){int16_t sample=int16_t(2048*(1+n/16));wav[44+2*n]=uint8_t(sample);wav[45+2*n]=uint8_t(uint16_t(sample)>>8);}assert(fs.write("slices.wav",0,wav,sizeof wav,true));WavView view;assert(library.load("slices.wav",view));
  auto project=std::make_unique<ProjectState>();make_empty_project(*project);
  for(unsigned t=0;t<16;++t){project->tracks[t].level=.1f+.02f*t;project->patterns[0].tracks[t].steps[t].enabled=true;}
  auto source=track_at(*project,0,3);source.instrument=Instrument::Sampler;source.sample_slot=0;source.sample_slices=4;source.sample_edit.slice_count=4;for(unsigned n=0;n<=4;++n)source.sample_edit.markers[n]=float(n)/4;
  assert(copy_text(project->assets[0].path,sizeof project->assets[0].path,"slices.wav"));project->assets[0].metadata.frames=64;project->assets[0].metadata.rate=48000;project->assets[0].metadata.channels=1;project->assets[0].metadata.bits=16;
  auto engine=std::make_unique<Engine>(8000);WavView views[kSampleAssets]{};views[0]=view;assert(engine->stage_project(*project,views));float audio[128]{};engine->render(audio,0);
  auto ui=std::make_unique<Performance>(*engine,*project);SessionFiles files(fs,library,*engine,*project,*ui);files.set_bindings(views);assert(ui->select_track(3));ui->set_mode(UiMode::Sampler);
  auto before=std::make_unique<PatternState>(*project,0);
  auto unchanged=[&]{for(unsigned t=0;t<16;++t)assert(same_track_snapshot(before->tracks[t],track_at(*project,0,t)));};
  allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(ui->soft_key(5) && ui->soft_key(0) && !files.service() && ui->notice()==UiNotice::MemoryFull);allocation_admission_hook=nullptr;unchanged();
  while(engine->command_space())assert(engine->submit({CommandType::SetTempo,0,0,120}));
  assert(ui->soft_key(5) && ui->soft_key(0) && !files.service());unchanged();engine->render(audio,0);ui->poll();
  assert(ui->soft_key(5) && ui->soft_key(0) && files.service());engine->render(audio,0);ui->poll();
  for(unsigned t=0;t<16;++t){auto mapped=track_at(*project,0,t);assert(mapped.instrument==Instrument::Sampler && mapped.sample_slot==0 && mapped.sample_slice==source.sample_edit.pads[t]%4);}
  assert(engine->submit({CommandType::Trigger,0,0,100}));engine->render(audio,64);double energy=0;for(float x:audio){assert(std::isfinite(x));energy+=x*x;}assert(energy>0);
  assert(ui->undo());engine->render(audio,0);ui->poll();unchanged();assert(ui->redo());engine->render(audio,0);ui->poll();assert(track_at(*project,0,15).instrument==Instrument::Sampler);
 }
 std::filesystem::current_path(cwd);std::filesystem::remove_all(directory);
 std::puts("Workflow interruptions: live Sampler MAP PADS, OOM/queue refusal, immutable sample playback and grouped Undo/Redo passed");
}
