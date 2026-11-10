#pragma once
// Included after engine_registry.h: child state uses the same GUID/chunk contract.
namespace groove {
inline EngineStateSource leaf_state_source(const SoundLeaf& s){return {&s.texture,&s.resonator,&s.model,s.parameters,&s.sample_slot,&s.sample_edit,&s.creative,&s.kit,&s.sample_loop,&s.sample_reverse,&s.sample_slice,&s.sample_slices,s.preset_name,s.daisy_adsr,&s.midi_program,s.midi_cc,s.midi_cc_used,&s.wavetable,&s.virtual_analog,&s.fm6,&s.additive,&s.phase_dist,&s.soundfont,nullptr,&s.formant,&s.granular};}
inline EngineStateView leaf_state_view(SoundLeaf& s){return {&s.texture,&s.resonator,&s.model,s.parameters,&s.sample_slot,&s.sample_edit,&s.creative,&s.kit,&s.sample_loop,&s.sample_reverse,&s.sample_slice,&s.sample_slices,s.preset_name,s.daisy_adsr,&s.midi_program,s.midi_cc,s.midi_cc_used,&s.wavetable,&s.virtual_analog,&s.fm6,&s.additive,&s.phase_dist,&s.soundfont,nullptr,&s.formant,&s.granular};}
inline const char* layer_engine_name(Instrument i){const auto* d=instrument_descriptor(unsigned(i));return d?d->name:"UNKNOWN";}
// Exclusions are explicit: native Texture/Resonator and Daisy drum banks own
// one DSP per track; MIDI emits externally; kits/composites cannot nest.
inline const char* layer_leaf_exclusion(Instrument i){
 const auto* d=engine_descriptor(unsigned(i));if(!d)return "Unregistered engine";
 if(i==Instrument::Empty || i==Instrument::Unknown)return "No registered sound";
 if(i==Instrument::Layer || i==Instrument::DrumKit)return "Composite nesting is not supported";
 if(i==Instrument::ExternalMidi)return "External MIDI has no internal PCM voice";
 if(d->polyphony.independent_voices && (d->polyphony.live || d->polyphony.triggered_leaf))return nullptr;
 return "DSP state belongs to one track, not an independent note";
}
inline bool layer_leaf_supported(Instrument i){return layer_leaf_exclusion(i)==nullptr;}
inline bool layer_part_matches(const LayerPart& p,unsigned key,unsigned velocity){return key>=p.key_low && key<=p.key_high && velocity>=p.velocity_low && velocity<=p.velocity_high;}
inline unsigned layer_part_cost(const LayerPart& p){unsigned cost=engine_voice_cost(p.sound.instrument,p.sound.model);if(p.fx.present)for(unsigned i=0;i<2;++i)if(!(p.fx.disabled&(1u<<i))){unsigned type=unsigned(p.fx.inserts[i*4]);if(type){const auto* fx=fx_descriptor(type);if(fx)cost+=fx->cost;}}return cost;}
inline unsigned layer_note_cost(const LayerState& s,unsigned key,unsigned velocity,uint8_t* selected=nullptr){unsigned cost=0;uint8_t mask=0;for(unsigned i=0;i<s.count;++i)if(layer_part_matches(s.parts[i],key,velocity)){mask|=uint8_t(1u<<i);cost+=layer_part_cost(s.parts[i]);}if(selected)*selected=mask;return cost;}
enum class LayerReason:uint8_t {Ready,Empty,Missing,Unknown,Unsupported,Cost,Voices,Assets};
// Compact common-header wording; full Layer/admission notices keep layer_reason.
inline const char* layer_short_reason(LayerReason reason){static const char* names[]={"READY","ADD CHILD","CHILD MISSING","UNKNOWN ENGINE","NOT LAYERABLE","COST LIMIT","VOICE LIMIT","ASSET MISSING"};return names[unsigned(reason)];}
inline const char* layer_reason(LayerReason reason){static const char* names[]={"READY","ADD A CHILD PRESET","CHILD PRESET MISSING","UNKNOWN CHILD ENGINE","UNSUPPORTED CHILD ENGINE","VOICE COST EXCEEDED","VOICE CAPACITY EXCEEDED","CHILD ASSET MISSING"};return names[unsigned(reason)];}
inline LayerReason layer_status(const LayerState& s,unsigned* cost=nullptr){
 if(cost){*cost=0;}
 if(!s.count){return LayerReason::Empty;}
 if(!valid_layer_shape(s)){return LayerReason::Unsupported;}
 for(unsigned i=0;i<s.count;++i){const auto& child=s.parts[i].sound;if(child.instrument==Instrument::Empty)return LayerReason::Missing;if(child.instrument==Instrument::Unknown)return LayerReason::Unknown;if(!layer_leaf_supported(child.instrument))return LayerReason::Unsupported;}
 // The maximum overlap occurs at one key-low / velocity-low intersection.
 // Split sounds reserve only simultaneous leaves, never disjoint ranges.
 unsigned maximum=0;for(unsigned k=0;k<s.count;++k)for(unsigned v=0;v<s.count;++v){unsigned n=layer_note_cost(s,s.parts[k].key_low,s.parts[v].velocity_low);if(n>maximum)maximum=n;}
 if(cost){*cost=maximum;}
 return maximum>VoiceBudget::layer_note?LayerReason::Cost:LayerReason::Ready;
}
inline bool layer_write_child(ChunkBuffer& out,const LayerPart& p){
 ChunkWriter<ChunkBuffer> w(out);if(p.sound.instrument==Instrument::Unknown)return p.raw_size && w.block(p.raw,p.raw_size);
 const auto* d=engine_descriptor(unsigned(p.sound.instrument));if(!d || p.sound.instrument==Instrument::Layer)return false;const char* guid=d->identity.guid;unsigned length=0;while(guid[length])++length;if(!w.u8(uint8_t(length)) || !w.block(reinterpret_cast<const uint8_t*>(guid),length) || !w.u16(1))return false;
 unsigned name=0;while(name<96 && p.sound.preset_name[name])++name;if(name==96)return false;uint32_t frame;if(name && (!out.begin_frame(0x4d414e50u,1,frame) || !w.block(reinterpret_cast<const uint8_t*>(p.sound.preset_name),name) || !out.end_frame(frame)))return false;
 const auto source=leaf_state_source(p.sound);for(unsigned i=0;i<d->state.count;++i){const auto& chunk=d->state.chunks[i];if(!chunk.emit(source,(d->state.required&(1u<<i))!=0))continue;if(!out.begin_frame(chunk.fourcc,chunk.version,frame) || !chunk.encode(out,source) || !out.end_frame(frame))return false;}
 return write_fx_frame(out,p.fx) && (!p.raw_size || w.block(p.raw,p.raw_size));
}
inline bool layer_read_child(ChunkSource src,LayerPart& p){
 const uint8_t* raw=src.data;const unsigned size=src.size;ChunkReader<ChunkSource> r(src);uint8_t length;uint16_t schema;char guid[65]{};Instrument inst=Instrument::Unknown;
 auto unknown=[&](){if(size>kLayerRawBytes)return false;p.sound=SoundLeaf{};p.fx=PresetFxSnapshot{};p.sound.instrument=Instrument::Unknown;p.raw_size=uint16_t(size);__builtin_memcpy(p.raw,raw,size);return true;};
 if(!r.u8(length) || length>64 || src.remaining()<length+2u)return unknown();
 for(unsigned i=0;i<length;++i){uint8_t c;if(!r.u8(c))return unknown();guid[i]=char(c);}if(!r.u16(schema) || schema!=1 || !engine_from_guid(guid,length,inst))return unknown();if(inst==Instrument::Layer)return false;
 p.sound=SoundLeaf{};p.fx=PresetFxSnapshot{};p.sound.instrument=inst;p.raw_size=0;const auto* d=engine_descriptor(unsigned(inst));auto view=leaf_state_view(p.sound);unsigned seen=0;bool name_seen=false;
 while(!src.at_end()){unsigned begin=src.pos;uint32_t kind,bytes;uint16_t version,flags;if(!r.u32(kind)||!r.u16(version)||!r.u16(flags)||!r.u32(bytes)||bytes>src.remaining())return unknown();ChunkSource sub{src.data+src.pos,bytes,0};src.pos+=bytes;bool handled=false;
  if(kind==kChunkPfxs && version==1 && !flags){if(p.fx.present || !read_fx_frame(sub,p.fx))return unknown();handled=true;}
  if(kind==0x4d414e50u && version==1 && !flags){if(name_seen || bytes>=96)return unknown();name_seen=true;for(unsigned i=0;i<bytes;++i){unsigned c=sub.data[i];if(c<32 || c>126)return unknown();p.sound.preset_name[i]=char(c);}p.sound.preset_name[bytes]=0;handled=true;}
  for(unsigned i=0;i<d->state.count;++i){const auto& chunk=d->state.chunks[i];if(chunk.fourcc==kind && chunk.version==version && !flags){if((seen&(1u<<i)) || !chunk.decode(sub,view))return unknown();seen|=1u<<i;handled=true;break;}}
  if(!handled){unsigned count=src.pos-begin;if(p.raw_size+count>kLayerRawBytes)return false;__builtin_memcpy(p.raw+p.raw_size,src.data+begin,count);p.raw_size=uint16_t(p.raw_size+count);}
 }
 return (seen&d->state.required)==d->state.required?true:unknown();
}
inline bool encode_layer(ChunkBuffer& out,const EngineStateSource& s){if(!s.layer || !valid_layer_shape(*s.layer))return false;ChunkWriter<ChunkBuffer> w(out);if(!w.u8(s.layer->count))return false;for(unsigned i=0;i<s.layer->count;++i){const auto& p=s.layer->parts[i];ChunkBuffer child;if(!layer_write_child(child,p) || !w.u16(p.source) || !w.f32(p.level) || !w.f32(p.pan) || !w.f32(p.detune) || !w.u8(p.key_low) || !w.u8(p.key_high) || !w.u8(p.velocity_low) || !w.u8(p.velocity_high) || !w.u32(child.size) || !w.block(child.data,child.size))return false;}return true;}
inline bool decode_layer(ChunkSource& src,const EngineStateView& s){if(!s.layer)return false;ChunkReader<ChunkSource> r(src);auto& layer=*s.layer;layer=empty_layer();if(!r.u8(layer.count)||layer.count>kLayerParts)return false;for(unsigned i=0;i<layer.count;++i){auto& p=layer.parts[i];uint32_t size;if(!r.u16(p.source)||!r.f32(p.level)||!r.f32(p.pan)||!r.f32(p.detune)||!r.u8(p.key_low)||!r.u8(p.key_high)||!r.u8(p.velocity_low)||!r.u8(p.velocity_high)||!r.u32(size)||size>src.remaining())return false;ChunkSource child{src.data+src.pos,size,0};src.pos+=size;if(!layer_read_child(child,p))return false;}return src.at_end() && valid_layer_shape(layer);}
}
