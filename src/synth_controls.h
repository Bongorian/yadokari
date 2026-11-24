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

// Top 4 cycles these banks; views 0..4 belong to the original synth pages.
constexpr unsigned kFirstSynthDetailView=5;
struct SynthDetailBank {const char* name;uint8_t locals[8];};
inline const SynthDetailBank* synth_detail_bank(Instrument inst,unsigned view){
 if(view<kFirstSynthDetailView)return nullptr;
 const unsigned bank=view-kFirstSynthDetailView;
 // Zero marks an empty knob. Local zero already lives on GENERATOR.
 static constexpr SynthDetailBank wavetable={"WARP / PM / UNISON",{WavetableWarpMode,WavetableWarpAmount,WavetablePmRatio,WavetablePmAmount,WavetableUnisonCount,WavetableDetune,WavetableSpread,WavetableTable}};
 static constexpr SynthDetailBank va[]={
  {"OSC / PWM 1/3",{6,7,22,16,8,9,10,18}},
  {"FILTER 2/3",{4,5,15,17,23,24,25}},
  {"PITCH / MOD 3/3",{19,20,21}}
 };
 static constexpr SynthDetailBank fm[]={
  {"OP LEVEL 1/3",{5,6,7,8,9,10}},
  {"OP DETUNE 2/3",{11,12,13,14,15,16}},
  {"ALGORITHM 3/3",{17,18,19,20}}
 };
 static constexpr SynthDetailBank additive[]={
  {"HARMONICS 1-8",{9,10,11,12,13,14,15,16}},
  {"HARMONICS 9-16",{17,18,19,20,21,22,23,24}}
 };
 switch(inst){
  case Instrument::Wavetable:return bank==0?&wavetable:nullptr;
  case Instrument::VirtualAnalog:return bank<3?&va[bank]:nullptr;
  case Instrument::Fm6:return bank<3?&fm[bank]:nullptr;
  case Instrument::Additive:return bank<2?&additive[bank]:nullptr;
  default:return nullptr;
 }
}
constexpr unsigned synth_top_for_view(unsigned view){
 return view>=kFirstSynthDetailView?3:view==4?2:view;
}
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
 if(index==3 && synth_detail_bank(c.instrument,kFirstSynthDetailView))return {"DETAILS",SynthAction::Lab,true};
 if(c.instrument==Instrument::Layer){if(index==0)return {"LAYER",SynthAction::Generator,true};if(index==2)return {"USE CHILD",SynthAction::LayerUse,true};if(index==3)return {"REMOVE",SynthAction::LayerRemove,true};}
 if(c.instrument==Instrument::Chimera){if(index==2)return {"ARTICULATION",SynthAction::Shape,true};if(index==3)return {"VOICE",SynthAction::Lab,true};}
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
