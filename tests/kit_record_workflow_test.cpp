#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <unistd.h>

class IncrementalAssets:public groove::DesktopSampleAssets {
public:
 bool supports_incremental_load()const override{return true;}
 bool begin_incremental_load(const char* path,uint32_t)override{if(active || !groove::copy_text(name,sizeof name,path))return false;active=true;steps=0;++starts;return true;}
 groove::SampleLoadProgress poll_incremental_load(groove::WavView& view)override{if(!active)return groove::SampleLoadProgress::Failed;++polls;if(++steps<3)return groove::SampleLoadProgress::Working;active=false;return load(name,view)?groove::SampleLoadProgress::Ready:groove::SampleLoadProgress::Failed;}
 void cancel_incremental_load()override{active=false;++cancels;}
 bool active=false;unsigned steps=0,starts=0,polls=0,cancels=0;char name[192]{};
};

int main(){
 using namespace groove;
 char directory[]="/tmp/mk3-kit-record-XXXXXX";
 assert(mkdtemp(directory));
 char previous[4096];assert(getcwd(previous,sizeof previous));assert(chdir(directory)==0);
 DesktopFiles fs;IncrementalAssets assets;
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 auto engine=std::make_unique<Engine>(8000);
 Performance ui(*engine,*project);SessionFiles files(fs,assets,*engine,*project,ui,"RECORDINGS");
 SampleAnalysisSlot analysis_worker;files.set_resident_analysis_worker(&analysis_worker);
 assert(ui.set_instrument(Instrument::DrumKit));float output[128]{},input[128]{};
 engine->render(output,0);
 ui.set_mode(UiMode::Sampler);ui.set_shift(true);
 assert(ui.soft_key(4));assert(ui.select_kit_pad(1));assert(files.service() && ui.live_recording() && ui.recording_target().kit_pad==0);
 for(unsigned block=0;block<125;++block){
  for(unsigned i=0;i<64;++i){float v=0.2f*sinf(6.2831853f*440*float(block*64+i)/8000);input[2*i]=input[2*i+1]=v;}
  engine->render_ports(output,nullptr,input,64);assert(files.service());
 }
 assert(ui.soft_key(4) && files.service());
 for(unsigned i=0;i<8 && ui.live_recording();++i){engine->render_ports(output,nullptr,input,64);assert(files.service());}
 RecordingResult result;assert(files.take_recording_result(result));
 assert(result.saved && result.verified && result.assigned && result.slot<64);
 auto kit=track_at(*project,0,0).kit;
 assert(kit.pads[0].sample[0]==result.slot && kit.pads[1].sample[0]==255);assert(ui.select_kit_pad(0));
 assert(fs.exists(project->assets[result.slot].path));
 // Audition follows the selected pad even when both pads reference the same
 // WAV. Its edited quarter-length region ends after 2,000 frames at 8 kHz.
 auto second=kit.pads[1];second.sample[0]=result.slot;second.edit.start=0.25f;second.edit.end=0.5f;second.edit.fade_ms=0;second.reverse=true;assert(ui.set_kit_pad(1,second));engine->render(output,0);
 assert(ui.soft_key(5) && files.service() && ui.sample_auditioning());engine->render(output,64);
 assert(ui.select_kit_pad(1) && !ui.sample_auditioning());assert(ui.soft_key(5) && files.service() && ui.sample_auditioning());engine->render(output,0);
 for(unsigned i=0;i<32;++i){engine->render(output,64);}assert(files.service() && !ui.sample_auditioning());
 // Two asset entries may name the same WAV with different metadata.
 project->assets[63]=project->assets[result.slot];project->assets[63].metadata.root=73;second.sample[0]=63;assert(ui.set_kit_pad(1,second));assert(copy_text(ui.browser().preview_path,sizeof ui.browser().preview_path,project->assets[63].path));assert(ui.preview_metadata().root==73);
 second=KitPadState{};assert(ui.set_kit_pad(1,second));project->assets[63]=SampleAssetState{};assert(ui.select_kit_pad(0));engine->render(output,0);
 assert(ui.browser().analysis_pending);assert(files.service() && analysis_worker.busy() && analysis_worker.process_one() && files.service());
 assert(!ui.browser().analysis_pending && ui.browser().analysis_source==ui.browser().preview.data);
 ui.browser().analysis_source=nullptr;assert(files.service() && files.service() && analysis_worker.process_one() && files.service());
 assert(ui.browser().analysis_source==ui.browser().preview.data);
 assert(ui.request_file(FileAction::AutoSlice) && files.service());
 assert(files.service() && analysis_worker.busy());
 auto changed=kit.pads[0].edit;changed.end=0.75f;assert(ui.sample_edit(changed));engine->render(output,0);
 assert(analysis_worker.process_one());assert(files.service());
 assert(!ui.browser().analysis_pending && kit.pads[0].edit.end==0.75f);
 assert(ui.request_file(FileAction::AutoSlice) && files.service());
 assert(files.service() && analysis_worker.process_one() && files.service());engine->render(output,0);
 assert(!ui.browser().analysis_pending && kit.pads[0].edit.slice_count>0);
 changed=kit.pads[0].edit;changed.start=0;changed.end=1;changed.slice_count=0;assert(ui.sample_edit(changed));engine->render(output,0);
 ui.set_shift(false);assert(ui.soft_key(5) && ui.soft_key(0) && files.service());
 engine->render(output,0);
 for(unsigned i=0;i<16;++i){assert(kit.pads[i].sample[0]==result.slot);assert(fabsf(kit.pads[i].edit.start-float(i)/16)<0.0001f);}
 assert(ui.undo());engine->render(output,0);assert(kit.pads[0].sample[0]==result.slot && kit.pads[1].sample[0]==255);assert(ui.redo());engine->render(output,0);for(unsigned i=0;i<16;++i)assert(kit.pads[i].sample[0]==result.slot);
 // SLICES: SELECT chooses silently, SHIFT+SELECT swaps pad sounds while
 // preserving the step lanes; the two assignments form one undo group.
 auto saved_step=kit.steps[0][3];saved_step.pitch_lock=7;assert(ui.set_kit_step(0,3,saved_step));engine->render(output,0);assert(ui.soft_key(2) && ui.select_kit_pad(0));assert(ui.control(5,0x08));ui.set_shift(true);assert(ui.pad(13,0x10,3000) && ui.pad(13,0x20,0));assert(ui.control(5,0));ui.set_shift(false);assert(fabsf(kit.pads[0].edit.start-1.0f/16)<0.0001f && kit.pads[1].edit.start==0);assert(kit.steps[0][3].pitch_lock==7 && !kit.steps[1][3].enabled);engine->render(output,0);assert(ui.undo());engine->render(output,0);assert(kit.pads[0].edit.start==0 && fabsf(kit.pads[1].edit.start-1.0f/16)<0.0001f);assert(ui.redo());engine->render(output,0);assert(ui.undo());engine->render(output,0);assert(ui.select_kit_pad(0));
 // One kit track turns the mapped pads into a 16-step phrase, preserving the
 // existing sequencer and allowing one undo for the entire operation.
 ui.set_shift(true);assert(ui.soft_key(7) && ui.soft_key(0));engine->render(output,0);for(unsigned pad=0;pad<16;++pad){for(unsigned step=0;step<16;++step)assert(bool(kit.steps[pad][step].enabled)==(pad==step));}assert(ui.undo());engine->render(output,0);for(unsigned pad=0;pad<16;++pad){for(unsigned step=0;step<16;++step)assert(!kit.steps[pad][step].enabled);}assert(ui.redo());engine->render(output,0);ui.set_shift(false);
 auto edit=kit.pads[0].edit;edit.start=0;edit.end=1;edit.slice_count=4;
 for(unsigned i=0;i<=4;++i)edit.markers[i]=float(i)/4;
 assert(ui.sample_edit(edit));engine->render(output,0);
 assert(ui.soft_key(5) && ui.soft_key(0) && files.service());engine->render(output,0);
 for(unsigned i=0;i<4;++i)assert(kit.pads[i].sample[0]==result.slot);
 for(unsigned i=4;i<16;++i)assert(kit.pads[i].sample[0]==255);
 assert(ui.request_file(FileAction::SaveKit) && ui.browser_editing());assert(copy_text(ui.browser().edit.name,96,"Recorded Kit"));assert(ui.browser_edit_key(0) && files.service());
 assert(fs.exists("KITS/Recorded Kit.mgxk"));
 assert(ui.select_track(1));
 assert(ui.play());engine->render(output,0);ui.poll();
 // Keep an already-sounding pad on track 0 alive while a kit is installed on
 // track 1. Applying the kit must not emit transport Stop/Start or reset time.
 Command hit{CommandType::Trigger,0,0,100};hit.parameter=4;hit.scalar=0;assert(engine->submit(hit));engine->render(output,64);assert(engine->active_sample_voices()>0);ClockEvent clock;while(engine->pop_clock(clock)){}ui.poll();uint64_t before=engine->samples_rendered();
 ProjectDecodeSlot decode_worker;files.set_project_load_worker(&decode_worker);assert(files.start_kit_load("KITS/Recorded Kit.mgxk"));while(files.kit_load_busy() && !assets.active){assert(files.service());decode_worker.process_one();}assert(assets.active && ui.select_track(0));assert(files.service() && !files.kit_load_busy() && !assets.active && assets.cancels==1);bool loaded=false,probe=false;assert(files.take_kit_load_result(loaded,probe) && !loaded && !probe);assert(track_at(*project,0,1).instrument!=Instrument::DrumKit);assert(ui.select_track(1));
 // A target change between final revision checks must cancel publication.
 struct FinalChange {Performance* ui;bool fired=false;} final_change{&ui};
 files.set_kit_probe_hook([](void* context,unsigned phase){auto& change=*static_cast<FinalChange*>(context);if(phase==4){change.fired=true;assert(change.ui->select_track(0));}},&final_change);
 assert(files.start_kit_load("KITS/Recorded Kit.mgxk",true));while(files.kit_load_busy()){assert(files.service());decode_worker.process_one();}
 assert(final_change.fired && files.take_kit_load_result(loaded,probe) && !loaded && probe);
 assert(track_at(*project,0,1).instrument!=Instrument::DrumKit);files.set_kit_probe_hook(nullptr,nullptr);assert(ui.select_track(1));
 assert(files.start_kit_load("KITS/Recorded Kit.mgxk"));while(files.kit_load_busy()){assert(files.service());decode_worker.process_one();}assert(files.take_kit_load_result(loaded,probe) && loaded && !probe);assert(assets.starts>0 && assets.polls>=3 && !assets.active);
 assert(ui.selected_track()==1 && ui.mode()==UiMode::Pad && ui.playing());
 engine->render(output,64);assert(engine->samples_rendered()==before+64 && engine->active_sample_voices()>0);while(engine->pop_clock(clock))assert(clock.type!=ClockEventType::Stop && clock.type!=ClockEventType::Start);
 assert(ui.playing());
 assert(track_at(*project,0,1).instrument==Instrument::DrumKit);
 assert(track_at(*project,0,1).kit.pads[15].sample[0]==255);
 assert(track_at(*project,0,0).kit.pads[0].sample[0]==result.slot && fs.exists(project->assets[result.slot].path));
 assert(chdir(previous)==0);
 std::filesystem::remove_all(directory);
}
