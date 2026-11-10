#pragma once
#include "led_state.h"
#include "engine_kind.h"
namespace groove { namespace ui {
// Fixed semantic pad palette; previews decode final wire bytes. The theme
// affects the LCD only, never the pad hue or pulse grammar.
struct PadPalette {
 static constexpr unsigned ready=10,focus=17,active=7,pending=4,record=1;
 static constexpr PadGround neutral{EngineKind::Empty,true};
 static constexpr uint64_t trigger_ms=120;
 static unsigned percent(LedState state,uint64_t ms,Motion motion){
  switch(state){
   case LedState::Off:case LedState::Idle:return 0;
   case LedState::Muted:case LedState::MutedPattern:case LedState::MutedBoth:return 18;
   case LedState::Assigned:return 55;
   case LedState::Queued:case LedState::Armed:case LedState::Warning:return motion==Motion::Off || (ms/(state==LedState::Warning && motion==Motion::Full?250:500))%2==0?100:18;
   default:return 100;
  }
 }
 // Semantic wire hues are independent of all nine display themes.
 static unsigned ground_token(PadGround ground){
  if(ground.neutral)return ready;
  switch(ground.kind){case EngineKind::Synth:return 10;case EngineKind::Sampler:return 7;case EngineKind::Kit:return 4;case EngineKind::Layer:return 14;case EngineKind::Audio:return 8;case EngineKind::Midi:return 15;case EngineKind::Empty:return 17;}
  return ready;
 }
 static unsigned token(LedState state,PadGround ground){
  switch(state){
   case LedState::Playing:case LedState::Triggered:case LedState::PerformanceActive:return active;
   case LedState::Recording:case LedState::Armed:return record;
   case LedState::Warning:case LedState::Queued:case LedState::Finalizing:return pending;
   case LedState::Selected:case LedState::Editing:return focus;
   default:return ground_token(ground);
  }
 }
 static uint32_t base(LedState state,const Theme&,PadGround ground){return mk3::indexed_appearance(token(state,ground));}
 static uint8_t encode(LedState state,uint64_t ms,Motion motion,const Theme& =Theme::by_index(Theme::kDefault),PadGround ground={}){
  unsigned p=percent(state,ms,motion);unsigned level=p==100?3:p==55?2:1;
  if(state==LedState::Assigned && !ground.neutral && ground.kind==EngineKind::Empty)level=1;
  return p?mk3::indexed(token(state,ground),level):0;
 }
 static uint32_t rgb(LedState state,const Theme& theme,PadGround ground,uint64_t ms,Motion motion){return LedStateEngine::preview(encode(state,ms,motion,theme,ground),theme);}
};
enum class PadRole:uint8_t { Disabled, Tracks, TrackSelect, Kit, KitSelect, Notes, Chords, StepChord, Steps, Clips, Scenes, Slices, Files, Assets, Settings, Cells, Routes, Scale, Generate, LiveFx, Mute, KitMute, Solo, KitSolo, SongMute };
inline const char* pad_role_label(PadRole role){
 switch(role){
  case PadRole::Disabled:return "PADS: INACTIVE";
  case PadRole::Tracks:return "PADS: PLAY TRACKS";
  case PadRole::TrackSelect:return "PADS: SELECT TRACK";
  case PadRole::Kit:return "PADS: PLAY KIT";
  case PadRole::KitSelect:return "PADS: SELECT SOUND";
  case PadRole::Notes:return "PADS: PLAY NOTES";
  case PadRole::Chords:return "PADS: PLAY CHORDS";
  case PadRole::StepChord:return "PADS: STEP CHORD";
  case PadRole::Steps:return "PADS: TOGGLE STEPS";
  case PadRole::Clips:return "PADS: LAUNCH CLIP";
  case PadRole::Scenes:return "PADS: LAUNCH SCENE";
  case PadRole::Slices:return "PADS: PLAY SLICES";
  case PadRole::Files:return "PADS: SELECT FILE";
  case PadRole::Assets:return "PADS: SELECT ASSET";
  case PadRole::Settings:return "PADS: SETTINGS";
  case PadRole::Cells:return "PADS: SELECT CELL";
  case PadRole::Routes:return "PADS: ROUTE TRACKS";
  case PadRole::Scale:return "PADS: SCALE NOTES";
  case PadRole::Generate:return "PADS: PULSE COUNT";
  case PadRole::LiveFx:return "PADS: FX / MORPH";
  case PadRole::Mute:return "PADS: TRACK MUTE";
  case PadRole::KitMute:return "PADS: KIT MUTE";
  case PadRole::Solo:return "PADS: TRACK SOLO";
  case PadRole::KitSolo:return "PADS: KIT SOLO";
  case PadRole::SongMute:return "PADS: SONG MASK";
 }
 return "PADS";
}
}}
