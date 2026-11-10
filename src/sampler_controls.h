#pragma once
#include "default_controls.h"
#include "recording_controls.h"
#include "ui/control_descriptor.h"
namespace groove {
// Left selects material faces; right executes processing, recording and exits.
enum class SamplerAction:uint8_t {None,Record,Page,Chop,MapPads,ChopSteps,OpenBrowser,LoopTake,FileLoop,ClearZones,Undo,Redo};
constexpr ui::TopWork top_work(SamplerAction a){switch(a){case SamplerAction::None:return ui::TopWork::Empty;case SamplerAction::Page:return ui::TopWork::Selection;case SamplerAction::OpenBrowser:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(SamplerAction action){switch(action){case SamplerAction::Record:return ui::icons::Record;case SamplerAction::OpenBrowser:return ui::icons::Open;case SamplerAction::Undo:return ui::icons::Undo;case SamplerAction::Redo:return ui::icons::Redo;default:return ui::icons::None;}}

using SamplerControl=ui::ControlDescriptor<SamplerAction>;
struct SamplerContext {
 bool shift=false;uint8_t sampler_page=0;bool drum_kit=false;
 RecordingContext recording{};bool auditioning=false;
 unsigned page=0,history_cursor=0,history_size=0;
};
inline SamplerControl sampler_button_control(unsigned index,const SamplerContext& c){
 if(index<3){const char* names[]={"PLAYBACK","TRIM","SLICES"};SamplerControl d{index==2 && c.drum_kit?"LAYERS":names[index],SamplerAction::Page,true};d.selected=index==c.sampler_page;return d;}
 if(index==3){SamplerControl d{"WAVE/ZONES",SamplerAction::Page,true,c.sampler_page==3?4:3,c.sampler_page==3?4:3};d.selected=c.sampler_page>=3;return d;}
 if(c.shift && index>=4 && index<=6){auto r=sampler_recording_control(index-4,c.recording,c.auditioning);SamplerControl d{r.label,SamplerAction::Record,r.enabled};d.selected=sampler_recording_selected(index-4,c.recording,c.auditioning);return d;}
 if(index==4)return {c.sampler_page==3?"STRETCH":"AUTO CHOP",SamplerAction::Chop,true};
 if(index==5)return {"MAP PADS",SamplerAction::MapPads,true};
 if(index==6)return {"BROWSER",SamplerAction::OpenBrowser,true};
 if(index==7){if(c.shift && c.sampler_page==3)return {"FILE LOOP",SamplerAction::FileLoop,true};if(c.shift && c.sampler_page==4)return {"CLEAR ZONES",SamplerAction::ClearZones,true};return {c.shift?"CHOP STEPS":"LOOP TAKE",c.shift?SamplerAction::ChopSteps:SamplerAction::LoopTake,true};}
 return {};
}
}
