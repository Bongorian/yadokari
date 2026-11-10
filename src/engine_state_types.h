#pragma once
#include "parameters.h"
#include "preset_fx.h"
#include "instrument_types.h"
#include "wavetable_state.h"
#include "virtual_analog_state.h"
#include "fm6_state.h"
#include "soundfont_state.h"
#include "spectral_states.h"
#include "formant_state.h"
#include "granular_state.h"
#include "sample_state.h"
#include "creative_state.h"
namespace groove {
constexpr unsigned kSampleAssets=64;
constexpr unsigned kPresetSlots=256;
constexpr uint16_t kNoPreset=65535;
constexpr unsigned kTracks=16, kSteps=256, kPatterns=16, kScenes=16, kLocks=4;
// The kit is one clip/track. Each pad shares the track clock and the engine's
// sampler voice pool. Steps store only sample-relevant locks, not audio data.
struct KitStepState {
 uint8_t enabled=0,velocity=100,probability=100,ratchet=1;
 uint8_t sample_lock=255,start_lock=255,filter_lock=255,reverse_lock=2;
 uint8_t level_lock=255,delay_lock=255,reverb_lock=255;
 uint8_t fx_lock[4]={255,255,255,255};
 int8_t pitch_lock=0;int8_t microtiming=0;
};
inline bool valid_kit_fx_locks(const KitStepState& s){if(s.fx_lock[0]!=255 && s.fx_lock[0]>=kFxCount)return false;for(unsigned i=1;i<4;++i)if(s.fx_lock[i]!=255 && s.fx_lock[i]>127)return false;return true;}
struct KitPadState {
 SampleEdit edit{};uint8_t sample[2]={255,255},velocity_split=64,layer_mode=0,gm_note=255;
 bool muted=false,reverse=false;float level=1,pan=0,filter=1,delay_send=0,reverb_send=0;float insert_fx[4]={0,0.5f,0.25f,1};
};
struct KitDefinition {KitPadState pads[16]{};uint16_t solo_mask=0;};
struct KitSequence {KitStepState steps[16][kSteps]{};};
struct DrumKitState:KitDefinition,KitSequence {
 KitDefinition& definition(){return *this;}const KitDefinition& definition()const{return *this;}
 KitSequence& sequence(){return *this;}const KitSequence& sequence()const{return *this;}
};
template<bool C,class T> struct StateQualifier {using type=T;};
template<class T> struct StateQualifier<true,T> {using type=const T;};
template<bool C> struct BasicKitView {
 template<class T> using Q=typename StateQualifier<C,T>::type;
 Q<KitDefinition>& definition_;Q<KitSequence>& sequence_;
 Q<KitPadState> (&pads)[16];Q<KitStepState> (&steps)[16][kSteps];Q<uint16_t>& solo_mask;
 BasicKitView(Q<KitDefinition>& d,Q<KitSequence>& s):definition_(d),sequence_(s),pads(d.pads),steps(s.steps),solo_mask(d.solo_mask){}
 BasicKitView(Q<DrumKitState>& s):BasicKitView(s,s){}
 BasicKitView(const BasicKitView&)=default;
 Q<KitDefinition>& definition()const{return definition_;}Q<KitSequence>& sequence()const{return sequence_;}
 BasicKitView& operator=(const BasicKitView& k){return assign(k);}
 template<class K> BasicKitView& operator=(const K& k){return assign(k);}
 template<class K> BasicKitView& assign(const K& k){for(unsigned i=0;i<16;++i){pads[i]=k.pads[i];for(unsigned j=0;j<kSteps;++j)steps[i][j]=k.steps[i][j];}solo_mask=k.solo_mask;return *this;}
};
template<class K> inline bool valid_kit_definition(const K& kit){
 for(unsigned pad=0;pad<16;++pad){const auto& p=kit.pads[pad];
  if(!valid_sample_edit(p.edit) || p.layer_mode>3 || (p.gm_note>127 && p.gm_note!=255) || p.velocity_split>127 ||
     (p.sample[0]>=kSampleAssets && p.sample[0]!=255) ||
     (p.sample[1]>=kSampleAssets && p.sample[1]!=255) ||
     !(p.level>=0 && p.level<=1) || !(p.pan>=-1 && p.pan<=1) ||
     !(p.filter>=0 && p.filter<=1) || !(p.delay_send>=0 && p.delay_send<=1) ||
     !(p.reverb_send>=0 && p.reverb_send<=1) || !(p.insert_fx[0]>=0 && p.insert_fx[0]<kFxCount && p.insert_fx[0]==float(unsigned(p.insert_fx[0]))) || !(p.insert_fx[1]>=0 && p.insert_fx[1]<=1) || !(p.insert_fx[2]>=0 && p.insert_fx[2]<=1) || !(p.insert_fx[3]>=0 && p.insert_fx[3]<=1))return false;
 }
 return true;
}
template<class K> inline bool valid_kit_sequence(const K& kit){
 for(unsigned pad=0;pad<16;++pad){
  for(const auto& s:kit.steps[pad])if(!valid_kit_fx_locks(s) || s.enabled>1 || s.velocity>127 ||
     s.probability>100 || s.ratchet<1 || s.ratchet>8 ||
     (s.sample_lock>=kSampleAssets && s.sample_lock!=255) ||
     (s.filter_lock>127 && s.filter_lock!=255) || (s.level_lock>127 && s.level_lock!=255) || (s.delay_lock>127 && s.delay_lock!=255) || (s.reverb_lock>127 && s.reverb_lock!=255) || s.reverse_lock>2 ||
     s.pitch_lock< -48 || s.pitch_lock>48 || s.microtiming< -48 || s.microtiming>48)return false;
 }
 return true;
}
template<class K> inline bool valid_drum_kit(const K& kit){return valid_kit_definition(kit) && valid_kit_sequence(kit);}

struct TextureState {float values[15]={0,.5f,.5f,0,.5f,.5f,1,.5f,0,0,0,0,1,0,0};};
inline bool valid_texture(const TextureState& s){for(unsigned i=0;i<15;++i)if(!valid_parameter(uint16_t(620+i),s.values[i]) || (parameter_descriptor(uint16_t(620+i))->interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;return true;}
struct ResonatorState {float values[8]={0,2,0.5f,0.5f,0.5f,0.5f,0,0};};
inline bool valid_resonator(const ResonatorState& s){for(unsigned i=0;i<8;++i)if(!valid_parameter(uint16_t(600+i),s.values[i]) || (parameter_descriptor(uint16_t(600+i))->interpolation==Interpolation::Discrete && floorf(s.values[i])!=s.values[i]))return false;return true;}
struct SoundLeaf { uint32_t definition_cookie=0; // transient immutable extension owner; never encoded
 GranularState granular{}; FormantState formant{}; PhaseDistState phase_dist{};AdditiveState additive{};SoundFontState soundfont{};Fm6State fm6{}; VirtualAnalogState virtual_analog{};WavetableState wavetable{};char preset_name[96]{}; TextureState texture{};ResonatorState resonator{}; SampleEdit sample_edit{}; uint8_t sample_slot=0;CreativeState creative{}; Instrument instrument=Instrument::Empty; uint8_t model=0; float parameters[5]={0.5f,0.5f,0.5f,0.5f,0.5f}; uint8_t sample_slice=0,sample_slices=1;bool sample_loop=false,sample_reverse=false;KitDefinition kit{};float daisy_adsr[4]={0.005f,0.12f,0.55f,0.12f};uint8_t midi_program=128,midi_cc[128]{},midi_cc_used[16]{}; };
constexpr unsigned kLayerParts=4,kLayerRawBytes=4096;
struct LayerPart {SoundLeaf sound{};PresetFxSnapshot fx{};uint8_t key_low=0,key_high=127,velocity_low=1,velocity_high=127;uint16_t source=kNoPreset;float level=0.5f,pan=0,detune=0;uint16_t raw_size=0;uint8_t raw[kLayerRawBytes]{};};
struct LayerState {uint8_t count=0;LayerPart parts[kLayerParts]{};};
const LayerState& empty_layer();
inline bool valid_layer_shape(const LayerState& s){if(s.count>kLayerParts)return false;for(unsigned i=0;i<s.count;++i){const auto& p=s.parts[i];if((p.source!=kNoPreset && p.source>=kPresetSlots) || !(p.level>=0 && p.level<=1) || !(p.pan>=-1 && p.pan<=1) || !(p.detune>=-1200 && p.detune<=1200) || p.raw_size>kLayerRawBytes || p.key_low>p.key_high || p.key_high>127 || p.velocity_low<1 || p.velocity_low>p.velocity_high || p.velocity_high>127 || !p.fx.valid() || p.sound.instrument==Instrument::Layer)return false;}return true;}

}
