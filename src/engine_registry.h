#pragma once
#include "native_budget.h"
#include "note_limits.h"
#include "instrument_types.h"
#include "parameters.h"
#include "engine_state_codec.h"
#include "visual_model.h"
namespace groove {
enum EnvelopeKind : uint8_t { EnvelopeNone=0, EnvelopeFixed=1, EnvelopeEditable=2 };
struct InstrumentDescriptor {Instrument id;const char* name;bool requires_48k;unsigned macro_controls=0;const char* family="SYNTH";uint8_t cpu_class=1,envelope=EnvelopeNone;};
enum class EngineAssetMode:uint8_t {None,Sample,Kit,OptionalSample,WaveRing,SoundFont};
struct EngineGuidInfo {const char* guid;bool uses_sampler,uses_creative,uses_kit,implemented;EngineAssetMode assets=EngineAssetMode::None;};
enum class HeroVisual : uint8_t { None, Scope, Harmonics, WavetableScan, Lanes, Clock, Reel, Fold, Spectrum, FlowFM, FilterAcid, SawFilter, PlaitsModel, Waveform, MidiInfo, Resonator, Texture, Oscillators };
enum class LabTail : uint8_t { None, SampleSlot, Tune, Fold };
enum class ShapeKind : uint8_t { None, Envelope, Lpg, FolderCurve, FixedCurve };
enum class EngineParameterScope:uint8_t { None, Track, Engine };
struct EngineParameterReference {EngineParameterScope scope=EngineParameterScope::None;uint16_t index=0;};
inline constexpr EngineParameterReference track_control(unsigned id){return id?EngineParameterReference{EngineParameterScope::Track,uint16_t(id)}:EngineParameterReference{};}
inline constexpr EngineParameterReference engine_control(unsigned local){return {EngineParameterScope::Engine,uint16_t(local)};}
struct EnginePageDescriptor {HeroVisual hero;ShapeKind shape;EngineParameterReference core[8];LabTail lab_tail=LabTail::None;};
enum class EngineDisplay : uint8_t { Percent,Milliseconds80,Pitch24,Rate2,Megahertz16,Divider1022,FourChoices,Varispeed,GrainMilliseconds,Fold7,Phase8,Ratio31,Decay1800,Tilt12,Partials123,Native };
// The declaration index is local to this engine. legacy_id is a stage-0 bridge,
// never a new allocation in the global ID space and never an engine identity.
enum class EngineValueTarget:uint8_t { None, Macro, Model, Sample, Envelope, Resonator, Texture, Wavetable, VirtualAnalog, Fm6, Additive, PhaseDist, Formant, Granular, Layer, SoundFont };
enum class EngineEditTier:uint8_t { Performance, Detail };
enum class EngineParameterType:uint8_t { Float, Integer, Boolean, Choice };
struct EngineParameterUi {EngineEditTier tier=EngineEditTier::Detail;EngineParameterType type=EngineParameterType::Float;const char* group="Engine";};
inline EngineParameterType declared_parameter_type(const ParameterDescriptor& p,bool choices=false){return choices?EngineParameterType::Choice:p.interpolation!=Interpolation::Discrete?EngineParameterType::Float:p.minimum==0 && p.maximum==1?EngineParameterType::Boolean:EngineParameterType::Integer;}
struct EngineParameterDeclaration {ParameterDescriptor value;uint16_t legacy_id=0;EngineDisplay display=EngineDisplay::Native;const char* const* choices=nullptr;uint8_t choice_count=0;EngineValueTarget target=EngineValueTarget::None;uint8_t offset=0;EngineParameterUi ui{};};
struct EngineParameterTable {const EngineParameterDeclaration* declarations=nullptr;uint16_t count=0;};
// A bounded view of voice-owned targets. Binding never owns or allocates DSP
// resources; the audio owner resolves values in declaration order, then applies
// them here at the existing trigger/control boundary.
struct EngineVoiceTargets {float* macros=nullptr;uint8_t* model=nullptr;SampleEdit* sample=nullptr;float* envelope=nullptr;float* native=nullptr;};
inline float SampleEdit::* sample_parameter_member(unsigned offset){static constexpr float SampleEdit::* members[]={&SampleEdit::speed,&SampleEdit::start,&SampleEdit::end,&SampleEdit::fade_ms,&SampleEdit::crossfade_ms,&SampleEdit::attack_ms,&SampleEdit::release_ms};return members[offset];}
inline void bind_engine_values(const EngineParameterTable& table,const float* values,unsigned count,const EngineVoiceTargets& target){
 for(unsigned i=0;i<count && i<table.count;++i){const auto& p=table.declarations[i];float v=values[i];
  switch(p.target){
  case EngineValueTarget::Macro:if(target.macros)target.macros[p.offset]=v;break;
  case EngineValueTarget::Model:if(target.model)*target.model=uint8_t(v);break;
  case EngineValueTarget::Envelope:if(target.envelope)target.envelope[p.offset]=v;break;
  case EngineValueTarget::Layer:case EngineValueTarget::Additive:case EngineValueTarget::Granular:case EngineValueTarget::Formant:case EngineValueTarget::PhaseDist:case EngineValueTarget::SoundFont:case EngineValueTarget::Fm6:case EngineValueTarget::VirtualAnalog:case EngineValueTarget::Wavetable:case EngineValueTarget::Resonator:case EngineValueTarget::Texture:if(target.native)target.native[p.offset]=v;break;
  case EngineValueTarget::Sample:if(target.sample){target.sample->*sample_parameter_member(p.offset)=v;}break;
  case EngineValueTarget::None:break;
  }
 }
}
using EngineVoiceBinding=void (*)(const EngineParameterTable&,const float*,unsigned,const EngineVoiceTargets&);
enum class VoiceResource:uint8_t { None, Plaits, Granular, Count };
struct EnginePolyphony {uint8_t voices=1;bool live=false;uint8_t shared_slots=0,shared_budget=0;const uint8_t* model_cost=nullptr;VoiceResource resource=VoiceResource::None;uint8_t model_count=0,base_cost=1;bool independent_voices=true;bool triggered_leaf=false;};
// Keep an independently supported per-track voice count while recalibrating
// its weighted cost. Shared/model budgets are deliberately separate.
constexpr EnginePolyphony calibrated_polyphony(EnginePolyphony p,unsigned cost){p.voices=uint8_t(p.voices/p.base_cost*cost);p.base_cost=uint8_t(cost);return p;}
constexpr EnginePolyphony triggered_leaf_polyphony(EnginePolyphony p){p.triggered_leaf=true;return p;}
inline VisualDeclaration engine_visual(HeroVisual hero){
 switch(hero){case HeroVisual::None:case HeroVisual::MidiInfo:return {VisualElement::None,VisualSource::None,VisualCadence::Static,VisualFallback::Empty,ResponseModel::None,"CONTROL / NO PCM"};
 case HeroVisual::Waveform:return kSampleVisual;
 case HeroVisual::FlowFM:return {VisualElement::Scope,VisualSource::TrackPcm,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::None,"TRACK POST FADER",VisualStatus::None,VisualRecipe::Fm2};
 case HeroVisual::Oscillators:return {VisualElement::Scope,VisualSource::TrackPcm,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::None,"TRACK POST FADER",VisualStatus::None,VisualRecipe::Oscillators};
 case HeroVisual::Harmonics:return {VisualElement::Bars,VisualSource::Harmonics,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::None,"HARMONIC RECIPE / ACTUAL ENVELOPE"};
 case HeroVisual::WavetableScan:return {VisualElement::Curve,VisualSource::WavetableScan,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::None,"POSITION / SCAN"};
 case HeroVisual::Texture:return {VisualElement::Spectrum,VisualSource::TrackSpectrum,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"TEXTURE OUTPUT FFT",VisualStatus::TextureBuffer};
 case HeroVisual::Spectrum:case HeroVisual::Resonator:return {VisualElement::Spectrum,VisualSource::TrackSpectrum,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"OUTPUT FFT / MONO"};
 default:return {VisualElement::Scope,VisualSource::TrackPcm,VisualCadence::Audio,VisualFallback::Parameters};}}
struct EngineDescriptor {InstrumentDescriptor instrument;EngineGuidInfo identity;EnginePageDescriptor page;EngineParameterTable parameters;EnginePolyphony polyphony;EngineStateContract state=engine_state_contract();EngineVoiceBinding bind=bind_engine_values;NativeCostTable runtime_cost{};VisualDeclaration visual=engine_visual(page.hero);};
inline EngineParameterDeclaration engine_legacy_declaration(unsigned local,unsigned id,const char* name=nullptr,EngineDisplay display=EngineDisplay::Native){auto p=*parameter_descriptor(uint16_t(id));p.id=uint16_t(local);if(name)p.name=name;EngineParameterDeclaration d{p,uint16_t(id),display};
 if(id>=32 && id<=36){d.target=EngineValueTarget::Macro;d.offset=uint8_t(id-32);}
 else if(id>=80 && id<=86){d.target=EngineValueTarget::Sample;d.offset=uint8_t(id-80);}
 else if(id>=104 && id<=107){d.target=EngineValueTarget::Envelope;d.offset=uint8_t(id-104);}
 else if(id>=600 && id<=607){d.target=EngineValueTarget::Resonator;d.offset=uint8_t(id-600);}
 else if(id>=620 && id<=634){d.target=EngineValueTarget::Texture;d.offset=uint8_t(id-620);}
 d.ui.type=declared_parameter_type(d.value);
 d.ui.group=d.target==EngineValueTarget::Sample?"Sample":d.target==EngineValueTarget::Envelope?"Envelope":d.target==EngineValueTarget::Resonator?(id==601 || id>=606?"Exciter":"Resonator"):d.target==EngineValueTarget::Texture?(id<=625?"Grains":id<=629?"Blend":"Quality"):"Oscillator";
 const bool core=(id>=32 && id<=36) || (id>=80 && id<=82) || id==600 || (id>=602 && id<=605) || (id>=620 && id<=625) || id==630;
 d.ui.tier=core?EngineEditTier::Performance:EngineEditTier::Detail;
 return d;}
inline EngineParameterDeclaration engine_macro_declaration(unsigned local,const char* name,EngineDisplay display=EngineDisplay::Percent){return engine_legacy_declaration(local,32+local,name,display);}
inline constexpr const char* kPlaitsModelNames[]={"Virtual analog VCF","Phase distortion","DX7 bank 1","DX7 bank 2","DX7 bank 3","Wave terrain","String machine","Chiptune","Virtual analog","Waveshaping","FM / feedback","Granular formant","Harmonic oscillator","Wavetable terrain","Chord oscillator","Speech","Swarm","Filtered noise","Particle noise","String resonator","Modal resonator","Analog kick","Analog snare","Metallic hi-hat"};
// v475 default-patch peak * 1.25 / 6.25 us, rounded up; never below the old
// reservation. Model 15/18 had no active samples and retain their old value.
inline constexpr uint8_t kPlaitsModelCost[]={2,3,2,2,2,2,3,2,2,2,3,3,3,2,3,1,2,2,1,3,2,2,2,2};
inline EngineParameterDeclaration virtual_analog_declaration(unsigned local){
 const auto& p=kVirtualAnalogParameters[local];bool envelope=local>=11 && local<=14;
 const char* const* choices=local<2?kVirtualAnalogWaves:local==22?kVirtualAnalogSubs:local==23?kVirtualAnalogFilters:local==24?kVirtualAnalogSvfModes:nullptr;unsigned count=local<2?4:local==22?2:local==23 || local==24?3:0;
 return {p,0,EngineDisplay::Native,choices,uint8_t(count),EngineValueTarget::VirtualAnalog,uint8_t(local),{(local<5 || envelope)?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p,count>0),envelope?"Envelope":local==4 || local==5 || local==15 || local==17 || local>=23?"Filter":local==20 || local==21?"Pitch Envelope":local>=8 && local<=10?"PWM":"Oscillator"}};
}
inline EngineParameterDeclaration layer_declaration(unsigned local){const unsigned field=local%3;const char* name=field==0?"LEVEL":field==1?"PAN":"DETUNE";ParameterDescriptor p{uint16_t(local),name,field==0?0.0f:field==1?-1.0f:-1200.0f,field==2?1200.0f:1.0f,field==0?.5f:0,Interpolation::Linear,field==2?"ct":"",uint8_t(field==2?0:2),field==2?1.0f:.02f};return {p,0,EngineDisplay::Native,nullptr,0,EngineValueTarget::Layer,uint8_t(local),{EngineEditTier::Performance,EngineParameterType::Float,"Layer"}};}
inline EngineParameterDeclaration additive_declaration(unsigned local){const auto& p=kAdditiveParameters[local];return {p,0,EngineDisplay::Native,nullptr,0,EngineValueTarget::Additive,uint8_t(local),{local<9?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p,0),local>=5 && local<9?"Envelope":"Spectrum"}};}
inline EngineParameterDeclaration phase_dist_declaration(unsigned local){const auto& p=kPhaseDistParameters[local];return {p,0,EngineDisplay::Native,local==0?kPhaseDistWaves:nullptr,uint8_t(local==0?4:0),EngineValueTarget::PhaseDist,uint8_t(local),{local<9?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p,uint8_t(local==0?4:0)),local>=5 && local<9?"Envelope":"Spectrum"}};}
inline EngineParameterDeclaration formant_declaration(unsigned local){const auto& p=kFormantParameters[local];return {p,0,EngineDisplay::Native,local==0?kFormantModes:nullptr,uint8_t(local==0?2:0),EngineValueTarget::Formant,uint8_t(local),{local<9?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p,uint8_t(local==0?2:0)),local>=5 && local<9?"Envelope":"Generator"}};}
inline EngineParameterDeclaration granular_declaration(unsigned local){const auto& p=kGranularParameters[local];return {p,0,EngineDisplay::Native,nullptr,0,EngineValueTarget::Granular,uint8_t(local),{local<9?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p),local>=5 && local<=8?"Envelope":"Generator"}};}
inline EngineParameterDeclaration soundfont_declaration(unsigned local){const auto& p=kSoundFontParameters[local];return {p,0,EngineDisplay::Native,nullptr,0,EngineValueTarget::SoundFont,uint8_t(local),{local<4?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p),"SoundFont"}};}
inline EngineParameterDeclaration fm6_declaration(unsigned local){const auto& p=kFm6Parameters[local];return {p,0,EngineDisplay::Native,nullptr,0,EngineValueTarget::Fm6,uint8_t(local),{local<5?EngineEditTier::Performance:EngineEditTier::Detail,declared_parameter_type(p),local>=5 && local<17?"Operators":"Patch"}};}
inline const EngineDescriptor* engine_descriptor(unsigned id){
 using D=EngineDisplay;
 static const EngineParameterDeclaration parameters_layer[]={layer_declaration(0),layer_declaration(1),layer_declaration(2),layer_declaration(3),layer_declaration(4),layer_declaration(5),layer_declaration(6),layer_declaration(7),layer_declaration(8),layer_declaration(9),layer_declaration(10),layer_declaration(11)};
 static const EngineParameterDeclaration parameters_additive[]={additive_declaration(0),additive_declaration(1),additive_declaration(2),additive_declaration(3),additive_declaration(4),additive_declaration(5),additive_declaration(6),additive_declaration(7),additive_declaration(8),additive_declaration(9),additive_declaration(10),additive_declaration(11),additive_declaration(12),additive_declaration(13),additive_declaration(14),additive_declaration(15),additive_declaration(16),additive_declaration(17),additive_declaration(18),additive_declaration(19),additive_declaration(20),additive_declaration(21),additive_declaration(22),additive_declaration(23),additive_declaration(24)};
 static const EngineParameterDeclaration parameters_phase_dist[]={phase_dist_declaration(0),phase_dist_declaration(1),phase_dist_declaration(2),phase_dist_declaration(3),phase_dist_declaration(4),phase_dist_declaration(5),phase_dist_declaration(6),phase_dist_declaration(7),phase_dist_declaration(8)};
 static const EngineParameterDeclaration parameters_formant[]={formant_declaration(0),formant_declaration(1),formant_declaration(2),formant_declaration(3),formant_declaration(4),formant_declaration(5),formant_declaration(6),formant_declaration(7),formant_declaration(8)};
 static const EngineParameterDeclaration parameters_granular[]={granular_declaration(0),granular_declaration(1),granular_declaration(2),granular_declaration(3),granular_declaration(4),granular_declaration(5),granular_declaration(6),granular_declaration(7),granular_declaration(8),granular_declaration(9),granular_declaration(10),granular_declaration(11)};
 static const EngineParameterDeclaration parameters_soundfont[]={soundfont_declaration(0),soundfont_declaration(1),soundfont_declaration(2),soundfont_declaration(3),soundfont_declaration(4)};
 static const EngineParameterDeclaration parameters_fm6[]={fm6_declaration(0),fm6_declaration(1),fm6_declaration(2),fm6_declaration(3),fm6_declaration(4),fm6_declaration(5),fm6_declaration(6),fm6_declaration(7),fm6_declaration(8),fm6_declaration(9),fm6_declaration(10),fm6_declaration(11),fm6_declaration(12),fm6_declaration(13),fm6_declaration(14),fm6_declaration(15),fm6_declaration(16),fm6_declaration(17),fm6_declaration(18),fm6_declaration(19),fm6_declaration(20)};
 static const EngineParameterDeclaration parameters_6[]={engine_macro_declaration(0,"HARMONICS",D::Percent),engine_macro_declaration(1,"TIMBRE",D::Percent),engine_macro_declaration(2,"MORPH",D::Percent),engine_macro_declaration(3,"DECAY",D::Percent),engine_macro_declaration(4,"COLOR",D::Percent),{{5,"MODEL",0,23,0,Interpolation::Discrete,"",0,1,ApplyNextTrigger},0,D::Native,kPlaitsModelNames,24,EngineValueTarget::Model,0,{EngineEditTier::Detail,EngineParameterType::Choice,"Oscillator"}}};
 static const EngineParameterDeclaration parameters_7[]={engine_macro_declaration(0,"RATIO",D::Percent),engine_macro_declaration(1,"INDEX",D::Percent)};
 static const EngineParameterDeclaration parameters_11[]={engine_macro_declaration(0,"CUTOFF",D::Percent),engine_macro_declaration(1,"RESONANCE",D::Percent),engine_macro_declaration(2,"ENV AMT",D::Percent),engine_macro_declaration(3,"DECAY",D::Percent),engine_macro_declaration(4,"WAVE",D::Percent)};
 static const EngineParameterDeclaration parameters_12[]={engine_macro_declaration(0,"CROSSFADE",D::Milliseconds80),engine_macro_declaration(1,"PITCH",D::Pitch24),engine_macro_declaration(2,"RATE",D::Rate2),engine_macro_declaration(3,"SHAPE",D::Percent),engine_macro_declaration(4,"STEP DEPTH",D::Percent)};
 static const EngineParameterDeclaration parameters_13[]={engine_macro_declaration(0,"CLOCK",D::Megahertz16),engine_macro_declaration(1,"PITCH FOLLOW",D::Percent),engine_macro_declaration(2,"RAW DIV",D::Divider1022),engine_macro_declaration(3,"NOISE MIX",D::Percent),engine_macro_declaration(4,"ARPEGGIO",D::FourChoices)};
 static const EngineParameterDeclaration parameters_14[]={engine_macro_declaration(0,"VARISPEED",D::Varispeed),engine_macro_declaration(1,"GENE SIZE",D::GrainMilliseconds),engine_macro_declaration(2,"SLIDE",D::Percent),engine_macro_declaration(3,"OVERLAP",D::Percent),engine_macro_declaration(4,"OVERDUB",D::Percent)};
 static const EngineParameterDeclaration parameters_15[]={engine_macro_declaration(0,"FOLD",D::Fold7),engine_macro_declaration(1,"PHASE MOD",D::Phase8),engine_macro_declaration(2,"RATIO",D::Ratio31),engine_macro_declaration(3,"LPG DECAY",D::Decay1800),engine_macro_declaration(4,"DAMPING",D::Percent)};
 static const EngineParameterDeclaration parameters_16[]={engine_macro_declaration(0,"FREEZE",D::Percent),engine_macro_declaration(1,"BIN SHIFT",D::Pitch24),engine_macro_declaration(2,"TILT",D::Tilt12),engine_macro_declaration(3,"WET",D::Percent),engine_macro_declaration(4,"PARTIALS",D::Partials123)};
 static const EngineParameterDeclaration parameters_17[]={engine_macro_declaration(0,"VOICE",D::Percent),engine_macro_declaration(1,"TUNE",D::Percent),engine_macro_declaration(2,"TONE",D::Percent),engine_macro_declaration(3,"DECAY",D::Percent),engine_macro_declaration(4,"ACCENT",D::Percent)};
 static const EngineParameterDeclaration parameters_18[]={engine_macro_declaration(0,"VOICE",D::Percent),engine_macro_declaration(1,"TUNE",D::Percent),engine_macro_declaration(2,"TONE",D::Percent),engine_macro_declaration(3,"DECAY",D::Percent),engine_macro_declaration(4,"ACCENT",D::Percent)};
 static const EngineParameterDeclaration parameters_4[]={engine_legacy_declaration(0,80),engine_legacy_declaration(1,81),engine_legacy_declaration(2,82),engine_legacy_declaration(3,83),engine_legacy_declaration(4,84),engine_legacy_declaration(5,85),engine_legacy_declaration(6,86)};
 static const EngineParameterDeclaration parameters_5[]={engine_legacy_declaration(0,104),engine_legacy_declaration(1,105),engine_legacy_declaration(2,106),engine_legacy_declaration(3,107)};
 static const EngineParameterDeclaration parameters_20[]={engine_legacy_declaration(0,600),engine_legacy_declaration(1,601),engine_legacy_declaration(2,602),engine_legacy_declaration(3,603),engine_legacy_declaration(4,604),engine_legacy_declaration(5,605),engine_legacy_declaration(6,606),engine_legacy_declaration(7,607)};
 static const EngineParameterDeclaration parameters_21[]={engine_legacy_declaration(0,620),engine_legacy_declaration(1,621),engine_legacy_declaration(2,622),engine_legacy_declaration(3,623),engine_legacy_declaration(4,624),engine_legacy_declaration(5,625),engine_legacy_declaration(6,626),engine_legacy_declaration(7,627),engine_legacy_declaration(8,628),engine_legacy_declaration(9,629),engine_legacy_declaration(10,630),engine_legacy_declaration(11,631),engine_legacy_declaration(12,633),engine_legacy_declaration(13,634)};
 static const EngineParameterDeclaration parameters_wavetable[]={
 {kWavetableParameters[0],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,0,{EngineEditTier::Performance,EngineParameterType::Float,"Oscillator"}},
 {kWavetableParameters[1],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,1,{EngineEditTier::Performance,EngineParameterType::Float,"Scan"}},
 {kWavetableParameters[2],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,2,{EngineEditTier::Performance,EngineParameterType::Float,"Scan"}},
 {kWavetableParameters[3],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,3,{EngineEditTier::Performance,EngineParameterType::Float,"Envelope"}},
 {kWavetableParameters[4],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,4,{EngineEditTier::Performance,EngineParameterType::Float,"Envelope"}},
 {kWavetableParameters[5],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,5,{EngineEditTier::Detail,EngineParameterType::Float,"Envelope"}},
 {kWavetableParameters[6],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,6,{EngineEditTier::Detail,EngineParameterType::Float,"Envelope"}},
 {kWavetableParameters[7],0,D::Native,nullptr,0,EngineValueTarget::Wavetable,7,{EngineEditTier::Detail,EngineParameterType::Float,"Oscillator"}}
 };
 static const EngineParameterDeclaration parameters_va[]={virtual_analog_declaration(0),virtual_analog_declaration(1),virtual_analog_declaration(2),virtual_analog_declaration(3),virtual_analog_declaration(4),virtual_analog_declaration(5),virtual_analog_declaration(6),virtual_analog_declaration(7),virtual_analog_declaration(8),virtual_analog_declaration(9),virtual_analog_declaration(10),virtual_analog_declaration(11),virtual_analog_declaration(12),virtual_analog_declaration(13),virtual_analog_declaration(14),virtual_analog_declaration(15),virtual_analog_declaration(16),virtual_analog_declaration(17),virtual_analog_declaration(18),virtual_analog_declaration(19),virtual_analog_declaration(20),virtual_analog_declaration(21),virtual_analog_declaration(22),virtual_analog_declaration(23),virtual_analog_declaration(24),virtual_analog_declaration(25)};
 static const EngineDescriptor registry[]={
  {{Instrument::PolySaw,"VA SAW",false,0,"SYNTH",1,EnvelopeFixed},{"mk3.polysaw",false,false,false,true},{HeroVisual::SawFilter,ShapeKind::FixedCurve,{track_control(3),track_control(4),track_control(1),track_control(2),track_control(7),track_control(8),track_control(0),track_control(0)}},{},{4,true,0,0,nullptr}},
  {{Instrument::Sampler,"SAMPLER",false,0,"SAMPLER",2,EnvelopeEditable},{"mk3.sampler",true,false,false,true,EngineAssetMode::Sample},{HeroVisual::Waveform,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0)}},{parameters_4,sizeof parameters_4/sizeof parameters_4[0]},{4,true,0,0,nullptr},engine_state_contract(RequireSampler)},
  {{Instrument::DaisySynth,"DAISY VA",false,0,"SYNTH",2,EnvelopeEditable},{"mk3.daisy-synth",false,false,false,true},{HeroVisual::SawFilter,ShapeKind::Envelope,{track_control(3),track_control(4),track_control(1),track_control(2),track_control(7),track_control(8),track_control(0),track_control(0)}},{parameters_5,sizeof parameters_5/sizeof parameters_5[0]},{4,true,0,0,nullptr},engine_state_contract(RequireEnvelope)},
  {{Instrument::PlaitsMacro,"PLAITS 24",true,5,"MACRO",3,EnvelopeFixed},{"mk3.plaits",false,false,false,true},{HeroVisual::PlaitsModel,ShapeKind::Lpg,{engine_control(5),engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0)}},{parameters_6,sizeof parameters_6/sizeof parameters_6[0]},{4,true,4,4,kPlaitsModelCost,VoiceResource::Plaits,24}},
  {{Instrument::FM,"FM 2 OP",false,2,"FM",2,EnvelopeFixed},{"mk3.fm",false,false,false,true},{HeroVisual::FlowFM,ShapeKind::FixedCurve,{engine_control(0),engine_control(1),track_control(3),track_control(4),track_control(1),track_control(2),track_control(0),track_control(0)}},{parameters_7,sizeof parameters_7/sizeof parameters_7[0]},calibrated_polyphony({4,true,0,0,nullptr},2)},
  {{Instrument::ExternalMidi,"MIDI OUT",false,0,"MIDI",1,EnvelopeNone},{"mk3.ext-midi",false,false,false,true},{HeroVisual::MidiInfo,ShapeKind::None,{track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0)}},{},{kStepNotes,true,0,0,nullptr},engine_state_contract(RequireMidi)},
  {{Instrument::Acid,"ACID BASS",false,5,"SYNTH",1,EnvelopeEditable},{"mk3.acid",false,false,false,true},{HeroVisual::FilterAcid,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(3),track_control(4),track_control(0)}},{parameters_11,sizeof parameters_11/sizeof parameters_11[0]},calibrated_polyphony({4,true,0,0,nullptr},2)},
  {{Instrument::WaveSequence,"WAVE SEQUENCE",false,5,"CREATIVE",2,EnvelopeNone},{"mk3.waveseq",false,true,false,true,EngineAssetMode::WaveRing},{HeroVisual::Lanes,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)},LabTail::SampleSlot},{parameters_12,sizeof parameters_12/sizeof parameters_12[0]},triggered_leaf_polyphony(calibrated_polyphony({},3)),engine_state_contract(RequireCreative)},
  {{Instrument::ChipClock,"CHIP CLOCK LAB",false,5,"CREATIVE",1,EnvelopeNone},{"mk3.chipclock",false,true,false,true},{HeroVisual::Clock,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)},LabTail::Tune},{parameters_13,sizeof parameters_13/sizeof parameters_13[0]},triggered_leaf_polyphony(calibrated_polyphony({},2)),engine_state_contract(RequireCreative)},
  {{Instrument::TapeMicrosound,"TAPE / MICRO",false,5,"CREATIVE",2,EnvelopeNone},{"mk3.tape-micro",false,true,false,true,EngineAssetMode::OptionalSample},{HeroVisual::Reel,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)},LabTail::SampleSlot},{parameters_14,sizeof parameters_14/sizeof parameters_14[0]},triggered_leaf_polyphony(calibrated_polyphony({},2)),engine_state_contract(RequireCreative)},
  {{Instrument::WestCoast,"WEST COAST",false,5,"CREATIVE",2,EnvelopeFixed},{"mk3.westcoast",false,true,false,true},{HeroVisual::Fold,ShapeKind::FolderCurve,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)},LabTail::Fold},{parameters_15,sizeof parameters_15/sizeof parameters_15[0]},triggered_leaf_polyphony(calibrated_polyphony({},2)),engine_state_contract(RequireCreative)},
  {{Instrument::SpectralFreeze,"SPECTRAL FREEZE",false,5,"CREATIVE",3,EnvelopeNone},{"mk3.spectral",false,true,false,true,EngineAssetMode::OptionalSample},{HeroVisual::Spectrum,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)},LabTail::SampleSlot},{parameters_16,sizeof parameters_16/sizeof parameters_16[0]},triggered_leaf_polyphony(calibrated_polyphony({},5)),engine_state_contract(RequireCreative)},
  {{Instrument::DaisyAnalogDrums,"DAISY ANALOG DRUMS",false,5,"DRUM",2,EnvelopeNone},{"mk3.daisy-adrums",false,false,false,true},{HeroVisual::Scope,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_17,sizeof parameters_17/sizeof parameters_17[0]},calibrated_polyphony({},8)},
  {{Instrument::DaisySyntheticDrums,"DAISY SYNTH DRUMS",false,5,"DRUM",2,EnvelopeNone},{"mk3.daisy-sdrums",false,false,false,true},{HeroVisual::Scope,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_18,sizeof parameters_18/sizeof parameters_18[0]},calibrated_polyphony({},4)},
  {{Instrument::DrumKit,"DRUM KIT",false,0,"KIT",3,EnvelopeNone},{"mk3.drumkit",false,false,true,true,EngineAssetMode::Kit},{HeroVisual::None,ShapeKind::None,{track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0)}},{},{},engine_state_contract(RequireKit)},
  {{Instrument::Resonator,"RESONATOR",true,0,"PHYSICAL",3,EnvelopeNone},{"mk3.resonator",false,false,false,true},{HeroVisual::Resonator,ShapeKind::None,{engine_control(0),engine_control(2),engine_control(3),engine_control(4),engine_control(5),track_control(1),track_control(2),track_control(0)}},{parameters_20,sizeof parameters_20/sizeof parameters_20[0]},{4,true,0,0,nullptr,VoiceResource::None,0,1,false},engine_state_contract(RequireResonator),bind_engine_values,{kResonatorCosts,3,1}},
  {{Instrument::Texture,"TEXTURE",true,0,"PHYSICAL",3,EnvelopeNone},{"mk3.texture",false,false,false,true},{HeroVisual::Texture,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),engine_control(5),engine_control(10),track_control(1)}},{parameters_21,sizeof parameters_21/sizeof parameters_21[0]},{1,false,0,0,nullptr,VoiceResource::None,0,1,false},engine_state_contract(RequireTexture),bind_engine_values,{kTextureCosts,16,4}},
  {{Instrument::Empty,"EMPTY",false,0,"SYNTH",0,EnvelopeNone},{"mk3.empty",false,false,false,true},{HeroVisual::None,ShapeKind::None,{track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0),track_control(0)}},{},{}},
  {{Instrument::Wavetable,"WAVETABLE",false,0,"SYNTH",2,EnvelopeNone},{"mk3.wavetable",false,false,false,true},{HeroVisual::WavetableScan,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_wavetable,8},calibrated_polyphony({4,true,0,0,nullptr},2),wavetable_state_contract()},
  {{Instrument::VirtualAnalog,"VA DUAL",false,0,"SYNTH",3,EnvelopeEditable},{"mk3.va-dual",false,false,false,true},{HeroVisual::Oscillators,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_va,kVirtualAnalogParameterCount},calibrated_polyphony({16,true,0,0,nullptr,VoiceResource::None,0,2},4),virtual_analog_state_contract()},
  {{Instrument::Fm6,"FM 6",false,0,"SYNTH",3,EnvelopeNone},{"mk3.fm6",false,false,false,true},{HeroVisual::Scope,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_fm6,21},{12,true,0,0,nullptr,VoiceResource::None,0,3},fm6_state_contract()},
  {{Instrument::SoundFont,"SOUNDFONT",false,0,"SAMPLE",2,EnvelopeNone},{"mk3.soundfont",false,false,false,true,EngineAssetMode::SoundFont},{HeroVisual::Scope,ShapeKind::None,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),track_control(0),track_control(0),track_control(0),track_control(0)}},{parameters_soundfont,5},{8,true,0,0,nullptr,VoiceResource::None,0,2},soundfont_state_contract()},
  {{Instrument::Additive,"ADDITIVE",false,0,"SYNTH",2,EnvelopeEditable},{"mk3.additive",false,false,false,true},{HeroVisual::Harmonics,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_additive,25},calibrated_polyphony({8,true,0,0,nullptr,VoiceResource::None,0,2},3),additive_state_contract()},
  {{Instrument::PhaseDist,"PHASE DIST",false,0,"SYNTH",2,EnvelopeEditable},{"mk3.phase-dist",false,false,false,true},{HeroVisual::Scope,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_phase_dist,9},{8,true,0,0,nullptr,VoiceResource::None,0,2},phase_dist_state_contract()},
  {{Instrument::Layer,"LAYER",false,0,"SYNTH",3,EnvelopeNone},{"mk3.layer",false,false,false,true},{HeroVisual::Scope,ShapeKind::None,{}},{parameters_layer,12},{4,true,0,0,nullptr},layer_state_contract()},
  {{Instrument::Formant,"FORMANT",false,0,"SYNTH",2,EnvelopeEditable},{"mk3.formant",false,false,false,true},{HeroVisual::Scope,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_formant,9},{8,true,0,0,nullptr,VoiceResource::None,0,1},formant_state_contract()},
  {{Instrument::Granular,"GRANULAR",true,0,"SAMPLER",3,EnvelopeEditable},{"mk3.granular",false,false,false,true,EngineAssetMode::Sample},{HeroVisual::Scope,ShapeKind::Envelope,{engine_control(0),engine_control(1),engine_control(2),engine_control(3),engine_control(4),track_control(0),track_control(0),track_control(0)}},{parameters_granular,kGranularParameterCount},{32,true,4,32,nullptr,VoiceResource::Granular,0,8},granular_state_contract()},
 };
 return id<sizeof registry/sizeof registry[0]?&registry[id]:nullptr;
}
inline bool engine_rate_supported(Instrument engine,unsigned rate){const auto* d=engine_descriptor(unsigned(engine));return !d || !d->instrument.requires_48k || rate==48000;}
inline unsigned instrument_count(){return unsigned(Instrument::Unknown);}
inline const InstrumentDescriptor* instrument_descriptor(unsigned id){auto* d=engine_descriptor(id);return d?&d->instrument:nullptr;}
inline const EnginePageDescriptor* engine_page_descriptor(unsigned id){auto* d=engine_descriptor(id);return d?&d->page:nullptr;}
inline const EngineGuidInfo* engine_guid_info(Instrument id){auto* d=engine_descriptor(unsigned(id));return d?&d->identity:nullptr;}
inline const char* engine_guid(Instrument id){auto* d=engine_guid_info(id);return d?d->guid:"mk3.unknown";}
inline bool engine_from_guid(const char* guid,unsigned length,Instrument& out){for(unsigned i=0;i<instrument_count();++i){const auto* d=engine_descriptor(i);const char* name=d->identity.guid;unsigned n=0;while(n<length && name[n] && name[n]==guid[n])++n;if(n==length && !name[n]){out=d->instrument.id;return true;}}return false;}
inline const EngineParameterDeclaration* engine_parameter(Instrument engine,unsigned local){const auto* d=engine_descriptor(unsigned(engine));return d && local<d->parameters.count?d->parameters.declarations+local:nullptr;}
inline int engine_local_parameter(Instrument engine,uint16_t legacy){const auto* d=engine_descriptor(unsigned(engine));if(d)for(unsigned i=0;i<d->parameters.count;++i)if(legacy && d->parameters.declarations[i].legacy_id==legacy)return int(i);return -1;}
inline int engine_model_parameter(Instrument engine){const auto* d=engine_descriptor(unsigned(engine));if(d)for(unsigned i=0;i<d->parameters.count;++i)if(d->parameters.declarations[i].target==EngineValueTarget::Model)return int(i);return -1;}
inline EngineParameterReference engine_parameter_reference(Instrument instrument,uint16_t legacy){
 if(!legacy)return {};
 int local=engine_local_parameter(instrument,legacy);
 return local<0?EngineParameterReference{EngineParameterScope::Track,legacy}:EngineParameterReference{EngineParameterScope::Engine,uint16_t(local)};
}
inline EngineParameterReference engine_core_reference(Instrument instrument,unsigned slot){const auto* d=engine_descriptor(unsigned(instrument));return d && slot<8?d->page.core[slot]:EngineParameterReference{};}
inline uint16_t engine_legacy_reference(Instrument instrument,EngineParameterReference ref){if(ref.scope==EngineParameterScope::Track)return ref.index;if(ref.scope==EngineParameterScope::Engine){const auto* p=engine_parameter(instrument,ref.index);if(p)return p->legacy_id;}return 0;}
inline bool live_poly_capable(Instrument i){const auto* d=engine_descriptor(unsigned(i));return d && d->polyphony.live;}
inline VoiceResource engine_voice_resource(unsigned engine){const auto* d=engine_descriptor(engine);return d?d->polyphony.resource:VoiceResource::None;}
inline NativeCost engine_native_cost(Instrument engine,const float* values){const auto* d=engine_descriptor(unsigned(engine));if(!d || !d->runtime_cost.values)return {};auto table=d->runtime_cost;unsigned model=unsigned(values[0]),quality=table.qualities>1?unsigned(values[11]):0,index=model*table.qualities+quality;return table.values[index<table.count?index:table.count-1];}
inline unsigned engine_voice_cost(Instrument engine,unsigned model){const auto* d=engine_descriptor(unsigned(engine));return d && d->polyphony.model_cost && model<d->polyphony.model_count?d->polyphony.model_cost[model]:d?d->polyphony.base_cost:1;}
inline unsigned instrument_voice_limit(Instrument i,unsigned model=0){const auto* d=engine_descriptor(unsigned(i));if(!d)return 1;unsigned cost=engine_voice_cost(i,model);return cost?d->polyphony.voices/cost:0;}
inline unsigned macro_controls(unsigned engine){const auto* d=engine_descriptor(engine);return d?d->instrument.macro_controls:0;}
inline const char* instrument_control_name(Instrument engine,unsigned index){const auto* d=engine_descriptor(unsigned(engine));const auto* p=engine_parameter(engine,index);return d && index<d->instrument.macro_controls && p?p->value.name:"";}
struct ControlDisplay {float value;const char* unit;unsigned precision;};
inline ControlDisplay instrument_control_display(Instrument engine,unsigned index,float value){
 if(fabsf(value-0.5f)<0.0001f)value=0.5f;
 const auto* p=engine_parameter(engine,index);EngineDisplay display=p?p->display:EngineDisplay::Percent;
 switch(display){
 case EngineDisplay::Milliseconds80:return {value*80,"ms",0};case EngineDisplay::Pitch24:return {(value-0.5f)*24,"st",1};case EngineDisplay::Rate2:return {0.5f+value*1.5f,"x",2};
 case EngineDisplay::Megahertz16:return {0.5f*powf(16,value),"MHz",2};case EngineDisplay::Divider1022:return {1+floorf(value*1022),"div",0};case EngineDisplay::FourChoices:return {floorf(value*3.99f),"",0};
 case EngineDisplay::Varispeed:return {(value-0.5f)*4,"x",2};case EngineDisplay::GrainMilliseconds:return {(32+floorf(value*1984))/48,"ms",1};
 case EngineDisplay::Fold7:return {1+value*7,"x",2};case EngineDisplay::Phase8:return {value*8,"rad",2};case EngineDisplay::Ratio31:return {0.25f+floorf(value*31)/4,"x",2};case EngineDisplay::Decay1800:return {15+value*1800,"ms",0};
 case EngineDisplay::Tilt12:return {(value-0.5f)*12.0412f,"dB",1};case EngineDisplay::Partials123:return {floorf(4+value*123),"bins",0};
 default:return {value*100,"%",0};
 }
}
// Bounded declaration storage; no global-ID allocation when adding an engine.
constexpr unsigned kEngineMorphParameters=32;
struct ParameterSnapshot {
 float values[kCommonMorphParameters]{};
 Instrument engine=Instrument::Empty;char unknown_guid[65]{};uint16_t unknown_count=0;float engine_values[kEngineMorphParameters]{};
 ParameterSnapshot(){for(unsigned i=0;i<kCommonMorphParameters;++i)values[i]=parameter_descriptor(morph_parameter_at(i))->initial;}
};
struct TrackMorphState {Instrument instrument=Instrument::Empty;ParameterSnapshot a[4]{},b[4]{};};
inline bool valid_engine_morph(const ParameterSnapshot& s){
 if(s.engine==Instrument::Unknown){if(!s.unknown_guid[0] || s.unknown_guid[64] || s.unknown_count>kEngineMorphParameters)return false;for(unsigned i=0;i<s.unknown_count;++i)if(!__builtin_isfinite(s.engine_values[i]))return false;return true;}
 const auto* d=engine_descriptor(unsigned(s.engine));if(!d || d->parameters.count>kEngineMorphParameters)return false;
 for(unsigned i=0;i<d->parameters.count;++i){const auto& p=d->parameters.declarations[i].value;float v=s.engine_values[i];if(!(v>=p.minimum && v<=p.maximum) || (p.interpolation==Interpolation::Discrete && floorf(v)!=v))return false;}return true;
}

inline EngineAssetMode engine_asset_mode(Instrument engine){const auto* d=engine_descriptor(unsigned(engine));return d?d->identity.assets:EngineAssetMode::None;}
inline bool engine_optional_assets(Instrument engine){auto mode=engine_asset_mode(engine);return mode==EngineAssetMode::OptionalSample || mode==EngineAssetMode::WaveRing;}

}

#include "layer_codec.h"
