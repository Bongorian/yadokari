#include "performance.h"
#include "ui_renderer.h"
#include "software_canvas.h"
#include "ui/pad_feedback.h"
#include <cassert>
#include <memory>
#include <string>
#include <cstdio>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::PlaitsMacro;auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[128];e->render(audio,0);auto ui=std::make_unique<Performance>(*e,*p);
 assert(ui->set_lock(0,1,.4f,0));for(unsigned i=1;i<kLocks;++i)assert(ui->set_lock(0,uint16_t(i+2),.3f,i));e->render(audio,0);ui->poll();ui->set_mode(UiMode::Mixer);ui->set_shift(true);assert(ui->knob_parameter(0)==1 && ui->knob_parameter(1)==2);assert(ui->dispatch({ControllerInput::Type::KnobTouch,0,1}) && ui->dispatch({ControllerInput::Type::KnobTouch,1,1}));assert(ui->knob_reset_available(0) && ui->knob_reset_available(1));
 assert(!ui->reset_touched_knobs() && ui->notice()==UiNotice::PartialApplied && ui->notice_error());assert(std::string(ui->notice_text()).find("UNDO")!=std::string::npos);assert(track_at(*p,0,0).steps[0].locks[0].value==target_descriptor(1)->initial);e->render(audio,0);ui->poll();assert(ui->undo());e->render(audio,0);ui->poll();assert(track_at(*p,0,0).steps[0].locks[0].value==.4f);
 assert(ui->dispatch({ControllerInput::Type::KnobTouch,0,0}));assert(!ui->reset_touched_knobs() && ui->notice()!=UiNotice::PartialApplied);assert(ui->dispatch({ControllerInput::Type::KnobTouch,1,0}));ui->set_shift(false);
 // Replaying a grouped Undo can also stop after one successful command:
 // a task-owned pending Pattern blocks only the later track replacement.
 make_empty_project(*p);assert(e->stage_project(*p));e->render(audio,0);ui->project_loaded();assert(ui->set_instrument(Instrument::TapeMicrosound));e->render(audio,0);ui->poll();auto pending=std::make_unique<PatternState>(*p,0);assert(e->replace_pattern(0,*pending,2));assert(!ui->undo() && ui->notice()==UiNotice::PartialApplied);assert(p->tracks[0].instrument==Instrument::TapeMicrosound && p->tracks[0].engine_parameters[0]==.5f);e->render(audio,0);ui->poll();assert(ui->redo());e->render(audio,0);ui->poll();assert(p->tracks[0].engine_parameters[0]==.75f);assert(ui->undo());e->render(audio,0);ui->poll();assert(p->tracks[0].instrument==Instrument::Empty);
 // A syntactically present but unusable source reaches Granular's trigger
 // failure AFTER poly allocation. No zero-envelope owner may remain.
 make_empty_project(*p);p->tracks[0].instrument=Instrument::Granular;WavView views[kSampleAssets]{};views[0].frames=128;views[0].rate=48000;views[0].channels=1;views[0].bits=16;views[0].format=1;assert(e->stage_project(*p,views));e->render(audio,0);assert(e->submit({CommandType::Trigger,0,60,100}));e->render(audio,0);assert(e->poly_note_count(0)==0 && e->poly_owner_masks_valid());
 // Resonator's bounded pending-note queue rejects the seventeenth event.
 p->tracks[0].instrument=Instrument::Resonator;assert(e->stage_project(*p));e->render(audio,0);for(unsigned i=0;i<17;++i)assert(e->submit({CommandType::Trigger,0,60,100}));e->render(audio,0);assert(e->poly_owner_masks_valid());
 make_empty_project(*p);assert(e->stage_project(*p));e->render(audio,0);ui->project_loaded();ui->set_mode(UiMode::Synth);auto canvas=std::make_unique<UiRenderer>();std::string text;canvas->text_observer=[](const SoftwareCanvas::TextPlacement& value,void* owner){*static_cast<std::string*>(owner)+=std::string(value.text)+"\n";};canvas->text_observer_context=&text;auto engine_hint=[&]{std::string hint;for(unsigned i=0;i<canvas->page_advice.count;++i)hint+=std::string(canvas->page_advice.lines[i])+"\n";return hint.find("TOP 5")!=std::string::npos && hint.find("ENGINE")!=std::string::npos && hint.find("SOURCE")==std::string::npos;};canvas->draw(*p,*ui);assert(text.find("EMPTY TRACK")!=std::string::npos && engine_hint());
 assert(ui->set_instrument(Instrument::ExternalMidi));e->render(audio,0);ui->poll();text.clear();canvas->draw(*p,*ui);assert(text.find("MIDI OUT")!=std::string::npos && engine_hint());
 const unsigned expected[]={10,7,4,14,8,15,17};for(unsigned i=0;i<7;++i)assert(ui::PadPalette::ground_token({ui::EngineKind(i),false})==expected[i]);
 std::puts("Wave2 workflow: partial edit feedback+Undo, no-change failure, failed voice ownership, real Empty/MIDI guidance and fixed seven-kind palette passed");
}
