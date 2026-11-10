#pragma once
#include "ui/control_descriptor.h"
namespace groove {
// Routing group-FX page top row (the routing page's other face already
// uses the recording provider). Top7/8 stay enabled unconditionally,
// matching the page's historical gate; dispatch falls to undo/redo.
enum class RoutingFxAction:uint8_t {None,ExitFx,Mixer,Monitor,GroupPage,RecordToggle,ToggleSlot,SwapInserts,Quantize,Overdub,Undo,Redo};
constexpr ui::TopWork top_work(RoutingFxAction a){switch(a){case RoutingFxAction::None:return ui::TopWork::Empty;case RoutingFxAction::ExitFx:case RoutingFxAction::Monitor:case RoutingFxAction::GroupPage:case RoutingFxAction::ToggleSlot:case RoutingFxAction::Quantize:return ui::TopWork::Selection;case RoutingFxAction::Mixer:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(RoutingFxAction action){switch(action){case RoutingFxAction::Undo:return ui::icons::Undo;case RoutingFxAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using RoutingFxControl=ui::ControlDescriptor<RoutingFxAction>;
struct RoutingFxContext {
 bool shift=false,live=false,group_send_page=false;uint8_t group_fx_slot=0;
 bool monitor_on=false,record_quantized=false;
};
inline RoutingFxControl routing_fx_button_control(unsigned index,const RoutingFxContext& c){
 switch(index){
 case 0:{RoutingFxControl d{"MAIN / CUE",RoutingFxAction::ExitFx,true};d.selected=true;return d;}
 case 1:return {"MIXER",RoutingFxAction::Mixer,true};
 case 2:{RoutingFxControl d{"MONITOR",RoutingFxAction::Monitor,true};d.selected=c.monitor_on;return d;}
 case 3:{
  if(!c.shift && !c.live){RoutingFxControl d{c.group_send_page?"GROUP INSERTS":"GROUP SENDS",RoutingFxAction::GroupPage,true};d.selected=c.group_send_page;return d;}
  RoutingFxControl d{c.live?"STOP RECORD":"RECORD",RoutingFxAction::RecordToggle,true};d.selected=c.group_send_page;return d;}
 case 4:{
  if(!c.group_send_page){RoutingFxControl d{c.shift?"SWAP FX":c.group_fx_slot?"INSERT 2":"INSERT 1",c.shift?RoutingFxAction::SwapInserts:RoutingFxAction::ToggleSlot,true};d.selected=c.group_fx_slot==1;return d;}
  RoutingFxControl d{"REC MODE",RoutingFxAction::Quantize,true};d.selected=c.record_quantized;return d;}
 case 5:return {"OVERDUB IN",RoutingFxAction::Overdub,true};
 case 6:return {"UNDO",RoutingFxAction::Undo,true};
 case 7:return {"REDO",RoutingFxAction::Redo,true};
 }
 return {};
}
}
