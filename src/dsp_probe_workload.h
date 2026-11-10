#pragma once
#include "benchmark_project.h"
#include "future_factory_presets.h"
namespace groove {
inline constexpr unsigned kDspProbeRevision=5,kWave4ProbeRevision=3; // engine retirement changes coverage and the legacy mixed score.
struct DspCondition {
 enum Kind:unsigned {Idle,Leaf,Insert,Native,Voices,Layers,Chain,Extension};
 Kind kind=Idle;Instrument engine=Instrument::Empty;unsigned variant=0,tracks=1,notes=1;
};
inline unsigned dsp_variants(Instrument i){return i==Instrument::PlaitsMacro?24:i==Instrument::Texture?16:i==Instrument::Resonator || i==Instrument::DaisyAnalogDrums || i==Instrument::DaisySyntheticDrums?3:1;}
inline bool dsp_leaf(Instrument i){return i!=Instrument::ExternalMidi && i!=Instrument::DrumKit && i!=Instrument::Empty && i!=Instrument::Layer;}
inline unsigned dsp_leaf_cases(){unsigned count=1;for(unsigned i=0;i<instrument_count();++i)if(dsp_leaf(Instrument(i)))count+=dsp_variants(Instrument(i));return count;}
inline unsigned dsp_legacy_stress_start(){return dsp_leaf_cases()+kFxCount;}
inline unsigned dsp_stress_start(){return dsp_legacy_stress_start()+16+3+3+4+1;}
inline unsigned dsp_case_count(){return dsp_stress_start()+9;}
inline DspCondition dsp_condition(unsigned index){
 if(index>=dsp_stress_start()){static const DspCondition cases[]={
 {DspCondition::Extension,Instrument::Formant,0,1,8},{DspCondition::Extension,Instrument::Formant,1,1,8},
 {DspCondition::Extension,Instrument::VirtualAnalog,2,1,8},{DspCondition::Extension,Instrument::VirtualAnalog,3,1,8},
 {DspCondition::Extension,Instrument::Granular,4,1,2},{DspCondition::Extension,Instrument::Granular,5,1,4},
 {DspCondition::Extension,Instrument::VirtualAnalog,6,1,1},{DspCondition::Extension,Instrument::VirtualAnalog,7,1,1},
 {DspCondition::Extension,Instrument::VirtualAnalog,8,1,1}};return cases[index-dsp_stress_start()<9?index-dsp_stress_start():0];}
 if(!index)return {};
 unsigned n=index-1;
 for(unsigned i=0;i<instrument_count();++i)if(dsp_leaf(Instrument(i))){unsigned count=dsp_variants(Instrument(i));if(n<count)return {DspCondition::Leaf,Instrument(i),n,1,1};n-=count;}
 if(n<kFxCount){return {DspCondition::Insert,Instrument::VirtualAnalog,n,1,1};}n-=kFxCount;
 if(n<16){return {DspCondition::Native,Instrument::Texture,n,16,1};}n-=16;
 if(n<3){return {DspCondition::Native,Instrument::Resonator,n,16,1};}n-=3;
 if(n<3){constexpr Instrument engines[]={Instrument::Fm6,Instrument::VirtualAnalog,Instrument::Additive};return {DspCondition::Voices,engines[n],0,16,4};}n-=3;
 if(n<4)return {DspCondition::Layers,Instrument::Layer,n,16,n>=2?4u:1u};
 return {DspCondition::Chain,Instrument::DrumKit,0,16,1};
}
inline bool dsp_retrigger(Instrument i){return i==Instrument::Acid || i==Instrument::DaisyAnalogDrums || i==Instrument::DaisySyntheticDrums || i==Instrument::Resonator;}
inline void dsp_set_sound(TrackSettings& state,Instrument engine,unsigned variant){
 state.instrument=engine;state.level=.12f;state.cutoff=1;state.root_note=60;state.plaits_model=uint8_t(variant);state.sample_loop=true;
 if(engine==Instrument::Texture){state.texture.values[0]=float(variant/4);state.texture.values[11]=float(variant%4);state.texture.values[12]=52;state.texture.values[2]=state.texture.values[4]=state.texture.values[7]=state.texture.values[8]=state.texture.values[9]=1;}
 if(engine==Instrument::Resonator){state.resonator.values[0]=float(variant);state.resonator.values[1]=2;state.resonator.values[2]=.9995f;state.resonator.values[3]=state.resonator.values[4]=1;state.resonator.values[5]=.5f;}
 if(engine==Instrument::DaisyAnalogDrums || engine==Instrument::DaisySyntheticDrums)state.engine_parameters[0]=(float(variant)+.1f)/3;
 if(engine==Instrument::SoundFont)state.sample_slot=4;
 if(engine==Instrument::Layer){state.layer.count=uint8_t(variant%2?4:2);for(unsigned l=0;l<state.layer.count;++l){auto& p=state.layer.parts[l];p.sound.instrument=l%2?Instrument::PhaseDist:Instrument::Additive;p.level=1.f/state.layer.count;p.pan=l%2?.35f:-.35f;p.detune=float(l)*7;}}
}
inline void make_dsp_probe_project(ProjectState& p,const DspCondition& c){
 if(c.kind==DspCondition::Chain)make_fx_stress_project(p);else{make_empty_project(p);for(unsigned t=0;t<16;++t){p.tracks[t].instrument=Instrument::Empty;p.clips[t].playing=false;for(auto& s:p.patterns[0].tracks[t].steps)s.enabled=false;}for(unsigned t=0;t<c.tracks && c.kind!=DspCondition::Idle;++t)dsp_set_sound(p.tracks[t],c.engine,c.variant);}
 if(c.kind==DspCondition::Extension){auto& t=p.tracks[0];t.level=.12f;t.plaits_model=0;
  if(c.engine==Instrument::Formant)t.formant.values[0]=float(c.variant);
  if(c.variant==3){auto& v=t.virtual_analog.values;v[18]=1;v[19]=.5f;v[20]=12;v[23]=2;v[25]=.5f;}
  if(c.variant>=6){t.insert_fx[0]=c.variant==7?21:20;t.insert_fx[1]=c.variant==7?.5f:19.f/24;t.insert_fx[2]=.4f;t.insert_fx[3]=1;}
  if(c.variant==8){t.insert_fx[4]=21;t.insert_fx[5]=.5f;t.insert_fx[6]=.4f;t.insert_fx[7]=.7f;t.delay_send=t.reverb_send=.3f;p.effects.delay_mix=p.effects.reverb_mix=.3f;}
 }
 p.random_seed=0x57313001;p.tempo=120;p.routing.main=.08f;p.routing.cue=0;
 if(c.kind==DspCondition::Insert){p.tracks[0].insert_fx[0]=float(c.variant);p.tracks[0].insert_fx[1]=.7f;p.tracks[0].insert_fx[2]=.4f;p.tracks[0].insert_fx[3]=.75f;}
}
}
