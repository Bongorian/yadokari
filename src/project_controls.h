#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class ProjectTopAction:uint8_t {None,Save,Open,New,SaveAs,Confirm,Cancel,MidiRecord,InternalRec,Collect,Files,Undo,Redo};
constexpr ui::TopWork top_work(ProjectTopAction a){switch(a){case ProjectTopAction::None:return ui::TopWork::Empty;case ProjectTopAction::Files:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(ProjectTopAction action){switch(action){case ProjectTopAction::Save:return ui::icons::Store;case ProjectTopAction::Open:return ui::icons::Open;case ProjectTopAction::New:return ui::icons::New;case ProjectTopAction::SaveAs:return ui::icons::Store;case ProjectTopAction::Confirm:return ui::icons::Confirm;case ProjectTopAction::Cancel:return ui::icons::Cancel;case ProjectTopAction::MidiRecord:return ui::icons::Record;case ProjectTopAction::InternalRec:return ui::icons::Record;case ProjectTopAction::Collect:return ui::icons::Copy;case ProjectTopAction::Undo:return ui::icons::Undo;case ProjectTopAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using ProjectControl=ui::ControlDescriptor<ProjectTopAction>;
inline ProjectControl project_button(unsigned index,bool shift,bool pending,bool midi_capturing,unsigned history_cursor,unsigned history_size){
 if(pending){if(!index)return {"CONFIRM",ProjectTopAction::Confirm,true};if(index==1)return {"CANCEL",ProjectTopAction::Cancel,true};return {};}
 switch(index){
 case 0:return {"SAVE",ProjectTopAction::Save,true};
 case 1:return {"OPEN",ProjectTopAction::Open,true};
 case 2:return {"NEW",ProjectTopAction::New,true};
 case 3:return shift?ProjectControl{midi_capturing?"MIDI STOP":"MIDI OUT REC",ProjectTopAction::MidiRecord,true}:ProjectControl{"SAVE AS",ProjectTopAction::SaveAs,true};
 case 4:return shift?ProjectControl{midi_capturing?"MIDI STOP":"INTERNAL REC",ProjectTopAction::InternalRec,true}:ProjectControl{"COLLECT",ProjectTopAction::Collect,true};
 case 5:return {"FILES",ProjectTopAction::Files,true};
 case 6:return {"UNDO",ProjectTopAction::Undo,history_cursor>0};
 case 7:return {"REDO",ProjectTopAction::Redo,history_cursor<history_size};
 }
 return {};
}
}
