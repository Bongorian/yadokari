#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class EffectsAction:uint8_t {None,Page,ChainInserts,SwapInserts,ChainBypass,ChainEdit,PadInsert,ChainToggle,SendDelay,SendReverb,Undo,Redo};
constexpr ui::TopWork top_work(EffectsAction a){switch(a){case EffectsAction::None:return ui::TopWork::Empty;case EffectsAction::Page:case EffectsAction::ChainInserts:case EffectsAction::ChainToggle:return ui::TopWork::Selection;case EffectsAction::PadInsert:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(EffectsAction action){switch(action){case EffectsAction::Undo:return ui::icons::Undo;case EffectsAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using EffectsControl=ui::ControlDescriptor<EffectsAction>;
struct EffectsContext {
 bool shift=false;uint8_t fx_page=0;bool drum_kit=false;
 unsigned send_prefader=0,chain_node=0,insert_disabled=0;
 bool can_undo=false,can_redo=false;
};
inline EffectsControl effects_button_control(unsigned index,const EffectsContext& c){
 if(c.fx_page==5 && index>=3 && index<=6){
  bool node=c.chain_node>=1 && c.chain_node<=2;
  switch(index){
   case 3:return {"INSERTS",EffectsAction::ChainInserts,true};
   case 4:return {"SWAP",EffectsAction::SwapInserts,true};
   case 5:{EffectsControl d{"BYPASS",EffectsAction::ChainBypass,node};d.selected=node && (c.insert_disabled&(1u<<(c.chain_node-1)));return d;}
   case 6:return {"EDIT",EffectsAction::ChainEdit,true};
  }
 }
 if(index==6 && c.drum_kit){EffectsControl d{c.shift?"UNDO":"PAD INSERT",c.shift?EffectsAction::Undo:EffectsAction::PadInsert,!c.shift || c.can_undo};d.selected=c.fx_page==4 && !c.shift;return d;}
 if(index==5){EffectsControl d{c.fx_page==3?(c.shift?"SWAP FX":"INSERTS"):"INSERTS",c.fx_page==3 && c.shift?EffectsAction::SwapInserts:EffectsAction::ChainToggle,true};d.selected=c.fx_page==3 && !c.shift;return d;}
 if(index<5){
  static const char* names[]={"SEND FX","MASTER COMP","TRACK COMP"};
  if(index<3){EffectsControl d{names[index],EffectsAction::Page,true};d.selected=index==c.fx_page;return d;}
  if(index==3){EffectsControl d{c.send_prefader&1?"DELAY PRE":"DELAY POST",EffectsAction::SendDelay,true};d.selected=(c.send_prefader&1)!=0;return d;}
  EffectsControl d{c.send_prefader&2?"REVERB PRE":"REVERB POST",EffectsAction::SendReverb,true};d.selected=(c.send_prefader&2)!=0;return d;
 }
 if(index==6)return {"UNDO",EffectsAction::Undo,c.can_undo};
 if(index==7)return {"REDO",EffectsAction::Redo,c.can_redo};
 return {};
}
}
