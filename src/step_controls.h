#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class StepAction:uint8_t {None,Page,ClickMain,CountIn,NoteQuantize,NoteReplace,CopyStep,PasteStep,ViewCycle,PatPrev,PatNext,Undo,Redo};
constexpr ui::TopWork top_work(StepAction a){switch(a){case StepAction::None:return ui::TopWork::Empty;case StepAction::Page:case StepAction::ClickMain:case StepAction::CountIn:case StepAction::ViewCycle:return ui::TopWork::Selection;case StepAction::PatPrev:case StepAction::PatNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(StepAction action){switch(action){case StepAction::ClickMain:return ui::icons::Metronome;case StepAction::CopyStep:return ui::icons::Copy;case StepAction::PasteStep:return ui::icons::Paste;case StepAction::PatPrev:return ui::icons::Back;case StepAction::PatNext:return ui::icons::Forward;case StepAction::Undo:return ui::icons::Undo;case StepAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using StepControl=ui::ControlDescriptor<StepAction>;
struct StepContext {
 bool shift=false,drum_kit=false,click_main=false,count_in=false,note_quantized=false,note_replace=false,step_copied=false,tracker_view=false,step_overview=false;
 uint8_t page=0;bool can_undo=false,can_redo=false;
};
inline StepControl step_button_control(unsigned index,const StepContext& c){
 static const char* ranges[]={"1 - 16","17 - 32","33 - 48","49 - 64","65 - 80","81 - 96","97 - 112","113 - 128","129 - 144","145 - 160","161 - 176","177 - 192","193 - 208","209 - 224","225 - 240","241 - 256"};
 if(c.shift && index==6)return {"COPY STEP",StepAction::CopyStep,true};
 if(c.shift && index==7)return {"PASTE STEP",StepAction::PasteStep,c.step_copied};
 if(c.shift && index==1){StepControl d{c.click_main?"CLICK: MAIN":"CLICK: CUE",StepAction::ClickMain,true};d.selected=c.click_main;return d;}
 if(c.shift && index==2){StepControl d{c.count_in?"COUNT: 1 BAR":"COUNT: OFF",StepAction::CountIn,true};d.selected=c.count_in;return d;}
 if(c.shift && index==4){StepControl d{c.note_quantized?"REC: GRID":"REC: FREE",StepAction::NoteQuantize,true};d.selected=!c.note_quantized;return d;}
 if(c.shift && index==5){StepControl d{c.drum_kit?"PAD OVERDUB":c.note_replace?"REC: REPLACE":"REC: OVERDUB",StepAction::NoteReplace,!c.drum_kit};d.selected=!c.drum_kit && c.note_replace;return d;}
 if(c.shift && index==3){StepControl d{c.tracker_view?"16 STEP EDIT":c.step_overview?"TRACKER":"64 STEP VIEW",StepAction::ViewCycle,true};d.selected=c.page%4==3;return d;}
 if(index<4){StepControl d{ranges[(c.page/4)*4+index],StepAction::Page,true};d.selected=c.page%4==index;return d;}
 if(index==4)return {"PAT PREV",StepAction::PatPrev,true};
 if(index==5)return {"PAT NEXT",StepAction::PatNext,true};
 if(index==6)return {"UNDO",StepAction::Undo,c.can_undo};
 if(index==7)return {"REDO",StepAction::Redo,c.can_redo};
 return {};
}
}
