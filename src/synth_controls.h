#pragma once
#include "engine_pages.h"
#include "ui/control_descriptor.h"
namespace groove {
enum class SynthAction : uint8_t {
 None, Generator, Tone, Shape, Trim, Modulation, Lab, Source, Sample,
 Undo, Redo, StoreSound, RecallSound, Profile, WaveMacros, WaveClock,
 WaveTrigger, WaveReference, BakePreset, LayerUse, LayerRemove
};
constexpr ui::TopWork top_work(SynthAction a){switch(a){case SynthAction::None:return ui::TopWork::Empty;case SynthAction::Generator:case SynthAction::Tone:case SynthAction::Shape:case SynthAction::Trim:case SynthAction::Lab:case SynthAction::Profile:case SynthAction::WaveMacros:case SynthAction::WaveClock:return ui::TopWork::Selection;case SynthAction::Source:case SynthAction::Modulation:case SynthAction::Sample:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(SynthAction action){switch(action){case SynthAction::Modulation:return ui::icons::Modulation;case SynthAction::Source:return ui::icons::Source;case SynthAction::Undo:return ui::icons::Undo;case SynthAction::Redo:return ui::icons::Redo;case SynthAction::StoreSound:return ui::icons::Store;case SynthAction::RecallSound:return ui::icons::Recall;case SynthAction::WaveClock:return ui::icons::Clock;case SynthAction::BakePreset:return ui::icons::Bake;case SynthAction::LayerRemove:return ui::icons::Erase;default:return ui::icons::None;}}

using SynthControl = ui::ControlDescriptor<SynthAction>;
struct SynthControlContext {
 Instrument instrument=Instrument::Empty;
 bool shift=false, wave_profile=false, wave_clock=false, reference_armed=false;
 bool can_undo=false, can_redo=false,can_recall=false;
};
// One definition feeds the displayed label, availability and input dispatch.
// Modal and Note Repeat ownership are resolved by Performance before this.
inline SynthControl synth_button_control(unsigned index,const SynthControlContext& c) {
 const bool midi=c.instrument==Instrument::ExternalMidi;
 const bool sample=c.instrument==Instrument::Sampler || c.instrument==Instrument::DrumKit;
 if(index==4)return {"ENGINE",SynthAction::Source,true};
 if(index==5)return {"MODULATION",SynthAction::Modulation,true};
 if(index==6)return {"STORE POOL",SynthAction::StoreSound,c.instrument!=Instrument::Empty && c.instrument!=Instrument::Unknown};
 if(c.shift && index==7 && !midi && c.instrument!=Instrument::DrumKit && c.instrument!=Instrument::Empty && c.instrument!=Instrument::Unknown)return {"BAKE PRESET",SynthAction::BakePreset,true};
 if(index==7)return c.can_recall?SynthControl{"RECALL POOL",SynthAction::RecallSound,true}:SynthControl{};

 if(c.shift && index==3 && midi)return {c.wave_profile?"PROFILE ON":"WAVE PROFILE",SynthAction::Profile,true};
 if(index>=4)return {};
 if(c.wave_profile){
  switch(index){
   case 0:return {"MACROS",SynthAction::WaveMacros,true};
   case 1:return {"SN CLOCK",SynthAction::WaveClock,true};
   case 2:return {c.wave_clock?"SN TRIGGER":"",SynthAction::WaveTrigger,c.wave_clock};
   case 3:return {c.wave_clock?(c.reference_armed?"CONFIRM REF":"SET REF"):"",SynthAction::WaveReference,c.wave_clock};
  }
 }
 if(c.instrument==Instrument::Layer){if(index==0)return {"LAYER",SynthAction::Generator,true};if(index==2)return {"USE CHILD",SynthAction::LayerUse,true};if(index==3)return {"REMOVE",SynthAction::LayerRemove,true};}
 if(c.instrument==Instrument::Texture){if(index==0)return {"GRAINS",SynthAction::Generator,true};if(index==2)return {"BLEND",SynthAction::Shape,true};if(index==3)return {"INPUT / QUALITY",SynthAction::Lab,true};}
 if(c.instrument==Instrument::Resonator){if(index==0)return {"RESONATOR",SynthAction::Generator,true};if(index==2)return {"EXCITER",SynthAction::Shape,true};}
 switch(index){
  case 0:return {midi?"MIDI OUT":c.instrument==Instrument::DrumKit?"KIT":"GENERATOR",SynthAction::Generator,true};
  case 1:return {"TONE",SynthAction::Tone,!midi};
  case 2:{
   if(sample)return {"TRIM",SynthAction::Trim,true};
   const auto* page=engine_page_descriptor(unsigned(c.instrument));
   if(!page)return {"",SynthAction::Shape,!midi};
   switch(page->shape){
    case ShapeKind::None:return {};
    case ShapeKind::Lpg:return {"LPG",SynthAction::Shape,true};
    case ShapeKind::FolderCurve:return {"FOLD / LPG",SynthAction::Shape,true};
    case ShapeKind::FixedCurve:return {"FIXED ENV",SynthAction::Shape,true};
    case ShapeKind::Envelope:return {"ENVELOPE",SynthAction::Shape,true};
   }
   return {};
  }
  case 3:return unsigned(c.instrument)>=unsigned(Instrument::WaveSequence) && unsigned(c.instrument)<=unsigned(Instrument::SpectralFreeze)
   ?SynthControl{"ENGINE LAB",SynthAction::Lab,true}
   :SynthControl{};
  default:return {};
 }
}
}
