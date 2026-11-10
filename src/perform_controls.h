#pragma once
#include "ui/control_descriptor.h"
namespace groove {
// Perform top row across its subpages (morph/macros, gesture, mutation,
// live FX). The audio/looper face (page 5) stays with the audio provider.
// SHIFT+Top2 owns GESTURE on each face; the historical CAP Q shortcut
// remains outside this Top grammar. See docs/wave-five-layout.md.
enum class PerformAction:uint8_t {None,PageMorph,PageMacros,StoreA,StoreB,ResetLive,ControlAll,LiveFx,Undo,Redo,FxQuantize,FxTail,FxBank,GotoMutation,GotoGesture,MutationGenerate,MutationFreeze,MutationCommit,MutationRevert,Back,PagePrev,PageNext,CaptureRetro,GestureRecord,GesturePlay,GestureClear,Dice};
constexpr ui::TopWork top_work(PerformAction a){switch(a){case PerformAction::None:return ui::TopWork::Empty;case PerformAction::PageMorph:case PerformAction::PageMacros:case PerformAction::GotoGesture:case PerformAction::GotoMutation:case PerformAction::FxQuantize:case PerformAction::FxTail:return ui::TopWork::Selection;case PerformAction::Back:case PerformAction::LiveFx:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(PerformAction action){switch(action){case PerformAction::StoreA:return ui::icons::Store;case PerformAction::StoreB:return ui::icons::Store;case PerformAction::Undo:return ui::icons::Undo;case PerformAction::Redo:return ui::icons::Redo;case PerformAction::Back:return ui::icons::Back;case PerformAction::PagePrev:return ui::icons::Back;case PerformAction::PageNext:return ui::icons::Forward;case PerformAction::CaptureRetro:return ui::icons::Record;case PerformAction::GestureRecord:return ui::icons::Record;case PerformAction::GesturePlay:return ui::icons::Play;case PerformAction::GestureClear:return ui::icons::Erase;default:return ui::icons::None;}}

using PerformControl=ui::ControlDescriptor<PerformAction>;
struct PerformContext {
 bool shift=false;uint8_t page=0,fx_quantize=0;bool fx_tail=false,fx_bank=false;
 bool mutation_frozen=false,gesture_recording=false,gesture_playing=false,control_all=false;
 bool can_undo=false,can_redo=false;
};
inline PerformControl perform_button_control(unsigned index,const PerformContext& c){
 // Four named faces on the left; execution and same-target exits on the right.
 if(index<4){
  if(c.page==4 && c.shift && index==2)return {c.fx_quantize==0?"TRIGGER NOW":c.fx_quantize==1?"TRIGGER BEAT":"TRIGGER BAR",PerformAction::FxQuantize,true};
  if(c.page==4 && c.shift && index==3)return {c.fx_tail?"TAIL":"DRY",PerformAction::FxTail,true};
  const char* names[]={"A/B MORPH","MACROS","GESTURE","MUTATION"};
  const PerformAction actions[]={PerformAction::PageMorph,PerformAction::PageMacros,PerformAction::GotoGesture,PerformAction::GotoMutation};
  PerformControl d{names[index],actions[index],true};d.selected=c.page==index;return d;
 }
 if(index==6){PerformControl d{"LIVE FX",PerformAction::LiveFx,true};d.selected=c.page==4;return d;}
 if(c.page==3){
  if(index==4)return {c.shift?(c.mutation_frozen?"UNFREEZE":"FREEZE"):"GENERATE",c.shift?PerformAction::MutationFreeze:PerformAction::MutationGenerate,true};
  if(index==5)return {c.shift?"REVERT":"COMMIT",c.shift?PerformAction::MutationRevert:PerformAction::MutationCommit,true};
 }
 else if(c.page==2){
  if(index==4)return {c.shift?"CLEAR":c.gesture_recording?"END RECORD":"RECORD",c.shift?PerformAction::GestureClear:PerformAction::GestureRecord,true};
  if(index==5)return {c.shift?"CAPTURE":c.gesture_playing?"STOP LOOP":"PLAY LOOP",c.shift?PerformAction::CaptureRetro:PerformAction::GesturePlay,true};
 }
 else if(c.page==4){
  if(index==4)return {c.shift?"RESET LIVE":c.fx_bank?"FX BANK 2":"FX BANK 1",c.shift?PerformAction::ResetLive:PerformAction::FxBank,true};
  if(index==5){PerformControl d{"CONTROL ALL",PerformAction::ControlAll,true};d.selected=c.control_all;return d;}
 }
 else {
  if(index==4)return {c.shift?"RESET LIVE":"STORE A",c.shift?PerformAction::ResetLive:PerformAction::StoreA,true};
  if(index==5){PerformControl d{c.shift?"CONTROL ALL":"STORE B",c.shift?PerformAction::ControlAll:PerformAction::StoreB,true};d.selected=c.shift && c.control_all;return d;}
 }
 return {}; // PERFORM T8 is the common AUDIO entry, on every face.
}
}
