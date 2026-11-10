#pragma once
#include "future_factory_presets.h"
#include "audio_stress_assets.h"
namespace groove {
// Draft revision 2 (Daisy drum replacement; not comparable to revision1). Deliberately empty pool; attached engine state only.
inline void make_wave4_workload(ProjectState& p,unsigned which,const AudioStressAssets* streams=nullptr){
 make_empty_project(p);p.tempo=120;p.random_seed=0x57340001;
 constexpr Instrument engines[]={Instrument::Additive,Instrument::PhaseDist,Instrument::VirtualAnalog,Instrument::Wavetable,Instrument::Fm6,Instrument::PlaitsMacro,Instrument::FM,Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,Instrument::Acid,Instrument::Resonator,Instrument::Texture,Instrument::ChipClock,Instrument::WestCoast,Instrument::PolySaw};
 for(unsigned t=0;t<16;++t){auto v=track_at(p,0,t);v.instrument=engines[t];if(t>=7 && t<=9)v.engine_parameters[0]=float(t-7)*.5f;v.level=.12f;v.root_note=uint8_t(48+t%12);v.length=16;p.clips[t].playing=true;for(unsigned n=0;n<16;n+=4){auto& s=v.steps[n];s.enabled=true;s.note=v.root_note;s.velocity=90;s.gate=40;}}
 if(which==1 || which==2){unsigned layers=which==1?2:4;for(unsigned t=0;t<4;++t){auto v=track_at(p,0,t);v.instrument=Instrument::Layer;v.layer.count=uint8_t(layers);for(unsigned i=0;i<layers;++i){auto& child=v.layer.parts[i];child.sound.instrument=i%2?Instrument::PhaseDist:Instrument::Additive;child.level=1.f/layers;child.pan=layers>1?float(i)*2/(layers-1)-1:0;child.detune=float(i)*7;}}}
 if(which==3){for(unsigned t=0;t<16;++t){auto v=track_at(p,0,t);v.instrument=Instrument::SoundFont;v.sample_slot=4;}}
 if(which==4 && streams){p.song.enabled=true;p.song.length=1;p.song.sections[0].bars=64;for(unsigned i=0;i<4;++i){p.assets[i]=streams->assets[i];auto& a=p.audio[12+i];a.kind=i<2?TrackKind::Looper:TrackKind::Audio;a.length=LoopLength::Free;a.source=0;a.gain=.1f;a.clips[0]={0,0,AudioStressAssets::Frames,1,uint8_t(i)};}}
}
}
