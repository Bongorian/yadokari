#pragma once
#include "project.h"
namespace groove {
// The descriptor declares the addressing model; every consumer uses this walk.
template<class Sound> inline uint64_t leaf_asset_mask(const Sound& s){
 uint64_t mask=0;auto mark=[&](unsigned slot){if(slot<kSampleAssets)mask|=uint64_t(1)<<slot;};
 switch(engine_asset_mode(s.instrument)){
 case EngineAssetMode::SoundFont:case EngineAssetMode::Sample:case EngineAssetMode::OptionalSample:mark(s.sample_slot);break;
 case EngineAssetMode::Kit:for(const auto& pad:s.kit.pads)for(auto slot:pad.sample)mark(slot);break;
 case EngineAssetMode::WaveRing:for(unsigned i=0;i<s.creative.length[0];++i)mark((s.sample_slot+s.creative.value[0][i])%kSampleAssets);break;
 default:break;
 }return mask;
}
template<class Sound> inline uint64_t sound_asset_mask(const Sound& s){if(s.instrument==Instrument::Unknown)return ~uint64_t(0);if(s.instrument!=Instrument::Layer)return leaf_asset_mask(s);uint64_t mask=0;for(unsigned i=0;i<s.layer.count;++i){if(s.layer.parts[i].sound.instrument==Instrument::Unknown)return ~uint64_t(0);mask|=leaf_asset_mask(s.layer.parts[i].sound);}return mask;}
// Missing optional material is allowed per leaf, never inferred from a composite root.
template<class Sound> inline uint64_t required_sound_asset_mask(const Sound& s){
 if(s.instrument!=Instrument::Layer)return engine_optional_assets(s.instrument)?0:leaf_asset_mask(s);
 uint64_t mask=0;for(unsigned i=0;i<s.layer.count;++i){const auto& leaf=s.layer.parts[i].sound;if(!engine_optional_assets(leaf.instrument))mask|=leaf_asset_mask(leaf);}return mask;
}
// Include dormant phrase locks: a blank path does not make a referenced slot free.
inline uint64_t project_asset_mask(const ProjectState& p,unsigned except_track=kTracks,unsigned except_sound=kNoPreset){
 uint64_t mask=0;auto mark=[&](unsigned slot){if(slot<kSampleAssets)mask|=uint64_t(1)<<slot;};
 for(unsigned t=0;t<kTracks;++t)if(t!=except_track)mask|=sound_asset_mask(p.tracks[t]);
 for(unsigned i=0;i<kPresetSlots;++i)if(i!=except_sound)mask|=sound_asset_mask(p.sounds[i]);
 for(const auto& lane:p.audio)for(const auto& clip:lane.clips)mark(clip.asset);
 for(const auto& pattern:p.patterns)for(const auto& t:pattern.tracks){mark(t.frozen_asset);for(const auto& step:t.steps)for(const auto& lock:step.locks)if(lock.enabled && lock.id==300 && valid_step_sample_lock(lock.id,lock.value))mark(unsigned(lock.value));if(t.mutation.frozen)for(const auto& step:t.mutation_source)for(const auto& lock:step.locks)if(lock.enabled && lock.id==300 && valid_step_sample_lock(lock.id,lock.value))mark(unsigned(lock.value));for(const auto& lane:t.kit.steps)for(const auto& step:lane)mark(step.sample_lock);}
 return mask;
}
inline void remap_leaf_assets(SoundLeaf& s,const uint8_t (&mapping)[kSampleAssets]){
 switch(engine_asset_mode(s.instrument)){
 case EngineAssetMode::SoundFont:case EngineAssetMode::Sample:case EngineAssetMode::OptionalSample:case EngineAssetMode::WaveRing:s.sample_slot=mapping[s.sample_slot];break;
 case EngineAssetMode::Kit:for(auto& pad:s.kit.pads)for(auto& slot:pad.sample)if(slot<kSampleAssets)slot=mapping[slot];break;
 default:break;
 }
}
inline void remap_sound_assets(SoundState& s,const uint8_t (&mapping)[kSampleAssets]){if(s.instrument==Instrument::Layer)for(unsigned i=0;i<s.layer.count;++i)remap_leaf_assets(s.layer.parts[i].sound,mapping);else remap_leaf_assets(s,mapping);}

inline unsigned free_asset_slot(const ProjectState& p){const uint64_t used=project_asset_mask(p);for(unsigned i=0;i<kSampleAssets;++i)if(!p.assets[i].path[0] && !(used&(uint64_t(1)<<i)))return i;return kSampleAssets;}
inline bool same_material_binding(const SampleAssetState& a,const SampleAssetState& b){return bool(a.path[0])==bool(b.path[0]) && a.metadata.content_crc==b.metadata.content_crc && a.metadata.root==b.metadata.root && a.metadata.bpm==b.metadata.bpm;}

struct MaterialMapping {uint8_t slots[kSampleAssets]{};uint64_t needed=0,bound=0;};
inline bool plan_material_mapping(const ProjectState& current,const SoundState& sound,const SampleAssetState* incoming,MaterialMapping& out){
 if(!incoming || sound.instrument==Instrument::Unknown)return false;
 out.needed=sound_asset_mask(sound);out.bound=0;for(unsigned i=0;i<kSampleAssets;++i)out.slots[i]=uint8_t(i);
 const uint64_t occupied=project_asset_mask(current);
 auto reuse=[&](unsigned source,unsigned target){return incoming[source].path[0] && current.assets[target].path[0] && same_material_binding(incoming[source],current.assets[target]);};
 auto vacant=[&](unsigned target){return !current.assets[target].path[0] && !(occupied&(uint64_t(1)<<target)) && !(out.bound&(uint64_t(1)<<target));};
 // Ring offsets are relative addresses, including for Layer leaves. Join
 // overlapping rings, then place each component with one uniform rotation.
 unsigned parent[kSampleAssets];bool ring_slot[kSampleAssets]{};for(unsigned i=0;i<kSampleAssets;++i)parent[i]=i;
 auto root=[&](unsigned i){while(parent[i]!=i)i=parent[i];return i;};
 auto ring=[&](const auto& leaf){if(engine_asset_mode(leaf.instrument)!=EngineAssetMode::WaveRing)return;
  unsigned base=leaf.sample_slot;ring_slot[base]=true;uint64_t mask=leaf_asset_mask(leaf);
  for(unsigned i=0;i<kSampleAssets;++i)if(mask&(uint64_t(1)<<i)){ring_slot[i]=true;parent[root(i)]=root(base);}
 };
 if(sound.instrument==Instrument::Layer){for(unsigned i=0;i<sound.layer.count;++i)ring(sound.layer.parts[i].sound);}else ring(sound);
 uint64_t assigned=0;
 auto planned_reuse=[&](unsigned source,unsigned target){for(unsigned previous=0;previous<kSampleAssets;++previous)if((assigned&(uint64_t(1)<<previous)) && out.slots[previous]==target)return incoming[source].path[0] && same_material_binding(incoming[source],incoming[previous]);return false;};
 for(unsigned component=0;component<kSampleAssets;++component)if(ring_slot[component] && root(component)==component){
  bool placed=false;
  for(unsigned shift=0;shift<kSampleAssets && !placed;++shift){bool fits=true;
   for(unsigned i=0;i<kSampleAssets && fits;++i)if((out.needed&(uint64_t(1)<<i)) && ring_slot[i] && root(i)==component){unsigned target=(i+shift)%kSampleAssets;fits=(out.bound&(uint64_t(1)<<target))?planned_reuse(i,target):(reuse(i,target) || vacant(target));}
   if(!fits)continue;
   for(unsigned i=0;i<kSampleAssets;++i)if(ring_slot[i] && root(i)==component){out.slots[i]=uint8_t((i+shift)%kSampleAssets);if(out.needed&(uint64_t(1)<<i)){assigned|=uint64_t(1)<<i;out.bound|=uint64_t(1)<<out.slots[i];}}
   placed=true;
  }if(!placed)return false;
 }

 for(unsigned source=0;source<kSampleAssets;++source)if((out.needed&(uint64_t(1)<<source)) && !(assigned&(uint64_t(1)<<source))){
  unsigned target=kSampleAssets;
  for(unsigned i=0;i<kSampleAssets;++i)if(reuse(source,i)){target=i;break;}
  if(target==kSampleAssets && incoming[source].path[0])for(unsigned previous=0;previous<kSampleAssets;++previous)if((assigned&(uint64_t(1)<<previous)) && same_material_binding(incoming[source],incoming[previous])){target=out.slots[previous];break;}
  if(target==kSampleAssets)for(unsigned i=0;i<kSampleAssets;++i)if(vacant(i)){target=i;break;}
  if(target==kSampleAssets)return false;
  out.slots[source]=uint8_t(target);out.bound|=uint64_t(1)<<target;assigned|=uint64_t(1)<<source;
 }return true;
}

}
