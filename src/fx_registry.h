#pragma once
#include <stdint.h>
#include "visual_model.h"
namespace groove {
inline constexpr unsigned kFxCount=22;
// First descriptor-backed FX. Existing slots keep their type/A/B/wet storage.
// Choices declare the adapter's normalized wire mapping, not continuous values.
struct FxChoice {const char* name;const char* const* values;uint8_t count;};
struct FxDescriptor {unsigned type;const char* guid;const char* name;FxChoice controls[2];uint8_t bands,cost;bool external_input;VisualDeclaration visual{};};
inline const FxDescriptor* fx_descriptor(unsigned type){
 static const char* const speeds[]={"SLOW","FAST","BRAKE"};
 static const char* const shifts[]={"-12","-11","-10","-9","-8","-7","-6","-5","-4","-3","-2","-1","+0","+1","+2","+3","+4","+5","+6","+7","+8","+9","+10","+11","+12"};
 static const char* const modulators[]={"INPUT L","INPUT R","INPUT L+R"};
 static const char* const carriers[]={"SLOT SIGNAL","INPUT L","INPUT R","INPUT L+R"};
 static const FxDescriptor effects[]={
  {0,"mk3.fx.0","BYPASS",{{"",nullptr,0},{"",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::None,"RESPONSE MODEL / ACTIVITY"}},
  {1,"mk3.fx.1","LOW PASS",{{"CUTOFF",nullptr,0},{"RESONANCE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::LowPass,"RESPONSE MODEL / ACTIVITY"}},
  {2,"mk3.fx.2","HIGH PASS",{{"CUTOFF",nullptr,0},{"RESONANCE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::HighPass,"RESPONSE MODEL / ACTIVITY"}},
  {3,"mk3.fx.3","BAND PASS",{{"CUTOFF",nullptr,0},{"RESONANCE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::BandPass,"RESPONSE MODEL / ACTIVITY"}},
  {4,"mk3.fx.4","NOTCH",{{"CUTOFF",nullptr,0},{"RESONANCE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Notch,"RESPONSE MODEL / ACTIVITY"}},
  {5,"mk3.fx.5","OVERDRIVE",{{"DRIVE",nullptr,0},{"",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Drive,"RESPONSE MODEL / ACTIVITY"}},
  {6,"mk3.fx.6","FOLDER",{{"FOLD",nullptr,0},{"",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Fold,"RESPONSE MODEL / ACTIVITY"}},
  {7,"mk3.fx.7","TREMOLO",{{"RATE",nullptr,0},{"DEPTH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Modulation,"RESPONSE MODEL / ACTIVITY"}},
  {8,"mk3.fx.8","AUTO PAN",{{"RATE",nullptr,0},{"DEPTH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Width,"RESPONSE MODEL / ACTIVITY"}},
  {9,"mk3.fx.9","RING MOD",{{"RATE",nullptr,0},{"",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Modulation,"RESPONSE MODEL / ACTIVITY"}},
  {10,"mk3.fx.10","DECIMATOR",{{"RATE REDUCE",nullptr,0},{"BIT CRUSH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Quantize,"RESPONSE MODEL / ACTIVITY"}},
  {11,"mk3.fx.11","SOFT CLIP",{{"DRIVE",nullptr,0},{"OUTPUT",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Drive,"RESPONSE MODEL / ACTIVITY"}},
  {12,"mk3.fx.12","NOISE GATE",{{"THRESHOLD",nullptr,0},{"RELEASE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Gate,"RESPONSE MODEL / ACTIVITY"}},
  {13,"mk3.fx.13","TILT EQ",{{"TILT",nullptr,0},{"FREQUENCY",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Tilt,"RESPONSE MODEL / ACTIVITY"}},
  {14,"mk3.fx.14","STEREO WIDTH",{{"WIDTH",nullptr,0},{"BALANCE",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Width,"RESPONSE MODEL / ACTIVITY"}},
  {15,"mk3.fx.15","HARD CLIP",{{"DRIVE",nullptr,0},{"CEILING",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Clip,"RESPONSE MODEL / ACTIVITY"}},
  {16,"mk3.fx.16","CHORUS",{{"RATE",nullptr,0},{"DEPTH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Modulation,"RESPONSE MODEL / ACTIVITY"}},
  {17,"mk3.fx.17","FLANGER",{{"RATE",nullptr,0},{"DEPTH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Modulation,"RESPONSE MODEL / ACTIVITY"}},
  {18,"mk3.fx.18","PHASER",{{"RATE",nullptr,0},{"DEPTH",nullptr,0}},0,1,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Modulation,"RESPONSE MODEL / ACTIVITY"}},
  {19,"mk3.fx.19","VOCODER",{{"MODULATOR",modulators,3},{"CARRIER",carriers,4}},16,4,true,{VisualElement::Bars,VisualSource::Vocoder,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"VOCODER / 16 ACTUAL FOLLOWERS"}},
  {20,"mk3.fx.pitch-shift","PITCH SHIFT",{{"SHIFT",shifts,25},{"",nullptr,0}},0,2,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::None,"SEMITONE SHIFT / ACTIVITY"}},
  {21,"mk3.fx.rotary","ROTARY",{{"SPEED",speeds,3},{"DRIVE",nullptr,0}},0,2,false,{VisualElement::Response,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::None,"HORN + DRUM / RAMPED SPEED"}},
 };return type<kFxCount?&effects[type]:nullptr;
}
enum class SystemFx:uint8_t {Compressor,Delay,Reverb,Filter,Chain};
struct SystemFxVisual {const char* name;VisualDeclaration visual;};
inline const SystemFxVisual& system_fx_visual(SystemFx type){static const SystemFxVisual entries[]={
 {"COMPRESSOR",{VisualElement::Response,VisualSource::Parameters,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::Compressor,"TRANSFER MODEL / ACTUAL REDUCTION"}},
 {"DELAY",{VisualElement::Curve,VisualSource::Parameters,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::Decay,"REPEATS / INPUT ACTIVITY"}},
 {"REVERB",{VisualElement::Curve,VisualSource::Parameters,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::Decay,"DECAY PARAMETER MODEL"}},
 {"FILTER",{VisualElement::Response,VisualSource::Parameters,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::LowPass,"FILTER MODEL / POST-FADER FFT"}},
 {"CHAIN",{VisualElement::Bars,VisualSource::Parameters,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"MEASURED STAGE ACTIVITY"}}};return entries[unsigned(type)<5?unsigned(type):0];}
inline VisualDeclaration performance_visual(unsigned mode){bool position=mode==4 || mode==5 || mode==12 || mode==13;return {position?VisualElement::Ring:VisualElement::Response,VisualSource::Parameters,VisualCadence::Audio,VisualFallback::Parameters,position?ResponseModel::None:ResponseModel::Modulation,position?"ACTUAL PERFORMANCE PHASE":"PERFORMANCE PARAMETER MODEL"};}
inline unsigned fx_choice_index(float value,unsigned count){return count<2?0:value<=0?0:value>=1?count-1:unsigned(value*float(count-1)+.5f);}
inline const char* fx_choice_name(unsigned type,unsigned field,float value){const auto* d=fx_descriptor(type);return d && field>=1 && field<=2 && d->controls[field-1].count?d->controls[field-1].values[fx_choice_index(value,d->controls[field-1].count)]:nullptr;}
}
