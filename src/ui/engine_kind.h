#pragma once
#include "../project.h"
#include "theme.h"
namespace groove { namespace ui {
enum class EngineKind:uint8_t {Synth,Sampler,Kit,Layer,Audio,Midi,Empty};
struct EngineKindChip {const char* name;uint32_t color;};
inline EngineKind engine_kind(Instrument instrument,TrackKind kind=TrackKind::Instrument){
 if(kind!=TrackKind::Instrument)return EngineKind::Audio;
 switch(instrument){case Instrument::Empty:return EngineKind::Empty;case Instrument::ExternalMidi:return EngineKind::Midi;case Instrument::Sampler:return EngineKind::Sampler;case Instrument::DrumKit:return EngineKind::Kit;case Instrument::Layer:return EngineKind::Layer;default:return EngineKind::Synth;}
}
inline const EngineKindChip& engine_kind_chip(EngineKind kind){
 static constexpr EngineKindChip chips[]={{"SYNTH",0xff8ab4f8},{"SAMPLER",0xff70d5aa},{"KIT",0xffe8b36a},{"LAYER",0xffc4a4f2},{"AUDIO",0xff61cde6},{"MIDI",0xffff94b8},{"EMPTY",0xff8a94a0}};
 return chips[unsigned(kind)<7?unsigned(kind):6];
}
// One source for LCD ground, kind chips and LED ground. The locked theme's
// event tokens are intentionally independent of this monochrome projection.
inline uint32_t engine_kind_color(EngineKind kind,const Theme& theme){
 const uint32_t rgb=engine_kind_chip(kind).color;
 if(!theme.monochrome)return rgb;
 const unsigned y=(54*(rgb>>16&255)+183*(rgb>>8&255)+19*(rgb&255)+128)>>8;
 return 0xff000000u|y*0x010101u;
}
inline EngineKind track_engine_kind(const ProjectState& p,unsigned track){
 return track<16?engine_kind(p.tracks[track].instrument,p.audio[track].kind):EngineKind::Empty;
}
inline uint32_t track_color(const ProjectState& p,unsigned track,const Theme& theme){return engine_kind_color(track_engine_kind(p,track),theme);}
struct PadGround {EngineKind kind=EngineKind::Synth;bool neutral=false;};
}}
