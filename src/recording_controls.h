#pragma once
#include "project.h"
#include "ui/control_descriptor.h"
namespace groove {
struct RecordingTarget {unsigned track=0,pattern=0,generation=0;Instrument instrument=Instrument::Empty;uint8_t sample_slot=0,kit_pad=255;uint64_t revision=0;};
enum class RecordingOrigin:uint8_t {None,Input,Source,Resample,Overdub};
constexpr uint8_t icon_for(RecordingOrigin action){switch(action){case RecordingOrigin::Source:return ui::icons::Source;default:return ui::icons::None;}}

struct LiveRecordRequest {RecordingTarget target{};uint8_t source=0;bool quantized=false;RecordingOrigin origin=RecordingOrigin::Source;};
struct ResampleRequest {RecordingTarget target{};uint8_t source=0,bars=1;};
enum class RecordingAction:uint8_t {None,GroupFx,Mixer,Monitor,Capture,Record,Stop,Resample,Cancel,Bars,Quantize,Overdub,Undo,Redo,Source,RecordInput,RecordSource,Preview};
constexpr uint8_t icon_for(RecordingAction action){switch(action){case RecordingAction::Capture:return ui::icons::Record;case RecordingAction::Record:return ui::icons::Record;case RecordingAction::Stop:return ui::icons::Stop;case RecordingAction::Cancel:return ui::icons::Cancel;case RecordingAction::Undo:return ui::icons::Undo;case RecordingAction::Redo:return ui::icons::Redo;case RecordingAction::Source:return ui::icons::Source;case RecordingAction::RecordInput:return ui::icons::Record;case RecordingAction::RecordSource:return ui::icons::Record;case RecordingAction::Preview:return ui::icons::Play;default:return ui::icons::None;}}

constexpr ui::TopWork top_work(RecordingAction a){switch(a){case RecordingAction::None:return ui::TopWork::Empty;case RecordingAction::GroupFx:case RecordingAction::Monitor:case RecordingAction::Quantize:case RecordingAction::Bars:return ui::TopWork::Selection;case RecordingAction::Mixer:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
using RecordingControl=ui::ControlDescriptor<RecordingAction>;
struct RecordingContext {
 bool shift=false,playing=false,audio=false,scene_take=false,live=false,resampling=false,cancelling=false,pending=false,file_busy=false,can_undo=false,can_redo=false,quantized=false;
 unsigned bars=1,source=0;bool stopping=false;RecordingOrigin origin=RecordingOrigin::None;
};
inline bool recording_configuration_locked(const RecordingContext& c){return c.live || c.resampling || c.pending || c.file_busy;}
inline bool recording_start_allowed(const RecordingContext& c){return c.audio && !c.scene_take && !recording_configuration_locked(c);}
inline bool resample_start_allowed(const RecordingContext& c){return c.playing && recording_start_allowed(c);}
inline const char* resample_bar_label(unsigned bars){static const char* names[]={"1 BAR","2 BARS","3 BARS","4 BARS"};return bars>=1 && bars<=4?names[bars-1]:"? BARS";}
inline RecordingControl recording_button_control(unsigned index,const RecordingContext& c){
 bool idle=!recording_configuration_locked(c);
 switch(index){
  case 0:return {"GROUP FX",RecordingAction::GroupFx,true};
  case 1:return {"MIXER",RecordingAction::Mixer,true};
  case 2:return {"MONITOR",RecordingAction::Monitor,true};
  case 3:
   if(c.resampling)return {c.cancelling?"CANCELLING":"CANCEL",RecordingAction::Cancel,!c.cancelling};
   if(c.live)return {c.stopping?"STOPPING":"STOP RECORD",RecordingAction::Stop,!c.stopping};
   return {c.shift?"RECORD":"CAPTURE",c.shift?RecordingAction::Record:RecordingAction::Capture,idle && c.audio && !c.scene_take};
  case 4:
   if(c.shift){if(c.resampling)return {c.cancelling?"CANCELLING":"CANCEL",RecordingAction::Cancel,!c.cancelling};return {"RESAMPLE",RecordingAction::Resample,resample_start_allowed(c)};}
   return {"REC MODE",RecordingAction::Quantize,idle};
  case 5:
   if(c.shift)return {resample_bar_label(c.bars),RecordingAction::Bars,idle,1,4};
   return {c.source>=3 && c.source!=51 && c.source!=52?"OVERDUB SRC":"OVERDUB IN",RecordingAction::Overdub,idle && c.audio && c.playing && !c.scene_take};
  case 6:return {"UNDO",RecordingAction::Undo,idle && c.can_undo};
  case 7:return {"REDO",RecordingAction::Redo,idle && c.can_redo};
  default:return {};
 }
}
inline RecordingControl sampler_recording_control(unsigned index,const RecordingContext& c,bool previewing=false){
 if(index==1)return {previewing?"STOP PREVIEW":"PREVIEW",RecordingAction::Preview,!c.pending && (previewing || c.audio)};
 if(index!=0 && index!=2)return {};
 if(c.resampling)return {c.cancelling?"CANCELLING":"CANCEL",RecordingAction::Cancel,!c.cancelling};
 if(c.live)return {c.stopping?"STOPPING":index==0?"STOP RECORD":"STOP SOURCE",RecordingAction::Stop,!c.stopping};
 return {index==0?"REC INPUT":"REC SOURCE",index==0?RecordingAction::RecordInput:RecordingAction::RecordSource,recording_start_allowed(c)};
}
inline bool sampler_recording_selected(unsigned index,const RecordingContext& c,bool previewing=false){
 return index==1?previewing:index==0?c.origin==RecordingOrigin::Input:index==2 && c.origin==RecordingOrigin::Source;
}
inline bool recording_button_selected(unsigned index,const RecordingContext& c,bool monitor_on){
 switch(recording_button_control(index,c).action){
  case RecordingAction::Monitor:return monitor_on;
  case RecordingAction::Quantize:return c.quantized;
  case RecordingAction::Record:case RecordingAction::Stop:return c.live;
  case RecordingAction::Resample:case RecordingAction::Cancel:return c.resampling;
  default:return false;
 }
}
inline RecordingControl recording_source_control(const RecordingContext& c){return {"CAPTURE BUS",RecordingAction::Source,!recording_configuration_locked(c),0,int(kCaptureMax)};}
inline const char* recording_hint(const RecordingContext& c,unsigned state){
 if(c.resampling){if(c.cancelling)return "CANCELLING / destination kept";return state==1 || state==4?"ARMED / next bar / CANCEL available":"RESAMPLING / CANCEL available";}
 if(c.stopping)return "STOPPING / flushing audio";
 if(c.pending)return "REQUEST PENDING / settings held";
 if(c.live){if(state==0)return "CHECKING / MIXING / STOP RECORD";if(state==1 || state==4)return c.playing?"ARMED / next bar / STOP RECORD":"ARMED / PLAY to record / STOP available";return "RECORDING / source held / STOP RECORD";}
 if(c.file_busy)return "FILE JOB ACTIVE / wait to record";
 if(!c.audio)return "Audio unavailable";
 if(c.scene_take)return "Finish Arrangement take to record";
 if(!c.playing)return "PLAY to resample / SHIFT + Top6: bars";
 return "SHIFT + Top5: RESAMPLE / Top6: bars";
}
inline const char* sampler_recording_hint(const RecordingContext& c,unsigned state){
 if(c.live || c.pending || c.file_busy || !c.audio || c.scene_take)return recording_hint(c,state);
 return c.quantized && c.playing?"REC INPUT / REC SOURCE: next bar":"REC INPUT: external / REC SOURCE: selected bus";
}
}
