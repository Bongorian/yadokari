#pragma once
#include "benchmark_project.h"
#include "future_factory_presets.h"
namespace groove {
inline constexpr unsigned kDspProbeRevision=6,kWave4ProbeRevision=5; // Appended Wavetable cases preserve earlier case IDs.
struct DspCondition {
 enum Kind:unsigned {Idle,Leaf,Insert,Native,Voices,Layers,Chain,Extension,Saturate,Choir,Wavetable,WavetableUnison};
 Kind kind=Idle;Instrument engine=Instrument::Empty;unsigned variant=0,tracks=1,notes=1,second_inserts=0;
};
inline unsigned dsp_variants(Instrument i){return i==Instrument::PlaitsMacro?24:i==Instrument::Texture?16:i==Instrument::Resonator || i==Instrument::DaisyAnalogDrums || i==Instrument::DaisySyntheticDrums?3:1;}
inline bool dsp_leaf(Instrument i){return i!=Instrument::ExternalMidi && i!=Instrument::DrumKit && i!=Instrument::Empty && i!=Instrument::Layer;}
inline unsigned dsp_leaf_cases(){unsigned count=1;for(unsigned i=0;i<instrument_count();++i)if(dsp_leaf(Instrument(i)))count+=dsp_variants(Instrument(i));return count;}
inline unsigned dsp_legacy_stress_start(){return dsp_leaf_cases()+kFxCount;}
inline unsigned dsp_stress_start(){return dsp_legacy_stress_start()+16+3+3+4+1;}
inline unsigned dsp_wavetable_start(){return dsp_stress_start()+56;}
// Stock settings, moving table, PM alone, then each mode at both polarities
// with and without PM. Each configuration runs at one and four held notes.
constexpr unsigned kDspWavetableVariants=3+4*4;
struct WavetableProbeSetting {unsigned voices,notes,tracks;bool extended;unsigned base_note=60;bool bend=false;};
inline constexpr WavetableProbeSetting kUnisonProbes[]={
 {2,1,1,true},{1,3,1,true},{2,2,1,true},{5,1,1,true},{6,1,1,true},
 {7,1,1,false},{7,2,1,false},{3,4,1,false},{2,2,4,true},
 {1,5,3,true,84},{1,5,3,true,118},{1,5,3,true,81,true}
};
inline unsigned dsp_probe_note(const DspCondition& c,unsigned n){
 if(c.kind==DspCondition::WavetableUnison && kUnisonProbes[c.variant].base_note!=60)
  return kUnisonProbes[c.variant].base_note+n%2;
 return 60+3*n;
}
inline bool dsp_register_bend(const DspCondition& c){return c.kind==DspCondition::WavetableUnison && kUnisonProbes[c.variant].bend;}
inline unsigned dsp_unison_start(){return dsp_wavetable_start()+2*kDspWavetableVariants;}
inline unsigned dsp_case_count(){return dsp_unison_start()+sizeof(kUnisonProbes)/sizeof(*kUnisonProbes);}
inline DspCondition dsp_condition(unsigned index){
 if(index>=dsp_unison_start()){
  const unsigned row=index-dsp_unison_start();const auto& c=kUnisonProbes[row];
  return {DspCondition::WavetableUnison,Instrument::Wavetable,row,c.tracks,c.notes};
 }
 if(index>=dsp_wavetable_start()){
  const unsigned row=index-dsp_wavetable_start();
  return {DspCondition::Wavetable,Instrument::Wavetable,row%kDspWavetableVariants,1,row<kDspWavetableVariants?1u:4u};
 }
 if(index>=dsp_stress_start()+16){
  unsigned row=index-dsp_stress_start()-16;
  if(row<16)return {DspCondition::Insert,Instrument::VirtualAnalog,kTalkFx,row+1,1};
  row-=16;
  if(row<8)return {DspCondition::Choir,Instrument::Chimera,row%4+1,1,row<4?1u:4u};
  return {DspCondition::Insert,Instrument::VirtualAnalog,kTalkFx,16,1,row-7};
 }
 if(index>=dsp_stress_start()){static const DspCondition cases[]={
 {DspCondition::Extension,Instrument::Formant,0,1,8},{DspCondition::Extension,Instrument::Formant,1,1,8},
 {DspCondition::Extension,Instrument::VirtualAnalog,2,1,8},{DspCondition::Extension,Instrument::VirtualAnalog,3,1,8},
 {DspCondition::Extension,Instrument::Granular,4,1,2},{DspCondition::Extension,Instrument::Granular,5,1,4},
 {DspCondition::Extension,Instrument::VirtualAnalog,6,1,1},{DspCondition::Extension,Instrument::VirtualAnalog,7,1,1},
 {DspCondition::Extension,Instrument::VirtualAnalog,8,1,1},
 // Saturating fixtures (perf investigation): shared-pool pressure that
 // the stock cases never create. The probe re-hits these every 500 ms
 // with rotating pitches (see DspProbe::trigger), so held voices from
 // successive rounds coexist and pool exhaustion (held steals) plus
 // reserve_shared budget trims actually fire inside the window.
 {DspCondition::Saturate,Instrument::PlaitsMacro,2,8,8},
 {DspCondition::Saturate,Instrument::Granular,4,4,8},
 {DspCondition::Saturate,Instrument::PlaitsMacro,6,8,8},
 // NativeBudget discriminators (perf investigation): the four heavy-burst
 // Texture modes whose scaled admission overran the block deadline in the
 // v1020 full suite (cases 94-97), duplicated into the stress set so a
 // short stress run can judge burst-limit recalibration.
 {DspCondition::Extension,Instrument::Texture,12,16,1},
 {DspCondition::Extension,Instrument::Texture,13,16,1},
 {DspCondition::Extension,Instrument::Texture,14,16,1},
 {DspCondition::Extension,Instrument::Texture,15,16,1}};return cases[index-dsp_stress_start()<16?index-dsp_stress_start():0];}
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
// Retriggered cases: engines whose sound is a one-shot utterance/decay
// rather than a sustained render. Plaits models 15 (Speech) and 18
// (Particle noise) belong here: under the probe's single held trigger
// their output reaches digital silence within the first quanta, the
// product silence gate (PlaitsVoice O-01) shuts the voice, and the
// per-voice timing bin is never populated (PI5_PERF_PROFILE.md S9.4).
// Periodic retriggering is the same treatment Acid / Daisy drums /
// Resonator already get, and it exercises the same render path a
// sequencer step would.
inline bool dsp_retrigger(Instrument i,unsigned variant=0){return i==Instrument::Acid || i==Instrument::DaisyAnalogDrums || i==Instrument::DaisySyntheticDrums || i==Instrument::Resonator || (i==Instrument::PlaitsMacro && (variant==15 || variant==18));}
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
 if(c.kind==DspCondition::Insert)for(unsigned t=0;t<c.tracks;++t){
  auto& fx=p.tracks[t].insert_fx;fx[0]=float(c.variant);fx[1]=.7f;fx[2]=.4f;fx[3]=.75f;
  if(t<c.second_inserts){fx[4]=float(c.variant);fx[5]=.2f;fx[6]=.85f;fx[7]=.75f;}
 }
 if(c.kind==DspCondition::Wavetable){
  auto& t=p.tracks[0];t.plaits_model=0;
  if(c.variant>0){
   auto& v=t.wavetable.values;v[0]=52.375f;v[1]=31.5f;v[2]=3.7f;
   if(c.variant>=3){
    const unsigned setting=c.variant-3;
    v[WavetableWarpMode]=float(setting/4);
    v[WavetableWarpAmount]=(setting&1)?-.91f:.91f;
   }
   if(c.variant==2 || (c.variant>=3 && ((c.variant-3)&2))){
    v[WavetablePmRatio]=7.25f;v[WavetablePmAmount]=.89f;
   }
  }
 }
 if(c.kind==DspCondition::WavetableUnison){
  const auto& spec=kUnisonProbes[c.variant];
  for(unsigned i=0;i<c.tracks;++i){
   auto& t=p.tracks[i];t.plaits_model=0;auto& v=t.wavetable.values;
   v[0]=52.375f;v[1]=31.5f;v[2]=3.7f;v[WavetableUnisonCount]=float(spec.voices);
   v[WavetableDetune]=36;v[WavetableSpread]=1;
   if(spec.extended){v[WavetableWarpMode]=1;v[WavetableWarpAmount]=-.91f;v[WavetablePmAmount]=.89f;}
  }
 }
 if(c.kind==DspCondition::Choir)p.tracks[0].chimera.values[ChimeraUnison-kChimeraMacroCount]=float(c.variant);
}
}
