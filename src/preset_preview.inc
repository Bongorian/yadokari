// Task-side staging. The audio owner alone mounts/restores the temporary state.
bool Engine::preview_preset(unsigned t,const SoundState& sound,const WavView* views,const SampleAssetState* assets,uint64_t mask,uint32_t ticket){
 if(t>=kTracks || !ticket || !views || !assets || !valid_sound(sound) || sound.instrument==Instrument::Empty || sound.instrument==Instrument::Unknown || __atomic_load_n(&preset_preview_pending_,__ATOMIC_ACQUIRE) || !commands_->free_space())return false;
 if(!engine_rate_supported(sound.instrument,rate_))return false;
 if(!prepare_chimera_sound(sound.chimera_phrase,sound.layer))return false;
 auto& p=*preset_preview_;p.staged=sound;p.staged_mask=mask;for(unsigned i=0;i<kSampleAssets;++i)if(mask&(uint64_t(1)<<i)){p.staged_views[i]=views[i];p.staged_metadata[i]=assets[i].metadata;}
 __atomic_store_n(&preset_preview_pending_,1,__ATOMIC_RELEASE);Command c{CommandType::PreviewPreset,uint8_t(t),0,0};c.frames=ticket;c.pattern_revision=published_generation_;if(submit(c))return true;__atomic_store_n(&preset_preview_pending_,0,__ATOMIC_RELEASE);return false;
}
void Engine::stop_preset_preview(){
 if(preset_preview_track_>=kTracks){return;}unsigned t=preset_preview_track_;preset_preview_track_=255;
 note_off(t);clear_poly(t);voices_[t].sampler.stop();voices_[t].env=0;retire_sampler_source(t);
 restore_preview_mount(t);
 __atomic_store_n(&preview_state_,preview_ticket_<<1,__ATOMIC_RELEASE);
}
void Engine::restore_preview_mount(unsigned t){
 auto& p=*preset_preview_;
 const Instrument audition=track(t).instrument;
 mount_sound(track(t),p.before,audition==Instrument::Layer,audition==Instrument::DrumKit);p.before_fx.apply(track(t));track(t).preset_fx=p.before.fx;project_->audio[t].kind=p.before_kind;if(p.before_audio)audio_lane_mask_|=uint16_t(1u<<t);copy_track_field(track(t).midi_cc,p.before.midi_cc);copy_track_field(track(t).midi_cc_used,p.before.midi_cc_used);track(t).midi_program=p.before.midi_program;
 for(unsigned i=0;i<kSampleAssets;++i)if(p.active_mask&(uint64_t(1)<<i)){samples_[i]=p.before_views[i];project_->assets[i].metadata=p.before_metadata[i];}
 ++cc_generation_;runtime_[t].cc_dirty=true;refresh_parameters(t);
}
void Engine::start_preset_preview(const CommandState& c){
 if(!__atomic_load_n(&preset_preview_pending_,__ATOMIC_ACQUIRE))return;
 auto& p=*preset_preview_;const unsigned t=c.track;
 const uint32_t ticket=c.frames&0x7fffffffu;
 // A new browser selection ends the previous temporary mount.
 stop_preset_preview();
 VoiceRefusal refusal=VoiceRefusal::None;
 unsigned talk=0,tracts=0;
 if(p.staged.instrument==Instrument::Chimera)tracts=chimera_unison(p.staged.chimera);
 if(p.staged.instrument==Instrument::Layer){
  uint8_t selected=0;layer_note_cost(p.staged.layer,track(t).root_note,100,&selected);
  tracts=layer_chimera_tracts(p.staged.layer,selected,talk);
  if(!selected)refusal=VoiceRefusal::NoVoice;
 }
 if(tracts){
  // Resolve the proposed strip in place of its current reservation. Locks,
  // morphs and fading TALK tails count even before the preset is mounted.
  talk+=chimera_talk_slots();
  const auto current=track(t);
  if(current.frozen_asset>=kSampleAssets && p.staged.fx.present){
   const auto& rt=runtime_[t];const auto mod=mod_runtime(t);
   for(unsigned slot=0;slot<2;++slot){
    const unsigned bit=1u<<slot,offset=slot*4;
    const unsigned old_type=(current.insert_disabled&bit)?0:unsigned(rt.insert_target[offset]);
    const unsigned id=94+offset;
    const unsigned type=(p.staged.fx.disabled&bit)?0:unsigned(resolve_target_value(
     *project_,current,t,id,p.staged.fx.inserts[offset],rt.active_locks,unsigned(rt.automation_cell),
     control_all_,control_all_id_,control_all_delta_,rt.automation_live,&gesture_overlay_,rt.pattern,
     nullptr,&mod,&p.staged.mod_routes).value);
    talk-=rt.insert[slot].reserves_talk(old_type);
    talk+=rt.insert[slot].reserves_talk(type);
   }
  }
  refusal=chimera_refusal(tracts,talk,chimera_slots_!=0);
 }

 bool ok=c.pattern_revision==generation_ && refusal==VoiceRefusal::None;
 auto ready=[&](unsigned slot){return slot<kSampleAssets && (p.staged_mask&(uint64_t(1)<<slot)?p.staged_views[slot].frames && !p.staged_metadata[slot].missing:samples_[slot].frames && !project_->assets[slot].metadata.missing);};
 if(p.staged.instrument==Instrument::Sampler)ok=ok && ready(p.staged.sample_slot);
 if(p.staged.instrument==Instrument::DrumKit)for(const auto& pad:p.staged.kit.pads)for(auto slot:pad.sample)if(slot<kSampleAssets)ok=ok && ready(slot);
 int pad=-1;
 if(p.staged.instrument==Instrument::DrumKit){
  for(unsigned i=0;i<16;++i)if(!p.staged.kit.pads[i].muted && (!p.staged.kit.solo_mask || (p.staged.kit.solo_mask&(1u<<i))) && (p.staged.kit.pads[i].sample[0]<kSampleAssets || p.staged.kit.pads[i].sample[1]<kSampleAssets)){pad=int(i);break;}
  ok=ok && pad>=0;
 }
 if(ok){
  // A temporary mount changes only the composite payload it will play. The
  // dormant 59 KiB Layer and 8 KiB kit stay in their original track storage.
  const bool layer=p.staged.instrument==Instrument::Layer,kit=p.staged.instrument==Instrument::DrumKit;
  snapshot_sound(p.before,track(t),layer,kit);p.before_fx.capture(track(t));p.before_kind=project_->audio[t].kind;p.before_audio=(audio_lane_mask_&(1u<<t))!=0;project_->audio[t].kind=TrackKind::Instrument;audio_lane_mask_&=uint16_t(~(1u<<t));copy_track_field(p.before.midi_cc,track(t).midi_cc);copy_track_field(p.before.midi_cc_used,track(t).midi_cc_used);p.before.midi_program=track(t).midi_program;
  p.active_mask=p.staged_mask;for(unsigned i=0;i<kSampleAssets;++i)if(p.active_mask&(uint64_t(1)<<i)){p.before_views[i]=samples_[i];p.before_metadata[i]=project_->assets[i].metadata;samples_[i]=p.staged_views[i];project_->assets[i].metadata=p.staged_metadata[i];}
  const uint32_t previous=poly_track_mask_[t];
  uint32_t instances[kPolyVoiceCapacity];
  for(uint32_t mask=previous;mask;mask&=mask-1){unsigned i=unsigned(__builtin_ctz(mask));instances[i]=poly_voices_[i].voice.voice_instance;}
  const uint32_t old_epoch=sampler_source_epoch_[t];
  retire_sampler_source(t);mount_sound(track(t),p.staged,layer,kit);
  // Resolve the proposed strip before admission. Refreshing every held
  // voice or sending MIDI here would make a rejected audition audible.
  auto& rt=runtime_[t];const auto mod=mod_runtime(t);
  for(unsigned i=0;i<8;i+=4)rt.insert_target[i]=resolve_parameter(*project_,track(t),t,uint16_t(94+i),rt.active_locks,unsigned(rt.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,rt.automation_live,&gesture_overlay_,rt.pattern,nullptr,nullptr,&mod).value;
  const float old_velocity=rt.last_velocity;
  audition_triggering_=true;
  Voice* voice=trigger(t,100,pad>=0?-1:track(t).root_note,nullptr,nullptr,0,false,p.staged.instrument==Instrument::SoundFont || p.staged.instrument==Instrument::ExternalMidi,-1,pad);
  audition_triggering_=false;
  ok=voice!=nullptr;
  if(ok){
   preview_.stop();preview_ticket_=ticket;preset_preview_track_=uint8_t(t);
   for(uint32_t mask=previous&poly_track_mask_[t];mask;mask&=mask-1){
    unsigned i=unsigned(__builtin_ctz(mask));auto& slot=poly_voices_[i];
    if(slot.voice.voice_instance==instances[i])release_poly(slot,true);
   }
   if(voice!=&voices_[t])note_off_main(t);
   for(auto* leaf=voice;leaf;leaf=leaf->layer_peer)leaf->off_at=sample_count_+rate_*3/4;
   ++cc_generation_;rt.cc_dirty=true;refresh_parameters(t);
  }else{
   if(tracts){
    unsigned leaves=0;if(p.staged.instrument==Instrument::Layer){uint8_t mask=0;layer_note_cost(p.staged.layer,track(t).root_note,100,&mask);layer_chimera_tracts(p.staged.layer,mask,leaves);}
    talk=chimera_talk_slots()+leaves;refusal=chimera_refusal(tracts,talk,chimera_slots_!=0);
    if(refusal==VoiceRefusal::None)refusal=VoiceRefusal::Busy;
   }
   rt.last_velocity=old_velocity;sampler_source_epoch_[t]=old_epoch;restore_preview_mount(t);
  }
 }
 if(!ok && refusal==VoiceRefusal::None)refusal=VoiceRefusal::NoVoice;
 __atomic_store_n(&preview_result_,(uint64_t(ticket)<<32)|(ok?0:unsigned(refusal)|(talk<<8)),__ATOMIC_RELEASE);
 __atomic_store_n(&preview_state_,(ticket<<1)|unsigned(ok),__ATOMIC_RELEASE);
 __atomic_store_n(&preset_preview_pending_,0,__ATOMIC_RELEASE);
}
