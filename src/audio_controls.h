#pragma once
#include "audio_lane.h"
#include "ui/control_descriptor.h"
namespace groove {
enum class AudioAction:uint8_t {None,Looper,Audio,Record,Overdub,Duplicate,Toggle,Delete,Clear,Song,Undo,Back,Instrument};
constexpr ui::TopWork top_work(AudioAction a){switch(a){case AudioAction::None:return ui::TopWork::Empty;case AudioAction::Looper:case AudioAction::Audio:case AudioAction::Instrument:return ui::TopWork::Selection;case AudioAction::Song:case AudioAction::Back:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(AudioAction action){switch(action){case AudioAction::Audio:return ui::icons::Audio;case AudioAction::Record:return ui::icons::Record;case AudioAction::Duplicate:return ui::icons::Copy;case AudioAction::Delete:return ui::icons::Erase;case AudioAction::Clear:return ui::icons::Erase;case AudioAction::Undo:return ui::icons::Undo;case AudioAction::Back:return ui::icons::Back;case AudioAction::Instrument:return ui::icons::Synth;default:return ui::icons::None;}}

using AudioControl=ui::ControlDescriptor<AudioAction>;
inline AudioControl audio_button(unsigned index,TrackKind kind,bool shift,bool busy,bool undo,bool recording=false){
 const bool loop=kind==TrackKind::Looper;
 switch(index){
 case 0:return {shift?"INSTRUMENT":"LOOPER",shift?AudioAction::Instrument:AudioAction::Looper,!busy};
 case 1:return {shift?"SONG":"AUDIO",shift?AudioAction::Song:AudioAction::Audio,!busy};
 case 2:return {recording?"STOP RECORD":"RECORD",AudioAction::Record,recording || (!busy && kind!=TrackKind::Instrument)};
 case 3:return {loop?"OVERDUB":"DUPLICATE",loop?AudioAction::Overdub:AudioAction::Duplicate,!busy && kind!=TrackKind::Instrument};
 case 4:return {loop?"PLAY / STOP":"DELETE CLIP",loop?AudioAction::Toggle:AudioAction::Delete,!busy && kind!=TrackKind::Instrument};
 case 5:return {"CLEAR",AudioAction::Clear,!busy && kind!=TrackKind::Instrument};
 case 6:return {"TAKE UNDO",AudioAction::Undo,!busy && undo};
 case 7:return {"BACK",AudioAction::Back,true};default:return {};
 }
}
inline const char* audio_knob_name(unsigned i,TrackKind kind){static const char* loop[]={"TRACK","INPUT","LENGTH","BARS","FEEDBACK","LEVEL","PAN",""};static const char* audio[]={"TRACK","CLIP","START BAR","TRIM IN s","TRIM END s","CLIP GAIN","PAN","INPUT"};return i<8?(kind==TrackKind::Audio?audio[i]:loop[i]):"";}
}
