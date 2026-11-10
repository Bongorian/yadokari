#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class ModulationAction:uint8_t {None,Page,Multitimbral,AutoPagePrev,AutoPageNext,PatPrev,PatNext,Undo,Redo};
constexpr ui::TopWork top_work(ModulationAction a){switch(a){case ModulationAction::None:return ui::TopWork::Empty;case ModulationAction::Page:return ui::TopWork::Selection;case ModulationAction::PatPrev:case ModulationAction::PatNext:case ModulationAction::AutoPagePrev:case ModulationAction::AutoPageNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(ModulationAction action){switch(action){case ModulationAction::AutoPagePrev:return ui::icons::Back;case ModulationAction::AutoPageNext:return ui::icons::Forward;case ModulationAction::PatPrev:return ui::icons::Back;case ModulationAction::PatNext:return ui::icons::Forward;case ModulationAction::Undo:return ui::icons::Undo;case ModulationAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using ModulationControl=ui::ControlDescriptor<ModulationAction>;
struct ModulationContext {
 bool shift=false;uint8_t page=0;bool multitimbral=false,input_busy=false,can_undo=false,can_redo=false;
};
inline ModulationControl modulation_button_control(unsigned index,const ModulationContext& c){
 if(c.page==3 && index==4){ModulationControl d{c.multitimbral?"EXIT MODULE":"MULTITIMBRAL",ModulationAction::Multitimbral,!c.input_busy};d.selected=c.multitimbral;return d;}
 if(c.page==1 && index==4)return {"AUTO PAGE -",ModulationAction::AutoPagePrev,true};
 if(c.page==1 && index==5)return {"AUTO PAGE +",ModulationAction::AutoPageNext,true};
 if(index<4){static const char* labels[]={"ROUTES","AUTOMATION","MACRO MAP","MIDI"};ModulationControl d{index==3?"MIDI/RPN":labels[index],ModulationAction::Page,true,index==3?(c.shift?4:c.page==3?4:3):int(index),4};d.selected=index==(c.page==4?3:unsigned(c.page));return d;}
 if(index==4)return {"PAT PREV",ModulationAction::PatPrev,true};
 if(index==5)return {"PAT NEXT",ModulationAction::PatNext,true};
 if(index==6)return {"UNDO",ModulationAction::Undo,c.can_undo};
 if(index==7)return {"REDO",ModulationAction::Redo,c.can_redo};
 return {};
}
}
