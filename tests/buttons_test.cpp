// Unit 20a: SPEC 2.7 button assignments (doors, LEDs, strip modes,
// update confirm). Drives the real control() byte stream.
#include "performance.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace groove;
static void leds(const Performance& perf, uint8_t out[62]) { perf.button_leds(out); }
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 assert(engine.stage_project(project));
 auto settle = [&]() { float buf[1024]{}; engine.render(buf, 512); perf.poll(); };
 uint8_t led[62];
 // CHANNEL: Channel group door - Routing first, FX as the group tab.
 perf.control(8, 0x01); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Routing && !perf.routing_fx_page());
 leds(perf, led); assert(led[0]);
 perf.control(8, 0x01); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Effects);
 leds(perf, led); assert(led[0]);
 perf.control(8, 0x01); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Routing);
 // ARRANGER: Song page (S2-3), its own LED.
 perf.control(8, 0x02); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Song);
 leds(perf, led); assert(led[2] && !led[51]);
 // BROWSER owns LED 4; FILE/Project owns LED 8.
 perf.control(8, 0x04); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Browser);
 leds(perf, led); assert(led[4] && !led[8]);
 perf.control(8, 0x10); perf.control(8, 0x00);
 assert(perf.mode() == UiMode::Project);
 leds(perf, led); assert(led[8] && !led[4]);
 // VARIATION is the declared Generate entry: surface byte 5, bit 1.
 perf.control(5,0x02);perf.control(5,0);assert(perf.mode()==UiMode::Generate);
 // Strip mode buttons: no page move, LEDs 25/26/28.
 UiMode before = perf.mode();
 perf.control(5, 0x40); perf.control(5, 0x00);
 assert(perf.strip_mode() == StripMode::Pitch && perf.mode() == before);
 leds(perf, led); assert(led[25] && !led[26] && !led[28]);
 perf.control(5, 0x80); perf.control(5, 0x00);
 assert(perf.strip_mode() == StripMode::Mod);
 leds(perf, led); assert(led[26]);
 perf.control(3, 0x01); perf.control(3, 0x00);
 assert(perf.strip_mode() == StripMode::Notes);
 leds(perf, led); assert(led[28]);
 // PERFORM: first subpage + StripMode=Perform.
 perf.control(6, 0x01); perf.control(6, 0x00);
 assert(perf.mode() == UiMode::Perform && perf.performance_page() == 0);
 assert(perf.strip_mode() == StripMode::Perform);
 // MACRO: direct landing on the Perform Macro subpage, LED 11.
 perf.control(7, 0x01); perf.control(7, 0x00);
 assert(perf.mode() == UiMode::Perform && perf.performance_page() == 1);
 leds(perf, led); assert(led[11] && led[27]);
 // Shift+MACRO no longer toggles the wave profile (moved to Synth).
 perf.control(1, 0x40);
 perf.control(7, 0x01); perf.control(7, 0x00);
 assert(perf.mode() == UiMode::Perform && perf.performance_page() == 1);
 perf.control(1, 0x00);
 // EVENTS (Wave 5 item 2): global Undo, SHIFT+EVENTS: Redo, on any
 // page; it no longer enters Step note edit.
 perf.set_mode(UiMode::Perform);
 perf.control(4, 0x80); perf.control(4, 0x00); // empty history: no-op
 assert(perf.mode() == UiMode::Perform && !perf.note_edit());
 perf.set_mode(UiMode::Step);
 perf.control(1, 0x40); // SHIFT held for the shifted soft key
 assert(std::strcmp(perf.soft_label(5), "REC: OVERDUB") == 0);
 assert(perf.soft_key(5)); // note_replace toggle: an undoable edit
 assert(std::strcmp(perf.soft_label(5), "REC: REPLACE") == 0);
 perf.control(1, 0x00);
 perf.control(4, 0x80); perf.control(4, 0x00); // EVENTS: Undo
 assert(perf.mode() == UiMode::Step && !perf.note_edit());
 perf.control(1, 0x40);
 assert(std::strcmp(perf.soft_label(5), "REC: OVERDUB") == 0);
 perf.control(4, 0x80); perf.control(4, 0x00); // SHIFT+EVENTS: Redo
 assert(std::strcmp(perf.soft_label(5), "REC: REPLACE") == 0);
 perf.control(1, 0x00);
 // STEP long-press: note edit door; EVENTS LED now follows Undo availability.
 // a plain STEP press clears it again.
 perf.control(4, 0x08); // STEP down: plain press first
 assert(perf.mode() == UiMode::Step && !perf.note_edit());
 perf.animate(1000); // held past the 400 ms long-press threshold
 assert(perf.note_edit());
 leds(perf, led); assert(led[52]);
 perf.control(4, 0x00); // release
 perf.control(4, 0x08); perf.control(4, 0x00);
 assert(perf.mode() == UiMode::Step && !perf.note_edit());
 leds(perf, led); assert(led[49]);
 // FOLLOW button: display follow, LED 40; clip follow untouched and
 // now toggled from the Scene CLIP FOLLOW soft key.
 bool clip_before = true; // following_clip_ defaults to true
 perf.control(6, 0x10); perf.control(6, 0x00);
 assert(perf.follow_display());
 leds(perf, led); assert(led[40]);
 perf.set_mode(UiMode::Scene);
 assert(std::strcmp(perf.soft_label(2), "CLIP FOLLOW") == 0);
 assert(perf.soft_selected(2) == clip_before);
 assert(perf.soft_key(2));
 assert(perf.soft_selected(2) == false);
 // REC shift: count-in record start (arm + playing), no Perform jump.
 perf.set_mode(UiMode::Pad);
 perf.control(1, 0x40);
 perf.control(6, 0x40); perf.control(6, 0x00);
 assert(perf.note_recording() && perf.mode() == UiMode::Pad);
 perf.control(1, 0x00);
 perf.stop(); settle();
 // RESTART: transport Start (returns true, keeps mode).
 assert(perf.restart_transport());
 perf.stop(); settle();
 // SETTINGS shift: update goes through MDL-CONFIRM, never direct.
 perf.control(1, 0x40);
 perf.control(7, 0x02); perf.control(7, 0x00);
 assert(perf.update_confirm_pending() && perf.mode() == UiMode::Settings);
 assert(!perf.take_update_request());
 assert(std::strcmp(perf.soft_label(0), "CONFIRM") == 0);
 assert(perf.soft_key(1)); // CANCEL
 assert(!perf.update_confirm_pending() && !perf.take_update_request());
 perf.control(1, 0x40);
 perf.control(7, 0x02); perf.control(7, 0x00);
 perf.control(1, 0x00);
 assert(perf.update_confirm_pending());
 assert(perf.encoder_push()); // CONFIRM (stopped)
 assert(perf.take_update_request());
 // While playing, shift+SETTINGS refuses outright.
 assert(perf.play());
 perf.control(1, 0x40);
 perf.control(7, 0x02); perf.control(7, 0x00);
 perf.control(1, 0x00);
 assert(!perf.update_confirm_pending() && !perf.take_update_request());
 perf.stop(); settle();
 // W5 profile remains available on SHIFT+Top4; common verbs use fixed slots.
 perf.select_track(0);assert(perf.set_instrument(Instrument::ExternalMidi));perf.set_mode(UiMode::Synth);perf.set_shift(true);
 assert(std::strcmp(perf.soft_label(3),"WAVE PROFILE")==0);assert(perf.soft_key(3) && perf.wave_profile());assert(perf.soft_key(7) && !perf.wave_profile());perf.set_shift(false);
 for(unsigned engine_index=0;engine_index<instrument_count();++engine_index){SynthControlContext c;c.can_recall=true;c.instrument=Instrument(engine_index);assert(std::strcmp(synth_button_control(4,c).label,"ENGINE")==0);assert(std::strcmp(synth_button_control(5,c).label,"MODULATION")==0);assert(synth_button_control(6,c).action==SynthAction::StoreSound);assert(synth_button_control(7,c).action==SynthAction::RecallSound);}
 // W5 STEP short taps cycle; entering STEP is immediate.
 perf.set_mode(UiMode::Pad);perf.control(4,8);perf.control(4,0);
 // Reset through the existing alias so this case owns its starting view.
 while(perf.tracker_view() || perf.step_overview()){perf.set_shift(true);perf.soft_key(3);perf.set_shift(false);}
 perf.control(4,8);perf.control(4,0);assert(perf.step_overview());
 perf.control(4,8);perf.control(4,0);assert(perf.tracker_view());
 perf.control(4,8);perf.control(4,0);assert(!perf.tracker_view() && !perf.step_overview());
 // W5 A-H: sticky bank, shared held modifiers, and momentary Mixer peek.
 perf.set_mode(UiMode::Synth);perf.set_shift(true);perf.control(2,1);perf.control(2,0);perf.set_shift(false);
 assert(perf.track_bank()==1 && perf.selected_track()==8);
 perf.control(2,2);perf.control(2,0);assert(perf.selected_track()==9);
 perf.control(5,0x20);perf.control(2,1);perf.control(2,0);perf.control(5,0);assert(track_muted_global(project,8));
 perf.control(5,0x10);perf.control(2,2);perf.control(2,0);perf.control(5,0);assert(track_solo(project,9));
 perf.control(5,8);perf.control(2,4);perf.control(2,0);perf.control(5,0);assert(perf.mode()==UiMode::Clip && perf.selected_track()==10);
 perf.set_mode(UiMode::Synth);perf.control(2,1);perf.animate(perf.ui_time()+401);assert(perf.mode()==UiMode::Mixer);perf.control(2,0);assert(perf.mode()==UiMode::Synth);
 // W5 descriptor-backed reset is one undoable edit; slow crossings snap.
 perf.set_mode(UiMode::Mixer);assert(perf.set_parameter(2,.31f));
 perf.control(10,0x40);assert(perf.knob_reset_available(1));perf.control(6,4);perf.control(6,0);perf.control(10,0);assert(perf.knob_value(1)==0);assert(perf.undo());assert(fabsf(perf.knob_value(1)-.31f)<.0001f);
 perf.set_parameter(2,-.01f);perf.animate(perf.ui_time()+100);assert(perf.knob(1,1000));assert(perf.knob_value(1)==0);
 perf.set_parameter(2,-.01f);assert(perf.knob(1,2000));assert(perf.knob_value(1)>0);
 perf.set_mode(UiMode::Step);auto timed=track_at(project,project.active_pattern,perf.selected_track());timed.steps[perf.selected_step()].microtiming=-2;perf.animate(perf.ui_time()+100);assert(perf.knob(6,1000) && timed.steps[perf.selected_step()].microtiming==0);timed.steps[perf.selected_step()].microtiming=-2;assert(perf.knob(6,2000) && timed.steps[perf.selected_step()].microtiming>0);
 // W5 shortcut palette comes from theme tokens; bank survives SHIFT release.
 for(unsigned theme=0;theme<ui::Theme::kCount;++theme){const auto& colors=ui::Theme::by_index(theme);auto low=ui::LedStateEngine::shortcut(colors,false,false,false,ui::LedState::Off,0,0,ui::Motion::Off),high=ui::LedStateEngine::shortcut(colors,true,false,false,ui::LedState::Off,0,0,ui::Motion::Off);assert((low&3)==1 && (high&3)==2);assert((low>>2)==ui::LedStateEngine::token_hue(colors.secondary,colors));for(unsigned brightness=1;brightness<3;++brightness){auto dim=ui::LedStateEngine::shortcut(colors,true,false,false,ui::LedState::Off,0,0,ui::Motion::Off,brightness);assert((dim>>2)==(high>>2) && (dim&3)==brightness);}}
 // W5 quantized restart is owned by the DSP clock, not UI frame time.
 {Engine clocked(48000);auto* score=new ProjectState();make_empty_project(*score);score->tempo=120;assert(clocked.stage_project(*score));float pcm[1024]{};
 auto render=[&](unsigned frames){while(frames){unsigned n=frames<512?frames:512;clocked.render(pcm,n);frames-=n;}};
 render(512);clocked.submit({CommandType::Start,0,0,0});render(24000);ClockEvent ev;uint64_t initial=0;while(clocked.pop_clock(ev))if(ev.type==ClockEventType::Start)initial=ev.sample;
 clocked.submit({CommandType::Start,0,0,1});render(71000);while(clocked.pop_clock(ev))assert(ev.type!=ClockEventType::Start);
 render(2000);uint64_t restarted=0;while(clocked.pop_clock(ev))if(ev.type==ClockEventType::Start)restarted=ev.sample;assert(restarted-initial>=96000 && restarted-initial<=96001);delete score;}
 // Drain the file request queued by the sound-door/previous-button audit;
 // the real file-service owner consumes it between input frames.
 perf.take_file_action();
 // W5 Modal owns navigation and Undo; 4-D confirms and left cancels.
 perf.set_mode(UiMode::Project);perf.request_project(Performance::ProjectAction::New);assert(perf.modal_active());perf.control(4,0x80);perf.control(4,0);assert(perf.pending_project()!=Performance::ProjectAction::None);
 perf.control(1,0x20);perf.control(1,0);assert(!perf.modal_active());
 perf.request_project(Performance::ProjectAction::New);assert(perf.encoder_push());assert(!perf.modal_active());
 // W5 canonical doors are identical from any ordinary page.
 for(unsigned mode=0;mode<=unsigned(UiMode::Library);++mode){perf.set_mode(UiMode(mode));perf.control(6,1);perf.control(6,0);assert(perf.mode()==UiMode::Perform && perf.performance_page()==0);assert(perf.soft_key(7));assert(perf.audio_page());perf.control(7,2);perf.control(7,0);while(perf.settings_page()<6)assert(perf.encoder_direction(0x08));assert(perf.soft_key(6));assert(perf.mode()==UiMode::Diagnostics);}
 std::printf("buttons (2.7 doors/LEDs/strip/confirm) test passed\n");
 return 0;
}
