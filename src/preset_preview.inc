// Task-side staging. The audio owner alone mounts/restores the temporary state.
bool Engine::preview_preset(unsigned t,const SoundState& sound,const WavView* views,const SampleAssetState* assets,uint64_t mask,uint32_t ticket){
 if(t>=kTracks || !ticket || !views || !assets || !valid_sound(sound) || sound.instrument==Instrument::Empty || sound.instrument==Instrument::Unknown || __atomic_load_n(&preset_preview_pending_,__ATOMIC_ACQUIRE) || !commands_->free_space())return false;
 if(!engine_rate_supported(sound.instrument,rate_))return false;
 auto& p=*preset_preview_;p.staged=sound;p.staged_mask=mask;for(unsigned i=0;i<kSampleAssets;++i)if(mask&(uint64_t(1)<<i)){p.staged_views[i]=views[i];p.staged_metadata[i]=assets[i].metadata;}
 __atomic_store_n(&preset_preview_pending_,1,__ATOMIC_RELEASE);Command c{CommandType::PreviewPreset,uint8_t(t),0,0};c.frames=ticket;c.pattern_revision=published_generation_;if(submit(c))return true;__atomic_store_n(&preset_preview_pending_,0,__ATOMIC_RELEASE);return false;
}
void Engine::stop_preset_preview(){
 if(preset_preview_track_>=kTracks){return;}unsigned t=preset_preview_track_;auto& p=*preset_preview_;preset_preview_track_=255;
 note_off(t);clear_poly(t);voices_[t].sampler.stop();voices_[t].env=0;retire_sampler_source(t);
 mount_sound(track(t),p.before);p.before_fx.apply(track(t));track(t).preset_fx=p.before.fx;project_->audio[t].kind=p.before_kind;if(p.before_audio)audio_lane_mask_|=uint16_t(1u<<t);copy_track_field(track(t).midi_cc,p.before.midi_cc);copy_track_field(track(t).midi_cc_used,p.before.midi_cc_used);track(t).midi_program=p.before.midi_program;
 for(unsigned i=0;i<kSampleAssets;++i)if(p.active_mask&(uint64_t(1)<<i)){samples_[i]=p.before_views[i];project_->assets[i].metadata=p.before_metadata[i];}
 ++cc_generation_;runtime_[t].cc_dirty=true;refresh_parameters(t);
 __atomic_store_n(&preview_state_,preview_ticket_<<1,__ATOMIC_RELEASE);
}
void Engine::start_preset_preview(const CommandState& c){
 if(!__atomic_load_n(&preset_preview_pending_,__ATOMIC_ACQUIRE))return;
 auto& p=*preset_preview_;preview_.stop();preview_ticket_=c.frames&0x7fffffffu;
 bool ok=c.pattern_revision==generation_;
 auto ready=[&](unsigned slot){return slot<kSampleAssets && (p.staged_mask&(uint64_t(1)<<slot)?p.staged_views[slot].frames && !p.staged_metadata[slot].missing:samples_[slot].frames && !project_->assets[slot].metadata.missing);};
 if(p.staged.instrument==Instrument::Sampler)ok=ok && ready(p.staged.sample_slot);
 if(p.staged.instrument==Instrument::DrumKit)for(const auto& pad:p.staged.kit.pads)for(auto slot:pad.sample)if(slot<kSampleAssets)ok=ok && ready(slot);
 if(ok){unsigned t=c.track;snapshot_sound(p.before,track(t));p.before_fx.capture(track(t));p.before_kind=project_->audio[t].kind;p.before_audio=(audio_lane_mask_&(1u<<t))!=0;project_->audio[t].kind=TrackKind::Instrument;audio_lane_mask_&=uint16_t(~(1u<<t));copy_track_field(p.before.midi_cc,track(t).midi_cc);copy_track_field(p.before.midi_cc_used,track(t).midi_cc_used);p.before.midi_program=track(t).midi_program;
  p.active_mask=p.staged_mask;for(unsigned i=0;i<kSampleAssets;++i)if(p.active_mask&(uint64_t(1)<<i)){p.before_views[i]=samples_[i];p.before_metadata[i]=project_->assets[i].metadata;samples_[i]=p.staged_views[i];project_->assets[i].metadata=p.staged_metadata[i];}
  note_off(t);clear_poly(t);retire_sampler_source(t);mount_sound(track(t),p.staged);preset_preview_track_=uint8_t(t);++cc_generation_;runtime_[t].cc_dirty=true;refresh_parameters(t);
  int pad=-1;if(p.staged.instrument==Instrument::DrumKit){for(unsigned i=0;i<16;++i)if(!p.staged.kit.pads[i].muted && (p.staged.kit.pads[i].sample[0]<kSampleAssets || p.staged.kit.pads[i].sample[1]<kSampleAssets)){pad=int(i);break;}}
  if(auto* voice=trigger(t,100,pad>=0?-1:track(t).root_note,nullptr,nullptr,0,false,false,-1,pad))voice->off_at=sample_count_+rate_*3/4;
 }
 __atomic_store_n(&preview_state_,(preview_ticket_<<1)|unsigned(ok),__ATOMIC_RELEASE);
 __atomic_store_n(&preset_preview_pending_,0,__ATOMIC_RELEASE);
}
