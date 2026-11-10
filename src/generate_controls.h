#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class GenerateAction:uint8_t {None,Page,RotateLeft,RotateRight,Extract,Paste,PatPrev,PatNext,Undo,Redo,ApplyAll,Harmony};
constexpr ui::TopWork top_work(GenerateAction a){switch(a){case GenerateAction::None:return ui::TopWork::Empty;case GenerateAction::Page:return ui::TopWork::Selection;case GenerateAction::PatPrev:case GenerateAction::PatNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(GenerateAction action){switch(action){case GenerateAction::Paste:return ui::icons::Paste;case GenerateAction::PatPrev:return ui::icons::Back;case GenerateAction::PatNext:return ui::icons::Forward;case GenerateAction::Undo:return ui::icons::Undo;case GenerateAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using GenerateControl=ui::ControlDescriptor<GenerateAction>;
inline GenerateControl generate_button(unsigned index,bool shift,unsigned gen_page,bool groove_copied,unsigned history_cursor,unsigned history_size){
 if(gen_page==5 && index==4)return {"GENERATE",GenerateAction::Harmony,true};
 if(gen_page==2 && shift && index==5)return {"APPLY ALL",GenerateAction::ApplyAll,true};
 if(gen_page==0 && shift && index==4)return {"ROTATE LEFT",GenerateAction::RotateLeft,true};
 if(gen_page==0 && shift && index==5)return {"ROTATE RIGHT",GenerateAction::RotateRight,true};
 if(gen_page==2 && index==4)return {"EXTRACT",GenerateAction::Extract,true};
 if(gen_page==2 && index==5)return {"PASTE",GenerateAction::Paste,groove_copied};
 if(index<4){static const char* names[]={"RHYTHM","ACCUM","GROOVE","LANES"};GenerateControl d{index==3?(gen_page==3?"LANES":gen_page==4?"ARP":gen_page==5?"HARMONY":"LANES/ARP/HARM"):names[index],GenerateAction::Page,true,index==3?(shift?4:gen_page==3?4:gen_page==4?5:3):int(index),5};d.selected=index==gen_page || (index==3 && gen_page>=4);return d;}
 if(index==4)return {"PAT PREV",GenerateAction::PatPrev,true};
 if(index==5)return {"PAT NEXT",GenerateAction::PatNext,true};
 if(index==6)return {"UNDO",GenerateAction::Undo,history_cursor>0};
 if(index==7)return {"REDO",GenerateAction::Redo,history_cursor<history_size};
 return {};
}
}
