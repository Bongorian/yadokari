#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unistd.h>
using namespace groove;
struct ReentrantFiles:DesktopFiles {
 Performance* cancel_ui=nullptr;bool cancel_on_read=false;
 bool read(const char* path,uint64_t offset,void* data,unsigned wanted,unsigned& got)override{
  bool ok=DesktopFiles::read(path,offset,data,wanted,got);
  if(cancel_on_read && is_midi_file(path)){cancel_on_read=false;check_cancel();}
  return ok;
 }
 void check_cancel(){if(!cancel_ui || !cancel_ui->encoder_direction(0x20))throw std::runtime_error("reentrant cancellation");}
};
static void check(bool ok,const char* stage){if(!ok)throw std::runtime_error(stage);}
int main(){
 char directory[]="build/mk3-wave5-import-XXXXXX";
 if(!mkdtemp(directory))return 1;
 int result=0;
 try{
  ReentrantFiles fs;DesktopSampleAssets assets;
  const uint8_t midi[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,0x90,60,100,24,0x80,60,0,0,0xff,0x2f,0};
  char path[160];check(join_path(path,sizeof path,directory,"phrase.MID"),"path");
  check(fs.write(path,0,midi,sizeof midi,true),"fixture write");
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);
  auto track=track_at(*p,0,0);track.instrument=Instrument::ExternalMidi;track.steps[10].enabled=true;
  auto engine=std::make_unique<Engine>();check(engine->stage_project(*p),"project stage");
  Performance ui(*engine,*p);SessionFiles files(fs,assets,*engine,*p,ui,directory);
  float frame[2]{};engine->render(frame,0);
  copy_text(ui.browser().directory,sizeof ui.browser().directory,directory);files.refresh();
  ui.set_mode(UiMode::Browser);ui.set_shift(true);
  std::fprintf(stderr,"request: files=%u name=%s modal=%u\n",ui.browser().count,ui.browser().entries[ui.browser().cursor].name,ui.modal_active());
  check(ui.soft_key(3),"initial IMPORT action");check(files.service(),"initial IMPORT service");
  std::fprintf(stderr,"after request: status=%s modal=%u\n",ui.browser().status,ui.modal_active());
  check(track.steps[10].enabled,"request must preserve sequence");
  check(ui.modal_active(),"IMPORT confirmation must own input");
  check(ui.encoder_direction(0x20),"cancel with LEFT");check(files.service(),"cancel service");
  check(!ui.modal_active() && track.steps[10].enabled,"cancel must preserve sequence");
  check(ui.soft_key(3),"rearm IMPORT action");check(files.service(),"rearm service");
  check(ui.modal_active() && track.steps[10].enabled,"rearm must confirm again");
  check(ui.encoder_push(),"confirm with PUSH");check(files.service(),"confirmed IMPORT service");
  for(unsigned n=0;n<40 && files.recording_busy();++n)check(files.service(),"import progress");
  check(!files.recording_busy(),"import completion");engine->render(frame,0);
  check(track.steps[0].enabled && track.steps[0].note==60 && !track.steps[10].enabled,"imported sequence");
  check(ui.undo(),"import undo");engine->render(frame,0);
  check(track.steps[10].enabled && !track.steps[0].enabled,"undo restores sequence");
  check(ui.soft_key(3),"target-change arm");check(ui.select_track(1),"simulate target change");check(ui.encoder_push(),"queue bound request");
  check(files.service(),"accept fixed target");
  check(ui.encoder_direction(0x20) && files.service(),"cancel fixed target before read");
  check(!ui.modal_active(),"changed editor context must not strand modal");
  check(track.steps[10].enabled && !track.steps[0].enabled,"target-change preserves sequence");
  check(ui.select_track(0),"restore selection");check(ui.soft_key(3),"queued-cancel arm");check(ui.encoder_push(),"queued-cancel confirm");
  check(ui.encoder_direction(0x20),"cancel queued request");check(files.service(),"queued-cancel service");
  check(!ui.modal_active() && !files.recording_busy() && track.steps[10].enabled,"queued cancellation must preserve sequence");
  check(ui.soft_key(3) && ui.encoder_push() && files.service(),"accept cancellable backend job");
  check(ui.modal_active() && files.recording_busy(),"backend progress owns input");
  check(ui.encoder_direction(0x20),"cancel running import");check(files.service(),"drain cancellation before service");
  check(!ui.modal_active() && !files.recording_busy() && track.steps[10].enabled,"running cancel preserves sequence");
  check(ui.soft_key(3) && ui.encoder_push() && files.service(),"accept reentrant cancellation case");
  fs.cancel_ui=&ui;fs.cancel_on_read=true;
  for(unsigned n=0;n<40 && files.recording_busy();++n)check(files.service(),"reentrant cancel progress");
  check(!fs.cancel_on_read && !ui.modal_active() && !files.recording_busy(),"cancel during storage read must complete");
  check(track.steps[10].enabled && !track.steps[0].enabled,"storage callback cancellation must not publish");fs.cancel_ui=nullptr;
  std::puts("Wave5 MIDI import: request/cancel/rearm/PUSH/Undo passed");
 }catch(const std::exception& e){std::fprintf(stderr,"Wave5 MIDI import failed: %s\n",e.what());result=1;}
 std::filesystem::remove_all(directory);return result;
}
