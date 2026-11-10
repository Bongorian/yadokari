#pragma once
#include "clip_follow.h"
#include "ui/control_descriptor.h"
namespace groove {
enum class ClipControlAction:uint8_t { None,Copy,Double,Paste,Launch,Stop,Follow,NextPattern,Undo,Redo,Action,Cycles,Target,Probability,Variation };
constexpr ui::TopWork top_work(ClipControlAction a){switch(a){case ClipControlAction::None:return ui::TopWork::Empty;case ClipControlAction::Follow:case ClipControlAction::NextPattern:return ui::TopWork::Selection;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(ClipControlAction action){switch(action){case ClipControlAction::Variation:case ClipControlAction::Copy:return ui::icons::Copy;case ClipControlAction::Paste:return ui::icons::Paste;case ClipControlAction::Launch:return ui::icons::Play;case ClipControlAction::Stop:return ui::icons::Stop;case ClipControlAction::Follow:return ui::icons::Follow;case ClipControlAction::Undo:return ui::icons::Undo;case ClipControlAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using ClipControl=ui::ControlDescriptor<ClipControlAction>;
struct ClipControlContext {
 bool shift=false,follow_page=false,can_paste=false,can_double=false,can_undo=false,can_redo=false;
};
inline ClipControl clip_button_control(unsigned index,const ClipControlContext& c){
 switch(index){
  case 0:return c.shift?ClipControl{"DOUBLE TRACK",ClipControlAction::Double,c.can_double}:ClipControl{"COPY",ClipControlAction::Copy,true};
  case 1:return c.shift?ClipControl{"DUP + VAR",ClipControlAction::Variation,true}:ClipControl{"PASTE",ClipControlAction::Paste,c.can_paste};
  case 2:return {c.shift?"LAUNCH NOW":"LAUNCH",ClipControlAction::Launch,true};
  case 3:return {"STOP",ClipControlAction::Stop,true};
  case 4:return {c.follow_page?"DONE FOLLOW":"FOLLOW EDIT",ClipControlAction::Follow,true};
  case 5:return {"PAT NEXT",ClipControlAction::NextPattern,true};
  case 6:return {"UNDO",ClipControlAction::Undo,c.can_undo};
  case 7:return {"REDO",ClipControlAction::Redo,c.can_redo};
  default:return {};
 }
}
inline ClipControl clip_follow_knob_control(unsigned index){
 // Bases can be prepared while OFF; TARGET takes effect with Target action.
 switch(index){
  case 0:return {"ACTION",ClipControlAction::Action,true,0,4};
  case 1:return {"LOOPS",ClipControlAction::Cycles,true,1,64};
  case 2:return {"TARGET",ClipControlAction::Target,true,1,16};
  case 3:return {"CHANCE",ClipControlAction::Probability,true,0,100};
  default:return {};
 }
}
inline int clip_follow_control_value(const ClipFollow& f,ClipControlAction action){
 switch(action){
  case ClipControlAction::Action:return int(f.action);
  case ClipControlAction::Cycles:return f.cycles;
  case ClipControlAction::Target:return f.target+1;
  case ClipControlAction::Probability:return f.probability;
  default:return 0;
 }
}
inline bool edit_clip_follow_control(ClipFollow& f,const ClipControl& d,int delta){
 if(!d.enabled)return false;
 int value=d.adjusted(clip_follow_control_value(f,d.action),delta);
 switch(d.action){
  case ClipControlAction::Action:f.action=ClipFollowAction(value);break;
  case ClipControlAction::Cycles:f.cycles=uint8_t(value);break;
  case ClipControlAction::Target:f.target=uint8_t(value-1);break;
  case ClipControlAction::Probability:f.probability=uint8_t(value);break;
  default:return false;
 }
 return true;
}
}
