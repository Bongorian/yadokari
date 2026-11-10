#include "performance.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::DaisyAnalogDrums;
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*p));float out[2]{};engine->render(out,0);
 Performance ui(*engine,*p);
 assert(ui.dispatch({ControllerInput::Type::MuteModifier,0,1}));assert(ui.dispatch({ControllerInput::Type::SoloModifier,0,1}));
 ui.set_mode(UiMode::Project);ui.request_project(Performance::ProjectAction::New);assert(ui.modal_active());
 assert(ui.dispatch({ControllerInput::Type::MuteModifier,0,0}) && !ui.mute_modifier());assert(ui.dispatch({ControllerInput::Type::SoloModifier,0,0}) && !ui.solo_modifier());
 auto selection=ui.selected_track();auto pattern=p->active_pattern;
 assert(!ui.encoder(1) && !ui.encoder_direction(0x08));
 using T=ControllerInput::Type;
 assert(!ui.dispatch({T::Track,1}) && !ui.dispatch({T::Mode,0,int(UiMode::Synth)}));
 assert(!ui.dispatch({T::Undo}) && !ui.dispatch({T::Redo}) && !ui.dispatch({T::Play}));
 assert(ui.selected_track()==selection && p->active_pattern==pattern && ui.mode()==UiMode::Project && !ui.playing());
 assert(ui.encoder_direction(0x20) && !ui.modal_active());
 // Release during a confirmation must restore Peek only after the owner closes.
 ui.set_mode(UiMode::Synth);assert(ui.control(2,2));ui.animate(ui.ui_time()+401);assert(ui.mode()==UiMode::Mixer);
 ui.request_project(Performance::ProjectAction::New);assert(ui.modal_active());assert(ui.control(2,0));ui.animate(ui.ui_time()+500);
 assert(ui.modal_active() && ui.mode()==UiMode::Project);assert(ui.encoder_direction(0x20));ui.animate(ui.ui_time()+1);
 assert(ui.mode()==UiMode::Synth && ui.selected_track()==1);
 // A modal must cancel the pending STEP gesture, including its release action.
 ui.set_mode(UiMode::Step);auto step_page=ui.navigation().key;
 ui.control(4,8);ui.request_project(Performance::ProjectAction::New);assert(ui.modal_active());ui.animate(ui.ui_time()+500);
 assert(!ui.note_edit());ui.encoder_direction(0x20);ui.set_mode(UiMode::Step);ui.control(4,0);assert(ui.navigation().key==step_page);
 // FOLLOW EDIT is a real subpage in history and HOME/RETURN.
 ui.set_mode(UiMode::Clip);assert(ui.soft_key(0) && ui.clip_follow_page());ui.set_focus(Focus::Step);
 assert(ui.page_back() && !ui.clip_follow_page());assert(ui.page_forward() && ui.clip_follow_page());
 ui.set_focus(Focus::Step);assert(ui.home_or_return() && ui.mode()==UiMode::Pad);
 assert(ui.home_or_return() && ui.clip_follow_page() && ui.axes().focus==Focus::Step);
 ui.project_loaded();assert(ui.axes().history.empty() && !ui.modal_active() && ui.track_bank()==0);
 // Center snap follows the same declaration for base and lock edits.
 ui.set_mode(UiMode::Mixer);ui.set_shift(true);assert(ui.set_lock(0,2,-.01f));
 ui.animate(ui.ui_time()+100);assert(ui.knob(1,1000));bool centered=false;
 for(const auto& lock:track_at(*p,0,0).steps[0].locks)if(lock.enabled && lock.id==2)centered=lock.value==0;
 assert(centered);ui.set_shift(false);
 // Kit mix defaults edit the pad, including with SHIFT held; no stray step lock.
 assert(ui.set_instrument(Instrument::DrumKit));ui.set_mode(UiMode::Pad);assert(ui.select_kit_pad(0));
 assert(ui.knob_parameter(3)==kit_mix_parameter_id(0,1));assert(ui.knob(3,2000));float pan=p->tracks[0].kit.pads[0].pan;assert(pan>0);
 ui.set_shift(true);assert(ui.dispatch({T::KnobTouch,3,1}));assert(ui.control(6,4));ui.control(6,0);ui.dispatch({T::KnobTouch,3,0});ui.set_shift(false);
 assert(p->tracks[0].kit.pads[0].pan==0);assert(ui.undo());assert(p->tracks[0].kit.pads[0].pan==pan);
 ui.set_mode(UiMode::Synth);assert(ui.soft_key(4) && ui.source_select() && !std::strcmp(ui.soft_label(4),"LOAD ENGINE"));assert(ui.soft_key(7) && !ui.source_select());
 // No-op assignments for another modal cannot erase the Browser's axis metadata.
 ui.set_mode(UiMode::Browser);auto& b=ui.browser();b.count=1;b.cursor=0;copy_text(b.entries[0].name,96,"fixture.wav");
 assert(ui.begin_browser_edit(BrowserOperation::Rename));assert(ui.axes().modal==ModalKind::Rename);
 ui.set_wave_profile(false);assert(ui.axes().modal==ModalKind::Rename);assert(ui.encoder_direction(0x20));
 for(unsigned theme=0;theme<ui::Theme::kCount;++theme){
  const auto& color=ui::Theme::by_index(theme);
  auto led=[&](bool selected,bool muted,ui::LedState state,uint64_t ms){return ui::LedStateEngine::shortcut(color,false,selected,muted,state,0,ms,ui::Motion::Full);};
  assert((led(false,false,ui::LedState::Off,0)>>2)!=(led(true,false,ui::LedState::Off,0)>>2));
  assert((led(true,true,ui::LedState::Off,0)>>2)!=(led(true,false,ui::LedState::Off,0)>>2));
  assert(led(true,false,ui::LedState::Recording,0)==led(true,false,ui::LedState::Recording,750));
  assert(led(true,false,ui::LedState::Finalizing,0)==led(true,false,ui::LedState::Finalizing,750));
  assert(led(true,false,ui::LedState::Armed,0)!=led(true,false,ui::LedState::Armed,750));
 }
 for(unsigned mode=0;mode<=unsigned(UiMode::Library);++mode){ui.set_mode(UiMode(mode));for(unsigned i=0;i<8;++i)if(!ui.soft_enabled(i))assert(!ui.soft_key(i));}
 std::puts("Wave5 self-review: modal isolation, gestures, history, reset/snap, Top and six theme LEDs passed");
}
