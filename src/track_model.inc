// Ownership lists shared by storage, views and transactional snapshots.
using TrackField_definition_cookie=uint32_t;
using TrackField_preset_name=char[96];
using TrackField_track_name=char[96];
using TrackField_preset_identity=PresetIdentity;
using TrackField_preset_fx=PresetFxSnapshot;
using TrackField_output_trim=float;
using TrackField_preset_slot=uint16_t;
using TrackField_fm6=Fm6State;
using TrackField_layer=LayerState;
using TrackField_additive=AdditiveState;
using TrackField_phase_dist=PhaseDistState;
using TrackField_formant=FormantState;
using TrackField_granular=GranularState;
using TrackField_soundfont=SoundFontState;
using TrackField_virtual_analog=VirtualAnalogState;
using TrackField_wavetable=WavetableState;
using TrackField_texture=TextureState;
using TrackField_resonator=ResonatorState;
using TrackField_sample_edit=SampleEdit;
using TrackField_creative=CreativeState;
using TrackField_instrument=Instrument;
using TrackField_plaits_model=uint8_t;
using TrackField_engine_parameters=float[5];
using TrackField_daisy_adsr=float[4];
using TrackField_sample_loop=bool;
using TrackField_sample_reverse=bool;
using TrackField_sample_slice=uint8_t;
using TrackField_sample_slices=uint8_t;
using TrackField_sample_slot=uint8_t;
using TrackField_output_bus=uint8_t;
using TrackField_send_prefader=uint8_t;
using TrackField_insert_fx=float[8];
using TrackField_insert_disabled=uint8_t;
using TrackField_compressor=float[7];
using TrackField_arp_mode=uint8_t;
using TrackField_arp_rate=uint8_t;
using TrackField_arp_octaves=uint8_t;
using TrackField_groove=GrooveState;
using TrackField_midi_cc=uint8_t[128];
using TrackField_midi_cc_used=uint8_t[16];
using TrackField_nrpn=NrpnSlot[4];
using TrackField_pitch_bend=uint16_t;
using TrackField_pressure=uint8_t;
using TrackField_expression_used=uint8_t;
using TrackField_midi_program=uint8_t;
using TrackField_midi_channel=uint8_t;
using TrackField_root_note=uint8_t;
using TrackField_level=float;
using TrackField_pan=float;
using TrackField_cutoff=float;
using TrackField_drive=float;
using TrackField_delay_send=float;
using TrackField_reverb_send=float;
using TrackField_crusher=float;
using TrackField_lfo_depth=float;
using TrackField_lfo_rate=float;
using TrackField_lfo_sync=float;
using TrackField_lfo_division=float;
using TrackField_aftertouch_depth=float;
using TrackField_transpose=int8_t;
using TrackField_scale_mask=uint16_t;
using TrackField_scale_root=uint8_t;
using TrackField_velocity_scale=float;
using TrackField_clip_follow=ClipFollow;
using TrackField_mutation=MutationState;
using TrackField_mutation_source=StepState[kSteps];
using TrackField_accumulator=AccumulatorState;
using TrackField_lanes_enabled=bool;
using TrackField_lanes=TriggerLane[3];
using TrackField_automation=AutomationLane[2];
using TrackField_steps=StepState[kSteps];
using TrackField_length=uint16_t;
using TrackField_direction=PlaybackDirection;
using TrackField_speed_num=uint8_t;
using TrackField_speed_den=uint8_t;
using TrackField_frozen_asset=uint8_t;
using TrackField_sound_override=uint16_t;
#define ENGINE_INSTANCE_FIELDS(X) \
 X(TrackField_definition_cookie,definition_cookie,=0) \
 X(TrackField_preset_name,preset_name,{}) \
 X(TrackField_layer,layer,{}) \
 X(TrackField_additive,additive,{}) \
 X(TrackField_phase_dist,phase_dist,{}) \
 X(TrackField_formant,formant,{}) \
 X(TrackField_granular,granular,{}) \
 X(TrackField_soundfont,soundfont,{}) \
 X(TrackField_fm6,fm6,{}) \
 X(TrackField_virtual_analog,virtual_analog,{}) \
 X(TrackField_wavetable,wavetable,{}) \
 X(TrackField_texture,texture,{}) \
 X(TrackField_resonator,resonator,{}) \
 X(TrackField_sample_edit,sample_edit,{}) \
 X(TrackField_creative,creative,{}) \
 X(TrackField_instrument,instrument,=Instrument::Empty) \
 X(TrackField_plaits_model,plaits_model,=0) \
 X(TrackField_engine_parameters,engine_parameters,={0.5f,0.5f,0.5f,0.5f,0.5f}) \
 X(TrackField_daisy_adsr,daisy_adsr,={0.005f,0.12f,0.55f,0.12f}) \
 X(TrackField_sample_loop,sample_loop,=false) \
 X(TrackField_sample_reverse,sample_reverse,=false) \
 X(TrackField_sample_slice,sample_slice,=0) \
 X(TrackField_sample_slices,sample_slices,=1) \
 X(TrackField_sample_slot,sample_slot,=0)
#define TRACK_MIDI_PRESET_FIELDS(X) \
 X(TrackField_midi_cc,midi_cc,{}) \
 X(TrackField_midi_cc_used,midi_cc_used,{}) \
 X(TrackField_midi_program,midi_program,=128)
#define TRACK_COMMON_FIELDS(X) \
 X(TrackField_track_name,track_name,{}) \
 X(TrackField_preset_identity,preset_identity,{}) \
 X(TrackField_preset_fx,preset_fx,{}) \
 X(TrackField_output_trim,output_trim,=1) \
 X(TrackField_preset_slot,preset_slot,=kNoPreset) \
 X(TrackField_output_bus,output_bus,=1) \
 X(TrackField_send_prefader,send_prefader,=0) \
 X(TrackField_insert_fx,insert_fx,={0,0.5f,0.25f,1,0,0.5f,0.25f,1}) \
 X(TrackField_insert_disabled,insert_disabled,=0) \
 X(TrackField_compressor,compressor,={0.5f,4,10,120,0,0,0}) \
 X(TrackField_arp_mode,arp_mode,=0) \
 X(TrackField_arp_rate,arp_rate,=1) \
 X(TrackField_arp_octaves,arp_octaves,=1) \
 X(TrackField_groove,groove,{}) \
 X(TrackField_nrpn,nrpn,{}) \
 X(TrackField_pitch_bend,pitch_bend,=8192) \
 X(TrackField_pressure,pressure,=0) \
 X(TrackField_expression_used,expression_used,=0) \
 X(TrackField_midi_channel,midi_channel,=0) \
 X(TrackField_root_note,root_note,=60) \
 X(TrackField_level,level,=0.8f) \
 X(TrackField_pan,pan,=0) \
 X(TrackField_cutoff,cutoff,=1) \
 X(TrackField_drive,drive,=0) \
 X(TrackField_delay_send,delay_send,=0) \
 X(TrackField_reverb_send,reverb_send,=0) \
 X(TrackField_crusher,crusher,=0) \
 X(TrackField_lfo_depth,lfo_depth,=0) \
 X(TrackField_lfo_rate,lfo_rate,=1) \
 X(TrackField_lfo_sync,lfo_sync,=0) \
 X(TrackField_lfo_division,lfo_division,=6) \
 X(TrackField_aftertouch_depth,aftertouch_depth,=0) \
 X(TrackField_transpose,transpose,=0) \
 X(TrackField_scale_mask,scale_mask,=0x0fff) \
 X(TrackField_scale_root,scale_root,=0) \
 X(TrackField_velocity_scale,velocity_scale,=1)
#define TRACK_SETTINGS_FIELDS(X) TRACK_COMMON_FIELDS(X) TRACK_MIDI_PRESET_FIELDS(X)
#define PHRASE_FIELDS(X) \
 X(TrackField_clip_follow,clip_follow,{}) \
 X(TrackField_mutation,mutation,{}) \
 X(TrackField_mutation_source,mutation_source,{}) \
 X(TrackField_accumulator,accumulator,{}) \
 X(TrackField_lanes_enabled,lanes_enabled,=false) \
 X(TrackField_lanes,lanes,={TriggerLane{},flat_lane(100),flat_lane(96)}) \
 X(TrackField_automation,automation,{}) \
 X(TrackField_steps,steps,{}) \
 X(TrackField_length,length,=16) \
 X(TrackField_direction,direction,=PlaybackDirection::Forward) \
 X(TrackField_speed_num,speed_num,=1) \
 X(TrackField_speed_den,speed_den,=1) \
 X(TrackField_frozen_asset,frozen_asset,=255) \
 X(TrackField_sound_override,sound_override,=65535)
#define TRACK_ALL_FIELDS(X) ENGINE_INSTANCE_FIELDS(X) TRACK_SETTINGS_FIELDS(X) PHRASE_FIELDS(X)
#define DECLARE_STATE_FIELD(T,N,...) T N __VA_ARGS__;
struct EngineInstance {ENGINE_INSTANCE_FIELDS(DECLARE_STATE_FIELD) KitDefinition kit{};};
struct TrackSettings:EngineInstance {TRACK_SETTINGS_FIELDS(DECLARE_STATE_FIELD)};
struct PhraseState {PHRASE_FIELDS(DECLARE_STATE_FIELD) KitSequence kit{};};
// Detached transaction/import snapshot; never stored once per project pattern.
struct TrackState {TRACK_ALL_FIELDS(DECLARE_STATE_FIELD) DrumKitState kit{};
 TrackState()=default;
 template<class V> TrackState(const V& v){assign(v);}
 template<class V> TrackState& operator=(const V& v){assign(v);return *this;}
 template<class V> void assign(const V& v);
};
#undef DECLARE_STATE_FIELD
template<class T> inline void copy_track_field(T& to,const T& from){static_assert(__is_trivially_copyable(T),"track fields are POD snapshots");if(&to!=&from)__builtin_memcpy(&to,&from,sizeof(T));}
template<class T,unsigned N> inline void copy_track_field(T (&to)[N],const T (&from)[N]){for(unsigned i=0;i<N;++i)copy_track_field(to[i],from[i]);}
template<bool C> struct BasicTrackView {
 template<class T> using Q=typename StateQualifier<C,T>::type;
#define DECLARE_VIEW_FIELD(T,N,...) Q<T>& N;
 TRACK_ALL_FIELDS(DECLARE_VIEW_FIELD)
#undef DECLARE_VIEW_FIELD
 BasicKitView<C> kit;
 BasicTrackView(Q<TrackSettings>& t,Q<PhraseState>& p):
#define INIT_TRACK_FIELD(T,N,...) N(t.N),
#define INIT_PHRASE_FIELD(T,N,...) N(p.N),
 ENGINE_INSTANCE_FIELDS(INIT_TRACK_FIELD) TRACK_SETTINGS_FIELDS(INIT_TRACK_FIELD) PHRASE_FIELDS(INIT_PHRASE_FIELD) kit(t.kit,p.kit){}
#undef INIT_TRACK_FIELD
#undef INIT_PHRASE_FIELD
#define INIT_VIEW_FIELD(T,N,...) N(v.N),
 BasicTrackView(Q<TrackState>& v):TRACK_ALL_FIELDS(INIT_VIEW_FIELD) kit(v.kit){}
 template<bool Other> BasicTrackView(const BasicTrackView<Other>& v):TRACK_ALL_FIELDS(INIT_VIEW_FIELD) kit(v.kit.definition(),v.kit.sequence()){}
#undef INIT_VIEW_FIELD
 // Borrow the pool's engine state while retaining this track's strip and phrase.
 // No kit/phrase copy or allocation is needed at a step trigger.
 BasicTrackView(const BasicTrackView& v,Q<SoundState>& s):BasicTrackView(v,static_cast<Q<SoundLeaf>&>(s),s.layer){}
 BasicTrackView(const BasicTrackView& v,Q<SoundLeaf>& s):BasicTrackView(v,s,v.layer){}
 BasicTrackView(const BasicTrackView& v,Q<SoundLeaf>& s,Q<LayerState>& layers):
 definition_cookie(s.definition_cookie),preset_name(s.preset_name),layer(layers),additive(s.additive),phase_dist(s.phase_dist),formant(s.formant),granular(s.granular),soundfont(s.soundfont),fm6(s.fm6),virtual_analog(s.virtual_analog),wavetable(s.wavetable),texture(s.texture),resonator(s.resonator),sample_edit(s.sample_edit),creative(s.creative),instrument(s.instrument),plaits_model(s.model),engine_parameters(s.parameters),daisy_adsr(s.daisy_adsr),sample_loop(s.sample_loop),sample_reverse(s.sample_reverse),sample_slice(s.sample_slice),sample_slices(s.sample_slices),sample_slot(s.sample_slot),
#define INIT_SHARED_FIELD(T,N,...) N(v.N),
 TRACK_COMMON_FIELDS(INIT_SHARED_FIELD)
#define INIT_MIDI_FIELD(T,N,...) N(s.instrument==Instrument::ExternalMidi?s.N:v.N),
 TRACK_MIDI_PRESET_FIELDS(INIT_MIDI_FIELD) PHRASE_FIELDS(INIT_SHARED_FIELD) kit(s.kit,v.kit.sequence()){}
#undef INIT_MIDI_FIELD
#undef INIT_SHARED_FIELD
 BasicTrackView(const BasicTrackView&)=default;
 template<class V> void assign(const V& v){
#define COPY_VIEW_FIELD(T,N,...) copy_track_field(N,v.N);
 TRACK_ALL_FIELDS(COPY_VIEW_FIELD)
#undef COPY_VIEW_FIELD
 kit=v.kit;}
 BasicTrackView& operator=(const BasicTrackView& v){assign(v);return *this;}
 template<class V> BasicTrackView& operator=(const V& v){assign(v);return *this;}
};
using TrackView=BasicTrackView<false>;
using ConstTrackView=BasicTrackView<true>;
template<class V> void TrackState::assign(const V& v){
#define COPY_STATE_FIELD(T,N,...) copy_track_field(N,v.N);
 TRACK_ALL_FIELDS(COPY_STATE_FIELD)
#undef COPY_STATE_FIELD
 for(unsigned i=0;i<16;++i){kit.pads[i]=v.kit.pads[i];for(unsigned j=0;j<kSteps;++j)kit.steps[i][j]=v.kit.steps[i][j];}kit.solo_mask=v.kit.solo_mask;
}
