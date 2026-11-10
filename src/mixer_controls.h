#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class MixerAction:uint8_t {None,Freeze,Twin,PatPrev,PatNext,Undo,Redo};
constexpr ui::TopWork top_work(MixerAction a){switch(a){case MixerAction::None:return ui::TopWork::Empty;case MixerAction::PatPrev:case MixerAction::PatNext:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(MixerAction action){switch(action){case MixerAction::Twin:case MixerAction::Freeze:return ui::icons::Bake;case MixerAction::PatPrev:return ui::icons::Back;case MixerAction::PatNext:return ui::icons::Forward;case MixerAction::Undo:return ui::icons::Undo;case MixerAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using MixerControl=ui::ControlDescriptor<MixerAction>;
inline MixerControl mixer_button(unsigned index,bool frozen,unsigned page,unsigned history_cursor,unsigned history_size){
 if(index==0){MixerControl d{frozen?"UNFREEZE":"FREEZE",MixerAction::Freeze,true};d.selected=page%4==0;return d;}
 if(index==1)return {"TWIN",MixerAction::Twin,true};
 if(index<4){MixerControl d{"",MixerAction::None,false};d.selected=page%4==index;return d;}
 if(index==4)return {"PAT PREV",MixerAction::PatPrev,true};
 if(index==5)return {"PAT NEXT",MixerAction::PatNext,true};
 if(index==6)return {"UNDO",MixerAction::Undo,history_cursor>0};
 if(index==7)return {"REDO",MixerAction::Redo,history_cursor<history_size};
 return {};
}
}
