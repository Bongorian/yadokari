#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <unistd.h>
using namespace groove;
int main(){
 char root[]="/tmp/mk3-ui390-XXXXXX";assert(mkdtemp(root));auto previous=std::filesystem::current_path();std::filesystem::current_path(root);DesktopFiles fs;DesktopSampleAssets library;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto engine=std::make_unique<Engine>();
 assert(engine->stage_project(*p));float audio[128]{};engine->render(audio,0);
 Performance ui(*engine,*p);SessionFiles files(fs,library,*engine,*p,ui,root);
 auto flush=[&]{engine->render(audio,64);ui.poll();};
 char marker[160],location[160];assert(join_path(marker,sizeof marker,root,"CURRENT.TXT"));assert(write_project_location(fs,marker,"GROOVE/My Song.mgx"));assert(read_project_location(fs,marker,location,sizeof location) && !std::strcmp(location,"GROOVE/My Song.mgx"));assert(fs.write(marker,0,"../escape",9,true));assert(!read_project_location(fs,marker,location,sizeof location));
 char initial[160],named[160];assert(join_path(initial,sizeof initial,root,"Initial.mgxs"));assert(copy_text(named,sizeof named,"PROJECTS/My Song.mgxs"));
 assert(files.save(initial));ui.set_mode(UiMode::Project);assert(ui.soft_key(3) && ui.browser_editing());copy_text(ui.browser().edit.name,sizeof ui.browser().edit.name,"My Song");assert(ui.soft_key(0));flush();assert(files.service());assert(fs.exists(named) && !std::strcmp(ui.project_path(),named));
 assert(copy_file(fs,initial,"PROJECTS/Initial.mgxs"));assert(ui.soft_key(3));copy_text(ui.browser().edit.name,sizeof ui.browser().edit.name,"Initial");assert(ui.soft_key(0));assert(!files.service());assert(!std::strcmp(ui.project_path(),named));
 assert(files.load(initial));flush();assert(!std::strcmp(ui.project_path(),initial));
 // Existing uppercase projects must reopen through the real Browser action.
 char upper[160];assert(join_path(upper,sizeof upper,root,"EXISTING.MGX"));assert(files.save(upper));ui.set_mode(UiMode::Browser);ui.take_file_action();copy_text(ui.browser().directory,sizeof ui.browser().directory,root);copy_text(ui.browser().entries[0].name,sizeof ui.browser().entries[0].name,"EXISTING.MGX");ui.browser().entries[0].directory=false;ui.browser().count=1;ui.browser().cursor=0;assert(ui.request_file(FileAction::Open) && files.service());flush();assert(!std::strcmp(ui.project_path(),upper));assert(browser_project_kind("Kit.MgXK") && browser_project_kind("Patch.MGXP") && !browser_project_kind("Old.MGK") && !browser_project_kind("song.mgx.wav"));

 ui.new_project_name();ui.request_save();assert(ui.browser_editing() && !ui.take_save_request());ui.cancel_browser_edit();
 // Runtime expression must not mutate saved bend / CC bases.
 assert(ui.set_instrument(Instrument::ExternalMidi));flush();MidiEvent midi;while(engine->pop_midi(midi)){}
 ui.set_strip_mode(StripMode::Pitch);assert(ui.strip_input(16383,16383));flush();bool bent=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0xe0 && midi.bytes[1]==127 && midi.bytes[2]==127)bent=true;assert(bent);assert(track_at(*p,0,0).pitch_bend==8192);
 assert(ui.strip_input(0,16383));flush();bool centered=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0xe0 && midi.bytes[1]==0 && midi.bytes[2]==64)centered=true;assert(centered);
 ui.set_strip_mode(StripMode::Mod);assert(ui.strip_input(16383,16383));flush();bool mod=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0xb0 && midi.bytes[1]==1 && midi.bytes[2]==127)mod=true;assert(mod);assert(!(track_at(*p,0,0).midi_cc_used[0]&2));
 // Full command queue: release is retained until the DSP consumes it.
 while(engine->command_space()){assert(engine->submit({CommandType::SetFill,0,0,0}));}
 assert(!ui.strip_input(0,16383));flush();flush();bool reset=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0xb0 && midi.bytes[1]==1 && midi.bytes[2]==0)reset=true;assert(reset);
 ui.set_strip_mode(StripMode::Notes);assert(ui.strip_input(1,16383));flush();bool on=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0x90 && midi.bytes[1]==60)on=true;assert(on);
 ui.release_all();flush();bool off=false;while(engine->pop_midi(midi))if((midi.bytes[0]&0xf0)==0x80 && midi.bytes[1]==60)off=true;assert(off);
 // Source chooser must show its own knob labels, not stale generator labels.
 ui.set_mode(UiMode::Synth);assert(ui.set_instrument(Instrument::PolySaw));flush();assert(ui.soft_key(4));assert(!*ui.knob_label(2));assert(ui.soft_key(4));assert(!std::strcmp(ui.soft_label(2),"FIXED ENV"));
 assert(ui.set_instrument(Instrument::WestCoast));assert(ui.soft_key(2));assert(ui.knob_parameter(0)==engine_target(Instrument::WestCoast,0) && ui.knob_parameter(1)==engine_target(Instrument::WestCoast,3) && !ui.knob_parameter(2));
 assert(ui.set_instrument(Instrument::Sampler));assert(ui.soft_key(2));assert(ui.mode()==UiMode::Sampler && ui.sampler_page()==1);
 // Exercise the actual Top dispatch after consolidating Synth descriptors.
 // SOURCE must leave generator labels/edits only after a page is selected;
 // Top4 routes ordinary engines to Modulation and creative engines to Lab.
 for(unsigned id=0;id<instrument_count();++id){
  ui.set_shift(false);ui.set_mode(UiMode::Synth);assert(ui.set_instrument(Instrument(id)));flush();
  assert(ui.set_wave_profile(false));if(ui.layer_page())assert(ui.soft_key(1));assert(ui.soft_key(4) && ui.source_select());
  assert(!std::strcmp(ui.soft_label(4),"LOAD ENGINE"));
  assert(ui.soft_key(7) && !ui.source_select());
  if(id==unsigned(Instrument::Sampler) || id==unsigned(Instrument::DrumKit)){
   assert(!std::strcmp(ui.soft_label(2),"TRIM"));
   assert(ui.soft_key(2) && ui.mode()==UiMode::Sampler && ui.sampler_page()==1);
   ui.set_mode(UiMode::Synth);
  }
  if(id==unsigned(Instrument::Layer)){assert(ui.soft_key(0));assert(!std::strcmp(ui.soft_label(3),"REMOVE"));continue;}
  const bool lab=id>=unsigned(Instrument::WaveSequence) && id<=unsigned(Instrument::SpectralFreeze);
  const bool texture=id==unsigned(Instrument::Texture);
  assert(ui.soft_key(lab || texture?3:5));
  assert(ui.mode()==((lab || texture)?UiMode::Synth:UiMode::Modulation));
  if(texture){assert(ui.instrument_view()==3 && !std::strcmp(ui.soft_label(3),"INPUT / QUALITY"));assert(ui.soft_key(5) && ui.mode()==UiMode::Modulation);}
  if(lab)assert(ui.instrument_view()==3 && !std::strcmp(ui.soft_label(3),"ENGINE LAB"));
 }
 ui.set_mode(UiMode::Synth);assert(ui.set_instrument(Instrument::ExternalMidi));flush();
 assert(!ui.soft_enabled(1) && !ui.soft_key(1));
 ui.set_shift(true);assert(!std::strcmp(ui.soft_label(6),"STORE POOL"));
 assert(!ui.soft_enabled(1) && !ui.soft_key(1));ui.set_shift(false);
 ui.set_shift(true);assert(ui.soft_key(3) && ui.wave_profile());ui.set_shift(false);
 assert(ui.soft_key(0) && !ui.wave_clock());assert(!ui.soft_enabled(6) && !ui.soft_key(6));
 assert(ui.soft_key(1) && ui.wave_clock());
 assert(ui.soft_key(7) && ui.sn_ref_confirmation());
 assert(ui.encoder_direction(0x20) && !ui.sn_ref_confirmation());
 ui.set_shift(true);assert(ui.soft_key(7) && !ui.wave_profile());ui.set_shift(false);
 flush();
 // Static body stays retained during transport; a parameter edit still repaints.
 ui.set_mode(UiMode::Settings);ui.soft_key(0);auto renderer=std::make_unique<UiRenderer>();ui.animate(1000);renderer->draw(*p,ui);ui.animate(2000);renderer->draw(*p,ui);assert(ui.play());flush();renderer->pixels[0][100*480+10]=0xffdebeef;ui.animate(3000);renderer->draw(*p,ui);assert(renderer->pixels[0][100*480+10]==0xffdebeef);assert(ui.encoder(1) && ui.knob(0,1000));ui.animate(3100);renderer->draw(*p,ui);assert(renderer->pixels[0][100*480+10]!=0xffdebeef);
 std::filesystem::current_path(previous);std::filesystem::remove_all(root);std::puts("UI390 named save, strip release, synth controls, playing static body passed");
}
