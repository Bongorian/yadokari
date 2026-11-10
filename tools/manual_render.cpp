#include "engine.h"
#include "performance.h"
#include "ui_renderer.h"
#include "ui/text_alignment.h"
#include "page_descriptors.h"
#include "demo_project.h"
#include "../platform/desktop/sample_assets.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <filesystem>

// Documentation fixture only. Uses the actual controller and renderer; never
// connects to hardware or changes the user's session/assets.
int main(int argc,char** argv) {
 const bool themed=argc==3 && !std::strcmp(argv[1],"--theme");unsigned theme_index=themed?unsigned(std::strtoul(argv[2],nullptr,10)):groove::ui::Theme::kDefault;if(theme_index>=groove::ui::Theme::kCount)return 1;
 const bool wave5_review=argc==2 && !std::strcmp(argv[1],"--wave5-review");
 using namespace groove;
 auto p=std::make_unique<ProjectState>(); make_demo(*p);
 auto e=std::make_unique<Engine>(); DesktopSampleAssets assets;
 WavView bindings[kSampleAssets]{};
 if(!assets.prepare(*p,bindings) || !e->stage_project(*p,bindings))return 1;
 Performance ui(*e,*p);auto preferences=ui.device_settings();preferences.theme=uint8_t(theme_index);if(!ui.set_device_settings(preferences))return 1; auto screen=std::make_unique<UiRenderer>();
 uint64_t ui_frames=0;
 auto advance=[&](unsigned frames=128){float block[256];while(frames){unsigned n=frames>128?128:frames;e->render(block,n);ui_frames+=n;ui.animate(ui_frames/48);ui.poll();MidiEvent m;while(e->pop_midi(m)){}ClockEvent c;while(e->pop_clock(c)){}frames-=n;}};
 advance();
 std::filesystem::path frame_dir=std::filesystem::path("build/manual-themes")/std::to_string(theme_index);if(themed)std::filesystem::create_directories(frame_dir);
 struct PlacementAudit{FILE* file;const char* frame;};
 FILE* frames=themed?std::fopen((frame_dir/"frames.tsv").c_str(),"w"):nullptr;if(frames)std::fprintf(frames,"frame\tmode\tengine\tpage\ttracker\ttype\tleft\tright\ttitle\n");
 FILE* placements=themed?std::fopen((frame_dir/"type-placements.tsv").c_str(),"w"):nullptr;PlacementAudit audit_text{placements,"setup"};
 if(placements){std::fprintf(placements,"frame\tside\tx\ty\twidth\trole\tnumeric\tband_y\tband_h\ttext\tfield_x\tfield_w\talign\tkind\terrors\n");screen->text_observer=[](const SoftwareCanvas::TextPlacement& t,void* context){const auto& audit=*static_cast<PlacementAudit*>(context);if(t.side>1 || t.width<0)throw std::runtime_error("invalid text placement");if(t.band_h){const auto& f=ui::font_face(t.role);int error=2*ui::cap_top(f,t.y)+f.cap_height-(2*t.band_y+t.band_h);(void)error;}std::fprintf(audit.file,"%s\t%u\t%d\t%d\t%d\t%u\t%u\t%d\t%d\t%s\t%d\t%d\t%u\t%u\t%u\n",audit.frame,t.side,t.x,t.y,t.width,unsigned(t.role),t.numeric,t.band_y,t.band_h,t.text,t.field_x,t.field_w,unsigned(t.align),unsigned(t.kind),ui::alignment_errors(t));};screen->text_observer_context=&audit_text;}
 FILE* controls=std::fopen(wave5_review?"build/wave5-review/screens-controls.tsv":"build/manual-controls.tsv","w");if(!controls)return 1;
 std::fprintf(controls,"screen\tmode\tshift\tkind\tnumber\tlabel\tenabled\ttype\twork\tslot\tvalue\tparameter\tlabel_width\tplacement_exception\n");
 auto shot=[&](const char* name){
  audit_text.frame=name;const auto& meaning=ui::screen_meaning(screen_type(ui));if(frames)std::fprintf(frames,"%s\t%s\t%u\t%u\t%u\t%s\t%s\t%s\t%s\n",name,mode_name(ui.mode()),unsigned(p->tracks[ui.selected_track()].instrument),ui.instrument_view(),unsigned(ui.tracker_view()),meaning.name,meaning.left,meaning.right,ui.page_title());
  if(!std::strstr(name,"overlay") && !std::strstr(name,"help")){ui.browser().status[0]=0;ui_frames+=120000;ui.animate(ui_frames/48);}
  ui.notify(UiNotice::None); // Drop notices left by the exhaustive control audit before documenting a normal page.
  const auto track=track_at(*p,p->active_pattern,ui.selected_track());unsigned slot=track.instrument==Instrument::DrumKit?track.kit.pads[ui.selected_kit_pad()].sample[0]:track.sample_slot;
  WavView sample=slot<kSampleAssets?bindings[slot]:WavView{};
  if(sample.data && !std::strstr(name,"analysis-pending") && (ui.mode()==UiMode::Sampler || (ui.mode()==UiMode::Synth && track.instrument==Instrument::Sampler))){analyze_sample(sample,ui.browser().analysis);ui.browser().analysis_source=sample.data;}
  advance();screen->draw(*p,ui,0,0,sample);advance(2400);ui_frames+=4800;ui.animate(ui_frames/48);screen->draw(*p,ui,0,0,sample);
  char path[160];if(themed)std::snprintf(path,sizeof path,"%s/%s.ppm",frame_dir.c_str(),name);else std::snprintf(path,sizeof path,"build/manual-%s.ppm",name);FILE* f=std::fopen(path,"wb");if(!f)throw std::runtime_error("screen open");std::fprintf(f,"P6\n960 272\n255\n");
  for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t v=screen->pixels[x/480][y*480+x%480];unsigned char rgb[]={uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};if(std::fwrite(rgb,1,3,f)!=3)throw std::runtime_error("screen write");}
  if(std::fclose(f))throw std::runtime_error("screen close");
  bool prior_shift=ui.shifted();for(unsigned modifier=0;modifier<2;++modifier){ui.set_shift(modifier);for(unsigned i=0;i<8;++i){const auto top=ui.soft_presentation(i);std::fprintf(controls,"%s\t%s\t%d\tbutton\t%u\t%s\t%d\t%s\t%s\t%u\t\t0\t%d\t%d\n",name,mode_name(ui.mode()),ui.shifted(),i+1,ui.soft_label(i),ui.soft_enabled(i),meaning.name,ui::top_work_name(top.work),ui.top_slot(i),ui::text_width(top.label,ui::FontRole::Small)+(top.icon?18:0),int(top_placement_exception(ui,i)));std::fprintf(controls,"%s\t%s\t%d\tknob\t%u\t%s\t%d\t%s\tvalue\t%u\t%.6f\t%u\t%d\t0\n",name,mode_name(ui.mode()),ui.shifted(),i+1,ui.knob_label(i),*ui.knob_label(i)!=0,meaning.name,i,ui.knob_display_value(i),ui.knob_parameter(i),ui::text_width(ui.knob_label(i),ui::FontRole::Small));}}ui.set_shift(prior_shift);

 };
 if(wave5_review){
  ui.select_track(4);ui.set_mode(UiMode::Synth);ui.soft_key(4);shot("source");ui.soft_key(4);
  ui.set_mode(UiMode::Browser);auto& b=ui.browser();b.count=1;b.cursor=0;
  copy_text(b.directory,sizeof b.directory,"Imported");copy_text(b.entries[0].name,sizeof b.entries[0].name,"Phrase.MID");b.entries[0].directory=false;
  ui.set_shift(true);if(!ui.soft_key(3))return 1;shot("midi-import-confirm");return std::fclose(controls);
 }
 ui.select_track(0);ui.set_mode(UiMode::Pad);shot("pad");
 ui.select_track(3);ui.set_mode(UiMode::Step);ui.encoder(-int(ui.selected_step()));ui.set_shift(true);ui.knob(2,1000);ui.knob(3,1000);shot("step-attributes-shift");ui.set_shift(false);shot("step-attributes");ui.set_shift(true);ui.soft_key(3);shot("step-attributes-overview");ui.soft_key(3);ui.set_shift(false);shot("tracker-attributes");ui.set_shift(true);ui.knob(2,-1000);ui.knob(3,-1000);ui.soft_key(3);ui.set_shift(false);ui.select_track(0);
 ui.set_mode(UiMode::Step);shot("step");ui.set_shift(true);shot("step-shift");ui.set_shift(false);ui.set_shift(true);ui.soft_key(3);ui.soft_key(3);ui.set_shift(false);if(!ui.tracker_view())throw std::runtime_error("tracker fixture did not enter TRACKER");shot("tracker");ui.dispatch({ControllerInput::Type::KnobTouch,0,1});shot("tracker-overlay");ui.dispatch({ControllerInput::Type::KnobTouch,5,1});shot("tracker-multitouch-overlay");ui.dispatch({ControllerInput::Type::KnobTouch,5,0});ui.dispatch({ControllerInput::Type::KnobTouch,0,0});ui.control(4,0x08);ui_frames+=24000;ui.animate(ui_frames/48);shot("step-help");ui.control(4,0);
 ui.select_track(4);ui.set_mode(UiMode::Synth);shot("synth");ui.control(7,0x20);ui.control(7,0);shot("source");
 // Authored selection rows exercise the real shared renderer without reading
 // the host's build directory or installing sounds into the documentation pool.
 auto& shelf=ui.browser();shelf.library_list=true;shelf.library_instrument=track_at(*p,0,4).instrument;shelf.library_kits=false;shelf.library_total=shelf.count=3;shelf.cursor=0;
 const char* choices[]={"Wide Analog Pad","Quiet Glass Keys","Moving Bass"};for(unsigned n=0;n<3;++n){copy_text(shelf.entries[n].name,96,choices[n]);shelf.library_factory[n]=n!=1;shelf.library_revision[n]=1;shelf.library_engine[n]=shelf.library_instrument;}
 shot("engine-sounds");ui.soft_key(7);ui.set_mode(UiMode::Library);shelf.library_list=true;shelf.count=shelf.library_total=3;shelf.cursor=0;shot("presets");shelf.library_kits=true;for(auto& kind:shelf.library_engine)kind=Instrument::DrumKit;shot("kits");ui.set_mode(UiMode::Synth);
 ui.control(7,0x20);ui.control(7,0);ui.select_source(unsigned(Instrument::Sampler));ui.take_file_action();shot("engine-no-sounds");ui.soft_key(7);
 {auto saved=ui.browser();auto& menu=ui.browser();ui.set_mode(UiMode::Browser);menu.new_project_list=true;menu.library_list=false;menu.count=menu.folder_total=3;menu.folder_offset=menu.cursor=0;copy_text(menu.entries[0].name,96,"EMPTY");copy_text(menu.entries[1].name,96,"Example Template A.mgxs");copy_text(menu.entries[2].name,96,"Example Template B.mgxs");shot("new-templates");ui.set_mode(UiMode::Project);ui.browser()=saved;ui.take_file_action();}
 ui.set_mode(UiMode::Keyboard);shot("keyboard");ui.set_mode(UiMode::Chord);shot("chords");
 ui.set_mode(UiMode::Clip);shot("clip");ui.set_shift(true);shot("clip-copy-scope");if(!ui.begin_variation())throw std::runtime_error("variation fixture failed");shot("clip-variation");ui.modal_cancel();ui.set_shift(false);ui.soft_key(0);shot("clip-follow");ui.soft_key(0);
 ui.set_mode(UiMode::Scene);shot("scene");{auto occupied=p->scenes[15];p->scenes[15]=SceneState{};ui.set_shift(true);if(!ui.begin_scene_duplicate())throw std::runtime_error("scene duplicate fixture failed");shot("scene-duplicate");ui.modal_cancel();ui.set_shift(false);p->scenes[15]=occupied;}
 {auto saved=p->scenes[1];for(unsigned t=0;t<16;++t)p->scenes[1].pattern_for_track[t]=uint8_t((t*3)%8);p->scenes[1].mute_mask=uint16_t(1u<<5);ui.encoder(1);shot("scene-varied");ui.encoder(-1);p->scenes[1]=saved;}
 ui.set_mode(UiMode::Song);shot("song");{auto song=p->song;auto chain=p->chain;p->song=SongState{};p->chain.length=0;shot("song-empty");p->song=song;p->chain=chain;}{auto saved=p->song;if(!saved.length && ui.song_skeleton()){shot("song-skeleton");ui.undo();advance();}p->song=saved;}
 {auto original=p->song;auto& sg=p->song;sg.length=4;for(unsigned i=0;i<4;++i){sg.sections[i]=SongSection{};sg.sections[i].scene=uint8_t(i);sg.sections[i].bars=uint16_t(2*(i+1));}sg.enabled=true;sg.loop=true;shot("song-sections");
 sg.length=3;sg.sections[0]=SongSection{};sg.sections[0].ticks=7680;for(unsigned t=0;t<16;++t)sg.sections[0].patterns[t]=uint8_t(t%4);sg.sections[0].playing_mask=0xffff;sg.sections[0].mute_mask=uint16_t(1u<<3);sg.sections[1]=SongSection{};sg.sections[1].scene=2;sg.sections[1].bars=8;sg.sections[2]=SongSection{};sg.sections[2].scene=1;sg.sections[2].bars=4;shot("song-snapshot");p->song=original;}ui.soft_key(4);shot("song-take");ui.soft_key(5);advance();
 ui.set_mode(UiMode::Mixer);shot("mixer");ui.set_mode(UiMode::Effects);shot("fx");ui.soft_key(3);shot("inserts");
 for(unsigned type=0;type<20;++type){ui.set_parameter(94,float(type));ui.set_parameter(95,0.6f);ui.set_parameter(96,0.5f);char name[32];std::snprintf(name,sizeof name,"insert-%u",type);shot(name);}ui.soft_key(1);shot("compressor");
 ui.set_mode(UiMode::Modulation);ui.soft_key(1);shot("automation");ui.soft_key(2);shot("macro-map");ui.soft_key(3);shot("midi");ui.set_shift(true);shot("midi-input");ui.soft_key(3);ui.set_shift(false);shot("nrpn");
 ui.set_mode(UiMode::Perform);shot("perform");ui.soft_key(1);shot("macros");ui.soft_key(6);shot("live-fx");ui.soft_key(2);shot("gesture");ui.soft_key(3);shot("mutation");
 ui.set_mode(UiMode::Generate);shot("generate");ui.soft_key(2);ui.set_shift(true);shot("groove-apply-all");ui.set_shift(false);ui.soft_key(0);
 ui.set_mode(UiMode::Modulation);ui.soft_key(0);shot("modulation");ui.set_parameter(11,1);ui.set_parameter(12,4);advance(30000);shot("modulation-sync");ui.set_parameter(11,0);
 for(unsigned mode: {unsigned(UiMode::Generate),unsigned(UiMode::Perform)}){ui.set_mode(UiMode(mode));for(unsigned i=0;i<6;++i)ui.encoder_direction(0x20);unsigned last=5;for(unsigned page=0;page<=last;++page){char name[64];std::snprintf(name,sizeof name,"%s-page-%u",mode_name(UiMode(mode)),page);shot(name);ui.encoder_direction(0x08);}}
 ui.set_mode(UiMode::Synth);ui.set_instrument(Instrument::DrumKit);advance();ui.set_mode(UiMode::Effects);for(unsigned i=0;i<6;++i)ui.encoder_direction(0x20);for(unsigned page=0;page<6;++page){char name[64];std::snprintf(name,sizeof name,"effects-page-%u",page);shot(name);ui.encoder_direction(0x08);}ui.set_mode(UiMode::Pad);shot("kit-pads");{auto prior=track_at(*p,p->active_pattern,ui.selected_track()).kit.steps[0][0];auto hit=prior;hit.enabled=true;if(!ui.set_kit_step(0,0,hit))throw std::runtime_error("kit clip fixture failed");advance();ui.set_mode(UiMode::Clip);shot("clip-kit");ui.set_mode(UiMode::Pad);ui.set_kit_step(0,0,prior);advance();}
 {auto lane=p->audio[ui.selected_track()];ui.set_mode(UiMode::Perform);for(unsigned i=0;i<5;++i)ui.encoder_direction(0x08);lane.kind=TrackKind::Looper;ui.set_audio_lane(ui.selected_track(),lane);advance();shot("looper");lane.kind=TrackKind::Audio;ui.set_audio_lane(ui.selected_track(),lane);advance();shot("audio-timeline");lane.kind=TrackKind::Instrument;ui.set_audio_lane(ui.selected_track(),lane);advance();}
 ui.select_track(5);ui.set_mode(UiMode::Sampler);shot("sampler");ui.soft_key(1);shot("trim");ui.soft_key(2);shot("slices");ui.set_shift(true);shot("sampler-record");ui.set_shift(false);
 auto source_region=track_at(*p,p->active_pattern,5).sample_edit;auto cropped_region=source_region;cropped_region.start=.12f;cropped_region.end=.88f;ui.sample_edit(cropped_region);advance();
 ui.set_mode(UiMode::Synth);ui.set_wave_profile(false);ui.soft_key(0);shot("sampler-source-idle");
 ui.pad(mk3::pad_logical(5),0x10,3000);shot("sampler-source-playing");ui.pad(mk3::pad_logical(5),0x20,0);
 ui.browser().analysis_source=nullptr;ui.browser().analysis_background=true;ui.browser().analysis_pending=true;ui.browser().analysis_pending_source=bindings[track_at(*p,0,5).sample_slot].data;shot("sampler-analysis-pending");ui.browser().analysis_background=false;ui.browser().analysis_pending=false;ui.browser().analysis_pending_source=nullptr;
 ui.set_mode(UiMode::Sampler);ui.soft_key(1);ui.dispatch({ControllerInput::Type::KnobTouch,0,1});shot("sampler-start-marker");ui.dispatch({ControllerInput::Type::KnobTouch,0,0});ui.dispatch({ControllerInput::Type::KnobTouch,1,1});shot("sampler-end-marker");ui.dispatch({ControllerInput::Type::KnobTouch,1,0});

 ui.sample_edit(source_region);advance();ui.soft_key(2);ui.encoder_direction(0x08);shot("wave");ui.encoder_direction(0x08);shot("zones");
 // Regression: SHIFT+encoder must reach the Warp toggle.
 ui.set_shift(true);bool warp=ui.encoder_push();ui.set_shift(false);
 ui.set_mode(UiMode::Routing);shot("routing");ui.play();advance();ui.set_shift(true);shot("resample");ui.set_shift(false);ui.stop();advance();
 ui.soft_key(0);shot("group-fx");ui.soft_key(3);shot("group-sends");ui.knob(7,32000);ui.knob(7,18000);shot("group-parallel");
 ui.set_mode(UiMode::Project);shot("project");ui.set_shift(true);shot("midi-export");ui.set_shift(false);ui.soft_key(2);shot("new-confirm");ui.cancel_project();
 auto settings_page=[&](unsigned target){ui.set_mode(UiMode::Settings);while(ui.settings_page()>target)ui.encoder_direction(0x20);while(ui.settings_page()<target)ui.encoder_direction(0x08);};
 settings_page(0);shot("settings");settings_page(1);shot("settings-display");ui.encoder(1);shot("settings-theme");ui.encoder(2);shot("settings-display-last");ui.encoder(-3);settings_page(2);shot("settings-lights");settings_page(3);shot("settings-pad");settings_page(4);shot("settings-audio");ui.dispatch({ControllerInput::Type::KnobTouch,0,1});shot("settings-audio-overlay");ui.dispatch({ControllerInput::Type::KnobTouch,0,0});settings_page(5);shot("settings-midi");settings_page(6);shot("settings-system");

 // Browser model is explicitly an illustrative listing, not a live SD capture.
 ui.set_mode(UiMode::Browser);ui.take_file_action();auto& b=ui.browser();copy_text(b.directory,sizeof b.directory,"ASSETS/SAMPLES");copy_text(b.entries[0].name,sizeof b.entries[0].name,"demo-slices.wav");b.count=1;b.cursor=0;copy_text(b.status,sizeof b.status,"Documentation fixture / host renderer");shot("browser");ui.set_shift(true);shot("browser-shift");ui.soft_key(7);shot("folder-name");ui.cancel_browser_edit();ui.set_shift(false);ui.set_shift(true);ui.soft_key(5);shot("rename");ui.cancel_browser_edit();ui.soft_key(6);shot("delete-confirm");ui.cancel_browser_edit();ui.set_shift(false);
 ui.set_mode(UiMode::Diagnostics);shot("diagnostics");
 ui.set_mode(UiMode::Clip);ui.play();advance();ui.launch_clip(ui.selected_track(),1,true);shot("clip-pending");ui.stop();advance();
 ui.set_mode(UiMode::Scene);ui.play();advance();ui.launch_scene(1,true);shot("scene-pending");ui.stop();advance();
 ui.set_mode(UiMode::Browser);b.count=1;b.cursor=0;copy_text(b.entries[0].name,sizeof b.entries[0].name,"Phrase.MID");ui.set_shift(true);if(!ui.soft_key(3))return 1;shot("midi-import-confirm");ui.encoder_direction(0x20);ui.set_shift(false);
 b.count=0;ui.set_mode(UiMode::Sampler);bool queued=ui.request_file(FileAction::Capture);auto before=ui.pending_file_action();ui.set_mode(UiMode::Browser);auto after=ui.pending_file_action();
 // All engine/page controls, read from the same model as physical Top/knobs.
 ui.take_file_action();
 FILE* audit=std::fopen("build/synth-control-audit.tsv","w");if(!audit)return 1;
 std::fprintf(audit,"engine\tpage\tshift\tcontrol\tnumber\tlabel\tenabled\tparameter\n");
 for(unsigned inst=0;inst<instrument_count();++inst){ui.set_mode(UiMode::Synth);ui.set_instrument(Instrument(inst));advance();
  for(unsigned page=0;page<4;++page){ui.set_mode(UiMode::Synth);ui.set_shift(false);auto page_kind=synth_button_control(page,{Instrument(inst)}).action;if(page_kind!=SynthAction::Generator && page_kind!=SynthAction::Tone && page_kind!=SynthAction::Shape && page_kind!=SynthAction::Lab && page_kind!=SynthAction::Trim)continue;if(!ui.soft_enabled(page))continue;ui.soft_key(page);
   if(themed){auto kind=synth_button_control(page,{Instrument(inst)}).action;if(kind==SynthAction::Generator || kind==SynthAction::Tone || kind==SynthAction::Shape || kind==SynthAction::Lab){char name[64];std::snprintf(name,sizeof name,"engine-%02u-page-%u",inst,page);shot(name);}}
   for(unsigned shift=0;shift<2;++shift){ui.set_shift(shift);for(unsigned k=0;k<8;++k){std::fprintf(audit,"%s\t%u\t%u\tTop\t%u\t%s\t%d\t0\n",instrument_descriptor(inst)->name,page,shift,k+1,ui.soft_label(k),ui.soft_enabled(k));std::fprintf(audit,"%s\t%u\t%u\tKnob\t%u\t%s\t%d\t%u\n",instrument_descriptor(inst)->name,page,shift,k+1,ui.knob_label(k),ui.knob_label(k)[0]!=0,ui.knob_parameter(k));}}
  }
 }
 ui.set_shift(false);ui.set_mode(UiMode::Synth);if(!ui.set_instrument(Instrument::Formant))return 1;advance(4096);if(!ui.store_sound(250))return 1;advance(4096);if(!ui.set_instrument(Instrument::Layer))return 1;advance(4096);ui.soft_key(0);while(ui.layer_pool_cursor()<250)ui.knob(2,1000);if(!ui.soft_key(2))return 1;advance(4096);if(track_at(*p,p->active_pattern,ui.selected_track()).layer.count!=1)return 1;
 for(unsigned page=0;page<5;++page){if(page)ui.knob(7,1000);if(page==1){for(unsigned n=0;n<3;++n)ui.knob(3,16000);ui.knob(4,-16000);ui.knob(4,-16000);ui.knob(4,-11000);ui.knob(5,19000);ui.knob(6,-12000);}if(page==2)ui.knob(3,7000);if(page==4){ui.knob(3,10000);ui.knob(4,6000);}advance(4096);char name[64];std::snprintf(name,sizeof name,"layer-edit-%u",page);shot(name);}
 std::fclose(audit);ui.set_shift(false);ui.set_mode(UiMode::Synth);ui.set_instrument(Instrument::FM);ui.soft_key(0);shot("synth-fm");ui.soft_key(1);shot("synth-tone");
 ui.set_instrument(Instrument::DaisySynth);ui.set_mode(UiMode::Synth);ui.soft_key(2);shot("synth-adsr");
 ui.set_instrument(Instrument::WestCoast);ui.set_mode(UiMode::Synth);ui.soft_key(2);shot("synth-folder");
 ui.set_mode(UiMode::Synth);ui.set_instrument(Instrument::VirtualAnalog);advance();ui.set_mode(UiMode::Effects);for(unsigned i=0;i<6;++i)ui.encoder_direction(0x20);for(unsigned i=0;i<3;++i)ui.encoder_direction(0x08);ui.set_parameter(94,20);ui.set_parameter(95,19.f/24);advance();shot("effects-pitch-shift");ui.set_parameter(94,21);ui.set_parameter(95,.5f);advance();shot("effects-rotary");ui.set_mode(UiMode::Synth);ui.begin_preset_save();ui.knob(2,1000);shot("preset-include-fx");ui.cancel_browser_edit();
 ui.set_mode(UiMode::Project);ui.soft_key(3);shot("save-as");ui.cancel_browser_edit();
 FILE* report=std::fopen("build/manual-review-probes.txt","w");if(!report)return 1;
 std::fprintf(report,"Current code review probes; host only, no hardware or file writes\nBrowser overwrite: queued=%d before_capture=%d after_capture=%d\nSampler ZONES SHIFT+encoder: accepted=%d (Warp dispatch reachable)\n",queued,before==FileAction::Capture,after==FileAction::Capture,warp);
 ui.set_mode(UiMode::Pad);p->tempo=99;shot("type-099");p->tempo=100;shot("type-100");
 // Catalogue uses exactly the shared icon primitive at native LCD resolution.
 if(themed){auto sheet=std::make_unique<SoftwareCanvas>();sheet->theme=ui::Theme::by_index(theme_index);for(unsigned side=0;side<2;++side)sheet->fill(side,0,0,480,272,sheet->theme.background);
  sheet->label(0,12,2,"ICON LANGUAGE / 14 PX",sheet->theme.secondary,ui::FontRole::Small);sheet->label(1,12,2,"ONE STROKE / THEME COLOR",sheet->theme.secondary,ui::FontRole::Small);
  static_assert(ui::icons::Count+2<=48,"catalogue grid capacity");
  for(unsigned n=0;n<ui::icons::Count+2;++n){unsigned id=n+1;bool active=id>=ui::icons::Count;if(active)id=id==ui::icons::Count?ui::icons::Play:id==ui::icons::Count+1?ui::icons::Stop:ui::icons::Record;unsigned col=n%6,side=col/3;int x=int(col%3)*160+10,y=26+int(n/6)*30;sheet->icon(side,x,y,id,sheet->theme.accent,active);sheet->label_band(side,x+20,y-2,130,18,active?(id==ui::icons::Play?"Play active":id==ui::icons::Stop?"Stop active":"Record active"):ui::icons::shapes[id].name,sheet->theme.text,ui::FontRole::Small);}
  FILE* file=std::fopen((frame_dir/"icons.ppm").c_str(),"wb");if(!file)return 1;std::fprintf(file,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t v=sheet->pixels[x/480][y*480+x%480];unsigned char rgb[]={uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};if(std::fwrite(rgb,1,3,file)!=3)return 1;}if(std::fclose(file))return 1;
 }
 int status=std::fclose(report)|std::fclose(controls);
 if(placements)status|=std::fclose(placements);
 if(frames)status|=std::fclose(frames);
 return status || !queued || before!=FileAction::Capture || after!=FileAction::Capture || !warp;
}
