#pragma once
#include "ui/control_descriptor.h"
#include "recording_controls.h"
namespace groove {
// Perform top row across its subpages (morph/macros, gesture, mutation,
// live FX). The audio/looper face (page 5) stays with the audio provider.
// SHIFT+Top2 owns GESTURE on each face; the historical CAP Q shortcut
// remains outside this Top grammar. See docs/wave-five-layout.md.
enum PerformPage:uint8_t {PerformMorph,PerformMacros,PerformGesture,PerformMutation,PerformLiveFx,PerformLooper,PerformTools,PerformVocoder,PerformRecording,PerformFxMap,PerformModule,PerformTimeline,PerformVowel,PerformPageCount};
constexpr unsigned live_fx_pad_effect(unsigned pad,bool bank_b){
 return pad>=2 && pad<=(bank_b?3u:12u)?(bank_b?pad+10:pad-1):0;
}
enum class PerformAction:uint8_t {None,PageMorph,PageMacros,StoreA,StoreB,ResetLive,ControlAll,LiveFx,Undo,Redo,FxQuantize,FxTail,FxBank,GotoMutation,GotoGesture,MutationGenerate,MutationFreeze,MutationCommit,MutationRevert,Back,PagePrev,PageNext,CaptureRetro,GestureRecord,GesturePlay,GestureClear,Dice,Tools,Vocoder,Recording,FxMap,Module,Looper,ModuleConfirm,Insert1,Insert2,RecordBus,RecordInput,RecordCapture,RecordResample,RecordMode,RecordMonitor,RecordBars,MapTrack,MapGroup,MapMain,MapLive,MapPreset,Vowel,SelectVowel};
constexpr ui::TopWork top_work(PerformAction a){switch(a){case PerformAction::None:return ui::TopWork::Empty;case PerformAction::SelectVowel:case PerformAction::PageMorph:case PerformAction::PageMacros:case PerformAction::GotoGesture:case PerformAction::GotoMutation:case PerformAction::FxQuantize:case PerformAction::FxTail:return ui::TopWork::Selection;case PerformAction::Vowel:case PerformAction::Back:case PerformAction::LiveFx:case PerformAction::Tools:case PerformAction::Vocoder:case PerformAction::Recording:case PerformAction::FxMap:case PerformAction::Module:case PerformAction::MapGroup:case PerformAction::MapMain:case PerformAction::MapLive:case PerformAction::MapPreset:return ui::TopWork::Exit;case PerformAction::MapTrack:case PerformAction::Looper:case PerformAction::RecordMode:case PerformAction::RecordMonitor:case PerformAction::RecordBars:return ui::TopWork::Selection;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(PerformAction action){switch(action){case PerformAction::StoreA:return ui::icons::Store;case PerformAction::StoreB:return ui::icons::Store;case PerformAction::Undo:return ui::icons::Undo;case PerformAction::Redo:return ui::icons::Redo;case PerformAction::Back:return ui::icons::Back;case PerformAction::PagePrev:return ui::icons::Back;case PerformAction::PageNext:return ui::icons::Forward;case PerformAction::CaptureRetro:return ui::icons::Record;case PerformAction::GestureRecord:return ui::icons::Record;case PerformAction::GesturePlay:return ui::icons::Play;case PerformAction::GestureClear:return ui::icons::Erase;default:return ui::icons::None;}}

using PerformControl=ui::ControlDescriptor<PerformAction>;
struct PerformContext {
 bool shift=false;uint8_t page=0,fx_quantize=0;bool fx_tail=false,fx_bank=false;
 bool mutation_frozen=false,gesture_recording=false,gesture_playing=false,control_all=false;
 bool can_undo=false,can_redo=false,module=false;
 RecordingContext record{};unsigned group=8;
};
inline PerformControl perform_button_control(unsigned index,const PerformContext& c){
 using A=PerformAction;
 if(c.page==PerformTools){
  if(index==3)return {"VOWELS",A::SelectVowel,true};
  if(index==4)return {"RECORDING",A::Recording,true};
  if(index==5)return {"FX MAP",A::FxMap,true};
  if(index==6)return {"MIDI MODULE",A::Module,true};
  if(index==7)return {"BACK",A::Back,true};
  return {};
 }
 if(c.page==PerformVowel){
  if(index==4)return {"TALK INSERT 1",A::Insert1,true};
  if(index==5)return {"TALK INSERT 2",A::Insert2,true};
  if(index==7)return {"BACK",A::Back,true};
  return {};
 }
 if(c.page==PerformVocoder){
  if(index==6)return {"VOWELS",A::Vowel,true};
  if(index==4)return {"USE INSERT 1",A::Insert1,true};
  if(index==5)return {"USE INSERT 2",A::Insert2,true};
  if(index==7)return {"BACK",A::Back,true};
  return {};
 }
 if(c.page==PerformModule){
  if(index==4)return {c.module?"EXIT MODULE":"ENTER MODULE",A::ModuleConfirm,!recording_configuration_locked(c.record)};
  if(index==7)return {"BACK",A::Back,true};
  return {};
 }
 if(c.page==PerformFxMap){
  static const PerformControl links[]={{"TRACK FX",A::MapTrack,true},{"GROUP FX",A::MapGroup,true},
   {"MASTER FX",A::MapMain,true},{"LIVE FX",A::MapLive,true},{"SAVE PRESET",A::MapPreset,true}};
  if(index==4){
   static constexpr const char* names[]={"GROUP A FX","GROUP B FX","GROUP C FX","GROUP D FX","GROUP E FX","GROUP F FX","GROUP G FX","GROUP H FX"};
   return {c.group<8?names[c.group]:"ROUTING",A::MapGroup,true};
  }
  if(index==0)return links[0];
  if(index>=4 && index<8)return links[index-3];
  return {};
 }
 if(c.page==PerformRecording){
  const auto& r=c.record;const bool idle=!recording_configuration_locked(r);
  if(index==0)return {"MONITOR",A::RecordMonitor,true};
  if(index==1)return {r.quantized?"NEXT BAR":"IMMEDIATE",A::RecordMode,idle};
  if(index==2)return {resample_bar_label(r.bars),A::RecordBars,idle};
  if(index==3)return {"LOOPER",A::Looper,true};
  if(index==4)return {"CAPTURE AUDIO",A::RecordCapture,recording_start_allowed(r)};
  const char* stop_record=r.stopping?"STOPPING":"STOP RECORD";
  if(index==5)return {r.live && !r.resampling?stop_record:"REC INPUT",A::RecordInput,r.live && !r.resampling?!r.stopping:recording_start_allowed(r)};
  if(index==6)return {r.live && !r.resampling?stop_record:"REC SOURCE",A::RecordBus,r.live && !r.resampling?!r.stopping:recording_start_allowed(r)};
  if(index==7)return {r.resampling?(r.cancelling?"CANCELLING":"CANCEL"):"RESAMPLE",A::RecordResample,r.resampling?!r.cancelling:resample_start_allowed(r)};
  return {};
 }
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
  if(index==5)return {c.shift?"RECALL NOTES":c.gesture_playing?"STOP LOOP":"PLAY LOOP",c.shift?PerformAction::CaptureRetro:PerformAction::GesturePlay,true};
 }
 else if(c.page==4){
  if(index==4)return {c.shift?"RESET LIVE":c.fx_bank?"FX BANK 2":"FX BANK 1",c.shift?PerformAction::ResetLive:PerformAction::FxBank,true};
  if(index==5){PerformControl d{"CONTROL ALL",PerformAction::ControlAll,true};d.selected=c.control_all;return d;}
 }
 else {
  if(index==4)return {c.shift?"RESET LIVE":"STORE A",c.shift?PerformAction::ResetLive:PerformAction::StoreA,true};
  if(index==5)return {c.shift?"STORE B":"VOCODER",c.shift?A::StoreB:A::Vocoder,true};
 }
 return {}; // Top8 is handled by the shared LOOPER / LIVE TOOLS navigation.
}
}
