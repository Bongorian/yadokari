#pragma once
#include "ui/control_descriptor.h"
#include "ui_axes.h"
#include "engine_registry.h"
namespace groove {
enum class StatusAction:uint8_t {TransportPlay,TransportStop,Record,Follow,Lock,Metronome,Clock,StripPerform,StripPitch,StripMod,StripNotes,Family};
constexpr uint8_t icon_for(StatusAction action){using namespace ui::icons;switch(action){
 case StatusAction::TransportPlay:return Play;case StatusAction::TransportStop:return Stop;case StatusAction::Record:return Record;
 case StatusAction::Follow:return Follow;case StatusAction::Lock:return Lock;case StatusAction::Metronome:return Metronome;case StatusAction::Clock:return Clock;
 case StatusAction::StripPerform:return StripPerform;case StatusAction::StripPitch:return Pitch;case StatusAction::StripMod:return StripMod;case StatusAction::StripNotes:return Notes;default:return None;}}
using StatusControl=ui::ControlDescriptor<StatusAction>;
struct HeaderStatusContext {bool playing=false,recording=false,armed=false,follow=false,lock=false,metronome=false,external_clock=false;StripMode strip=StripMode::Perform;};
inline StatusControl header_status(unsigned index,const HeaderStatusContext& c){StatusControl d;switch(index){
 case 0:d={c.recording?"Recording":c.armed?"Record armed":c.playing?"Playing":"Stopped",c.recording || c.armed?StatusAction::Record:c.playing?StatusAction::TransportPlay:StatusAction::TransportStop,true};d.selected=c.recording || c.playing;break;
 case 1:d={"Follow",StatusAction::Follow,true};d.selected=c.follow;break;
 case 2:d={"Lock editing",StatusAction::Lock,true};d.selected=c.lock;break;
 case 3:d={"Metronome",StatusAction::Metronome,true};d.selected=c.metronome;break;
 case 4:d={"External clock",StatusAction::Clock,true};d.selected=c.external_clock;break;
 case 5:d=c.strip==StripMode::Pitch?StatusControl{"Strip pitch",StatusAction::StripPitch,true}:c.strip==StripMode::Mod?StatusControl{"Strip modulation",StatusAction::StripMod,true}:c.strip==StripMode::Notes?StatusControl{"Strip notes",StatusAction::StripNotes,true}:StatusControl{"Strip perform",StatusAction::StripPerform,true};d.selected=true;break;
 default:break;}return d;}
inline const char* engine_family_name(Instrument instrument){const auto* d=instrument_descriptor(unsigned(instrument));return d?d->family:"";}
// Presentation order is independent of the numeric IDs used in saved projects.
inline constexpr Instrument kEngineDisplayOrder[]={
 Instrument::DaisyAnalogDrums,Instrument::DaisySyntheticDrums,
 Instrument::PolySaw,Instrument::DaisySynth,Instrument::Acid,Instrument::Empty,
 Instrument::Wavetable,Instrument::VirtualAnalog,Instrument::Additive,Instrument::PhaseDist,Instrument::Layer,Instrument::Formant,
 Instrument::Sampler,Instrument::SoundFont,Instrument::Granular,
 Instrument::PlaitsMacro,Instrument::FM,Instrument::Fm6,
 Instrument::WaveSequence,Instrument::ChipClock,Instrument::TapeMicrosound,Instrument::WestCoast,Instrument::SpectralFreeze,
 Instrument::ExternalMidi,Instrument::DrumKit,
 Instrument::Resonator,Instrument::Texture,Instrument::Chimera
};
static_assert(sizeof kEngineDisplayOrder/sizeof *kEngineDisplayOrder==unsigned(Instrument::Unknown));
inline int engine_display_index(Instrument engine){
 for(unsigned i=0;i<instrument_count();++i)if(kEngineDisplayOrder[i]==engine)return int(i);
 return int(instrument_count());
}
inline Instrument step_engine_display(Instrument engine,int delta){
 int next=engine_display_index(engine)+delta;
 if(next<0)next=0;
 if(next>=int(instrument_count()))next=int(instrument_count())-1;
 return kEngineDisplayOrder[next];
}
inline StatusControl engine_family_control(Instrument instrument){
 const auto* d=instrument_descriptor(unsigned(instrument));if(!d || instrument==Instrument::Empty || instrument==Instrument::Unknown)return {};
 const char* family=engine_family_name(instrument);
 struct Family {const char* name;uint8_t icon;};
 static constexpr Family families[]={{"DRUM",ui::icons::Drum},{"SYNTH",ui::icons::Synth},{"SAMPLER",ui::icons::Sampler},{"MACRO",ui::icons::Macro},{"FM",ui::icons::Fm},{"CREATIVE",ui::icons::Creative},{"MIDI",ui::icons::Midi},{"KIT",ui::icons::Kit},{"PHYSICAL",ui::icons::Physical}};
 if(instrument==Instrument::Layer)return {family,StatusAction::Family,true,0,1,1,false,ui::icons::Layer};
 for(const auto& f:families)if(!__builtin_strcmp(family,f.name))return {family,StatusAction::Family,true,0,1,1,false,f.icon};
 return {family,StatusAction::Family,true};
}
}
