#include "performance.h"
#include "midi_input_router.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <memory>
using namespace groove;
using ui::LedState;
static void live_surface_regressions(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 project->clips[0]={0,true};
 track_at(*project,0,0).instrument=Instrument::PolySaw;
 track_at(*project,0,0).length=64;
 auto engine=std::make_unique<Engine>(8000);
 assert(engine->stage_project(*project));
 auto ui=std::make_unique<Performance>(*engine,*project);
 auto flush=[&](unsigned frames=1){
  while(frames){
   float audio[128]{};
   const unsigned count=frames<64?frames:64;
   engine->render(audio,count);ui->poll();frames-=count;
  }
 };
 flush();
 assert(ui->play());flush();
 assert(ui->select_pattern(1));ui->set_mode(UiMode::Step);ui->step_page(0);
 assert(ui->control(6,0x10) && ui->control(6,0) && ui->follow_display());
 flush(19300);
 assert(ui->page()==0);
 assert(ui->pad(12,0x10,3000) && ui->pad(12,0x20,0) && ui->selected_step()==0);
 ui->set_mode(UiMode::Perform);
 assert(ui->soft_key(6) && ui->performance_page()==4);
 assert(!std::strcmp(ui->knob_label(3),"SONG FX TYPE") && !std::strcmp(ui->knob_label(4),"SONG FX AMT"));
 ui->set_shift(true);
 assert(ui->pad(14,0x10,3000) && ui->pad(14,0x20,0));
 ui->set_shift(false);flush();
 assert(ui->performance_fx_latched() && engine->performance_fx_current()!=PerformanceFx::Off);
 assert(ui->soft_key(7) && ui->performance_page()==5);flush();
 assert(!ui->performance_fx_latched() && engine->performance_fx_current()==PerformanceFx::Off);
 assert(ui->control(6,1) && ui->control(6,0) && ui->soft_key(6));
 assert(ui->set_performance_fx(PerformanceFx::Gate,.75f,true));flush();
 assert(ui->performance_fx_latched() && engine->performance_fx_current()!=PerformanceFx::Off);
 assert(ui->set_control_all(true));flush();
 assert(ui->on_perform_page(PerformMorph) && !ui->performance_fx_latched() && engine->performance_fx_current()==PerformanceFx::Off);
 assert(ui->set_control_all(false));
 assert(ui->launch_scene(2,true));flush();
 assert(ui->pending_scene()==2);
 MidiInputRouter router;
 assert(router.accept(*engine,*project,{0,0,1,{0xfc}}));flush();
 assert(ui->pending_scene()<0 && !engine->diagnostic_playing());
 assert(ui->play());flush();
 assert(ui->set_repeat_latch(true) && ui->control(3,0x10) && ui->control(3,0));
 flush();
 assert(ui->repeat_latch() && ui->note_repeat_mode());
 for(unsigned space=engine->command_space();space;--space)assert(engine->submit({CommandType::SetFill,0,0,0}));
 assert(ui->control(6,0x80) && ui->control(6,0));flush();
 assert(!ui->note_repeat_mode() && !ui->repeat_latch() && !engine->diagnostic_playing());

 project->song.enabled=true;project->song.length=1;
 auto& section=project->song.sections[0];
 section.ticks=kSongTicksPerBar;section.playing_mask=1;section.mute_mask=1;section.restart_mask=0xffff;
 assert(engine->stage_project(*project));
 ui=std::make_unique<Performance>(*engine,*project);flush();
 assert(ui->play());flush();
 ui->set_mode(UiMode::Mixer);
 assert(ui->audible_muted(0) && ui->pad_feedback(0)==LedState::Muted);
}
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*project));
 float audio[128]{};engine->render(audio,0);Performance ui(*engine,*project);
 auto flush=[&]{engine->render(audio,64);ui.poll();};
 ui::LedFrame frame;
 auto pad_state=[&](unsigned logical){ui.led_frame(frame);return frame.pad_state[mk3::pad_logical(logical)];};
 assert(ui.begin_project_save_as());assert(!ui.home_or_return() && ui.browser_editing());ui.cancel_browser_edit();
 // Library is the final supported controller mode. Invalid values remain rejected.
 assert(ui.dispatch({ControllerInput::Type::Mode,0,int(UiMode::Song)}));assert(ui.mode()==UiMode::Song);
 assert(!ui.dispatch({ControllerInput::Type::Mode,0,int(UiMode::Library)+1}));
 ui.set_mode(UiMode::Step);ui.set_mode(UiMode::Keyboard);
 assert(ui.page_back() && ui.pad_mode()==PadMode::Step);
 assert(ui.page_forward() && ui.pad_mode()==PadMode::Keyboard);
 // Audio roles do not divert explicitly requested editing pages.
 project->audio[0].kind=TrackKind::Looper;ui.set_mode(UiMode::Step);
 assert(!ui.audio_page() && ui.mode()==UiMode::Step && ui.axes().history.current().mode==uint8_t(UiMode::Step));
 assert(ui.pad_mode()==PadMode::Step);
 project->audio[0].kind=TrackKind::Instrument;
 // Back/RETURN also honor a role changed since the history/ticket was made.
 ui.set_mode(UiMode::Step);ui.set_mode(UiMode::Synth);
 project->audio[0].kind=TrackKind::Looper;assert(ui.page_back());
 assert(!ui.audio_page() && ui.mode()==UiMode::Step && ui.axes().history.current().mode==uint8_t(UiMode::Step));
 project->audio[0].kind=TrackKind::Instrument;ui.set_mode(UiMode::Synth);
 assert(ui.home_or_return());project->audio[0].kind=TrackKind::Looper;
 assert(ui.home_or_return() && !ui.audio_page() && ui.mode()==UiMode::Synth);project->audio[0].kind=TrackKind::Instrument;
 // SETTINGS: arrows page categories, turn/up/down selects within the category.
 ui.set_mode(UiMode::Settings);ui.soft_key(0);ui.led_frame(frame);
 assert(!frame.buttons[59] && frame.buttons[60]);
 const uint8_t arrow=frame.buttons[60];
 auto device=ui.device_settings();assert(device.led==3 && frame.buttons[9]==0x7f);
 device.led=1;assert(ui.set_device_settings(device));ui.led_frame(frame);
 assert(frame.buttons[9]==0x7f/3 && frame.buttons[60]==uint8_t((arrow&~3u)|1));
 ui.set_mode(UiMode::Diagnostics);assert(ui.soft_key(4) && ui.diagnostic_test()==1);
 ui.led_frame(frame);
 for(unsigned i:{29u,30u,58u,59u})if(frame.buttons[i])assert(frame.buttons[i]==mk3::indexed(17,1));
 assert(frame.buttons[29] || frame.buttons[30]);assert(frame.buttons[58] || frame.buttons[59]);
 ui.set_mode(UiMode::Settings);
 device.led=3;assert(ui.set_device_settings(device));
 assert(ui.encoder_direction(0x08) && ui.settings_page()==1);
 assert(ui.encoder_direction(0x08) && ui.settings_page()==2);
 for(unsigned page=3;page<7;++page)assert(ui.encoder_direction(0x08) && ui.settings_page()==page);
 ui.led_frame(frame);assert(frame.buttons[59] && !frame.buttons[60]);
 assert(!frame.buttons[58] && !frame.buttons[61]);
 // Every synth stays in SYNTH when paging; actions/other modes use Top keys.
 for(unsigned id=0;id<instrument_count();++id){
  ui.set_mode(UiMode::Synth);assert(ui.set_instrument(Instrument(id)));flush();
  assert(ui.set_wave_profile(false));assert(ui.soft_key(0));
  for(unsigned n=0;n<5;++n){assert(ui.encoder_direction(0x08));assert(ui.mode()==UiMode::Synth);}
  ui.led_frame(frame);assert(!frame.buttons[60]);
  for(unsigned n=0;n<5;++n){assert(ui.encoder_direction(0x20));assert(ui.mode()==UiMode::Synth);}
  assert(ui.instrument_view()==0);ui.led_frame(frame);assert(!frame.buttons[59]);
 }
 // A held pad cannot mask mute or recording; playhead is visible at the edit cursor.
 ui::LedIntent intent{LedState::Editing,8};ui::LedStateEngine::overlay(intent,LedState::Playing,8);
 assert(intent.state==LedState::Playing);ui::LedStateEngine::overlay(intent,LedState::Muted);
 ui::LedStateEngine::overlay(intent,LedState::Triggered);assert(intent.state==LedState::Muted);
 ui::LedStateEngine::overlay(intent,LedState::Recording);ui::LedStateEngine::overlay(intent,LedState::Triggered);
 assert(intent.state==LedState::Recording);
 assert(ui.set_instrument(Instrument::DaisyAnalogDrums));ui.set_mode(UiMode::Clip);flush();ui.stop();flush();
 assert(pad_state(project->clips[0].pattern)==LedState::Selected);
 // Queued/armed states remain readable with animation off. Reduced warns at 1 Hz.
 const ui::LedIntent warning{LedState::Warning,1};
 assert(ui::LedStateEngine::encode(warning,0,0,ui::Motion::Off)==ui::LedStateEngine::encode(warning,.8f,750,ui::Motion::Off));
 assert(ui::LedStateEngine::encode(warning,0,0,ui::Motion::Reduced)==ui::LedStateEngine::encode(warning,0,250,ui::Motion::Reduced));
 assert(ui::LedStateEngine::encode(warning,0,0,ui::Motion::Full)!=ui::LedStateEngine::encode(warning,0,250,ui::Motion::Full));
 // Audio recording remains on its frozen destination after selecting another track.
 ui.set_mode(UiMode::Perform);for(unsigned n=0;n<5;++n)ui.encoder_direction(0x08);
 RecordingTarget target=ui.current_recording_target();ui.live_recording(true,0,&target);
 assert(ui.recording_led_state()==LedState::Armed);assert(ui.select_track(1));
 assert(pad_state(0)==LedState::Armed && pad_state(1)!=LedState::Armed);
 assert(engine->start_live_record(false,0,0));flush();assert(ui.recording_led_state()==LedState::Recording);
 assert(pad_state(0)==LedState::Recording);ui.led_frame(frame);
 assert(frame.buttons[29]==mk3::indexed(1,3));
 assert(engine->stop_live_record());flush();assert(ui.recording_led_state()==LedState::Finalizing);
 ui.live_recording(false);engine->finish_live_record();
 // Audio-track MUTE/SOLO use track states even if its retained source is a Kit.
 assert(ui.select_track(0));assert(ui.set_instrument(Instrument::DrumKit));flush();
 assert(ui.audio_page());track_at(*project,0,0).kit.pads[1].muted=true;
 ui.control(5,0x20);assert(pad_state(1)==LedState::Assigned);ui.control(5,0);
 track_at(*project,0,0).kit.solo_mask=2;
 ui.control(5,0x10);assert(pad_state(1)==LedState::Idle);ui.control(5,0);
 // Track identity is stable; selected and muted vary brightness. Activity uses motion.
 assert(ui.select_track(0));assert(ui.set_instrument(Instrument::DaisyAnalogDrums));flush();
 ui.set_mode(UiMode::Routing);assert(ui.pad(13,0x10,2000));ui.led_frame(frame);
 assert(frame.pads[13]==ui::PadPalette::encode(LedState::Selected,ui.ui_time(),ui.visual_settings().motion,ui::Theme::by_index(ui.visual_settings().theme),ui::PadGround{ui.selected_engine_kind()}));assert(ui.pad(13,0x20,0));
 ui.set_mode(UiMode::Settings);assert(!ui.pad(3,0x10,2000));ui.led_frame(frame);
 assert(!frame.pads[3]);assert(ui.pad(3,0x20,0));
 ui.set_mode(UiMode::Step);assert(ui.pad(12,0x10,2000));ui.led_frame(frame);
 assert(frame.pads[12]==ui::PadPalette::encode(LedState::Selected,ui.ui_time(),ui.visual_settings().motion,ui::Theme::by_index(ui.visual_settings().theme),ui::PadGround{ui.selected_engine_kind()}));assert(ui.pad(12,0x20,0));
 ui.set_mode(UiMode::Pad);assert(ui.pad(12,0x10,2000));ui.led_frame(frame);
 assert(frame.pads[12]==mk3::indexed(ui::PadPalette::active,3));
 ui.set_mode(UiMode::Browser);ui.browser().count=0;ui.led_frame(frame);assert(!frame.pads[12]);
 assert(ui.pad(12,0x20,0));flush();ui.animate(2000);ui.set_mode(UiMode::Pad);ui.led_frame(frame);
 assert(frame.pads[12]!=mk3::indexed(ui::PadPalette::active,3));
 ui.modifier(0x20,true);assert(ui.pad(12,0x10,2000));ui.led_frame(frame);assert(frame.pad_state[12]==LedState::Muted && (frame.pads[12]&3)==1);
 assert(ui.pad(12,0x20,0));assert(ui.pad(12,0x10,2000));assert(ui.pad(12,0x20,0));ui.modifier(0x20,false);flush();
 // An automatic track trigger is bounded, and cannot flash a Kit pad/file.
 EngineEvent note{};note.type=EngineEventType::Note;note.track=0;note.generation=engine->staged_generation();
 ui.accept_event(note);ui.led_frame(frame);assert(frame.pads[12]==mk3::indexed(ui::PadPalette::active,3));
 ui.animate(2000+ui::PadPalette::trigger_ms);ui.led_frame(frame);assert(frame.pads[12]!=mk3::indexed(ui::PadPalette::active,3));
 assert(ui.set_instrument(Instrument::DrumKit));flush();ui.accept_event(note);ui.led_frame(frame);
 assert(frame.pads[12]!=mk3::indexed(ui::PadPalette::active,3));
 // Reduced suppresses event flashes. Off keeps state changes but no pulses.
 for(auto motion:{ui::Motion::Full,ui::Motion::Reduced,ui::Motion::Off}){
  assert(!ui::PadPalette::encode(LedState::Idle,0,motion));
  assert((ui::PadPalette::encode(LedState::MutedBoth,0,motion)&3)==1);
  assert(ui::PadPalette::encode(LedState::Recording,0,motion)==ui::PadPalette::encode(LedState::Recording,750,motion));
 }
 assert(ui::PadPalette::encode(LedState::Queued,0,ui::Motion::Full)!=ui::PadPalette::encode(LedState::Queued,500,ui::Motion::Full));
 assert(ui::PadPalette::encode(LedState::Queued,0,ui::Motion::Off)==ui::PadPalette::encode(LedState::Queued,500,ui::Motion::Off));
 // Full keeps a very short accepted tap visible for 120 ms without holding
 // the note. At expiry the Keyboard root returns to its 100% ground state.
 ui.set_mode(UiMode::Keyboard);ui.animate(2400);assert(ui.pad(12,0x10,2000));assert(ui.pad(12,0x20,0));ui.led_frame(frame);
 assert(frame.pads[12]==mk3::indexed(ui::PadPalette::active,3));
 ui.animate(2520);ui.led_frame(frame);assert(frame.pads[12]==ui::PadPalette::encode(LedState::Selected,ui.ui_time(),ui.visual_settings().motion,ui::Theme::by_index(ui.visual_settings().theme),ui::PadGround{ui.selected_engine_kind()}));
 // Motion Off stops autonomous strip motion; Reduced retains just a beat marker.
 ui.set_mode(UiMode::Settings);for(unsigned n=0;n<6 && ui.settings_page();++n)assert(ui.encoder_direction(0x20));assert(ui.settings_page()==0);assert(ui.knob(0,-2000));
 assert(ui.visual_settings().motion==ui::Motion::Off);assert(ui.play());flush();ui.animate(1000);ui.led_frame(frame);
 auto still=frame;ui.animate(1060);ui.led_frame(frame);
 for(unsigned i=0;i<25;++i)assert(frame.strip_level[i]==still.strip_level[i]);
 assert(ui.knob(0,1000));ui.led_frame(frame);unsigned lit=0;
 for(auto level:frame.strip_level)lit+=level!=0;
 assert(lit==1);
 live_surface_regressions();
 std::puts("UI navigation, recording ownership, LED priority and motion passed");
}
