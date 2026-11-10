#pragma once
#include "ui/control_descriptor.h"
namespace groove {
// Fallback top row shared by pages without their own provider:
// step-range selectors, pattern prev/next and history undo/redo.
enum class DefaultAction:uint8_t {None,Page,PatPrev,PatNext,Undo,Redo};
constexpr ui::TopWork top_work(DefaultAction a){switch(a){case DefaultAction::None:return ui::TopWork::Empty;case DefaultAction::Page:return ui::TopWork::Selection;case DefaultAction::PatPrev:case DefaultAction::PatNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(DefaultAction action){switch(action){case DefaultAction::PatPrev:return ui::icons::Back;case DefaultAction::PatNext:return ui::icons::Forward;case DefaultAction::Undo:return ui::icons::Undo;case DefaultAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using DefaultControl=ui::ControlDescriptor<DefaultAction>;
inline DefaultControl default_button(unsigned index,unsigned page,unsigned history_cursor,unsigned history_size){
 static const char* pages[]={"STEP 1-16","STEP 17-32","STEP 33-48","STEP 49-64","STEP 65-80","STEP 81-96","STEP 97-112","STEP 113-128","STEP 129-144","STEP 145-160","STEP 161-176","STEP 177-192","STEP 193-208","STEP 209-224","STEP 225-240","STEP 241-256"};
 if(index<4){DefaultControl d{pages[(page/4)*4+index],DefaultAction::Page,true};d.selected=page%4==index;return d;}
 if(index==4)return {"PAT PREV",DefaultAction::PatPrev,true};
 if(index==5)return {"PAT NEXT",DefaultAction::PatNext,true};
 if(index==6)return {"UNDO",DefaultAction::Undo,history_cursor>0};
 if(index==7)return {"REDO",DefaultAction::Redo,history_cursor<history_size};
 return {};
}
}
