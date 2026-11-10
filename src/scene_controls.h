#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class SceneAction:uint8_t {None,Store,Follow,LaunchMode,Perform,ClipFollow,PatNext,Undo,Redo,Duplicate};
constexpr ui::TopWork top_work(SceneAction a){switch(a){case SceneAction::None:return ui::TopWork::Empty;case SceneAction::Follow:case SceneAction::LaunchMode:case SceneAction::ClipFollow:return ui::TopWork::Selection;case SceneAction::Perform:case SceneAction::PatNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(SceneAction action){switch(action){case SceneAction::Duplicate:return ui::icons::Copy;case SceneAction::Store:return ui::icons::Store;case SceneAction::Follow:return ui::icons::Follow;case SceneAction::ClipFollow:return ui::icons::Follow;case SceneAction::PatNext:return ui::icons::Forward;case SceneAction::Undo:return ui::icons::Undo;case SceneAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using SceneControl=ui::ControlDescriptor<SceneAction>;
inline SceneControl scene_button(unsigned index,bool launch_quantized,bool following,bool chain_enabled,unsigned history_cursor,unsigned history_size,bool shift=false){
 SceneControl d;
 switch(index){
 case 0:d=shift?SceneControl{"DUP SCENE",SceneAction::Duplicate,true}:SceneControl{"STORE SCENE",SceneAction::Store,true};break;
 case 1:d={"FOLLOW",SceneAction::Follow,true};d.selected=chain_enabled;break;
 case 2:d={launch_quantized?"QUANTIZED":"IMMEDIATE",SceneAction::LaunchMode,true};d.selected=launch_quantized;break;
 case 3:d={"PERFORM",SceneAction::Perform,true};break;
 case 4:d={"CLIP FOLLOW",SceneAction::ClipFollow,true};d.selected=following;break;
 case 5:d={"PAT NEXT",SceneAction::PatNext,true};break;
 case 6:d={"UNDO",SceneAction::Undo,history_cursor>0};break;
 case 7:d={"REDO",SceneAction::Redo,history_cursor<history_size};break;
 }
 return d;
}
}
