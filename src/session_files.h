#pragma once
#include "browser_folder.h"
#include "twin_prepare.h"
#include "preset_assets.h"
#include "content_shelf.h"
#include "performance.h"
#include "project_location.h"
#include "project_files.h"
#include "browser_state.h"
#include "wav_writer.h"
#include "midi_capture.h"
#include "midi_import_job.h"
#include "dx7_import.h"
#include "sample_analysis_slot.h"
#include "time_stretch.h"
#include "project_save_slot.h"
#include "project_load_slot.h"
#include "kit_asset_verifier.h"
#include "alloc_policy.h"
namespace groove {
// The dry overdub mix assumes the source is played once, unchanged, over
// exactly the recorded bars. Revalidate these assumptions until installation.
inline bool overdub_track_compatible(ConstTrackView t,const SampleAssetState& asset,uint32_t frames,uint32_t rate,uint16_t tempo,float tuning){
 if(!frames || !rate || !tempo || t.instrument!=Instrument::Sampler || !asset.path[0] || asset.metadata.frames!=frames || t.sample_edit.start!=0 || t.sample_edit.end!=1 || t.sample_edit.speed!=1 || t.sample_edit.mode!=SampleMode::OneShot || t.sample_reverse || t.sample_loop || t.sample_edit.slice_count || t.sample_slices!=1 || t.speed_num!=1 || t.speed_den!=1 || t.direction!=PlaybackDirection::Forward || !t.steps[0].enabled || int(t.root_note)+int(t.steps[0].note)-60!=int(asset.metadata.root) || t.steps[0].probability!=100 || t.steps[0].microtiming || t.steps[0].ratchet!=1 || t.steps[0].condition!=TrigCondition::Always || t.steps[0].sound!=kNoPreset || t.transpose || t.lanes_enabled || t.arp_mode || t.mutation.amount || t.pitch_bend!=8192 || t.scale_mask!=0x0fff || t.accumulator.pitch || t.groove.amount || tuning!=0)return false;
 for(uint8_t note:t.steps[0].chord)if(note!=255)return false;
 for(const auto& lock:t.steps[0].locks)if(lock.enabled && ((lock.id==engine_target(Instrument::Sampler,0) && lock.value!=1) || (lock.id==engine_target(Instrument::Sampler,1) && lock.value!=0) || (lock.id==engine_target(Instrument::Sampler,2) && lock.value!=1) || (lock.id==260 && lock.value!=8192)))return false;
 uint64_t actual=uint64_t(frames)*tempo,one=uint64_t(rate)*240;unsigned bars=unsigned((actual+one/2)/one);uint64_t expected=one*bars,difference=actual>expected?actual-expected:expected-actual;if(!bars || bars>4 || t.length!=bars*16 || difference>uint64_t(tempo)*2)return false;
 for(unsigned i=1;i<t.length;++i){if(t.steps[i].enabled)return false;}
 return true;
}
inline unsigned overdub_capture_source(unsigned selected){return selected==0 || selected==2 || selected==51 || selected==52?1:selected;}
inline bool overdub_source_safe(const RoutingState& routing,unsigned source,unsigned target){unsigned track=capture_track(source);return target<16 && (source==1 || (track<16 && track!=target) || (source>=53 && source<=60 && !(routing.groups[source-53].members&(1u<<target))));}
struct LoopbackRecordResult {bool ready=false,saved=false,verified=false,cleaned=false;unsigned frames=0,rms_l_u=0,rms_r_u=0,peak_l_u=0,peak_r_u=0,tone_l_u=0,tone_r_u=0,dropped=0;};
struct RecordingResult {char partial[96]{};unsigned queue_highwater=0;bool ready=false,saved=false,verified=false,assigned=false;unsigned failure=0,frames=0,slot=64,peak_l_u=0,peak_r_u=0,dropped=0;};
// Portable, task-owned file workflow. Audio receives only immutable views/commands.
class SessionFiles {
 TwinPrepare* twin_job_=nullptr;
 char folder_directory_[160]{};bool folder_favorites_=false;
 PresetLibrary preset_library_;AssetCatalog asset_catalog_;char content_error_[96]{};char mounted_presets_[kTracks][160]{};
 void synchronize_bindings(){unsigned to=0,from=0;while(ui_.take_asset_clone(to,from))bindings_[to]=bindings_[from];}
 bool resolve_assets(ProjectState& p){content_error_[0]=0;return asset_catalog_.resolve_project(fs_,p,content_error_,sizeof content_error_);}
 bool refresh_preset_library(){library_index_initialized_=preset_library_.refresh(fs_);return library_index_initialized_;}
 bool plan_registration(PresetRegistration& registration,SoundState& sound,const SampleAssetState* assets,unsigned track=kTracks){
  if(track==kTracks)track=ui_.selected_track();
  auto budget=[&](unsigned slot){if(definition_projection_fits(definitions_,project_,preserved_,track,slot,sound.definition_cookie))return true;copy_text(content_error_,96,"Unknown payload budget / sound kept");return false;};
  if(!sound.identity.assigned() && !allocate_user_identity(fs_,preset_library_,sound.identity))return false;
  bool referenced[kPresetSlots]{};for(const auto& t:project_.tracks){if(t.preset_slot<kPresetSlots)referenced[t.preset_slot]=true;}for(const auto& p:project_.patterns)for(const auto& t:p.tracks){if(t.sound_override<kPresetSlots)referenced[t.sound_override]=true;for(const auto& step:t.steps)if(step.sound<kPresetSlots)referenced[step.sound]=true;for(const auto& step:t.mutation_source)if(step.sound<kPresetSlots)referenced[step.sound]=true;}
  unsigned empty=kPresetSlots;
  for(unsigned i=0;i<kPresetSlots;++i){const auto& old=project_.sounds[i];if(same_preset_revision(old.identity,sound.identity)){if(!same_preset_content(old,project_.assets,sound,assets) || !same_extension_owners(old.definition_cookie,sound.definition_cookie)){copy_text(content_error_,sizeof content_error_,"GUID/revision content conflict / sound kept");return false;}if(!budget(i))return false;registration.slot=uint16_t(i);registration.sound=sound;return true;}if(empty==kPresetSlots && old.instrument==Instrument::Empty && !old.identity.assigned() && !referenced[i])empty=i;}
  if(empty==kPresetSlots){copy_text(content_error_,sizeof content_error_,"Embedded preset pool full / sound kept");return false;}if(!budget(empty))return false;registration.slot=uint16_t(empty);registration.sound=sound;return true;
 }

 // Exact identity is validated against the embedded canonical definition. Keeping
 // its asset mapping avoids overwriting the separately saved working track.
 bool resolve_pinned_presets(ProjectState& p,const PreservedStore* store=nullptr){return refresh_preset_library() && resolve_pinned_content(fs_,preset_library_,asset_catalog_,p,content_error_,sizeof content_error_,store);}
 void update_preset_status(){
  for(unsigned t=0;t<kTracks;++t){const auto& id=project_.tracks[t].preset_identity;uint32_t latest=id.assigned()?preset_library_.latest(id.guid):0;ui_.newer_preset_revision[t]=latest>id.revision?latest:0;ui_.embedded_preset_fallback[t]=id.assigned() && preset_library_.exact(id)<0;}
 }
 bool heal_save_assets(){
  auto* copy=alloc_new<ProjectState>(project_);if(!copy)return false;bool ok=resolve_assets(*copy);if(ok)for(unsigned i=0;i<kSampleAssets;++i)copy_text(project_.assets[i].path,96,copy->assets[i].path);delete copy;return ok;
 }
 // Service-owned PCM staging: aggregate 8192 frames (32 KiB) per SD write.
 // Reused across takes, never allocated or accessed by the DSP render owner.
 static constexpr unsigned LiveWriteFrames=8192;
 unsigned live_buffered_frames_=0;
 uint8_t* live_write_buffer_=nullptr;
 bool prepare_record_buffer(){if(!live_write_buffer_)live_write_buffer_=alloc_array<uint8_t>(LiveWriteFrames*4);return live_write_buffer_!=nullptr;}
 void (*save_observer_)(void*,unsigned,bool)=nullptr;void* save_observer_context_=nullptr;
 AsyncProjectSave* project_save_=nullptr;bool project_save_probe_=false,project_save_report_probe_=false;uint8_t project_save_report_=0;
 PreservedStore preserved_;PreservedStore kit_store_;DefinitionExtensions definitions_;
 bool adopt_definition_extensions(ProjectState& p,PreservedStore& store,DefinitionExtensions* replacement=nullptr){
  if(store.dropped_unknown){copy_text(content_error_,96,"Unknown payload exceeds budget / sound kept");return false;}
  auto& owners=replacement?*replacement:definitions_;
  auto retain=[&](PreservedTrackEntry& e,uint32_t& cookie){PreservedTrackEntry value;if(!value.copy_from(e))return false;value.auxiliary.clear();bool ok=owners.retain(value,cookie);value.clear();return ok;};
  for(unsigned t=0;t<kTracks;++t)if(!retain(store.tracks[t],p.tracks[t].definition_cookie))return false;
  for(unsigned i=0;i<kPresetSlots;++i)if(!retain(store.sounds[i],p.sounds[i].definition_cookie))return false;
  return true;
 }
 bool synchronize_definition_extensions(){
  if(!definition_projection_fits(definitions_,project_,preserved_))return false;
  for(unsigned t=0;t<kTracks;++t)if(!definitions_.project(preserved_.tracks[t],project_.tracks[t].definition_cookie))return false;
  for(unsigned i=0;i<kPresetSlots;++i)if(!definitions_.project(preserved_.sounds[i],project_.sounds[i].definition_cookie))return false;
  return preserved_.used_bytes()<=PreservedStore::kMaxTotalBytes;
 }
 bool same_extension_owners(uint32_t a,uint32_t b)const{if(a==b)return true;const auto* x=definitions_.find(a);const auto* y=definitions_.find(b);return x && y && same_definition_extensions(*x,*y);}


 AsyncProjectRead* project_read_=nullptr;ProjectState* kit_ready_=nullptr;TrackState* kit_target_=nullptr;char kit_path_[192]{};unsigned kit_pattern_=0,kit_track_=0,kit_generation_=0,kit_version_=0,kit_asset_cursor_=0,kit_final_cursor_=0;bool kit_audition_cancel_=false,kit_audition_=false,kit_install_preset_=false,preset_audition_active_=false;unsigned preset_audition_track_=0,preset_audition_generation_=0;const char* kit_error_="";bool kit_probe_=false,kit_report_probe_=false,kit_preparing_=false,kit_verified_=false,kit_decoding_seen_=false,kit_asset_loading_=false,kit_final_check_=false;uint8_t kit_report_=0;KitAssetVerifier* kit_verifier_=nullptr;SampleAssetState* kit_asset_snapshot_=nullptr;WavView* kit_prepared_=nullptr;void (*kit_probe_hook_)(void*,unsigned)=nullptr;void* kit_probe_context_=nullptr;
 bool resample_active_=false,resample_cancelled_=false;uint8_t overdub_source_=1;
 bool probe_recording_=false;unsigned probe_input_dropped_=0,record_input_dropped_=0;float live_peak_l_=0,live_peak_r_=0;LoopbackRecordResult probe_result_{};RecordingResult record_result_{};
 IFileAccess& fs_;ISampleLibrary& library_;Engine& engine_;ProjectState& project_;Performance& ui_;MidiCapture* midi_capture_;MidiImportJob midi_import_;uint32_t preview_ticket_=0,favorite_revision_=0;FileRevision browser_preview_revision_{};bool browser_preview_revision_valid_=false;char browser_preview_revision_path_[160]{};bool browser_load_active_=false,browser_load_soundfont_=false;RecordingTarget browser_soundfont_target_{};FileAction browser_load_action_=FileAction::None;char browser_load_path_[160]{};FileRevision browser_load_before_{};bool browser_load_before_valid_=false;WavView bindings_[64]{};using RecordTarget=RecordingTarget;bool capture_pending_=false;RecordTarget capture_target_{},live_target_{},overdub_target_{};char capture_directory_[96]="RECORDINGS";bool live_active_=false,live_stop_pending_=false,live_stop_sent_=false,live_write_failed_=false,live_quantized_=false;uint32_t live_frames_=0;char live_temp_[96]{},live_path_[96]{};bool overdub_=false,overdub_invalid_=false,mixing_=false,preparing_=false,verifying_=false;unsigned overdub_track_=0,overdub_pattern_=0,overdub_slot_=0,overdub_generation_=0,overdub_tempo_=0;uint64_t overdub_revision_=0;uint32_t overdub_frames_=0,mix_frame_=0;char overdub_base_[96]{},mix_temp_[96]{},mix_path_[96]{};WavView overdub_view_{},overdub_file_{};uint64_t overdub_offset_=0,source_size_=0,verify_cursor_=0;uint32_t source_expected_crc_=0,source_baseline_crc_=0,source_audio_baseline_crc_=0;ProjectCrc mix_crc_{},source_crc_{},source_audio_crc_{},mix_source_crc_{},record_data_crc_{},mix_input_crc_{},verify_output_crc_{},verify_input_crc_{};
 WavView stretch_source_{};uint8_t* stretch_read_buffer_=nullptr;char stretch_source_path_[160]{},stretch_temp_[176]{};uint64_t stretch_source_offset_=0,stretch_source_bytes_=0,stretch_assignment_=0;FileRevision stretch_revision_{};float stretch_ratio_=1;unsigned stretch_read_frame_=0,stretch_source_slot_=0,stretch_generation_=0;bool stretch_preparing_=false,stretch_has_revision_=false,stretch_output_started_=false;
 TimeStretch stretch_{};bool stretch_active_=false;bool freeze_active_=false,freeze_writing_=false;unsigned freeze_track_=0,freeze_slot_=64,freeze_generation_=0,freeze_target_pattern_=0;uint64_t freeze_revision_=0;char freeze_path_[160]{};FileWriter* freeze_writer_=nullptr;uint32_t freeze_written_=0;FileWriter* stretch_writer_=nullptr;float* stretch_input_=nullptr;char stretch_path_[160]{};unsigned stretch_track_=0,stretch_pattern_=0,stretch_free_slot_=64;uint8_t stretch_kit_pad_=255;uint16_t stretch_channels_=0;SampleFileAnalysis analysis_;bool analysis_active_=false,analysis_chop_=false,resident_analysis_=false,resident_wait_=false;unsigned analysis_generation_=0,analysis_pattern_=0,analysis_track_=0,analysis_slot_=0,analysis_kit_pad_=255;uint32_t analysis_token_=0;char analysis_path_[160]{};const void* analysis_source_=nullptr;SampleEdit analysis_edit_{};SampleAnalysisSlot* resident_worker_=nullptr;WavView resident_view_{};float resident_start_=0,resident_end_=1;
 void cancel_analysis(){if(analysis_active_ && resident_analysis_ && resident_worker_)resident_worker_->cancel();analysis_active_=resident_analysis_=resident_wait_=false;++analysis_token_;analysis_.cancel();ui_.browser().analysis_pending=false;ui_.browser().analysis_pending_source=nullptr;}
 bool begin_analysis(const char* path,WavView view,bool chop){cancel_analysis();analysis_pattern_=project_.active_pattern;analysis_track_=ui_.selected_track();const auto t=track_at(project_,analysis_pattern_,analysis_track_);analysis_kit_pad_=t.instrument==Instrument::DrumKit?ui_.selected_kit_pad():255;const auto& target_edit=analysis_kit_pad_<16?t.kit.pads[analysis_kit_pad_].edit:t.sample_edit;const unsigned target_slot=analysis_kit_pad_<16?t.kit.pads[analysis_kit_pad_].sample[0]:t.sample_slot;if((chop && target_slot>=kSampleAssets) || !copy_text(analysis_path_,sizeof analysis_path_,path))return false;float start=chop?target_edit.start:0,end=chop?target_edit.end:1;if(view.data && !view.reader && resident_worker_ && view.frames && view.rate && (view.channels==1 || view.channels==2) && start>=0 && start<end && end<=1){resident_analysis_=resident_wait_=true;resident_view_=view;resident_start_=start;resident_end_=end;}else if(view.reader){if(!analysis_.start(path,start,end))return false;}else return false;analysis_active_=true;ui_.browser().analysis_pending=true;analysis_source_=view.reader?static_cast<const void*>(view.reader):static_cast<const void*>(view.data);ui_.browser().analysis_pending_source=analysis_source_;analysis_chop_=chop;analysis_generation_=engine_.staged_generation();analysis_slot_=target_slot;analysis_edit_=target_edit;return true;}
 void complete_analysis(const SampleAnalysis& result,bool ok){analysis_active_=resident_analysis_=resident_wait_=false;auto& b=ui_.browser();b.analysis_pending=false;b.analysis_pending_source=nullptr;ui_.mark_dirty();if(!ok){if(analysis_chop_){copy_text(b.status,sizeof b.status,"Sample analysis failed");ui_.notify(UiNotice::Failed);}return;}if(analysis_generation_!=engine_.staged_generation())return;const void* preview_source=b.preview.reader?static_cast<const void*>(b.preview.reader):static_cast<const void*>(b.preview.data);if(preview_source==analysis_source_ || analysis_chop_ || (sample_visual_page() && selected_sample_source()==analysis_source_)){b.analysis=result;b.analysis_source=analysis_source_;ui_.mark_dirty();}if(!analysis_chop_)return;
  const auto t=track_at(project_,analysis_pattern_,analysis_track_);const unsigned current_pad=t.instrument==Instrument::DrumKit?analysis_kit_pad_:255;const auto& target_edit=current_pad<16?t.kit.pads[current_pad].edit:t.sample_edit;const unsigned target_slot=current_pad<16?t.kit.pads[current_pad].sample[0]:t.sample_slot;bool unchanged=(analysis_kit_pad_==255 || ui_.selected_kit_pad()==analysis_kit_pad_) && analysis_generation_==engine_.staged_generation() && project_.active_pattern==analysis_pattern_ && ui_.selected_track()==analysis_track_ && target_slot==analysis_slot_ && !__builtin_strcmp(project_.assets[analysis_slot_].path,analysis_path_) && target_edit.slice_count==analysis_edit_.slice_count && target_edit.start==analysis_edit_.start && target_edit.end==analysis_edit_.end;
  for(unsigned i=0;i<=analysis_edit_.slice_count && unchanged;++i){unchanged=target_edit.markers[i]==analysis_edit_.markers[i];}
  for(unsigned i=0;i<16 && unchanged;++i){unchanged=target_edit.pads[i]==analysis_edit_.pads[i];}
  if(!unchanged){copy_text(b.status,sizeof b.status,"Auto chop cancelled / target changed");ui_.notify(UiNotice::Failed);return;}auto edit=target_edit;edit.zone_count=0;apply_transients(edit,result);bool applied=ui_.sample_edit(edit);copy_text(b.status,sizeof b.status,applied?"Auto chop complete":"Auto chop failed / engine busy");if(!applied)ui_.notify(UiNotice::Failed);ui_.mark_dirty();}
 const void* selected_sample_source(){const auto t=track_at(project_,project_.active_pattern,ui_.selected_track());unsigned slot=t.instrument==Instrument::DrumKit?t.kit.pads[ui_.selected_kit_pad()].sample[0]:t.sample_slot;if((t.instrument!=Instrument::DrumKit && t.instrument!=Instrument::Sampler) || slot>=kSampleAssets || !project_.assets[slot].path[0])return nullptr;WavView view=library_.lookup(project_.assets[slot].path);return view.reader?static_cast<const void*>(view.reader):static_cast<const void*>(view.data);}
 void clear_stretch(){
  delete stretch_writer_;stretch_writer_=nullptr;
  delete[] stretch_input_;stretch_input_=nullptr;delete[] stretch_read_buffer_;stretch_read_buffer_=nullptr;
  if(stretch_output_started_)fs_.remove(stretch_temp_);
  stretch_active_=stretch_preparing_=stretch_output_started_=false;
 }
 void stretch_fail(const char* message){clear_stretch();copy_text(ui_.browser().status,sizeof ui_.browser().status,message);ui_.notify(UiNotice::Failed);ui_.stretch_set_modal(0);ui_.mark_dirty();}
 bool stretch_source_current(){FileRevision revision;return fs_.size(stretch_source_path_)==stretch_source_bytes_ && (!stretch_has_revision_ || (fs_.revision(stretch_source_path_,revision) && same_file_revision(stretch_revision_,revision)));}
 bool stretch_target_current()const{
  if(stretch_generation_!=engine_.staged_generation() || stretch_assignment_!=ui_.assignment_revision(stretch_pattern_,stretch_track_))return false;
  const auto t=track_at(project_,stretch_pattern_,stretch_track_);
  unsigned slot=stretch_kit_pad_<16 && t.instrument==Instrument::DrumKit?t.kit.pads[stretch_kit_pad_].sample[0]:t.sample_slot;
  return slot==stretch_source_slot_ && !__builtin_strcmp(project_.assets[slot].path,stretch_source_path_);
 }
 bool begin_stretch(){
  if(stretch_active_ || freeze_active_)return false;
  const auto tr=track_at(project_,project_.active_pattern,ui_.selected_track());
  unsigned pad=tr.instrument==Instrument::DrumKit?ui_.selected_kit_pad():255;
  unsigned slot=pad<16?tr.kit.pads[pad].sample[0]:tr.sample_slot;
  if(slot>=kSampleAssets || !project_.assets[slot].path[0]){stretch_fail("No sample to stretch");return false;}
  const char* path=project_.assets[slot].path;WavView view=library_.lookup(path);
  if(!view.frames || !view.rate || (view.channels!=1 && view.channels!=2)){stretch_fail("Sample not loaded / cannot stretch");return false;}
  float ratio=ui_.stretch_ratio_pct()/100.0f;
  uint64_t out_frames=uint64_t(double(view.frames)*double(ratio)+0.5);
  if(out_frames*uint64_t(view.channels)*2ull>268435456ull){stretch_fail("Stretch refused / output over 256 MiB");return false;}
  if((!view.data && !view.reader) || ratio<0.5f || ratio>2){stretch_fail("Stretch failed / bad source or ratio");return false;}
  unsigned free_slot=free_asset_slot(project_);
  if(free_slot>=64){stretch_fail("No free sample slot for stretch");return false;}
  if(!copy_text(stretch_source_path_,sizeof stretch_source_path_,path) || !copy_text(stretch_path_,sizeof stretch_path_,path)){stretch_fail("Stretch failed / bad path");return false;}
  const char* base=filename(path);unsigned stem_end=unsigned(base-path),last_dot=0;
  for(unsigned i=stem_end;path[i];++i)if(path[i]=='.')last_dot=i;
  if(last_dot>stem_end)stem_end=last_dot;else while(path[stem_end])++stem_end;
  unsigned pct=ui_.stretch_ratio_pct();char tag[]="_x000.wav";tag[2]=char('0'+pct/100%10);tag[3]=char('0'+pct/10%10);tag[4]=char('0'+pct%10);
  if(!copy_text(stretch_path_+stem_end,sizeof stretch_path_-stem_end,tag)){stretch_fail("Stretch failed / bad path");return false;}
  const unsigned tag_end=stem_end+5;bool available=false;
  for(unsigned candidate=1;candidate<=9;++candidate){
   char suffix[]="-0.wav";suffix[1]=char('0'+candidate);
   if(!copy_text(stretch_path_+tag_end,sizeof stretch_path_-tag_end,candidate==1?".wav":suffix) || !copy_text(stretch_temp_,sizeof stretch_temp_,stretch_path_)){stretch_fail("Stretch failed / bad path");return false;}
   unsigned n=0;while(stretch_temp_[n])++n;
   if(!copy_text(stretch_temp_+n,sizeof stretch_temp_-n,".part")){stretch_fail("Stretch failed / bad path");return false;}
   if(!fs_.exists(stretch_path_) && !fs_.exists(stretch_temp_)){available=true;break;}
  }
  if(!available){stretch_fail("Stretch refused / output names full");return false;}
  stretch_has_revision_=fs_.revision(path,stretch_revision_);stretch_source_bytes_=fs_.size(path);stretch_source_offset_=0;
  if(view.reader){WavView file;if(!sample_file_header(fs_,path,file,stretch_source_offset_) || file.frames!=view.frames || file.rate!=view.rate || file.channels!=view.channels || file.bits!=view.bits || file.format!=view.format){stretch_fail("Stretch failed / source changed");return false;}}
  stretch_input_=alloc_array<float>(size_t(view.frames)*view.channels);
  if(view.reader)stretch_read_buffer_=alloc_array<uint8_t>(kFileScanBufferBytes);
  if(!stretch_input_ || (view.reader && !stretch_read_buffer_)){stretch_fail("Stretch failed / out of memory");return false;}
  stretch_source_=view;stretch_ratio_=ratio;stretch_read_frame_=0;stretch_channels_=view.channels;
  stretch_track_=ui_.selected_track();stretch_pattern_=project_.active_pattern;stretch_kit_pad_=uint8_t(pad);stretch_free_slot_=free_slot;stretch_source_slot_=slot;
  stretch_generation_=engine_.staged_generation();stretch_assignment_=ui_.assignment_revision(stretch_pattern_,stretch_track_);
  stretch_active_=stretch_preparing_=true;
  ui_.stretch_set_modal(2);ui_.stretch_progress(0);copy_text(ui_.browser().status,sizeof ui_.browser().status,"Reading source / stretching");ui_.mark_dirty();return true;
 }
 void service_stretch(){
  if(!stretch_active_)return;
  if(!stretch_target_current()){stretch_fail("Stretch cancelled / target changed");return;}
  if(stretch_preparing_){
   WavView source=stretch_source_;unsigned stride=wav_frame_bytes(source);
   unsigned frames=source.frames-stretch_read_frame_,limit=kFileScanBufferBytes/stride;if(frames>limit)frames=limit;
   if(!frames || !stretch_source_current()){stretch_fail("Stretch failed / source changed");return;}
   if(source.reader){
    unsigned got=0,count=frames*stride;
    if(!fs_.read(stretch_source_path_,stretch_source_offset_+uint64_t(stretch_read_frame_)*stride,stretch_read_buffer_,count,got) || got!=count || !stretch_source_current()){stretch_fail("Stretch failed / source read error");return;}
    source.reader=nullptr;source.data=stretch_read_buffer_;source.frames=frames;
   }else source.data+=size_t(stretch_read_frame_)*stride;
   for(unsigned f=0;f<frames;++f)for(unsigned ch=0;ch<stretch_channels_;++ch)stretch_input_[size_t(stretch_read_frame_+f)*stretch_channels_+ch]=wav_sample(source,f,ch);
   stretch_read_frame_+=frames;if(stretch_read_frame_<stretch_source_.frames)return;
   delete[] stretch_read_buffer_;stretch_read_buffer_=nullptr;
   if(!stretch_.init(stretch_input_,stretch_source_.frames,stretch_channels_,stretch_ratio_)){stretch_fail("Stretch failed / out of memory");return;}
   if(fs_.exists(stretch_path_) || fs_.exists(stretch_temp_)){stretch_fail("Stretch failed / output already exists");return;}
   stretch_writer_=alloc_new<FileWriter>(fs_,stretch_temp_);uint8_t header[44];
   if(!stretch_writer_ || !wav_header_ch(header,stretch_.target_frames(),stretch_source_.rate,stretch_channels_) || !stretch_writer_->write(header,44)){stretch_fail("Stretch failed / cannot write");return;}
   stretch_output_started_=true;stretch_preparing_=false;return;
  }
  stretch_.step(8);uint32_t ready=stretch_.ready_frames();
  if(ready && stretch_writer_){
   const float* data=stretch_.ready_data();uint8_t buf[4096];uint32_t done_frames=0;bool ok=true;
   while(ok && done_frames<ready){unsigned chunk=ready-done_frames<1024?ready-done_frames:1024;for(unsigned i=0;i<chunk*stretch_channels_;++i){uint16_t sample=uint16_t(pcm16(data[size_t(done_frames)*stretch_channels_+i]));buf[i*2]=uint8_t(sample);buf[i*2+1]=uint8_t(sample>>8);}ok=stretch_writer_->write(buf,chunk*stretch_channels_*2);done_frames+=chunk;}
   if(!ok){stretch_fail("Stretch failed / write error");return;}stretch_.consume(ready);
  }
  ui_.stretch_progress(unsigned(stretch_.progress()*100.0f));if(!stretch_.done())return;
  bool ok=stretch_writer_ && stretch_writer_->finish();delete stretch_writer_;stretch_writer_=nullptr;
  if(!ok || !stretch_source_current() || fs_.exists(stretch_path_) || !fs_.move(stretch_temp_,stretch_path_)){stretch_fail("Stretch failed / output not installed");return;}
  stretch_output_started_=false;clear_stretch();WavView view;bool loaded=library_.load(stretch_path_,view);
  bool bound=loaded && !project_.assets[stretch_free_slot_].path[0] && bind(stretch_free_slot_,stretch_path_,view,stretch_track_,stretch_pattern_,stretch_kit_pad_);
  copy_text(ui_.browser().status,sizeof ui_.browser().status,bound?"Stretched sample assigned":loaded?"Stretch saved / not assigned":"Stretch failed / check storage");
  ui_.notify(loaded?UiNotice::Saved:UiNotice::Failed);ui_.stretch_set_modal(0);ui_.mark_dirty();
 }
 void cancel_stretch(){if(!stretch_active_)return;clear_stretch();copy_text(ui_.browser().status,sizeof ui_.browser().status,"Stretch cancelled");ui_.stretch_set_modal(0);ui_.mark_dirty();}
 void freeze_fail(const char* message){if(freeze_writer_){delete freeze_writer_;freeze_writer_=nullptr;}if(freeze_writing_ && freeze_path_[0])fs_.remove(freeze_path_);engine_.freeze_capture_abort();engine_.freeze_capture_release();freeze_active_=false;freeze_writing_=false;copy_text(ui_.browser().status,sizeof ui_.browser().status,message);ui_.notify(UiNotice::Failed);ui_.freeze_set_modal(0);ui_.mark_dirty();}
 bool begin_freeze(){
  if(freeze_active_ || stretch_active_ || live_active_)return false;
  unsigned track=ui_.selected_track();
  const unsigned pattern=project_.clips[track].pattern;
  const auto t=track_at(project_,pattern,track);
  if(t.frozen_asset<kSampleAssets){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Track already frozen");ui_.notify(UiNotice::Failed);return false;}
  if(!ui_.audio_available()){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Freeze failed / audio unavailable");ui_.notify(UiNotice::Failed);return false;}
  if(!project_.clips[track].playing){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Freeze needs the track clip playing");ui_.notify(UiNotice::Failed);return false;}
  unsigned slot=free_asset_slot(project_);
  if(slot>=64 || !fs_.directory(capture_directory_)){copy_text(ui_.browser().status,sizeof ui_.browser().status,"No free sample slot for freeze");ui_.notify(UiNotice::Failed);return false;}
  char name[20]="Freeze-000.wav";bool free=false;
  for(unsigned i=1;i<1000;++i){name[7]=char('0'+i/100);name[8]=char('0'+(i/10)%10);name[9]=char('0'+i%10);if(!join_path(freeze_path_,sizeof freeze_path_,capture_directory_,name))return false;if(!fs_.exists(freeze_path_)){free=true;break;}}
  if(!free)return false;
  if(!ui_.playing())engine_.submit({CommandType::Start,0,0,0});
  const uint64_t capacity=uint64_t(double(engine_.sample_rate())*60.0/project_.tempo/4.0*t.speed_den/(t.speed_num?t.speed_num:1)*t.length+0.5)+engine_.sample_rate();
  if(capacity>0xffffffffu || !engine_.freeze_capture_begin(track,uint32_t(capacity))){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Freeze refused / cycle too long or stopped");ui_.notify(UiNotice::Failed);return false;}
  freeze_track_=track;freeze_slot_=slot;freeze_generation_=engine_.staged_generation();freeze_target_pattern_=pattern;freeze_revision_=ui_.assignment_revision(pattern,track);freeze_active_=true;freeze_writing_=false;freeze_written_=0;
  ui_.freeze_set_modal(1);ui_.freeze_progress(0);copy_text(ui_.browser().status,sizeof ui_.browser().status,"Freezing track / capturing one cycle");ui_.mark_dirty();return true;
 }
 void service_freeze(){
  if(!freeze_active_)return;
  if(freeze_generation_!=engine_.staged_generation() || freeze_revision_!=ui_.assignment_revision(freeze_target_pattern_,freeze_track_)){freeze_fail("Freeze cancelled / target changed");return;}
  if(!freeze_writing_){
   unsigned st=engine_.freeze_capture_state();
   ui_.freeze_progress(unsigned(engine_.freeze_capture_progress()*100.0f));
   if(st==3){
    uint32_t frames=engine_.freeze_capture_frames();
    freeze_writer_=alloc_new<FileWriter>(fs_,freeze_path_);
    uint8_t header[44];
    if(!frames || !freeze_writer_ || !wav_header_ch(header,frames,engine_.sample_rate(),2) || !freeze_writer_->write(header,44)){freeze_fail("Freeze failed / cannot write");return;}
    freeze_writing_=true;freeze_written_=0;
   } else if(st!=1 && st!=2 && st!=5){freeze_fail("Freeze failed / capture interrupted");return;}
   else return;
  }
  uint32_t frames=engine_.freeze_capture_frames();const int16_t* data=engine_.freeze_capture_data();
  if(!data || !freeze_writer_){freeze_fail("Freeze failed / capture lost");return;}
  uint32_t remaining=frames-freeze_written_;unsigned chunk=remaining>16384?16384:remaining;
  if(chunk && !freeze_writer_->write(reinterpret_cast<const uint8_t*>(data+size_t(freeze_written_)*2),chunk*4)){freeze_fail("Freeze failed / write error");return;}
  freeze_written_+=chunk;
  if(freeze_written_<frames)return;
  bool ok=freeze_writer_->finish();delete freeze_writer_;freeze_writer_=nullptr;
  unsigned pattern=engine_.freeze_capture_pattern();
  engine_.freeze_capture_release();freeze_active_=false;freeze_writing_=false;
  WavView view;bool loaded=ok && library_.load(freeze_path_,view);
  bool bound=false;
  if(loaded && !project_.assets[freeze_slot_].path[0]){
   SampleAssetState assets[kSampleAssets];for(unsigned i=0;i<kSampleAssets;++i)assets[i]=project_.assets[i];
   copy_text(assets[freeze_slot_].path,sizeof assets[freeze_slot_].path,freeze_path_);
   auto& m=assets[freeze_slot_].metadata;m.rate=view.rate;m.frames=view.frames;m.bits=view.bits;m.channels=uint8_t(view.channels);m.bpm=uint16_t(project_.tempo);m.root=60;m.missing=false;
   bindings_[freeze_slot_]=view;
   bound=ui_.set_frozen_asset(freeze_track_,pattern,freeze_slot_,bindings_,assets);
  }
  copy_text(ui_.browser().status,sizeof ui_.browser().status,bound?"Track frozen":loaded?"Freeze saved / not applied":"Freeze failed / check storage");
  ui_.notify(bound||loaded?UiNotice::Saved:UiNotice::Failed);ui_.freeze_set_modal(0);ui_.mark_dirty();
 }
 void cancel_freeze(){if(!freeze_active_)return;if(freeze_writer_){delete freeze_writer_;freeze_writer_=nullptr;}if(freeze_writing_)fs_.remove(freeze_path_);engine_.freeze_capture_abort();engine_.freeze_capture_release();freeze_active_=false;freeze_writing_=false;copy_text(ui_.browser().status,sizeof ui_.browser().status,"Freeze cancelled");ui_.freeze_set_modal(0);ui_.mark_dirty();}
 #if defined(__GNUC__) && !defined(__clang__)
 __attribute__((optimize("Os"),noinline))
 #endif
 bool begin_twin(){
  auto fail=[&](const char* message){ui_.twin_end_ui(message);ui_.notify(UiNotice::TwinFailed);return false;};
  if(twin_job_ || engine_.freeze_capture_state()!=0 || freeze_active_ || stretch_active_ || live_active_ || kit_load_busy() || project_save_busy() || !ui_.audio_available() || ui_.note_repeat_mode() || !ui_.twin_config().valid())return fail("TWIN BUSY / CHECK RANGE (MAX 16 NOTES)");
  unsigned t=ui_.selected_track();if(project_.twins[t].prepared())project_.twins[t].active=project_.tracks[t].instrument==Instrument::Sampler && project_.tracks[t].preset_slot==project_.twins[t].sampled;const auto current=track_at(project_,project_.active_pattern,t);auto kind=current.instrument;
  if(project_.audio[t].kind!=TrackKind::Instrument || current.frozen_asset<kSampleAssets || project_.twins[t].active || kind==Instrument::Empty || kind==Instrument::Unknown || kind==Instrument::ExternalMidi || kind==Instrument::DrumKit || kind==Instrument::Texture)return fail("TWIN NEEDS AN INACTIVE SYNTH TRACK");
  if(ui_.playing() && project_.clips[t].playing){const auto phrase=track_at(project_,project_.clips[t].pattern,t);for(const auto& step:phrase.steps)if(step.enabled)return fail("TWIN PREPARE: STOP OR SILENCE THIS CLIP");}
  const auto& old=project_.twins[t];unsigned source=old.source,sampled=old.sampled;
  if(!old.prepared()){source=sampled=kNoPreset;for(unsigned i=0;i<kPresetSlots;++i)if(project_.sounds[i].instrument==Instrument::Empty && !project_.sounds[i].identity.assigned() && !preset_slot_referenced(project_,i)){if(source==kNoPreset)source=i;else{sampled=i;break;}}if(sampled==kNoPreset)return fail("TWIN NEEDS TWO FREE POOL SLOTS");}
  unsigned asset=free_asset_slot(project_);if(old.prepared()){unsigned previous=project_.sounds[old.sampled].sample_slot;if(previous<kSampleAssets && !(project_asset_mask(project_,kTracks,old.sampled)&(uint64_t(1)<<previous)))asset=previous;}
  if(asset>=kSampleAssets)return fail("TWIN NEEDS A FREE MATERIAL SLOT");
  PresetIdentity source_id{},sampled_id{};
  if(old.prepared()){source_id=project_.sounds[old.source].identity;sampled_id=project_.sounds[old.sampled].identity;if(source_id.revision==0xffffffffu || sampled_id.revision==0xffffffffu)return fail("TWIN REVISION EXHAUSTED");++source_id.revision;++sampled_id.revision;}
  else if(!allocate_user_identity(fs_,preset_library_,source_id,current.preset_identity.assigned()?&current.preset_identity:nullptr) || !allocate_user_identity(fs_,preset_library_,sampled_id,&source_id))return fail("TWIN IDENTITY UNAVAILABLE");
  twin_job_=alloc_new<TwinPrepare>(fs_,library_,engine_,project_);if(!twin_job_)return fail("TWIN MEMORY REFUSED");
  if(!twin_job_->begin(t,ui_.twin_config(),asset,source,sampled,source_id,sampled_id,capture_directory_)){const char* message=twin_job_->error();fail(message);delete twin_job_;twin_job_=nullptr;return false;}ui_.twin_begin_ui();return true;
 }
 #if defined(__GNUC__) && !defined(__clang__)
 __attribute__((optimize("Os"),noinline))
 #endif
 void service_twin(){if(!twin_job_)return;twin_job_->service();ui_.twin_progress_ui(twin_job_->note_index(),twin_job_->note_count(),twin_job_->progress());if(!twin_job_->done())return;
  bool ok=twin_job_->ok();if(ok)bindings_[twin_job_->asset_slot()]=twin_job_->material_view();ui_.twin_end_ui(ok?"TWIN READY":twin_job_->error());ui_.notify(ok?UiNotice::TwinChanged:UiNotice::TwinFailed);delete twin_job_;twin_job_=nullptr;
 }
 bool sample_visual_page()const{if(ui_.audio_page() || (ui_.mode()==UiMode::Step && ui_.tracker_view()))return false;if(ui_.mode()!=UiMode::Sampler && track_at(project_,project_.active_pattern,ui_.selected_track()).instrument==Instrument::DrumKit)return false;switch(ui_.mode()){case UiMode::Sampler:case UiMode::Synth:case UiMode::Pad:case UiMode::Step:case UiMode::Keyboard:case UiMode::Chord:return true;default:return false;}}
 void service_sampler_analysis(){if(!sample_visual_page() || (analysis_active_ && analysis_chop_))return;const auto t=track_at(project_,project_.active_pattern,ui_.selected_track());unsigned slot=t.instrument==Instrument::DrumKit?t.kit.pads[ui_.selected_kit_pad()].sample[0]:t.sample_slot;if((t.instrument!=Instrument::DrumKit && t.instrument!=Instrument::Sampler) || slot>=kSampleAssets || !project_.assets[slot].path[0])return;const char* path=project_.assets[slot].path;WavView view=library_.lookup(path);if((!view.data && !view.reader) || (view.data && !resident_worker_))return;const void* source=view.reader?static_cast<const void*>(view.reader):static_cast<const void*>(view.data);auto& b=ui_.browser();if(!source || b.analysis_source==source || (analysis_active_ && analysis_source_==source))return;begin_analysis(path,view,false);}
 void service_analysis(){if(resident_worker_ && resident_worker_->ready()){SampleAnalysis result;uint32_t token=0;if(resident_worker_->collect(result,token) && analysis_active_ && resident_analysis_ && token==analysis_token_)complete_analysis(result,true);}if(resident_worker_ && analysis_active_ && resident_analysis_ && resident_wait_ && resident_worker_->submit(resident_view_,resident_start_,resident_end_,analysis_token_))resident_wait_=false;if(!analysis_active_ || resident_analysis_)return;analysis_.service();if(!analysis_.busy())complete_analysis(analysis_.result(),analysis_.done());}
 bool selected_path(char* path){auto& b=ui_.browser();return b.cursor<b.count && (b.library_list?copy_text(path,160,b.library_paths[b.cursor]):join_path(path,160,b.directory,b.entries[b.cursor].name));}
 // Existing slots are immutable while they may be referenced by another track
 // or Undo. Identical resident PCM may have a new cache address after a file
 // timestamp change; stream views must retain the same reader instance.
 static bool same_wav_view(const WavView& a,const WavView& b){if(a.frames!=b.frames || a.rate!=b.rate || a.channels!=b.channels || a.bits!=b.bits || a.format!=b.format)return false;if(a.reader || b.reader)return a.reader && a.reader==b.reader;if(!a.data || !b.data)return false;uint64_t bytes=uint64_t(a.frames)*wav_frame_bytes(a);return a.data==b.data || !__builtin_memcmp(a.data,b.data,size_t(bytes));}
 bool select_load_slot(const char* path,WavView view,unsigned& slot){
  unsigned selected=track_at(project_,project_.active_pattern,ui_.selected_track()).sample_slot;bool found=false;uint32_t crc=0;bool have_crc=false;slot=kSampleAssets;
  for(unsigned i=0;i<kSampleAssets;++i){const auto& asset=project_.assets[i];if(!asset.path[0] || __builtin_strcmp(asset.path,path))continue;found=true;
   if(!same_wav_view(bindings_[i],view))return false;
   if(asset.metadata.content_crc){if(!have_crc){if(!file_checksum(fs_,path,crc))return false;have_crc=true;}if(crc!=asset.metadata.content_crc)return false;}
   if(slot==kSampleAssets || i==selected)slot=i;
  }
  if(!found)slot=free_asset_slot(project_);
  return slot<kSampleAssets;
 }
 bool bind(unsigned slot,const char* path,WavView view,unsigned target=255,unsigned pattern=255,unsigned kit_pad=255){if(target==255 && project_.audio[ui_.selected_track()].kind!=TrackKind::Instrument){unsigned t=ui_.selected_track(),clip=project_.audio[t].kind==TrackKind::Looper?0:ui_.audio_clip_cursor();return bind_audio(slot,path,view,t,clip,project_.audio[t].clips[clip].tick,project_.audio[t]);}char checked[96];if(slot>=64 || !copy_text(checked,sizeof checked,path))return false;bool existing=project_.assets[slot].path[0] && !__builtin_strcmp(project_.assets[slot].path,path);auto m=existing?project_.assets[slot].metadata:SampleMetadata{};if(!existing)sample_file_root(fs_,path,m.root);m.rate=view.rate;m.frames=view.frames;m.bits=view.bits;m.channels=uint8_t(view.channels);m.missing=false;if(!file_checksum(fs_,path,m.content_crc))return false;if(!ui_.assign_sample(slot,view,&m,target,pattern,kit_pad))return false;copy_text(project_.assets[slot].path,96,checked);bindings_[slot]=view;return true;}
 void show_recorded_sample(const char* path,WavView view){cancel_analysis();auto& b=ui_.browser();b.preview=view;copy_text(b.preview_path,sizeof b.preview_path,path);copy_text(b.selected_name,sizeof b.selected_name,filename(path));b.analysis_source=nullptr;b.analysis_pending=false;b.analysis_pending_source=nullptr;if(view.data){if(!resident_worker_ || !begin_analysis(path,view,false)){analyze_sample(view,b.analysis);b.analysis_source=view.data;}}else if(view.reader && !begin_analysis(path,view,false))b.analysis=SampleAnalysis{};ui_.mark_dirty();}

 bool recover_live_part(const char* path,const char* directory){if(live_active_ && !__builtin_strcmp(path,live_temp_))return false;uint8_t header[44];unsigned got=0;uint64_t size=fs_.size(path);if(size<=44 || size>uint64_t(0xffffffffu)+11 || !fs_.read(path,0,header,44,got) || got!=44 || !wav_tag(header,"RIFF") || !wav_tag(header+8,"WAVE") || !wav_tag(header+12,"fmt ") || wav_u32(header+16)!=16 || wav_u16(header+20)!=1 || wav_u16(header+22)!=2 || wav_u16(header+32)!=4 || wav_u16(header+34)!=16 || !wav_tag(header+36,"data"))return false;uint64_t usable=44+(size-44)/4*4;uint32_t rate=wav_u32(header+24),frames=uint32_t((usable-44)/4);if(!frames || rate<1000 || rate>768000 || !wav_header(header,frames,rate))return false;char name[]="Recovered-00.wav",temp[]="Recovered-00.tmp",out[160],staging[160];bool free=false;for(unsigned i=1;i<100;++i){name[10]=temp[10]=char('0'+i/10);name[11]=temp[11]=char('0'+i%10);if(!join_path(out,sizeof out,directory,name) || !join_path(staging,sizeof staging,directory,temp))return false;if(!fs_.exists(out) && !fs_.exists(staging)){free=true;break;}}if(!free || !copy_file_prefix(fs_,path,staging,usable) || !fs_.write(staging,0,header,44,false) || fs_.size(staging)!=usable || fs_.exists(out))return false;return fs_.move(staging,out);}
 RecordTarget record_target()const{unsigned track=ui_.selected_track(),pattern=project_.active_pattern;const auto t=track_at(project_,pattern,track);unsigned pad=t.instrument==Instrument::DrumKit?ui_.selected_kit_pad():255;unsigned slot=pad<16?t.kit.pads[pad].sample[0]:t.sample_slot;return {track,pattern,engine_.staged_generation(),t.instrument,uint8_t(slot),uint8_t(pad),ui_.assignment_revision(pattern,track)};}
 bool target_unchanged(const RecordTarget& target)const{if(target.generation!=engine_.staged_generation() || target.track>=kTracks || target.pattern>=kPatterns)return false;const auto t=track_at(project_,target.pattern,target.track);unsigned slot=target.kit_pad<16?t.kit.pads[target.kit_pad].sample[0]:t.sample_slot;return t.instrument==target.instrument && slot==target.sample_slot && ui_.assignment_revision(target.pattern,target.track)==target.revision;}
 bool begin_live(uint32_t frames=0,uint8_t source=255,bool quantized=false,uint8_t restart_track=255,uint8_t restart_pattern=255,uint8_t bars=0,const RecordTarget* requested=nullptr,RecordingOrigin origin=RecordingOrigin::None){if(live_active_ || !ui_.audio_available() || !prepare_record_buffer() || !fs_.directory(capture_directory_))return false;char name[20]="Live-000.wav",temporary[20]="Live-000.part";bool free=false;for(unsigned i=1;i<1000;++i){name[5]=temporary[5]=char('0'+i/100);name[6]=temporary[6]=char('0'+(i/10)%10);name[7]=temporary[7]=char('0'+i%10);if(!join_path(live_path_,sizeof live_path_,capture_directory_,name) || !join_path(live_temp_,sizeof live_temp_,capture_directory_,temporary))return false;if(!fs_.exists(live_path_) && !fs_.exists(live_temp_)){free=true;break;}}uint8_t header[44];if(!free || !wav_header(header,0,engine_.sample_rate()))return false;uint64_t reserve_frames=frames?frames:bars?uint64_t(audio_bar_frames(engine_.sample_rate(),project_.tempo))*bars:uint64_t(engine_.sample_rate())*kLoopFreeSeconds;if(!fs_.begin_recording(live_temp_,header,44,44+reserve_frames*4)){fs_.remove(live_temp_);return false;}if(!engine_.start_live_record(quantized,frames,source,restart_track,restart_pattern,bars)){fs_.remove(live_temp_);return false;}live_quantized_=quantized;live_active_=true;live_stop_pending_=live_stop_sent_=live_write_failed_=false;live_frames_=live_buffered_frames_=0;live_peak_l_=live_peak_r_=0;record_input_dropped_=engine_.input_audio_dropped();live_target_=requested?*requested:record_target();ui_.live_recording(true,source==255?project_.routing.capture_bus:source,&live_target_,quantized && ui_.playing(),origin);return true;}
 bool recording_controls_busy()const{return twin_job_ || recording_busy() || stretch_active_ || freeze_active_ || capture_pending_ || browser_load_active_;}
 void sync_recording_controls(){ui_.recording_file_busy(recording_controls_busy());ui_.recording_cancel_pending(resample_active_ && resample_cancelled_);ui_.recording_stop_pending(live_active_ && live_stop_pending_);}
 struct RecordingUiSync {SessionFiles& owner;~RecordingUiSync(){owner.sync_recording_controls();}};
 bool begin_resample(){
  auto context=ui_.recording_controls();context.file_busy=recording_controls_busy();
  const auto request=ui_.resample_request();
  if(!resample_start_allowed(context)){copy_text(ui_.browser().status,sizeof ui_.browser().status,recording_hint(context,engine_.live_record_state()));return false;}
  if(request.source>kCaptureMax || request.bars<1 || request.bars>4){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Invalid resample request / destination kept");return false;}
  if(!target_unchanged(request.target)){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Resample target changed / request cancelled");return false;}
  if(!begin_live(0,request.source,true,255,255,request.bars,&request.target)){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Resample unavailable / check audio and storage");return false;}
  resample_active_=true;resample_cancelled_=false;ui_.resampling(true,&request);
  copy_text(ui_.browser().status,sizeof ui_.browser().status,"Wet resample armed / next bar");return true;
 }
 bool cancel_resample(){
  if(!resample_active_)return false;
  // Own the cancellation before service_live can finalize a completed bar.
  // Keep buffers/files until the DSP acknowledgement and queue drain.
  resample_cancelled_=true;live_stop_pending_=true;
  live_stop_sent_=engine_.live_record_done() || engine_.stop_live_record(false);
  ui_.recording_cancel_pending(true);
  copy_text(ui_.browser().status,sizeof ui_.browser().status,"Cancelling resample / flushing audio");return true;
 }
 bool begin_requested_record(){
  auto context=ui_.recording_controls();context.file_busy=recording_controls_busy();const auto request=ui_.live_record_request();
  auto fail=[&](const char* message){copy_text(ui_.browser().status,sizeof ui_.browser().status,message);return false;};
  if(!recording_start_allowed(context))return fail(recording_hint(context,engine_.live_record_state()));
  if(request.source>kCaptureMax || !target_unchanged(request.target))return fail("Recording target changed / request cancelled");
  if(!begin_live(0,request.source,request.quantized,255,255,0,&request.target,request.origin))return fail("Record unavailable / check storage and audio");
  return true;
 }
 void service_record_cancel(){
  auto action=ui_.pending_file_action();
  if(resample_active_ && (action==FileAction::CancelResample || action==FileAction::RecordToggle || action==FileAction::SampleRecordToggle)){ui_.take_file_action();cancel_resample();return;}
  if(ui_.record_stop_requested()){
   ui_.take_file_action();
   if(!live_active_)return;
   if(preparing_ || mixing_){end_overdub(false);return;}
   live_stop_pending_=true;ui_.recording_stop_pending(true);
   copy_text(ui_.browser().status,sizeof ui_.browser().status,"Stopping recording / flushing audio");
  }
 }

 bool lane_record_=false,lane_free_=false,lane_rounded_=false;unsigned lane_track_=0,lane_clip_=0;uint32_t lane_revision_=0,lane_generation_=0,lane_pad_=0,lane_tick_=0;AudioLane lane_before_{};
 bool lane_target_valid()const{return lane_generation_==engine_.staged_generation() && ui_.audio_revision(lane_track_)==lane_revision_;}
 bool bind_audio(unsigned slot,const char* path,WavView view,unsigned track,unsigned clip,uint32_t tick,const AudioLane& before,uint32_t crc=0){
  char checked[96];if(slot>=kSampleAssets || clip>=kAudioClips || !copy_text(checked,sizeof checked,path))return false;
  auto a=before;a.enabled=true;a.clips[clip]={tick,0,view.frames,1,uint8_t(slot)};
  SampleMetadata meta{};meta.rate=view.rate;meta.frames=view.frames;meta.bits=view.bits;meta.channels=uint8_t(view.channels);meta.content_crc=crc;
  if(!ui_.set_audio_lane(track,a,true,slot,view,meta))return false;
  copy_text(project_.assets[slot].path,sizeof project_.assets[slot].path,checked);project_.assets[slot].metadata=meta;bindings_[slot]=view;return true;
 }
 bool begin_audio_record(bool dub){
  if(!prepare_record_buffer())return false;
  const auto request=ui_.take_request();unsigned t=request.target.track;
  auto fail=[&](const char* text){copy_text(ui_.browser().status,sizeof ui_.browser().status,text);return false;};
  if(t>=16 || recording_controls_busy() || !ui_.playing() || request.target.generation!=engine_.staged_generation() || ui_.audio_revision(t)!=ui_.queued_audio_revision())return fail("Audio recording busy / target changed");
  const auto& a=project_.audio[t];if(a.kind==TrackKind::Instrument)return false;
  if(a.kind==TrackKind::Audio && (!project_.song.enabled || !project_.song.length))return fail("Enable SONG playback before punch in");
  unsigned clip=a.kind==TrackKind::Looper?0:ui_.queued_audio_clip();
  lane_before_=a;lane_track_=t;lane_clip_=clip;lane_revision_=ui_.audio_revision(t);lane_generation_=engine_.staged_generation();lane_free_=a.kind==TrackKind::Looper && a.length==LoopLength::Free;lane_rounded_=false;lane_pad_=0;
  if(dub){
   const auto& c=a.clips[0];if(a.kind!=TrackKind::Looper || c.asset>=64 || c.first || !a.enabled || !overdub_source_safe(project_.routing,a.source,t))return fail("Overdub needs loop / source excluding destination");
   const auto& asset=project_.assets[c.asset];WavView file;uint64_t offset=0;
   if(!sample_file_header(fs_,asset.path,file,offset) || file.rate!=engine_.sample_rate() || file.frames!=c.frames)return fail("Overdub needs complete loop at engine rate");
   WavView old=library_.lookup(asset.path);if(!old.data && !old.reader)return fail("Reload loop asset");
   char name[]="Dub-000.wav",temp[]="Dub-000.part";bool free=false;
   for(unsigned i=1;i<1000;++i){name[4]=temp[4]=char('0'+i/100);name[5]=temp[5]=char('0'+i/10%10);name[6]=temp[6]=char('0'+i%10);if(!join_path(mix_path_,sizeof mix_path_,capture_directory_,name) || !join_path(mix_temp_,sizeof mix_temp_,capture_directory_,temp))return false;if(!fs_.exists(mix_path_) && !fs_.exists(mix_temp_)){free=true;break;}}
   if(!free || !copy_text(overdub_base_,sizeof overdub_base_,asset.path))return false;
   lane_record_=true;overdub_=true;overdub_invalid_=false;preparing_=live_active_=true;mixing_=verifying_=false;overdub_source_=a.source;overdub_track_=t;overdub_pattern_=request.target.pattern;overdub_slot_=c.asset;overdub_generation_=lane_generation_;overdub_tempo_=project_.tempo;overdub_frames_=file.frames;overdub_view_=old;overdub_file_=file;overdub_offset_=offset;source_size_=fs_.size(overdub_base_);source_expected_crc_=asset.metadata.content_crc;verify_cursor_=0;source_crc_=ProjectCrc{};source_audio_crc_=ProjectCrc{};overdub_target_=request.target;
   ui_.live_recording(true,a.source,&overdub_target_,true,RecordingOrigin::Overdub);return true;
  }
  uint32_t frames=0;unsigned bars=0;
  if(a.kind==TrackKind::Looper){if(lane_free_)frames=engine_.sample_rate()*kLoopFreeSeconds;else bars=a.length==LoopLength::Pattern?(track_at(project_,project_.clips[t].pattern,t).length+15)/16:a.bars;}
  else{uint32_t bar=audio_bar_frames(engine_.sample_rate(),project_.tempo);frames=(67108864u/bar)*bar;}
  if(!begin_live(frames,a.source,true,255,255,uint8_t(bars),&request.target,RecordingOrigin::Source))return false;
  lane_record_=true;return true;
 }
 bool begin_overdub(){if(!prepare_record_buffer())return false;auto fail=[&](const char* reason){copy_text(ui_.browser().status,sizeof ui_.browser().status,reason);return false;};const auto request=ui_.take_request();if(!target_unchanged(request.target))return fail("Overdub cancelled / target changed");if(live_active_ || !ui_.playing())return fail("Overdub needs playback / stop current recording");unsigned target=request.target.track,pattern=request.target.pattern;unsigned source=overdub_capture_source(request.source);if(!overdub_source_safe(project_.routing,source,target))return fail("Overdub source includes destination / choose Input");overdub_source_=uint8_t(source);const auto t=track_at(project_,pattern,target);const auto& asset=project_.assets[t.sample_slot];WavView file;uint64_t offset=0;if(!project_.clips[target].playing || project_.clips[target].pattern!=pattern || !sample_file_header(fs_,asset.path,file,offset) || file.rate!=engine_.sample_rate() || !overdub_track_compatible(t,asset,file.frames,file.rate,project_.tempo,project_.tuning[asset.metadata.root%12]))return fail("Overdub needs full 1-4 bar take / use LOOP TAKE");WavView old=library_.lookup(asset.path);if((!old.data && !old.reader) || old.frames!=file.frames || old.rate!=file.rate || old.channels!=file.channels || old.bits!=file.bits || old.format!=file.format)return fail("Overdub source changed / reload sample");char name[]="Dub-000.wav",temp[]="Dub-000.part";bool free=false;for(unsigned i=1;i<1000;++i){name[4]=temp[4]=char('0'+i/100);name[5]=temp[5]=char('0'+(i/10)%10);name[6]=temp[6]=char('0'+i%10);if(!join_path(mix_path_,sizeof mix_path_,capture_directory_,name) || !join_path(mix_temp_,sizeof mix_temp_,capture_directory_,temp))return false;if(!fs_.exists(mix_path_) && !fs_.exists(mix_temp_)){free=true;break;}}if(!free || !copy_text(overdub_base_,sizeof overdub_base_,asset.path))return false;overdub_=true;overdub_invalid_=false;preparing_=true;mixing_=verifying_=false;live_active_=true;overdub_track_=target;overdub_pattern_=pattern;overdub_slot_=t.sample_slot;overdub_generation_=engine_.staged_generation();overdub_revision_=ui_.assignment_revision(pattern,target);overdub_tempo_=project_.tempo;overdub_frames_=file.frames;overdub_view_=old;overdub_file_=file;overdub_offset_=offset;source_size_=fs_.size(overdub_base_);source_expected_crc_=asset.metadata.content_crc;verify_cursor_=0;source_crc_=ProjectCrc{};source_audio_crc_=ProjectCrc{};overdub_target_=request.target;ui_.live_recording(true,overdub_source_,&overdub_target_,true,RecordingOrigin::Overdub);copy_text(ui_.browser().status,sizeof ui_.browser().status,"Checking source / overdub armed");return true;}
 bool overdub_target_valid()const{if(lane_record_)return lane_target_valid() && project_.tempo==overdub_tempo_ && overdub_source_safe(project_.routing,overdub_source_,overdub_track_);return overdub_source_safe(project_.routing,overdub_source_,overdub_track_) && overdub_generation_==engine_.staged_generation() && ui_.assignment_revision(overdub_pattern_,overdub_track_)==overdub_revision_ && project_.active_pattern==overdub_pattern_ && project_.tempo==overdub_tempo_ && project_.clips[overdub_track_].playing && project_.clips[overdub_track_].pattern==overdub_pattern_ && track_at(project_,overdub_pattern_,overdub_track_).sample_slot==overdub_slot_ && overdub_track_compatible(track_at(project_,overdub_pattern_,overdub_track_),project_.assets[overdub_slot_],overdub_frames_,overdub_file_.rate,overdub_tempo_,project_.tuning[project_.assets[overdub_slot_].metadata.root%12]) && project_.assets[overdub_slot_].metadata.content_crc==source_expected_crc_ && !__builtin_strcmp(project_.assets[overdub_slot_].path,overdub_base_);}
 void end_overdub(bool success){lane_record_=false;preparing_=verifying_=mixing_=overdub_=live_active_=false;ui_.live_recording(false);copy_text(ui_.browser().status,sizeof ui_.browser().status,success?"Overdub assigned / Undo restores take":"Overdub not assigned / source retained");ui_.notify(success?UiNotice::OverdubDone:UiNotice::OverdubFailed);}
 // Source checks are incremental so filesystem work never enters the DSP callback.
 void service_prepare(){if(!preparing_)return;if(!overdub_target_valid() || !ui_.playing() || fs_.size(overdub_base_)!=source_size_){end_overdub(false);return;}uint8_t bytes[4096];unsigned got=0;unsigned count=unsigned(source_size_-verify_cursor_>sizeof bytes?sizeof bytes:source_size_-verify_cursor_);if(!count || !fs_.read(overdub_base_,verify_cursor_,bytes,count,got) || got!=count){end_overdub(false);return;}uint64_t audio_end=overdub_offset_+uint64_t(overdub_frames_)*wav_frame_bytes(overdub_file_);for(unsigned i=0;i<count;++i){uint64_t at=verify_cursor_+i;source_crc_.add(bytes[i]);if(at>=overdub_offset_ && at<audio_end){source_audio_crc_.add(bytes[i]);if(overdub_view_.data && bytes[i]!=overdub_view_.data[at-overdub_offset_]){end_overdub(false);return;}}}verify_cursor_+=count;if(verify_cursor_<source_size_)return;source_baseline_crc_=source_crc_.value();source_audio_baseline_crc_=source_audio_crc_.value();if(source_expected_crc_ && source_expected_crc_!=source_baseline_crc_){end_overdub(false);return;}preparing_=false;live_active_=false;if(!begin_live(overdub_frames_,overdub_source_,true,uint8_t(overdub_track_),uint8_t(overdub_pattern_),0,&overdub_target_,RecordingOrigin::Overdub)){end_overdub(false);return;}record_data_crc_=ProjectCrc{};}
 void finish_mix(){if(fs_.exists(mix_path_) || !fs_.move(mix_temp_,mix_path_)){end_overdub(false);return;}unsigned slot=free_asset_slot(project_);WavView mixed;SampleMetadata meta{};bool ok=slot<64 && library_.load(mix_path_,mixed);if(ok){meta=project_.assets[overdub_slot_].metadata;meta.rate=mixed.rate;meta.frames=mixed.frames;meta.bits=mixed.bits;meta.channels=uint8_t(mixed.channels);meta.content_crc=mix_crc_.value();meta.missing=false;char checked[96];if(ok)ok=copy_text(checked,sizeof checked,mix_path_);if(ok)ok=lane_record_?bind_audio(slot,mix_path_,mixed,lane_track_,0,0,lane_before_,meta.content_crc):ui_.assign_overdub(slot,mixed,meta,overdub_view_,overdub_track_,overdub_pattern_);if(ok){copy_text(project_.assets[slot].path,sizeof project_.assets[slot].path,checked);bindings_[slot]=mixed;}}end_overdub(ok);}
 void service_verify(){if(!overdub_target_valid() || fs_.size(overdub_base_)!=source_size_ || fs_.size(mix_temp_)!=44+uint64_t(overdub_frames_)*4 || fs_.size(live_path_)!=44+uint64_t(overdub_frames_)*4){end_overdub(false);return;}uint8_t bytes[4096];unsigned got=0;uint64_t output_size=44+uint64_t(overdub_frames_)*4;unsigned source_count=verify_cursor_<source_size_?unsigned(source_size_-verify_cursor_>sizeof bytes?sizeof bytes:source_size_-verify_cursor_):0;unsigned output_count=verify_cursor_<output_size?unsigned(output_size-verify_cursor_>sizeof bytes?sizeof bytes:output_size-verify_cursor_):0;if(source_count){if(!fs_.read(overdub_base_,verify_cursor_,bytes,source_count,got) || got!=source_count){end_overdub(false);return;}for(unsigned i=0;i<source_count;++i)source_crc_.add(bytes[i]);}if(output_count){if(!fs_.read(mix_temp_,verify_cursor_,bytes,output_count,got) || got!=output_count){end_overdub(false);return;}for(unsigned i=0;i<output_count;++i)verify_output_crc_.add(bytes[i]);}if(output_count){if(!fs_.read(live_path_,verify_cursor_,bytes,output_count,got) || got!=output_count){end_overdub(false);return;}uint8_t expected_header[44];if(!wav_header(expected_header,overdub_frames_,engine_.sample_rate())){end_overdub(false);return;}for(unsigned i=0;i<output_count;++i){uint64_t at=verify_cursor_+i;if(at<44){if(bytes[i]!=expected_header[at]){end_overdub(false);return;}}else verify_input_crc_.add(bytes[i]);}}verify_cursor_+=sizeof bytes;if(verify_cursor_<source_size_ || verify_cursor_<output_size)return;if(source_crc_.value()!=source_baseline_crc_ || verify_output_crc_.value()!=mix_crc_.value() || verify_input_crc_.value()!=record_data_crc_.value()){end_overdub(false);return;}verifying_=false;finish_mix();}
 void service_mix(){if(!mixing_)return;if(verifying_){service_verify();return;}if(!overdub_target_valid() || fs_.size(overdub_base_)!=source_size_){end_overdub(false);return;}unsigned count=overdub_frames_-mix_frame_;if(count>256)count=256;uint8_t base[2048],input[1024],output[1024];unsigned bytes=wav_frame_bytes(overdub_file_),got=0;if(!count || bytes>8 || !fs_.read(overdub_base_,overdub_offset_+uint64_t(mix_frame_)*bytes,base,count*bytes,got) || got!=count*bytes || !fs_.read(live_path_,44+uint64_t(mix_frame_)*4,input,count*4,got) || got!=count*4){end_overdub(false);return;}for(unsigned i=0;i<count*bytes;++i)mix_source_crc_.add(base[i]);for(unsigned i=0;i<count*4;++i)mix_input_crc_.add(input[i]);WavView b=overdub_file_;b.data=base;b.reader=nullptr;b.frames=count;WavView in{input,count,engine_.sample_rate(),2,16,1,nullptr};for(unsigned i=0;i<count;++i)for(unsigned ch=0;ch<2;++ch){float value=wav_sample(b,i,b.channels==1?0:ch)*(lane_record_?lane_before_.feedback:1)+wav_sample(in,i,ch);uint16_t q=uint16_t(pcm16(value));output[4*i+2*ch]=uint8_t(q);output[4*i+2*ch+1]=uint8_t(q>>8);}if(!fs_.write(mix_temp_,44+uint64_t(mix_frame_)*4,output,count*4,false)){end_overdub(false);return;}for(unsigned i=0;i<count*4;++i)mix_crc_.add(output[i]);mix_frame_+=count;if(mix_frame_<overdub_frames_)return;if(mix_source_crc_.value()!=source_audio_baseline_crc_ || mix_input_crc_.value()!=record_data_crc_.value()){end_overdub(false);return;}verifying_=true;verify_cursor_=0;source_crc_=ProjectCrc{};verify_output_crc_=ProjectCrc{};verify_input_crc_=ProjectCrc{};copy_text(ui_.browser().status,sizeof ui_.browser().status,"Verifying overdub / source preserved");}

 void finish_probe_record(bool saved){
  LoopbackRecordResult result;result.ready=true;result.saved=saved;result.frames=live_frames_;result.dropped=engine_.input_audio_dropped()-probe_input_dropped_;
  WavView file;uint64_t offset=0;
  bool verified=saved && sample_file_header(fs_,live_path_,file,offset) && file.frames==live_frames_ && file.rate==engine_.sample_rate() && file.channels==2 && file.bits==16 && file.format==1;
  double sum_l=0,sum_r=0,tone_ls=0,tone_lc=0,tone_rs=0,tone_rc=0;float peak_l=0,peak_r=0;uint8_t bytes[4096];
  const double radians=6.283185307179586*440.0/double(engine_.sample_rate());const double step_c=cos(radians),step_s=sin(radians);double phase_c=1,phase_s=0;
  for(unsigned at=0;verified && at<live_frames_;){
   unsigned count=live_frames_-at;if(count>1024)count=1024;unsigned got=0;
   if(!fs_.read(live_path_,offset+uint64_t(at)*4,bytes,count*4,got) || got!=count*4){verified=false;break;}
   WavView chunk{bytes,count,file.rate,2,16,1,nullptr};
   for(unsigned i=0;i<count;++i){float l=wav_sample(chunk,i,0),r=wav_sample(chunk,i,1);sum_l+=double(l)*l;sum_r+=double(r)*r;tone_ls+=l*phase_s;tone_lc+=l*phase_c;tone_rs+=r*phase_s;tone_rc+=r*phase_c;double next_c=phase_c*step_c-phase_s*step_s;phase_s=phase_s*step_c+phase_c*step_s;phase_c=next_c;if(fabsf(l)>peak_l)peak_l=fabsf(l);if(fabsf(r)>peak_r)peak_r=fabsf(r);}
   at+=count;
  }
  result.verified=verified;
  if(verified && live_frames_){result.rms_l_u=unsigned(sqrt(sum_l/live_frames_)*1000000+0.5);result.rms_r_u=unsigned(sqrt(sum_r/live_frames_)*1000000+0.5);result.peak_l_u=unsigned(peak_l*1000000+0.5);result.peak_r_u=unsigned(peak_r*1000000+0.5);double scale=2000000.0/live_frames_;result.tone_l_u=unsigned(sqrt(tone_ls*tone_ls+tone_lc*tone_lc)*scale+0.5);result.tone_r_u=unsigned(sqrt(tone_rs*tone_rs+tone_rc*tone_rc)*scale+0.5);}
  if(fs_.exists(live_temp_))fs_.remove(live_temp_);
  if(fs_.exists(live_path_))fs_.remove(live_path_);
  result.cleaned=!fs_.exists(live_temp_) && !fs_.exists(live_path_);
  live_active_=probe_recording_=false;ui_.live_recording(false);probe_result_=result;
 }
 void service_live(){if(!live_active_)return;if(preparing_){service_prepare();return;}if(mixing_){service_mix();return;}if(overdub_ && !overdub_target_valid())overdub_invalid_=true;uint8_t* bytes=live_write_buffer_;unsigned count=live_buffered_frames_;LiveRecordFrame f;while(count<LiveWriteFrames && engine_.pop_live_record(f)){uint16_t l=uint16_t(pcm16(f.l)),r=uint16_t(pcm16(f.r));float pl=fabsf(f.l),pr=fabsf(f.r);if(pl>live_peak_l_)live_peak_l_=pl;if(pr>live_peak_r_)live_peak_r_=pr;bytes[4*count]=uint8_t(l);bytes[4*count+1]=uint8_t(l>>8);bytes[4*count+2]=uint8_t(r);bytes[4*count+3]=uint8_t(r>>8);++count;}live_buffered_frames_=count;bool flush=count==LiveWriteFrames || engine_.live_record_done() || live_stop_pending_;if(count && flush && !live_write_failed_){if(live_frames_>(0xffffffffu-36)/4-count || !fs_.write(live_temp_,44+uint64_t(live_frames_)*4,bytes,count*4,false)){live_write_failed_=true;live_stop_pending_=true;live_stop_sent_=false;}else{if(overdub_)for(unsigned i=0;i<count*4;++i)record_data_crc_.add(bytes[i]);live_frames_+=count;}live_buffered_frames_=0;}if(live_write_failed_)live_buffered_frames_=0;if(live_stop_pending_ && !live_stop_sent_ && !engine_.live_record_done())live_stop_sent_=engine_.stop_live_record(live_quantized_ && !(lane_record_ && lane_free_) && !live_write_failed_ && !overdub_ && !resample_cancelled_);if(!engine_.live_record_done() || !engine_.live_record_empty())return;if(lane_record_ && !overdub_ && lane_free_ && live_frames_ && !live_write_failed_ && !engine_.live_record_failed()){
 if(!lane_rounded_){uint32_t rounded=quantized_loop_frames(live_frames_,engine_.sample_rate(),project_.tempo),bar=audio_bar_frames(engine_.sample_rate(),project_.tempo);uint32_t cap=engine_.sample_rate()*kLoopFreeSeconds;if(rounded>cap)rounded=(cap/bar)*bar;lane_pad_=rounded>live_frames_?rounded-live_frames_:0;if(rounded<live_frames_)live_frames_=rounded;lane_rounded_=true;}
 if(lane_pad_){uint8_t silence[4096]{};unsigned n=lane_pad_>1024?1024:lane_pad_;if(!fs_.write(live_temp_,44+uint64_t(live_frames_)*4,silence,n*4,false)){live_write_failed_=true;lane_pad_=0;}else{live_frames_+=n;lane_pad_-=n;}return;}
 }
 bool ok=!live_write_failed_ && !engine_.live_record_failed() && live_frames_>0 && (!resample_active_ || (engine_.live_record_bars_complete() && !resample_cancelled_));bool empty_stop=live_stop_pending_ && !live_frames_ && !live_write_failed_ && !engine_.live_record_failed() && !resample_active_ && !overdub_;bool cancel_clean=!(resample_cancelled_ || empty_stop) || fs_.remove(live_temp_);uint8_t header[44];if(lane_record_ && !lane_free_ && !overdub_ && !engine_.live_record_bars_complete() && !ui_.playing())ok=false;if(ok)ok=wav_header(header,live_frames_,engine_.sample_rate()) && fs_.finish_recording(live_temp_,header,44,44+uint64_t(live_frames_)*4) && !fs_.exists(live_path_) && fs_.move(live_temp_,live_path_);unsigned record_failure=live_write_failed_?5u:engine_.live_record_failure_reason();engine_.finish_live_record();if(probe_recording_){finish_probe_record(ok);return;}if(overdub_){if(!ok || live_frames_!=overdub_frames_ || overdub_invalid_ || engine_.live_record_conflict() || !overdub_target_valid() || !wav_header(header,overdub_frames_,engine_.sample_rate()) || fs_.exists(mix_temp_) || !fs_.write(mix_temp_,0,header,44,true)){end_overdub(false);return;}mix_frame_=0;mix_crc_=ProjectCrc{};mix_source_crc_=ProjectCrc{};mix_input_crc_=ProjectCrc{};for(unsigned i=0;i<44;++i)mix_crc_.add(header[i]);mixing_=true;copy_text(ui_.browser().status,sizeof ui_.browser().status,"Mixing overdub / source preserved");return;}live_active_=false;ui_.live_recording(false);bool assigned=false;unsigned assigned_slot=64;if(ok && (lane_record_?lane_target_valid():target_unchanged(live_target_))){unsigned slot=free_asset_slot(project_);WavView view;assigned=slot<64 && library_.load(live_path_,view) && (lane_record_?bind_audio(slot,live_path_,view,lane_track_,lane_clip_,lane_before_.kind==TrackKind::Audio?engine_.live_record_tick():0,lane_before_):bind(slot,live_path_,view,live_target_.track,live_target_.pattern,live_target_.kit_pad));if(assigned){assigned_slot=slot;show_recorded_sample(live_path_,view);}}RecordingResult result;result.queue_highwater=engine_.live_record_highwater();if(!ok)copy_text(result.partial,sizeof result.partial,live_temp_);result.ready=true;result.failure=record_failure;result.saved=ok;result.assigned=assigned;result.frames=live_frames_;result.slot=assigned_slot;result.peak_l_u=unsigned(live_peak_l_*1000000+0.5f);result.peak_r_u=unsigned(live_peak_r_*1000000+0.5f);result.dropped=engine_.input_audio_dropped()-record_input_dropped_;WavView checked;uint64_t offset=0;result.verified=ok && sample_file_header(fs_,live_path_,checked,offset) && checked.frames==live_frames_ && checked.rate==engine_.sample_rate() && checked.channels==2 && checked.bits==16 && checked.format==1;record_result_=result;lane_record_=false;copy_text(ui_.browser().status,sizeof ui_.browser().status,resample_cancelled_?(cancel_clean?"Resample cancelled / destination kept":"Cancelled / partial retained; destination kept"):empty_stop?(cancel_clean?"Recording stopped before audio / destination kept":"Stopped / empty partial retained; destination kept"):assigned?(resample_active_?"Wet resample assigned / LOOP TAKE for overdub":"Recorded / assigned to track"):ok?"Recorded to WAV / not assigned":"Recording interrupted / partial retained; target kept");ui_.notify((resample_cancelled_ || empty_stop)?(cancel_clean?UiNotice::None:UiNotice::Failed):assigned?UiNotice::SampleRecorded:ok?UiNotice::Saved:UiNotice::Failed);if(resample_active_)ui_.bake_recorded(live_target_.track,assigned);resample_active_=resample_cancelled_=false;ui_.resampling(false);}


 uint32_t device_saved_revision_=0;uint64_t device_due_=0,space_due_=0;
 void service_device(){
  if(live_active_ || project_save_busy() || kit_load_busy() || browser_load_active_)return;
  uint64_t now=ui_.ui_time();
  if(ui_.device_revision()!=device_saved_revision_ && now>=device_due_){uint8_t bytes[16];encode_device(ui_.device_settings(),bytes);bool ok=fs_.directory("GROOVE") && fs_.write("GROOVE/DEVICE.TMP",0,bytes,16,true);uint8_t check[16];unsigned got=0;DeviceSettings decoded;ok=ok && fs_.read("GROOVE/DEVICE.TMP",0,check,16,got) && got==16 && decode_device(check,decoded) && !__builtin_memcmp(bytes,check,16);if(ok)ok=fs_.remove("GROOVE/DEVICE.CFG") && fs_.move("GROOVE/DEVICE.TMP","GROOVE/DEVICE.CFG");if(ok)device_saved_revision_=ui_.device_revision();ui_.device_saved(ok);device_due_=now+1000;}
  if(ui_.mode()==UiMode::Settings && ui_.settings_page()==6 && now>=space_due_){uint64_t free=0,total=0;bool ok=fs_.space(free,total);ui_.storage_space(ok,free,total);space_due_=now+5000;}
 }
public:
 SessionFiles(IFileAccess& f,ISampleLibrary& l,Engine& e,ProjectState& p,Performance& ui,const char* recordings="RECORDINGS"):fs_(f),library_(l),engine_(e),project_(p),ui_(ui),midi_capture_(alloc_new<MidiCapture>(f,e)),midi_import_(f,e,p),analysis_(f){ui_.composition_file_service(true);uint8_t identity_seed[16];if(fs_.random_bytes(identity_seed,16))ui_.seed_preset_guids(identity_seed);copy_text(capture_directory_,sizeof capture_directory_,recordings);uint8_t bytes[16];unsigned got=0;DeviceSettings prefs;if((fs_.read("GROOVE/DEVICE.CFG",0,bytes,16,got) && got==16 && decode_device(bytes,prefs)) || (fs_.read("GROOVE/DEVICE.TMP",0,bytes,16,got) && got==16 && decode_device(bytes,prefs))){ui_.set_device_settings(prefs);ui_.device_saved(true);}device_saved_revision_=ui_.device_revision();}
 // Only enable for libraries whose resident sample buffers remain immutable and alive.
 void set_resident_analysis_worker(SampleAnalysisSlot* worker){resident_worker_=worker;ui_.browser().analysis_background=worker!=nullptr;}
 void set_project_save_worker(ProjectEncodeSlot* worker){if(!project_save_ && worker)project_save_=alloc_new<AsyncProjectSave>(fs_,*worker);}
 void set_kit_probe_hook(void (*hook)(void*,unsigned),void* context){kit_probe_hook_=hook;kit_probe_context_=context;}
 void kit_probe_phase(unsigned phase){if(kit_probe_ && kit_probe_hook_)kit_probe_hook_(kit_probe_context_,phase);}
 void set_project_load_worker(ProjectDecodeSlot* worker){if(!project_read_ && worker)project_read_=alloc_new<AsyncProjectRead>(fs_,*worker);}
 // MGXC fallback preservation (docs/project-format.md): payloads of
 // unknown engines/chunks in the current project, written back on save.
 const PreservedStore& preserved_store()const{return preserved_;}
 bool sync_preserved_from(PreservedStore& other){DefinitionExtensions replacement;if(!preserved_.copy_from(other) || !adopt_definition_extensions(project_,preserved_,&replacement))return false;definitions_.steal_from(replacement);bool referenced=false;for(const auto& t:project_.tracks)referenced|=t.preset_identity.assigned();if(referenced && !refresh_preset_library())return false;update_preset_status();return true;}
 bool copy_preserved_to(PreservedStore& out){return synchronize_definition_extensions() && out.copy_from(preserved_);}
 void clear_preserved(){preserved_.clear();definitions_.clear();}
 bool project_save_available()const{return project_save_!=nullptr;}
 bool project_save_busy()const{return project_save_ && project_save_->busy();}
 void project_locator(const char* path){copy_text(project_locator_,sizeof project_locator_,path);}
 bool remember_project(const char* path){ui_.project_path(path);bool ok=!project_locator_[0] || write_project_location(fs_,project_locator_,path);if(!ok)ui_.notify(UiNotice::SavedLocationFailed);return ok;}
 char project_locator_[160]{};
 bool save_named_project(){
  const auto& e=ui_.browser().edit;char parent[160],name[104],path[160];copy_text(parent,sizeof parent,e.source);char* slash=nullptr;for(char* p=parent;*p;++p)if(*p=='/')slash=p;if(slash)*slash=0;else parent[0]=0;
  if(!data_content_name(name,sizeof name,e.name,unsigned(ContentKind::Song)))return false;
  if(!browser_name_valid(e.name) || !join_path(path,sizeof path,parent,name) || fs_.exists(path)){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Name exists or invalid / choose another name");ui_.notify(UiNotice::SaveFailed);return false;}
  bool ok=project_save_available()?start_project_save(path):save(path);ui_.notify(ok?(project_save_available()?UiNotice::Saving:UiNotice::Saved):UiNotice::SaveFailed);return ok;
 }
 char project_save_path_[160]{};
 bool ensure_project_parent(const char* path){if(factory_path(path))return false;char parent[160];if(!copy_text(parent,sizeof parent,path))return false;parent_path(parent);return !parent[0] || fs_.directory(parent);}
 bool start_project_save(const char* path,bool probe=false){if(!ensure_project_parent(path) || !project_save_available() || project_save_busy() || kit_load_busy() || !ui_.require_project_save_safe())return false;ui_.sync_ui_settings();if(!ui_.prepare_preset_references() || !heal_save_assets() || !synchronize_definition_extensions())return false;if(!project_save_->start(project_,path,probe,&preserved_))return false;project_save_probe_=probe;copy_text(project_save_path_,sizeof project_save_path_,path);if(!probe)ui_.notify(UiNotice::Saving);sync_recording_controls();return true;}
 bool take_project_save_result(bool& success,bool& probe){if(!project_save_report_)return false;success=project_save_report_==1;probe=project_save_report_probe_;project_save_report_=0;return true;}
 void set_project_save_observer(void (*fn)(void*,unsigned,bool),void* context){save_observer_=fn;save_observer_context_=context;}
 void service_project_save(){if(!project_save_ || !project_save_->busy())return;unsigned phase=project_save_->service_phase();if(save_observer_)save_observer_(save_observer_context_,phase,true);project_save_->service(engine_.project_snapshot_safe(),engine_.project_event_loss());if(save_observer_)save_observer_(save_observer_context_,phase,false);auto outcome=project_save_->take_outcome();if(outcome==AsyncProjectSave::Outcome::None)return;bool lost=engine_.project_event_loss();bool ok=outcome==AsyncProjectSave::Outcome::Saved && !lost;bool remembered=!ok || project_save_probe_ || remember_project(project_save_path_);project_save_report_=ok?1:2;project_save_report_probe_=project_save_probe_;if(!project_save_probe_)ui_.notify(lost?UiNotice::ProjectUnsynced:ok?(remembered?UiNotice::SnapshotSaved:UiNotice::SavedLocationFailed):UiNotice::SaveFailed);project_save_probe_=false;}

 bool start_loopback_probe(){if(probe_recording_ || live_active_ || !begin_live(engine_.sample_rate(),1,false))return false;probe_result_=LoopbackRecordResult{};probe_input_dropped_=engine_.input_audio_dropped();probe_recording_=true;return true;}
 bool start_sample_workflow_probe(){return !live_active_ && begin_live(engine_.sample_rate(),1,false);}
 bool take_recording_result(RecordingResult& result){if(!record_result_.ready)return false;result=record_result_;record_result_=RecordingResult{};return true;}
 bool take_loopback_probe_result(LoopbackRecordResult& result){if(!probe_result_.ready)return false;result=probe_result_;probe_result_=LoopbackRecordResult{};return true;}
 ~SessionFiles(){ui_.composition_file_service(false);delete twin_job_;delete[] live_write_buffer_;clear_stretch();if(freeze_active_)cancel_freeze();delete kit_verifier_;delete[] kit_asset_snapshot_;delete[] kit_prepared_;dispose_kit_candidate();delete kit_target_;delete project_read_;delete project_save_;delete midi_capture_;}
 SessionFiles(const SessionFiles&)=delete;SessionFiles& operator=(const SessionFiles&)=delete;
 bool load_sample_path(const char* path){WavView view;unsigned slot=kSampleAssets;if(!path || !library_.load(path,view) || !select_load_slot(path,view,slot))return false;return bind(slot,path,view);}
 void copy_bindings(WavView* out){synchronize_bindings();for(unsigned i=0;i<kSampleAssets;++i)out[i]=bindings_[i];}
 void set_bindings(const WavView* views){for(unsigned i=0;i<kSampleAssets;++i)bindings_[i]=views?views[i]:WavView{};}
 // Normal file admission requires registered engines throughout the content.
 // The codec still preserves opaque data for inspection/roundtrip, not playback.
 bool admit_content_engines(const ProjectState& p){
  auto known=[](const auto& sound){if(!engine_descriptor(unsigned(sound.instrument)))return false;if(sound.instrument==Instrument::Layer)for(unsigned n=0;n<sound.layer.count;++n)if(!engine_descriptor(unsigned(sound.layer.parts[n].sound.instrument)))return false;return true;};
  bool ok=true;
  for(unsigned n=0;n<p.unavailable_targets.count;++n){Instrument engine;const char* guid=p.unavailable_targets.guid[n];ok=ok && engine_from_guid(guid,__builtin_strlen(guid),engine);}
  for(const auto& t:p.tracks)ok=ok && known(t);
  for(const auto& sound:p.sounds)ok=ok && known(sound);
  auto bank=[](const auto& snapshots){for(const auto& snapshot:snapshots)if(!engine_descriptor(unsigned(snapshot.engine)))return false;return true;};
  ok=ok && bank(p.morph_a) && bank(p.morph_b);for(unsigned n=0;n<3;++n)ok=ok && bank(p.morph_extra_a[n]) && bank(p.morph_extra_b[n]);
  if(!ok){copy_text(content_error_,sizeof content_error_,"Unsupported engine / current content kept");copy_text(ui_.browser().status,sizeof ui_.browser().status,content_error_);ui_.mark_dirty();}return ok;
 }
 bool validate_loaded_candidate(ProjectState& attempt,const char* path,bool verify_assets=true,const PreservedStore* store=nullptr){
  return attempt.content_kind==ContentKind::Song && admit_content_engines(attempt) && project_paths(attempt,path,true,true) && (!verify_assets || (resolve_assets(attempt) && resolve_pinned_presets(attempt,store) && verify_project_assets(fs_,attempt)));
 }

 bool supported_container(const char* path){uint8_t header[6];unsigned got=0;if(fs_.read(path,0,header,sizeof header,got) && got==sizeof header && !__builtin_memcmp(header,"MGXC",4) && (unsigned(header[4])+(unsigned(header[5])<<8))!=kFormatVersion){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Unsupported MGXC version / old files are not migrated");ui_.notify(UiNotice::Failed);return false;}return true;}
 bool kit_load_busy()const{return (project_read_ && project_read_->busy()) || kit_ready_!=nullptr;}
 bool start_kit_load(const char* path,bool probe=false,bool audition=false,bool install_preset=false){content_error_[0]=0;if(!supported_container(path))return false;
  if(!project_read_ || kit_load_busy() || project_save_busy() || !path || !copy_text(kit_path_,sizeof kit_path_,path))return false;
  kit_pattern_=project_.active_pattern;kit_track_=ui_.selected_track();kit_generation_=engine_.staged_generation();kit_target_=alloc_new<TrackState>();if(!kit_target_)return false;*kit_target_=track_at(project_,kit_pattern_,kit_track_);
  if(!project_read_->start(path)){delete kit_target_;kit_target_=nullptr;if(!probe){copy_text(ui_.browser().status,sizeof ui_.browser().status,"Kit read unavailable / previous kit kept");ui_.notify(UiNotice::Failed);}return false;}
  kit_install_preset_=!audition;kit_audition_=audition;kit_audition_cancel_=false;if(audition){auto& b=ui_.browser();preview_ticket_=0;b.previewing=true;copy_text(b.audition_path,sizeof b.audition_path,path);}
  kit_probe_=probe;kit_report_=0;kit_decoding_seen_=false;if(!probe){copy_text(ui_.browser().status,sizeof ui_.browser().status,audition?"Loading preset preview / STOP cancels":install_preset?"Loading preset / playback continues":"Loading kit / playback continues");ui_.mark_dirty();}return true;
 }
 const char* kit_load_error()const{return kit_error_;}
 bool take_kit_load_result(bool& ok,bool& probe){if(!kit_report_)return false;ok=kit_report_==1;probe=kit_report_probe_;kit_report_=0;return true;}
 bool kit_target_unchanged()const{return kit_target_ && kit_pattern_==project_.active_pattern && kit_track_==ui_.selected_track() && kit_generation_==engine_.staged_generation() && same_track_snapshot(*kit_target_,track_at(project_,kit_pattern_,kit_track_));}

 void dispose_kit_candidate(){if(kit_ready_){if(!project_read_ || !project_read_->recycle(kit_ready_))delete kit_ready_;kit_ready_=nullptr;}}
 void finish_kit_load(bool ok,const char* message=nullptr){const bool preview_cancelled=kit_audition_ && (kit_audition_cancel_ || !audition_selection(kit_path_,kit_track_,kit_generation_));kit_error_=ok?"":message?message:"Kit validation/allocation failed";if(kit_audition_ && !ok){ui_.browser().previewing=false;ui_.browser().audition_path[0]=0;}kit_store_.clear();if(kit_asset_loading_){library_.cancel_incremental_load();kit_asset_loading_=false;}if(!ok && !kit_probe_){copy_text(ui_.browser().status,sizeof ui_.browser().status,preview_cancelled?"Preset preview stopped":message?message:"Kit load failed / previous kit kept");if(!preview_cancelled)ui_.notify(UiNotice::Failed);}delete kit_verifier_;kit_verifier_=nullptr;delete[] kit_asset_snapshot_;kit_asset_snapshot_=nullptr;delete[] kit_prepared_;kit_prepared_=nullptr;dispose_kit_candidate();delete kit_target_;kit_target_=nullptr;kit_preparing_=kit_verified_=kit_final_check_=false;kit_report_=ok?1:2;kit_report_probe_=kit_probe_;kit_probe_=false;kit_audition_=kit_install_preset_=false;}
 void service_kit_load(){
  if(!project_read_)return;
  if(kit_audition_ && ui_.pending_file_action()==FileAction::StopPreview)kit_audition_cancel_=true;
  if(project_read_->busy()){if(kit_audition_cancel_ || !kit_target_unchanged() || (kit_audition_ && !audition_selection(kit_path_,kit_track_,kit_generation_)))project_read_->cancel();project_read_->service();if(project_read_->decoding() && !kit_decoding_seen_){kit_decoding_seen_=true;kit_probe_phase(0);}if(project_read_->busy())return;ProjectState* ready=nullptr;unsigned version=0;auto outcome=project_read_->take(ready,version,&kit_store_);if(outcome==AsyncProjectRead::Outcome::Ready){kit_ready_=ready;kit_version_=version;kit_probe_phase(1);}else{delete ready;finish_kit_load(false,outcome==AsyncProjectRead::Outcome::Cancelled?"Kit load cancelled / previous kit kept":"Kit read failed / previous kit kept");return;}}
  if(!kit_ready_)return;
  if(kit_audition_cancel_ || !kit_target_unchanged() || (kit_audition_ && !audition_selection(kit_path_,kit_track_,kit_generation_))){finish_kit_load(false,"Kit target changed / load cancelled");return;}
  if(engine_.project_event_loss()){finish_kit_load(false,"State lost / reload before loading kit");return;}
  if(engine_.project_events_pending())return;
  if(!kit_preparing_){if(!validate_audition(*kit_ready_,kit_path_) || !resolve_assets(*kit_ready_)){finish_kit_load(false,content_error_[0]?content_error_:"Invalid preset / sound kept");return;}kit_asset_snapshot_=alloc_array<SampleAssetState>(kSampleAssets);kit_prepared_=alloc_array<WavView>(kSampleAssets);if(!kit_asset_snapshot_ || !kit_prepared_){finish_kit_load(false);return;}for(unsigned i=0;i<kSampleAssets;++i)kit_asset_snapshot_[i]=project_.assets[i];kit_verifier_=alloc_new<KitAssetVerifier>(fs_,kit_ready_->assets,&library_);if(!kit_verifier_){finish_kit_load(false);return;}kit_asset_cursor_=0;kit_preparing_=true;kit_probe_phase(2);return;}
  if(!kit_verified_){auto result=kit_verifier_->service();if(result==KitAssetVerifier::Result::Failed){finish_kit_load(false,"Kit sample invalid / previous kit kept");return;}if(result==KitAssetVerifier::Result::Done){kit_verified_=true;kit_probe_phase(3);}return;}
  if(kit_asset_cursor_<kSampleAssets){unsigned slot=kit_asset_cursor_;const auto& asset=kit_ready_->assets[slot];if(!asset.path[0]){++kit_asset_cursor_;return;}FileRevision expected,current;bool has_revision=kit_verifier_->revision(slot,expected);if(has_revision && (!fs_.revision(asset.path,current) || !same_file_revision(expected,current))){finish_kit_load(false,"Kit sample changed / previous kit kept");return;}
   if(library_.supports_incremental_load()){if(!kit_asset_loading_){if(!library_.begin_incremental_load(asset.path,asset.metadata.content_crc)){finish_kit_load(false,"Kit sample unavailable / previous kit kept");return;}kit_asset_loading_=true;return;}auto result=library_.poll_incremental_load(kit_prepared_[slot]);if(result==SampleLoadProgress::Working)return;kit_asset_loading_=false;if(result!=SampleLoadProgress::Ready){finish_kit_load(false,"Kit sample unavailable / previous kit kept");return;}}
   else if(!library_.load(asset.path,kit_prepared_[slot])){finish_kit_load(false,"Kit sample unavailable / previous kit kept");return;}
   if(has_revision && (!fs_.revision(asset.path,current) || !same_file_revision(expected,current))){finish_kit_load(false,"Kit sample changed / previous kit kept");return;}++kit_asset_cursor_;return;
  }
  if(!kit_final_check_){kit_probe_phase(4);kit_final_cursor_=0;kit_final_check_=true;}
  while(kit_final_cursor_<kSampleAssets && !kit_ready_->assets[kit_final_cursor_].path[0])++kit_final_cursor_;
  if(kit_final_cursor_<kSampleAssets){unsigned i=kit_final_cursor_++;FileRevision expected,current;if(kit_verifier_->revision(i,expected) && (!fs_.revision(kit_ready_->assets[i].path,current) || !same_file_revision(expected,current))){finish_kit_load(false,"Kit sample changed / previous kit kept");return;}return;}
  kit_probe_phase(7);
  if(kit_install_preset_){bool same=true;for(unsigned i=0;i<kSampleAssets;++i)same=same && kit_same_asset(project_.assets[i],kit_asset_snapshot_[i]);auto* plan=alloc_new<PresetAuditionPlan>();uint64_t mask=0;bool ok=same && plan && plan_preset_assets(*plan,*kit_ready_,kit_prepared_,mask) && commit_prepared_preset(*kit_ready_,*plan,kit_path_,kit_store_);delete plan;finish_kit_load(ok,ok?nullptr:content_error_[0]?content_error_:"Preset load cancelled / sound kept");finish_library_commit(ok);if(ok)ui_.notify(UiNotice::Saved);return;}

  kit_probe_phase(6);bool same=true;for(unsigned i=0;i<kSampleAssets;++i)same=same && kit_same_asset(project_.assets[i],kit_asset_snapshot_[i]);bool ok=same && publish_preset_audition(*kit_ready_,kit_prepared_,kit_path_);finish_kit_load(ok,"Preset preview unavailable / sound kept");
 }

 // Restore a diagnostic's in-RAM snapshot through the same definition ownership
 // boundary as a file load. Does not write a song or change its persistent locator.
 bool restore_probe_snapshot(ProjectState& snapshot,const WavView* prepared,const PreservedStore& original){PreservedOwner imported;DefinitionExtensions replacement;if(!imported || !imported->copy_from(original) || !adopt_definition_extensions(snapshot,*imported,&replacement) || !engine_.stage_project(snapshot,prepared))return false;for(unsigned i=0;i<kSampleAssets;++i)bindings_[i]=prepared[i];project_=snapshot;preserved_.steal_from(*imported);definitions_.steal_from(replacement);ui_.project_loaded();update_preset_status();return true;}
 bool load(const char* path,bool kit=false,bool remember=true){content_error_[0]=0;if(kit || data_content_kind(fs_,path)==2 || data_content_kind(fs_,path)==3)return load_preset(path);if(!supported_container(path))return false;ProjectState* candidate=nullptr;unsigned version=0;RecoverySource source;
 auto validate=[&](ProjectState& attempt,unsigned,const PreservedStore& store){return validate_loaded_candidate(attempt,path,true,&store);};
 PreservedOwner loaded_owner;if(!loaded_owner)return false;auto& loaded_store=*loaded_owner;WavView prepared[kSampleAssets]{};bool ok=load_recoverable_candidate_checked(fs_,path,candidate,&version,&source,validate,&loaded_store);
 if(ok)ok=library_.prepare(*candidate,prepared);
 if(!ok){ui_.browser().missing=true;copy_text(ui_.browser().status,sizeof ui_.browser().status,content_error_[0]?content_error_:"Project invalid or sample missing; previous session kept");}
 DefinitionExtensions replacement;
 if(ok)ok=adopt_definition_extensions(*candidate,loaded_store,&replacement);
 if(ok)ok=engine_.stage_project(*candidate,prepared);
 if(ok){
  for(unsigned i=0;i<kSampleAssets;++i)bindings_[i]=prepared[i];
  project_=*candidate;preserved_.steal_from(loaded_store);definitions_.steal_from(replacement);ui_.project_loaded();update_preset_status();if(!kit && remember)remember_project(path);
  ui_.notify_recovery(source);
 }else ui_.notify(UiNotice::Failed);delete candidate;return ok;}
 bool load_soundfont(const char* path){
  if(!allocation_admitted(sizeof(ProjectState),2))return ui_.memory_refused();
  auto target=record_target();WavView view;if(!library_.load(path,view))return false;
  return finish_soundfont(path,view,target);
 }
 bool finish_soundfont(const char* path,WavView view,const RecordingTarget& target){
  unsigned bank=0,program=0;const uint32_t crc=soundfont_crc(view.soundfont);
  if(!target_unchanged(target) || ui_.selected_track()!=target.track || project_.active_pattern!=target.pattern || !view.soundfont || !soundfont_first(view.soundfont,bank,program))return false;
  auto* p=alloc_new<ProjectState>();auto* plan=alloc_new<PresetAuditionPlan>();auto* prepared=alloc_array<WavView>(kSampleAssets);bool ok=p && plan && prepared;
  if(ok){make_empty_project(*p);p->content_kind=ContentKind::Preset;auto t=track_at(*p,0,0);t.instrument=Instrument::SoundFont;ok=allocate_user_identity(fs_,preset_library_,t.preset_identity);t.soundfont.values[0]=float(bank);t.soundfont.values[1]=float(program);copy_text(t.preset_name,96,filename(path));ok=copy_text(p->assets[0].path,96,path);p->assets[0].metadata.content_crc=crc;prepared[0]=view;uint64_t mask=0;ok=ok && plan_preset_assets(*plan,*p,prepared,mask);if(ok){mount_sound(t,plan->sound);ok=ui_.apply_preset_track(t,nullptr,plan->views,plan->assets);if(ok)for(unsigned i=0;i<kSampleAssets;++i)bindings_[i]=plan->views[i];}}
  delete[] prepared;delete plan;delete p;return ok;
 }
 bool audition_selection(const char* path,unsigned track,unsigned generation){
  // A queued cursor/filter change cancels an explicitly requested preview
  // before decoding or asset preparation can hold up that navigation.
  if(ui_.browser().library_list)switch(ui_.pending_file_action()){
   case FileAction::LibraryMove:case FileAction::LibraryKind:case FileAction::LibraryOriginals:case FileAction::LibraryEngine:case FileAction::LibraryInspect:case FileAction::LibraryRestore:case FileAction::LibraryOpen:case FileAction::LibraryOpenKits:return false;
   default:break;
  }
  char selected[192];return (ui_.mode()==UiMode::Browser || ui_.mode()==UiMode::Library) && ui_.selected_track()==track && engine_.staged_generation()==generation && selected_path(selected) && !__builtin_strcmp(path,selected);}
 bool validate_audition(ProjectState& p,const char* path){if(!admit_content_engines(p) || !project_paths(p,path,true,true))return false;const auto instrument=p.tracks[0].instrument;return (p.content_kind==ContentKind::Preset || (p.content_kind==ContentKind::Kit && instrument==Instrument::DrumKit)) && instrument!=Instrument::Empty && instrument!=Instrument::Unknown;}
 struct PresetAuditionPlan {SoundState sound{};WavView views[kSampleAssets]{};SampleAssetState assets[kSampleAssets]{};};
 bool plan_preset_assets(PresetAuditionPlan& plan,const ProjectState& preset,const WavView* prepared,uint64_t& mapped_mask){
  synchronize_bindings();snapshot_sound(plan.sound,track_at(preset,0,0));
  auto ready=[&](const SoundLeaf& leaf){uint64_t mask=leaf_asset_mask(leaf);auto mode=engine_asset_mode(leaf.instrument);for(unsigned slot=0;slot<kSampleAssets;++slot)if(mask&(uint64_t(1)<<slot)){if(!preset.assets[slot].path[0]){if(mode==EngineAssetMode::OptionalSample || mode==EngineAssetMode::WaveRing)continue;return false;}const auto& view=prepared[slot];if(preset.assets[slot].metadata.missing)return false;if(mode==EngineAssetMode::SoundFont){if(!view.soundfont || !soundfont_name(view.soundfont,unsigned(leaf.soundfont.values[0]),unsigned(leaf.soundfont.values[1])))return false;}else if(!view.frames || view.soundfont || (!view.data && !view.reader))return false;}return true;};
  if(plan.sound.instrument==Instrument::Layer){if(layer_status(plan.sound.layer)!=LayerReason::Ready)return false;for(unsigned i=0;i<plan.sound.layer.count;++i)if(!ready(plan.sound.layer.parts[i].sound))return false;}else if(!ready(plan.sound))return false;
  MaterialMapping mapping;if(!plan_material_mapping(project_,plan.sound,preset.assets,mapping))return false;mapped_mask=mapping.bound;
  for(unsigned i=0;i<kSampleAssets;++i){plan.views[i]=bindings_[i];plan.assets[i]=project_.assets[i];}
  for(unsigned source=0;source<kSampleAssets;++source)if(mapping.needed&(uint64_t(1)<<source)){unsigned target=mapping.slots[source];bool favorite=plan.assets[target].metadata.favorite;plan.views[target]=prepared[source];plan.assets[target]=preset.assets[source];plan.assets[target].metadata.favorite|=favorite;}
  remap_sound_assets(plan.sound,mapping.slots);return true;
 }
 bool publish_preset_audition(const ProjectState& preset,const WavView* prepared,const char* path){
  auto* plan=alloc_new<PresetAuditionPlan>();if(!plan)return false;uint64_t mask=0;bool ok=plan_preset_assets(*plan,preset,prepared,mask);
  if(ok){uint32_t ticket=engine_.next_preview_ticket();ok=engine_.preview_preset(ui_.selected_track(),plan->sound,plan->views,plan->assets,mask,ticket);if(ok){preview_ticket_=ticket;preset_audition_active_=true;preset_audition_track_=ui_.selected_track();preset_audition_generation_=engine_.staged_generation();auto& b=ui_.browser();b.previewing=true;b.preview_instrument=plan->sound.instrument;b.preview_kit_mask=0;for(unsigned pad=0;pad<16;++pad)for(auto slot:plan->sound.kit.pads[pad].sample)if(slot<kSampleAssets)b.preview_kit_mask|=uint16_t(1u<<pad);b.audition_assigned=false;copy_text(b.audition_path,sizeof b.audition_path,path);copy_text(b.status,sizeof b.status,"Preset on selected track / STOP restores");ui_.mark_dirty();}}
  delete plan;return ok;
 }
 bool audition_preset(const char* path){
  content_error_[0]=0;
  if(project_read_)return start_kit_load(path,false,true);
  auto* p=alloc_new<ProjectState>();if(!p)return false;auto validate=[&](ProjectState& candidate,unsigned){return validate_audition(candidate,path) && resolve_assets(candidate) && verify_project_assets(fs_,candidate);};auto* views=alloc_array<WavView>(kSampleAssets);bool ok=views && load_recoverable_checked(fs_,path,*p,nullptr,nullptr,validate) && library_.prepare(*p,views) && publish_preset_audition(*p,views,path);delete[] views;delete p;return ok;
 }
 void service_preset_audition(){
  if(!preset_audition_active_){return;}auto& b=ui_.browser();uint32_t state=engine_.preview_state();if((state>>1)==preview_ticket_ && !(state&1)){preset_audition_active_=false;b.previewing=false;b.audition_path[0]=0;ui_.mark_dirty();return;}
  if(!audition_selection(b.audition_path,preset_audition_track_,preset_audition_generation_) && engine_.submit({CommandType::StopPreview,0,0,0})){preset_audition_active_=false;b.previewing=false;b.audition_path[0]=0;ui_.mark_dirty();}
 }
 bool commit_prepared_preset(ProjectState& preset,PresetAuditionPlan& plan,const char* path,const PreservedStore& imported){
  auto* registration=alloc_new<PresetRegistration>();PreservedTrackEntry extension;bool inserted=false;plan.sound.definition_cookie=0;
  bool ok=registration && !imported.dropped_unknown && capture_preset_extensions(imported,extension) && definitions_.retain(extension,plan.sound.definition_cookie,&inserted) && plan_registration(*registration,plan.sound,plan.assets);
  if(ok){auto t=track_at(preset,0,0);mount_sound(t,plan.sound);t.preset_slot=registration->slot;ok=ui_.apply_preset_track(t,nullptr,plan.views,plan.assets,&plan.sound.fx,registration);if(ok){unsigned target=ui_.selected_track();for(unsigned i=0;i<kSampleAssets;++i)bindings_[i]=plan.views[i];copy_text(mounted_presets_[target],160,path);update_preset_status();if(ui_.mode()==UiMode::Browser && !ui_.browser().library_list && !ui_.browser_editing())ui_.set_mode(UiMode::Pad);}}
  if(!ok && inserted)definitions_.discard_unpublished(plan.sound.definition_cookie);
  extension.clear();delete registration;return ok;
 }
 bool load_preset(const char* path){
  content_error_[0]=0;
  if(!supported_container(path) || !allocation_admitted(sizeof(ProjectState),3))return false;
  auto* preset=alloc_new<ProjectState>();auto* plan=alloc_new<PresetAuditionPlan>();auto* views=alloc_array<WavView>(kSampleAssets);RecoverySource source;unsigned version=0;uint64_t mask=0;PreservedOwner imported;
  auto validate=[&](ProjectState& p,unsigned){return validate_audition(p,path) && resolve_assets(p) && verify_project_assets(fs_,p);};
  bool ok=preset && plan && views && imported && load_recoverable_checked(fs_,path,*preset,&version,&source,validate,imported.get()) && library_.prepare(*preset,views) && plan_preset_assets(*plan,*preset,views,mask) && commit_prepared_preset(*preset,*plan,path,*imported);
  if(ok)ui_.notify_recovery(source);else{copy_text(ui_.browser().status,96,content_error_[0]?content_error_:"Preset unavailable / sound kept");ui_.notify(UiNotice::Failed);}delete[] views;delete plan;delete preset;return ok;
 }

 bool save_preset(const char* requested,unsigned track=kTracks,bool include_fx=false,bool duplicate=false){
  if(!ui_.require_project_save_safe() || !synchronize_definition_extensions())return false;
  if(track==kTracks)track=ui_.selected_track();
  if(track>=kTracks)return false;
  const auto source=track_at(project_,project_.active_pattern,track);if(source.instrument==Instrument::Empty || source.instrument==Instrument::Unknown)return false;
  char path[160];if(factory_path(requested)){if(!join_path(path,sizeof path,source.instrument==Instrument::DrumKit?"KITS":"PRESETS",filename(requested)))return false;}else if(!copy_text(path,sizeof path,requested))return false;
  char parent[160];copy_text(parent,sizeof parent,path);parent_path(parent);if(parent[0] && !fs_.directory(parent))return false;
  if(!refresh_preset_library() || !preset_library_.complete())return false;
  auto* p=alloc_new<ProjectState>();auto* registration=alloc_new<PresetRegistration>();if(!p || !registration){delete p;delete registration;return false;}
  PreservedOwner saved_owner;if(!saved_owner){delete registration;delete p;return false;}auto& saved_unknown=*saved_owner;if(!saved_unknown.tracks[0].copy_definition_from(preserved_.tracks[track])){delete registration;delete p;return false;}
  make_empty_project(*p);p->content_kind=source.instrument==Instrument::DrumKit?ContentKind::Kit:ContentKind::Preset;auto target=track_at(*p,0,0);copy_engine_instance(target,source);target.preset_slot=kNoPreset;target.preset_fx={};if(include_fx){p->preset_fx.capture(source);target.preset_fx=p->preset_fx;p->preset_fx.apply(target);}
  bool ok=copy_text(target.preset_name,sizeof target.preset_name,filename(path));data_strip_mgx_suffix(target.preset_name);
  const uint64_t needed=sound_asset_mask(source),required=required_sound_asset_mask(source);for(unsigned slot=0;slot<kSampleAssets && ok;++slot)if(needed&(uint64_t(1)<<slot)){if(!(required&(uint64_t(1)<<slot)) && !project_.assets[slot].path[0])continue;p->assets[slot]=project_.assets[slot];ok=p->assets[slot].path[0] && asset_catalog_.resolve(fs_,p->assets[slot]);}
  char previous_user_path[160]{};PresetIdentity next=source.preset_identity;
  if(ok && (duplicate || !next.assigned()))ok=allocate_user_identity(fs_,preset_library_,next,duplicate?&source.preset_identity:nullptr);
  else if(ok){
   bool changed=true;int exact=preset_library_.exact(next);auto* old=alloc_new<ProjectState>();
   if(exact>=0 && !preset_library_[unsigned(exact)].factory && __builtin_strcmp(path,preset_library_[unsigned(exact)].path))copy_text(previous_user_path,160,preset_library_[unsigned(exact)].path);
   if(exact>=0 && old){PreservedOwner original;PreservedTrackEntry extension;if(original && read_project_file(fs_,preset_library_[unsigned(exact)].path,*old,nullptr,original.get()) && capture_preset_extensions(*original,extension)){auto* a=alloc_new<SoundState>();auto* b=alloc_new<SoundState>();if(a && b){snapshot_sound(*a,track_at(*old,0,0));snapshot_sound(*b,target);changed=!same_preset_content(*a,old->assets,*b,p->assets) || !same_preset_extensions(extension,saved_unknown.tracks[0]);}delete b;delete a;}extension.clear();}
   if(exact<0){auto* working=alloc_new<SoundState>();if(working){snapshot_sound(*working,target);for(const auto& base:project_.sounds)if(same_preset_revision(base.identity,next)){changed=!same_preset_content(base,project_.assets,*working,p->assets) || !same_extension_owners(base.definition_cookie,source.definition_cookie);break;}delete working;}}
   delete old;bool factory=next.origin==PresetOrigin::Factory;if(factory){next.parent=next.guid;next.parent_revision=next.revision;next.origin=PresetOrigin::User;}
   if(changed || factory){uint32_t highest=preset_library_.latest(next.guid);for(const auto& stored:project_.sounds)if(stored.identity.guid==next.guid && stored.identity.revision>highest)highest=stored.identity.revision;for(const auto& mounted:project_.tracks)if(mounted.preset_identity.guid==next.guid && mounted.preset_identity.revision>highest)highest=mounted.preset_identity.revision;if(highest<next.revision)highest=next.revision;if(highest==0xffffffffu)ok=false;else next.revision=highest+1;}
  }
  LibraryEntry existing;if(ok && fs_.exists(path)){ok=!duplicate && read_library_entry(fs_,path,existing) && existing.identity.guid==next.guid;}
  target.preset_identity=next;auto* sound=alloc_new<SoundState>();if(ok && sound){snapshot_sound(*sound,target);ok=plan_registration(*registration,*sound,p->assets,track);}else ok=false;
  if(ok)ok=project_paths(*p,path,false) && save_recoverable_checked(fs_,path,*p,[&](){return engine_.project_snapshot_safe();},&saved_unknown);
  if(ok && previous_user_path[0] && !fs_.remove(previous_user_path))copy_text(content_error_,96,"Saved / previous filename could not be removed");
  if(ok){bool published=ui_.register_saved_preset(track,target,*registration);if(published){copy_text(mounted_presets_[track],160,path);}else copy_text(content_error_,sizeof content_error_,"Saved / reload preset to attach revision");refresh_preset_library();update_preset_status();}

  if(!ok)ui_.require_project_save_safe();
  delete sound;delete registration;delete p;return ok;
 }

 bool save_named_preset(bool overwrite){
  auto& b=ui_.browser();const auto edit=b.edit;char path[160],name[104];
  if(!ui_.preset_save_target_current() || !browser_name_valid(edit.name)){copy_text(b.status,sizeof b.status,"Preset target changed / save again");ui_.notify(UiNotice::SaveFailed);return false;}
  if(overwrite){if(!copy_text(path,sizeof path,edit.source))return false;}
  else{const auto kind=track_at(project_,project_.active_pattern,ui_.preset_save_track()).instrument==Instrument::DrumKit?ContentKind::Kit:ContentKind::Preset;if(!data_content_name(name,sizeof name,edit.name,unsigned(kind)))return false;if(!join_path(path,sizeof path,edit.source,name))return false;}
  if(factory_path(path)){ui_.notify(UiNotice::SaveFailed);return false;}
  if(fs_.exists(path) && !overwrite){if(edit.duplicate){copy_text(b.status,96,"Duplicate needs a new filename");ui_.notify(UiNotice::SaveFailed);return false;}ui_.confirm_preset_overwrite(path);return true;}
  char parent[160];copy_text(parent,sizeof parent,path);parent_path(parent);
  bool ok=(!parent[0] || fs_.directory(parent)) && save_preset(path,ui_.preset_save_track(),edit.include_fx,edit.duplicate);
  if(ok){ui_.bake_saved(path);}
  copy_text(b.status,sizeof b.status,ok?path:"Preset save failed / sound kept");ui_.notify(ok?UiNotice::Saved:UiNotice::SaveFailed);return ok;
 }
 static bool collect_directory(char* output,unsigned capacity,const char* root,const char* source){
  char stem[96];if(!copy_text(stem,sizeof stem,filename(source)))return false;data_strip_mgx_suffix(stem);unsigned n=unsigned(__builtin_strlen(stem));
  if(n>30){ProjectCrc crc;for(unsigned i=0;i<n;++i)crc.add(uint8_t(stem[i]));n=30;while(n && (uint8_t(stem[n])&0xc0)==0x80)--n;stem[n++]='-';const char hex[]="0123456789abcdef";for(unsigned i=0;i<8;++i)stem[n++]=hex[(crc.value()>>(28-4*i))&15];stem[n]=0;}
  for(char* c=stem;*c;++c)if(*c>='a' && *c<='z')*c=char(*c-'a'+'A');
  return copy_text(stem+n,sizeof stem-n,"-EXPORT") && join_path(output,capacity,root,stem);
 }
 bool collect(const char* directory,const char* preset=nullptr){
  if(!synchronize_definition_extensions())return false;
  PreservedOwner exported_owner;if(!exported_owner)return false;auto& exported=*exported_owner;
  if(__builtin_strlen(directory)+sizeof("/ASSETS/SOUNDFONTS/00-00000000-00.sf2")>sizeof project_.assets[0].path || factory_path(directory) || !ui_.require_project_save_safe())return false;
  ui_.sync_ui_settings();if(!fs_.directory(directory))return false;
  char asset_root[192],samples[192],banks[192],content[192],project[192];
  if(!join_path(asset_root,sizeof asset_root,directory,"ASSETS") || !fs_.directory(asset_root) ||
     !join_path(samples,sizeof samples,asset_root,"SAMPLES") || !fs_.directory(samples) ||
     !join_path(banks,sizeof banks,asset_root,"SOUNDFONTS") || !fs_.directory(banks) ||
     !join_path(content,sizeof content,directory,preset?"PRESETS":"PROJECTS") || !fs_.directory(content) ||
     !join_path(project,sizeof project,content,preset?"preset.mgxp":"project.mgxs"))return false;
  auto* copy=alloc_new<ProjectState>(project_);bool ok=copy!=nullptr;
  if(ok && preset)ok=read_project_file(fs_,preset,*copy,nullptr,&exported) && (copy->content_kind==ContentKind::Preset || copy->content_kind==ContentKind::Kit) && project_paths(*copy,preset,true,true) && resolve_assets(*copy) && verify_project_assets(fs_,*copy);
  const uint64_t needed=ok?project_asset_mask(*copy):0;
  for(unsigned i=0;i<64 && ok;++i){
   auto& asset=copy->assets[i];if(!(needed&(uint64_t(1)<<i))){asset={};continue;}if(!asset.path[0])continue;
   uint32_t crc=0;ok=file_checksum(fs_,asset.path,crc);bool bank=soundfont_file_header(fs_,asset.path);
   char name[24],to[192];bool found=false;const char hex[]="0123456789abcdef";
   for(unsigned attempt=0;ok && attempt<100 && !found;++attempt){
    name[0]=char('0'+i/10);name[1]=char('0'+i%10);name[2]='-';for(unsigned n=0;n<8;++n)name[3+n]=hex[(crc>>(28-4*n))&15];name[11]='-';name[12]=char('0'+attempt/10);name[13]=char('0'+attempt%10);
    copy_text(name+14,sizeof name-14,bank?".sf2":".wav");
    ok=join_path(to,sizeof to,bank?banks:samples,name);if(!ok)break;
    if(fs_.exists(to)){if(same_file(fs_,asset.path,to))found=true;continue;}
    uint32_t copied=0;ok=copy_file(fs_,asset.path,to) && file_checksum(fs_,to,copied) && copied==crc;found=ok;
   }
   ok=ok && found;if(ok){asset.metadata.content_crc=crc;ok=join_path(asset.path,sizeof asset.path,bank?"../ASSETS/SOUNDFONTS":"../ASSETS/SAMPLES",name);}
  }
  if(ok)ok=save_recoverable_checked(fs_,project,*copy,[&](){return engine_.project_snapshot_safe();},preset?&exported:&preserved_);
  if(!ok){ui_.require_project_save_safe();}
  delete copy;return ok;
 }

 bool save(const char* path){if(!ensure_project_parent(path) || !ui_.require_project_save_safe())return false;ui_.sync_ui_settings();if(!ui_.prepare_preset_references() || !heal_save_assets() || !synchronize_definition_extensions())return false;auto* copy=alloc_new<ProjectState>(project_);bool ok=copy && project_paths(*copy,path,false) && save_recoverable_checked(fs_,path,*copy,[&](){return engine_.project_snapshot_safe();},&preserved_);if(!ok)ui_.require_project_save_safe();delete copy;if(ok)remember_project(path);return ok;}
 bool save_kit(const char* path){return track_at(project_,project_.active_pattern,ui_.selected_track()).instrument==Instrument::DrumKit && save_preset(path,ui_.selected_track(),track_at(project_,project_.active_pattern,ui_.selected_track()).preset_fx.present);}

 bool mutate_browser_file(FileAction action){
  auto& b=ui_.browser();const auto& edit=b.edit;
  auto fail=[&](const char* message){copy_text(b.status,sizeof b.status,message);ui_.notify(UiNotice::Failed);ui_.mark_dirty();return false;};
  if(!fs_.browser_file_management_allowed())return fail("File management requires DATA / boot files kept");
  if(ui_.playing())return fail("Stop transport to manage files");
  if(recording_busy() || stretch_active_ || freeze_active_ || capture_pending_ || b.analysis_pending)return fail("File operation busy / finish current job");
  char source[160],target[160],parent[160];if(!canonical_path(parent,sizeof parent,"",b.directory) || !canonical_path(source,sizeof source,"",edit.source))return fail("Invalid source path");
  bool creating=action==FileAction::NewFolder;
  if(!creating && browser_system_path(source))return fail("Protected instrument file");
  if(action!=FileAction::DeleteFile){if(!browser_name_valid(edit.name) || !join_path(target,sizeof target,parent,edit.name))return fail("Invalid or protected name");if(creating || edit.directory){char* name=target;for(char* c=target;*c;++c)if(*c=='/')name=c+1;for(char* c=name;*c;++c)if(*c>='a' && *c<='z')*c=char(*c-'a'+'A');}if(browser_system_path(target))return fail("Invalid or protected name");if(fs_.exists(target))return fail("Name already exists / choose another");}
  if(creating){auto* entries=alloc_array<FileEntry>(64);unsigned count=0;bool complete=entries && fs_.list_checked(parent,entries,64,count);delete[] entries;if(!complete)return fail("Folder access cannot be verified");if(!fs_.directory(target))return fail("Folder creation failed");refresh();copy_text(b.status,sizeof b.status,"Folder created");ui_.mark_dirty();return true;}
  if(!fs_.exists(source))return fail("Source changed / reopen file menu");
  if(edit.directory){FileEntry entry;unsigned count=0;if(!fs_.list_checked(source,&entry,1,count) || count)return fail("Only empty folders can be changed");}
  else {if((b.previewing && browser_path_equal(b.audition_path,source)) || (b.analysis_pending && browser_path_equal(b.preview_path,source)))return fail("File is playing or being analyzed");bool unsupported_delete=action==FileAction::DeleteFile && browser_project_kind(source) && !browser_path_equal(source,ui_.project_path());uint8_t h[6];unsigned n=0;unsupported_delete=unsupported_delete && fs_.read(source,0,h,6,n) && n==6 && !__builtin_memcmp(h,"MGXC",4) && wav_u16(h+4)!=kFormatVersion;auto references=unsupported_delete?FileReferences::Clear:BrowserFileGuard::check(fs_,library_,project_,source);if(references==FileReferences::Used)return fail("File is referenced or cached / kept");if(references==FileReferences::Unknown)return fail("Cannot prove references / file kept");}
  bool ok=action==FileAction::DeleteFile?fs_.remove(source):fs_.move(source,target);
  if(!ok)return fail("File operation failed / source kept");
  library_index_initialized_=false;refresh();copy_text(b.status,sizeof b.status,action==FileAction::DeleteFile?"Deleted":"Renamed");ui_.mark_dirty();return true;
 }
 // Catalog uses a 64-row viewport over the bounded 2048-entry metadata index.
 unsigned library_cursor_=0;bool library_commit_pending_=false,library_index_initialized_=false;
 unsigned visible_library_count()const{const auto& b=ui_.browser();unsigned n=0;for(unsigned i=0;i<preset_library_.size();++i)n+=preset_library_.visible(i,b.library_instrument,b.library_kits,b.library_originals);return n;}
 int visible_library_row(unsigned nth)const{const auto& b=ui_.browser();for(unsigned i=0;i<preset_library_.size();++i)if(preset_library_.visible(i,b.library_instrument,b.library_kits,b.library_originals)){if(!nth--)return int(i);}return -1;}
 void refresh_library_rows(){auto& b=ui_.browser();b.library_total=visible_library_count();if(library_cursor_>=b.library_total)library_cursor_=b.library_total?b.library_total-1:0;b.library_offset=library_cursor_/64*64;b.count=0;
  for(unsigned i=b.library_offset;i<b.library_total && b.count<64;++i){int index=visible_library_row(i);if(index<0)break;const auto& row=preset_library_[unsigned(index)];unsigned at=b.count++;b.entries[at]=FileEntry{};copy_text(b.entries[at].name,96,row.name);b.entries[at].content_kind=uint8_t(row.kind);copy_text(b.library_paths[at],160,row.path);b.library_factory[at]=row.factory;b.library_revision[at]=row.identity.revision;b.library_engine[at]=row.instrument;}
  b.cursor=library_cursor_-b.library_offset;copy_text(b.status,96,!preset_library_.complete()?"Shelf index limit / use FILES":b.count?"TURN: SELECT / PREVIEW: LISTEN / LOAD":"No presets for this engine / KIND selects kits");ui_.mark_dirty();
 }
 void finish_library_commit(bool ok){if(!library_commit_pending_)return;library_commit_pending_=false;if(ok){auto& b=ui_.browser();b.library_list=false;b.previewing=false;ui_.set_mode(UiMode(b.library_return));}}
 bool browse_library(FileAction action){auto& b=ui_.browser();bool performance=action==FileAction::LibraryPrev || action==FileAction::LibraryNext || ((action==FileAction::KitPrev || action==FileAction::KitNext) && ui_.mode()!=UiMode::Library);
  if(performance){b.library_instrument=track_at(project_,project_.active_pattern,ui_.selected_track()).instrument;b.library_kits=action==FileAction::KitPrev || action==FileAction::KitNext || b.library_instrument==Instrument::DrumKit;b.library_originals=false;}
  if(action==FileAction::LibraryOpen || action==FileAction::LibraryOpenKits || action==FileAction::LibraryInspect || performance){if(((action==FileAction::LibraryOpen || action==FileAction::LibraryOpenKits) || !library_index_initialized_) && !refresh_preset_library())return false;library_cursor_=0;const auto id=project_.tracks[ui_.selected_track()].preset_identity;unsigned n=0;for(unsigned i=0;i<preset_library_.size();++i)if(preset_library_.visible(i,b.library_instrument,b.library_kits,b.library_originals)){if(id.assigned() && id.guid==preset_library_[i].identity.guid)library_cursor_=n;++n;}}
  if(action==FileAction::LibraryOpenKits){b.library_kits=true;b.library_list=true;b.library_return=uint8_t(UiMode::Pad);ui_.set_mode(UiMode::Library);}
  if(action==FileAction::LibraryRestore){if(!library_index_initialized_ && !refresh_preset_library())return false;library_cursor_=b.library_offset+b.cursor;}
  if(action==FileAction::LibraryEngine){b.library_instrument=Instrument(unsigned(b.library_delta));b.library_delta=0;library_cursor_=0;}
  if(action==FileAction::LibraryKind){b.library_kits=!b.library_kits;library_cursor_=0;}
  if(action==FileAction::LibraryOriginals){b.library_originals=!b.library_originals;library_cursor_=0;}
  unsigned total=visible_library_count();int delta=action==FileAction::LibraryMove?b.library_delta:(action==FileAction::LibraryNext || action==FileAction::KitNext)?1:(action==FileAction::LibraryPrev || action==FileAction::KitPrev)?-1:0;b.library_delta=0;
  if(total && delta)library_cursor_=unsigned((int(library_cursor_)+delta%int(total)+int(total))%int(total));
  refresh_library_rows();
  // Browsing only moves the metadata cursor. Audition is exclusively the
  // explicit PREVIEW action; performance previous/next deliberately installs.
  if(!b.count || !performance)return true;
  const char* path=b.library_paths[b.cursor];return project_read_?start_kit_load(path):load_preset(path);
 }
 bool save_current_preset(){unsigned track=ui_.selected_track();const auto& t=project_.tracks[track];if(!t.preset_identity.assigned())return ui_.begin_preset_save();if(!refresh_preset_library())return false;char path[160]{};int exact=preset_library_.exact(t.preset_identity);if(mounted_presets_[track][0] && !factory_path(mounted_presets_[track]) && exact>=0 && !__builtin_strcmp(mounted_presets_[track],preset_library_[unsigned(exact)].path))copy_text(path,160,mounted_presets_[track]);else if(exact>=0 && !preset_library_[unsigned(exact)].factory)copy_text(path,160,preset_library_[unsigned(exact)].path);else{char name[112];if(!data_content_name(name,sizeof name,t.preset_name,unsigned(t.instrument==Instrument::DrumKit?ContentKind::Kit:ContentKind::Preset)) || !join_path(path,160,t.instrument==Instrument::DrumKit?"KITS":"PRESETS",name))return false;}bool ok=save_preset(path,track,t.preset_fx.present);copy_text(ui_.browser().status,96,ok?path:content_error_[0]?content_error_:"Save failed / original kept");ui_.notify(ok?UiNotice::Saved:UiNotice::SaveFailed);return ok;}
 bool refresh_new_project(bool initial=false){
  auto& b=ui_.browser();unsigned cursor=initial?0:b.folder_offset+b.cursor;if(b.folder_delta){int next=int(cursor)+b.folder_delta;cursor=unsigned(next<0?0:next);}b.folder_delta=0;
  BrowserFolder folder;bool exists=fs_.exists("FACTORY/DEMOS");bool ok=!exists || folder.read(fs_,"FACTORY/DEMOS",[&](const FileEntry& entry){char path[192];uint8_t header[6];unsigned got=0;return !entry.directory && data_mgx_file(entry.name) && join_path(path,sizeof path,"FACTORY/DEMOS",entry.name) && data_content_kind(fs_,path)==1 && fs_.read(path,0,header,6,got) && got==6 && wav_u16(header+4)==kFormatVersion;});
  if(!ok)return ui_.composition_notice("TEMPLATE SCAN FAILED / PROJECT KEPT",true);
  if(initial && (b.previewing || (engine_.preview_state()&1)) && !engine_.submit({CommandType::StopPreview,0,0,0}))return ui_.composition_notice("PREVIEW STOP BUSY / PROJECT KEPT",true);
  if(initial && !folder.count){b.new_project_list=false;ui_.request_empty_project();return true;}
  b.new_project_list=true;b.library_list=false;b.favorites_only=false;b.previewing=false;b.preview_path[0]=b.audition_path[0]=0;b.preview={};b.folder_total=folder.count+1;if(cursor>=b.folder_total)cursor=b.folder_total-1;b.folder_offset=cursor/64*64;b.cursor=cursor-b.folder_offset;b.count=0;
  copy_text(b.directory,sizeof b.directory,"FACTORY/DEMOS");for(unsigned i=b.folder_offset;i<b.folder_total && b.count<64;++i){auto& row=b.entries[b.count++];if(!i){row={};copy_text(row.name,sizeof row.name,"EMPTY");}else row=folder.entry(i-1);}
  copy_text(b.status,sizeof b.status,"NEW PROJECT / EMPTY OR TEMPLATE / SOURCE KEPT");ui_.set_mode(UiMode::Browser);ui_.mark_dirty();return true;
 }
 bool open_new_project(){auto& b=ui_.browser();if(b.cursor>=b.count)return false;if(b.folder_offset+b.cursor==0){b.new_project_list=false;ui_.request_empty_project();return true;}char path[192];if(!join_path(path,sizeof path,"FACTORY/DEMOS",b.entries[b.cursor].name))return false;if(data_content_kind(fs_,path)!=1 || !load(path,false,false))return false;ui_.project_path("PROJECTS/Untitled.mgxs");ui_.new_project_name();ui_.set_mode(UiMode::Project);return ui_.composition_notice("TEMPLATE READY / UNTITLED / SAVE AS");}
 void refresh(){auto& b=ui_.browser();if(b.new_project_list){refresh_new_project();return;}if(b.library_list){refresh_library_rows();return;}
  bool reset=__builtin_strcmp(folder_directory_,b.directory) || folder_favorites_!=b.favorites_only;
  unsigned cursor=reset?0:b.folder_offset+b.cursor;
  copy_text(folder_directory_,sizeof folder_directory_,b.directory);folder_favorites_=b.favorites_only;
  if(!reset && b.folder_delta){int next=int(cursor)+b.folder_delta;cursor=unsigned(next<0?0:next);}b.folder_delta=0;
  BrowserFolder folder;bool ok=folder.read(fs_,b.directory,[&](const FileEntry& entry){
   if(!data_browser_entry(b.directory,entry.name,entry.directory))return false;
   if(!b.favorites_only || entry.directory)return true;
   char path[160];if(join_path(path,sizeof path,b.directory,entry.name))for(const auto& asset:project_.assets)if(asset.metadata.favorite && !__builtin_strcmp(asset.path,path))return true;return false;
  });
  b.folder_total=ok?folder.count:0;if(cursor>=b.folder_total)cursor=b.folder_total?b.folder_total-1:0;
  b.folder_offset=cursor/64*64;b.cursor=cursor-b.folder_offset;b.count=ok?folder.page(cursor,b.entries,64):0;
  for(unsigned i=0;i<b.count;++i){b.entries[i].content_kind=0;char path[192];if(!b.entries[i].directory && browser_project_kind(b.entries[i].name) && join_path(path,sizeof path,b.directory,b.entries[i].name)){b.entries[i].content_kind=uint8_t(data_content_kind(fs_,path));uint8_t h[6];unsigned n=0;if(fs_.read(path,0,h,6,n) && n==6 && !__builtin_memcmp(h,"MGXC",4)){b.entries[i].format_version=wav_u16(h+4);b.entries[i].unsupported=b.entries[i].format_version!=kFormatVersion;}}}
  copy_text(b.status,sizeof b.status,!ok?(folder.memory_failure?"Folder memory unavailable / retry":"Folder scan failed / retry"):b.count?"Select / WAV preview: Cue / Preset: track":"Empty folder");ui_.mark_dirty();
 }
 bool transfer_busy()const{return recording_controls_busy() || analysis_active_ || resident_wait_ || ui_.browser().analysis_pending || ui_.file_action_pending();}
 bool recording_busy()const{return twin_job_ || browser_load_active_ || live_active_ || (midi_capture_ && midi_capture_->busy()) || midi_import_.busy() || project_save_busy() || kit_load_busy();}
 MidiCapture& midi_capture(){return *midi_capture_;}
 bool midi_capture_toggle(bool internal=false){if(!midi_capture_)return false;bool ok=midi_capture_->busy()?midi_capture_->stop():midi_capture_->start(capture_directory_,internal);ui_.midi_capture_state(midi_capture_->busy()?(midi_capture_->internal()?2:1):0);if(ok)ui_.midi_capture_path(midi_capture_->path());else ui_.notify(UiNotice::Failed);return ok;}
 bool finish_browser_file(FileAction action,const char* path,WavView view,bool replay,FileRevision before,bool before_valid){auto& b=ui_.browser();bool ok=true;FileRevision after{};
 if(!replay){browser_preview_revision_valid_=before_valid && fs_.revision(path,after) && same_file_revision(before,after) && copy_text(browser_preview_revision_path_,sizeof browser_preview_revision_path_,path);if(browser_preview_revision_valid_)browser_preview_revision_=after;b.missing=false;b.preview=view;copy_text(b.preview_path,sizeof b.preview_path,path);b.metadata=SampleMetadata{};bool known=false;for(const auto& asset:project_.assets)if(asset.path[0] && !__builtin_strcmp(asset.path,path)){b.metadata=asset.metadata;known=true;break;}if(!known)sample_file_root(fs_,path,b.metadata.root);b.metadata.rate=view.rate;b.metadata.frames=view.frames;b.metadata.bits=view.bits;b.metadata.channels=uint8_t(view.channels);copy_text(b.selected_name,sizeof b.selected_name,filename(path));cancel_analysis();b.analysis_source=nullptr;if(view.data){if(!resident_worker_ || !begin_analysis(path,view,false)){analyze_sample(view,b.analysis);b.analysis_source=view.data;}}else if(!begin_analysis(path,view,false)){b.analysis=SampleAnalysis{};}}
 // Replay skips the rescan above, but asset metadata edits (including
 // undo/redo) must still reach the browser on reopen: re-merge the known
 // asset metadata and restamp the format fields from the cached preview.
 if(replay){for(const auto& asset:project_.assets)if(asset.path[0] && !__builtin_strcmp(asset.path,path)){b.metadata=asset.metadata;break;}b.metadata.rate=b.preview.rate;b.metadata.frames=b.preview.frames;b.metadata.bits=b.preview.bits;b.metadata.channels=uint8_t(b.preview.channels);}
 if(action==FileAction::Preview || action==FileAction::Open){Command c{CommandType::PreviewSample,0,0,0};c.sample=view;c.frames=engine_.next_preview_ticket();ok=engine_.submit(c);if(ok){preview_ticket_=c.frames;b.previewing=true;b.preview_instrument=Instrument::Sampler;b.preview_kit_mask=0;b.audition_assigned=false;copy_text(b.audition_path,sizeof b.audition_path,path);copy_text(b.status,sizeof b.status,"Preview / Cue only");}else copy_text(b.status,sizeof b.status,"Preview unavailable / engine busy");}
 if(action==FileAction::Load){unsigned slot=kSampleAssets;bool compatible=select_load_slot(path,view,slot);ok=compatible && bind(slot,path,view);copy_text(b.status,sizeof b.status,ok?"Loaded to selected track":compatible?"Load failed / engine or storage":"Source changed or asset bank full");}
 if(action==FileAction::Favorite){ok=ui_.set_sample_favorite(path,!ui_.sample_favorite(path));if(ok){favorite_revision_=ui_.favorite_revision();if(b.favorites_only)refresh();}else copy_text(b.status,sizeof b.status,"Load first or wait for engine");return ok;}return ok;
 }
 bool service(){synchronize_bindings();service_twin();if(!library_index_initialized_ && refresh_preset_library())update_preset_status();service_device();service_preset_audition();RecordingUiSync recording_sync{*this};sync_recording_controls();service_record_cancel();engine_.service_freeze_capture();bool was_live=live_active_;service_live();library_.service();service_analysis();service_stretch();service_freeze();service_sampler_analysis();if(!ui_.file_action_pending() && favorite_revision_!=ui_.favorite_revision()){favorite_revision_=ui_.favorite_revision();if(ui_.browser().favorites_only)refresh();}auto& preview_ui=ui_.browser();uint32_t preview_state=engine_.preview_state();if(preview_ui.previewing && preview_ticket_ && (preview_state>>1)==preview_ticket_ && !(preview_state&1)){preview_ui.previewing=false;preview_ui.audition_path[0]=0;ui_.mark_dirty();}if(auto cancel=ui_.take_midi_import_cancel())midi_import_.cancel(cancel);
 midi_import_.service([&](const MidiImport& imported){return ui_.publish_midi_import(imported);});
 MidiImportOutcome imported;if(midi_import_.take_result(imported)){bool owned=ui_.midi_import_finished(imported.ticket);if(owned || !ui_.modal_active()){copy_text(ui_.browser().status,sizeof ui_.browser().status,imported.message);ui_.notify(imported.stage==MidiImportStage::Applied?UiNotice::MidiImported:imported.stage==MidiImportStage::Failed?UiNotice::Failed:UiNotice::None);ui_.mark_dirty();}}
 auto previous_midi=midi_capture_?midi_capture_->state():MidiCapture::State::Idle;if(midi_capture_)midi_capture_->service();auto current_midi=midi_capture_?midi_capture_->state():MidiCapture::State::Idle;ui_.midi_capture_state(midi_capture_ && midi_capture_->busy()?(midi_capture_->internal()?2:1):0);if(previous_midi!=current_midi && current_midi==MidiCapture::State::Failed){ui_.midi_capture_path("");ui_.notify(UiNotice::Failed);}if(previous_midi==MidiCapture::State::Closing && current_midi==MidiCapture::State::Idle)ui_.notify(UiNotice::MidiSaved);service_project_save();service_kit_load();if(capture_pending_ && engine_.capture_ready()){capture_pending_=false;unsigned max=engine_.sample_rate()*4;auto* audio=alloc_array<float>(max*2);unsigned frames=audio?engine_.copy_capture(audio,max):0;char path[96],name[16]="Take-000.wav";bool ok=frames && fs_.directory(capture_directory_),free=false;for(unsigned i=1;ok && i<1000;++i){name[5]=char('0'+i/100);name[6]=char('0'+(i/10)%10);name[7]=char('0'+i%10);if(!join_path(path,sizeof path,capture_directory_,name)){ok=false;break;}if(!fs_.exists(path)){free=true;break;}}ok=ok && free;if(ok){FileWriter writer(fs_,path);ok=write_wav(writer,audio,frames,engine_.sample_rate()) && writer.finish();}delete[] audio;bool saved=ok,assigned=false;if(saved && target_unchanged(capture_target_)){unsigned slot=free_asset_slot(project_);WavView view;assigned=slot<64 && library_.load(path,view) && bind(slot,path,view,capture_target_.track,capture_target_.pattern,capture_target_.kit_pad);}copy_text(ui_.browser().status,sizeof ui_.browser().status,assigned?"Captured / assigned to requested track":saved?"Capture saved to WAV / not assigned":"Capture failed / check storage");ui_.notify(saved?UiNotice::Saved:UiNotice::Failed);}
 if(project_save_busy() || kit_load_busy())return true;
 if(browser_load_active_){if(browser_load_soundfont_ && (!target_unchanged(browser_soundfont_target_) || ui_.selected_track()!=browser_soundfont_target_.track || project_.active_pattern!=browser_soundfont_target_.pattern)){library_.cancel_incremental_load();browser_load_active_=browser_load_soundfont_=false;copy_text(ui_.browser().status,sizeof ui_.browser().status,"SF2 load cancelled / target changed");ui_.mark_dirty();return false;}WavView view;auto progress=library_.poll_incremental_load(view);if(progress==SampleLoadProgress::Working)return true;browser_load_active_=false;if(progress==SampleLoadProgress::Failed){browser_preview_revision_valid_=false;auto& b=ui_.browser();b.missing=true;copy_text(b.status,sizeof b.status,"Missing or unsupported audio file");ui_.mark_dirty();return false;}if(browser_load_soundfont_){browser_load_soundfont_=false;bool ok=finish_soundfont(browser_load_path_,view,browser_soundfont_target_);copy_text(ui_.browser().status,sizeof ui_.browser().status,ok?"SF2 assigned / SYNTH: BANK and PROGRAM":"SF2 not assigned / target changed");ui_.notify(ok?UiNotice::Saved:UiNotice::Failed);ui_.mark_dirty();return ok;}return finish_browser_file(browser_load_action_,browser_load_path_,view,false,browser_load_before_,browser_load_before_valid_);}
 auto action=ui_.take_file_action();if(action==FileAction::None)return true;auto& b=ui_.browser();char path[160];bool ok=true;
 if(action==FileAction::NewProjectMenu)return refresh_new_project(true);
 if(b.new_project_list && action==FileAction::Open)return open_new_project();
 if(action==FileAction::LibraryInspect || action==FileAction::LibraryRestore || action==FileAction::LibraryEngine || action==FileAction::LibraryOpen || action==FileAction::LibraryMove || action==FileAction::LibraryKind || action==FileAction::LibraryOriginals || action==FileAction::LibraryPrev || action==FileAction::LibraryNext || action==FileAction::LibraryOpenKits || action==FileAction::KitPrev || action==FileAction::KitNext)return browse_library(action);
 if(action==FileAction::SaveCurrentPreset)return save_current_preset();
 if(b.library_list && action==FileAction::Up){b.library_list=false;copy_text(b.directory,sizeof b.directory,"");b.cursor=0;refresh();return true;}
 if(b.library_list && (action==FileAction::Open || action==FileAction::Preview || action==FileAction::Load)){if(!selected_path(path))return false;if(action!=FileAction::Load)return audition_preset(path);library_commit_pending_=true;bool loaded=project_read_?start_kit_load(path):load_preset(path);if(!project_read_ || !loaded)finish_library_commit(loaded);return loaded;}
 if(action==FileAction::AudioRecord || action==FileAction::AudioOverdub)return begin_audio_record(action==FileAction::AudioOverdub);
 if(action==FileAction::SaveProject)return save_named_project();
 if(action==FileAction::RenameFile || action==FileAction::DeleteFile || action==FileAction::NewFolder)return mutate_browser_file(action);
 if(action==FileAction::BeginStretch)return begin_stretch();
 if(action==FileAction::CancelStretch){cancel_stretch();return true;}
 if(action==FileAction::BeginTwin)return begin_twin();
 if(action==FileAction::CancelTwin){if(twin_job_)twin_job_->cancel();return true;}
 if(action==FileAction::BeginFreeze){begin_freeze();return true;}
 if(action==FileAction::CancelFreeze){cancel_freeze();return true;}
 if(action==FileAction::MidiRecord || action==FileAction::MidiRecordInternal)return midi_capture_toggle(action==FileAction::MidiRecordInternal);
 if(action==FileAction::Resample){bool started=begin_resample();if(!started)ui_.notify(UiNotice::Failed);return started;}
 if(action==FileAction::CancelResample)return cancel_resample();
 if(action==FileAction::RecordToggle || action==FileAction::SampleRecordToggle){if(was_live && !live_active_)return true;if(live_active_){if(preparing_ || mixing_){end_overdub(false);return true;}live_stop_pending_=true;copy_text(b.status,sizeof b.status,"Stopping recording / flushing audio");return true;}bool started=begin_requested_record();if(!started)ui_.notify(UiNotice::Failed);return started;}
 if(action==FileAction::AuditionSelectedSample){
  const auto t=track_at(project_,project_.active_pattern,ui_.selected_track());bool kit=t.instrument==Instrument::DrumKit;unsigned pad=kit?ui_.selected_kit_pad():255;unsigned slot=kit?t.kit.pads[pad].sample[0]:t.instrument==Instrument::Sampler?t.sample_slot:kSampleAssets;
  if(slot>=kSampleAssets){copy_text(b.status,sizeof b.status,"No assigned sample to audition");ui_.notify(UiNotice::Failed);return false;}
  const char* selected=project_.assets[slot].path;WavView view=bindings_[slot];if(!selected[0] || (!view.data && !view.reader)){copy_text(b.status,sizeof b.status,"No assigned sample to audition");ui_.notify(UiNotice::Failed);return false;}
  if(ui_.sample_auditioning()){if(!engine_.submit({CommandType::StopPreview,0,0,0}))return false;b.previewing=false;b.audition_assigned=false;b.audition_path[0]=0;copy_text(b.status,sizeof b.status,"Sample audition stopped");ui_.mark_dirty();return true;}
  SampleEdit edit=kit?t.kit.pads[pad].edit:t.sample_edit;unsigned slice=t.sample_slice;if(edit.zone_count)slice=select_sample_zone(edit,kit?project_.assets[slot].metadata.root:t.root_note,100);float origin=edit.start,length=edit.end-edit.start;
  if(edit.slice_count){slice%=edit.slice_count;edit.start=origin+length*edit.markers[slice];edit.end=origin+length*edit.markers[slice+1];}else if(!kit && t.sample_slices>1){slice%=t.sample_slices;edit.start=origin+length*slice/t.sample_slices;edit.end=origin+length*(slice+1)/t.sample_slices;}
  if(!kit && t.sample_loop)edit.mode=SampleMode::Loop;
  if(!valid_sample_edit(edit)){copy_text(b.status,sizeof b.status,"Audition region unavailable");ui_.notify(UiNotice::Failed);return false;}
  Command c{CommandType::PreviewSample,0,0,uint16_t(kit?t.kit.pads[pad].reverse:t.sample_reverse)};c.parameter=1;c.sample=view;c.sample_edit=edit;c.frames=engine_.next_preview_ticket();if(!engine_.submit(c))return false;preview_ticket_=c.frames;b.previewing=true;b.audition_assigned=true;b.audition_track=uint8_t(ui_.selected_track());b.audition_pattern=uint8_t(project_.active_pattern);b.audition_pad=uint8_t(pad);copy_text(b.audition_path,sizeof b.audition_path,selected);copy_text(b.status,sizeof b.status,"Edited sample / Cue audition");ui_.mark_dirty();return true;
 }
 if(action==FileAction::Overdub){bool started=begin_overdub();if(!started)ui_.notify(UiNotice::Failed);return started;}
 if(action==FileAction::Capture){const auto request=ui_.take_request();if(!target_unchanged(request.target) || request.source!=project_.routing.capture_bus){copy_text(b.status,sizeof b.status,"Capture cancelled / target or source changed");ui_.notify(UiNotice::Failed);return false;}if(capture_pending_ || !engine_.request_capture())return false;capture_target_=request.target;capture_pending_=true;return true;}
 if(action==FileAction::ToggleFavorites){b.favorites_only=!b.favorites_only;refresh();return true;}
 if(action==FileAction::FolderMove){refresh();return true;}if(action==FileAction::Refresh){refresh();return true;}if(action==FileAction::Up){parent_path(b.directory);b.cursor=0;refresh();return true;}if(action==FileAction::Factory)return browse_library(FileAction::LibraryOpenKits);if(action==FileAction::Collect){const char* baked=ui_.bake_collect_path();if(baked){char destination[160];ok=collect_directory(destination,sizeof destination,"PRESETS",baked) && collect(destination,baked);copy_text(b.status,sizeof b.status,ok?destination:"Collect failed / preset kept");ui_.bake_collected(ok);ui_.notify(ok?UiNotice::Saved:UiNotice::SaveFailed);return ok;}char destination[160];if(!fs_.directory("PROJECTS"))return false;ok=collect_directory(destination,sizeof destination,"PROJECTS",ui_.project_path()) && collect(destination);copy_text(b.status,sizeof b.status,ok?destination:"Collect failed / check paths");ui_.notify(ok?UiNotice::Saved:UiNotice::SaveFailed);return ok;}if(action==FileAction::SavePreset || action==FileAction::OverwritePreset)return save_named_preset(action==FileAction::OverwritePreset);
 if(action==FileAction::StopPreview){if(!engine_.submit({CommandType::StopPreview,0,0,0}))return false;b.previewing=false;b.audition_path[0]=0;ui_.mark_dirty();return true;}
 if(action==FileAction::ImportWavLoop){const auto t=track_at(project_,project_.active_pattern,ui_.selected_track());const auto& asset=project_.assets[t.sample_slot];WavView file,loaded=library_.lookup(asset.path);uint64_t offset=0;uint32_t first=0,last=0;if(t.instrument!=Instrument::Sampler || !asset.path[0] || (!loaded.data && !loaded.reader) || t.sample_reverse || t.sample_edit.slice_count || t.sample_slices!=1 || !sample_file_header(fs_,asset.path,file,offset) || file.frames!=loaded.frames || file.rate!=loaded.rate || file.channels!=loaded.channels || file.bits!=loaded.bits || file.format!=loaded.format || !sample_file_loop(fs_,asset.path,file.frames,first,last)){copy_text(b.status,sizeof b.status,"No forward loop / clear slices and reverse");ui_.notify(UiNotice::LoopUnsupported);return false;}float begin=0,end=0;if(!sample_frame_fraction(first,file.frames,begin) || !sample_frame_fraction(last,file.frames,end) || begin>=end){copy_text(b.status,sizeof b.status,"Loop boundaries not representable");ui_.notify(UiNotice::LoopPrecision);return false;}auto edit=t.sample_edit;edit.start=begin;edit.end=end;edit.mode=SampleMode::Loop;bool applied=ui_.sample_edit(edit);copy_text(b.status,sizeof b.status,applied?"File loop applied / Undo available":"File loop unavailable / region too small");ui_.notify(applied?UiNotice::LoopImported:UiNotice::Failed);return applied;}
 if(action==FileAction::AutoSlice){
  const auto t=track_at(project_,project_.active_pattern,ui_.selected_track());
  const unsigned pad=t.instrument==Instrument::DrumKit?ui_.selected_kit_pad():255;
  const unsigned slot=pad<16?t.kit.pads[pad].sample[0]:t.sample_slot;
  if(slot>=kSampleAssets)return false;
  const auto& edit=pad<16?t.kit.pads[pad].edit:t.sample_edit;
  WavView view=library_.lookup(project_.assets[slot].path);
  if(view.data){if(resident_worker_ && begin_analysis(project_.assets[slot].path,view,true)){copy_text(b.status,sizeof b.status,"Analyzing sample / auto chop pending");ui_.mark_dirty();return true;}cancel_analysis();analyze_sample(view,b.analysis,edit.start,edit.end);b.analysis_source=view.data;auto next=edit;apply_transients(next,b.analysis);return ui_.sample_edit(next);}
  if(view.reader && begin_analysis(project_.assets[slot].path,view,true)){copy_text(b.status,sizeof b.status,"Analyzing sample / auto chop pending");ui_.mark_dirty();return true;}
  return false;
 }
 if(action==FileAction::MapPads){
  const auto selected=track_at(project_,project_.active_pattern,ui_.selected_track());
  if(selected.instrument==Instrument::DrumKit){bool mapped=ui_.map_kit_slices();copy_text(b.status,sizeof b.status,mapped?"Slice pads assigned / Undo available":"Slice mapping unavailable / previous kit kept");return mapped;}
  if(selected.instrument!=Instrument::Sampler || selected.sample_slot>=kSampleAssets || !bindings_[selected.sample_slot].frames){copy_text(b.status,sizeof b.status,"Load a Sampler sound before MAP PADS");return false;}
  if(!engine_.command_space()){return false;}
  if(!allocation_admitted(sizeof(PatternState),3)){return ui_.memory_refused();}
  auto* pattern=alloc_new<PatternState>(project_,project_.active_pattern);if(!pattern){return ui_.memory_refused();}pattern->include_settings=true;const auto& source=pattern->tracks[ui_.selected_track()];for(unsigned i=0;i<16;++i){pattern->tracks[i]=source;unsigned slices=source.sample_edit.slice_count?source.sample_edit.slice_count:source.sample_slices;pattern->tracks[i].sample_slices=uint8_t(slices);pattern->tracks[i].sample_slice=uint8_t(source.sample_edit.pads[i]%slices);}ok=ui_.replace_pattern_contents(*pattern);delete pattern;return ok;
 }
 if(action==FileAction::MidiImport){
  MidiImportRequest request;if(!ui_.take_midi_import_request(request))return false;
  uint32_t ticket=midi_import_.accept(request);
  if(!ticket){ui_.midi_import_finished(0);copy_text(b.status,sizeof b.status,"MIDI import busy / finish current request");return false;}
  ui_.midi_import_started(ticket);copy_text(b.status,sizeof b.status,"Importing MIDI events / LEFT: CANCEL");ui_.mark_dirty();return true;
 }

 if(!selected_path(path))return false;

 if(action==FileAction::Open && is_midi_file(path)){copy_text(b.selected_name,sizeof b.selected_name,filename(path));b.preview=WavView{};b.preview_path[0]=0;copy_text(b.status,sizeof b.status,"Shift+IMPORT, then PUSH / selected MIDI channel");ui_.mark_dirty();return true;}
 if(action==FileAction::Open && b.entries[b.cursor].directory){copy_text(b.directory,sizeof b.directory,path);b.cursor=0;refresh();return true;}
 if((action==FileAction::Open || action==FileAction::Load) && is_soundfont_file(path)){if(library_.supports_incremental_load()){browser_soundfont_target_=record_target();if(!library_.begin_incremental_load(path,0)){copy_text(b.status,sizeof b.status,library_.failure_reason());ui_.notify(UiNotice::Failed);return false;}browser_load_active_=browser_load_soundfont_=true;copy_text(browser_load_path_,sizeof browser_load_path_,path);copy_text(b.status,sizeof b.status,"Loading SF2 / previous sound kept");ui_.mark_dirty();return true;}ok=load_soundfont(path);copy_text(b.status,sizeof b.status,ok?"SF2 assigned / SYNTH: BANK and PROGRAM":library_.failure_reason());ui_.notify(ok?UiNotice::Saved:UiNotice::Failed);return ok;}
 if(action==FileAction::Open && is_dx7_file(path)){if(!ui_.require_project_save_safe())return false;char folder[160];ok=import_dx7_bank(fs_,path,folder,sizeof folder);if(ok){copy_text(b.directory,sizeof b.directory,folder);b.cursor=0;refresh();}copy_text(b.status,sizeof b.status,ok?"Imported 32 presets / pool unchanged":"Invalid DX7 bank or import failed");ui_.notify(ok?UiNotice::Saved:UiNotice::Failed);return ok;}
 if(action==FileAction::Duplicate && is_recording_part(path)){ok=recover_live_part(path,b.directory);if(ok)refresh();copy_text(b.status,sizeof b.status,ok?"Recovered WAV / select to preview":"Recovery failed / original kept");ui_.notify(ok?UiNotice::Saved:UiNotice::Failed);return ok;}
 if(action==FileAction::Duplicate){char to[160],name[112];bool available=false;for(unsigned attempt=1;attempt<100;++attempt){name[0]=char('0'+attempt/10);name[1]=char('0'+attempt%10);name[2]='-';if(!copy_text(name+3,sizeof name-3,filename(path)) || !join_path(to,sizeof to,b.directory,name))break;if(!fs_.exists(to)){available=true;break;}}ok=available && copy_file(fs_,path,to);copy_text(b.status,sizeof b.status,ok?"Duplicate created":"Duplicate failed");if(ok){refresh();}return ok;}
 unsigned project_kind=data_content_kind(fs_,path);if(action==FileAction::Preview && (project_kind==2 || project_kind==3)){bool result=audition_preset(path);if(!result){copy_text(b.status,sizeof b.status,content_error_[0]?content_error_:"Preset preview failed / sound kept");ui_.notify(UiNotice::Failed);}return result;}if(browser_project_kind(path) && !project_kind){copy_text(b.status,sizeof b.status,"Unsupported MGX format");return false;}if((action==FileAction::Open || action==FileAction::Load) && project_kind)return project_kind==3?(project_read_?start_kit_load(path,false,false,true):load_preset(path)):project_kind==2 && project_read_?start_kit_load(path):load(path,project_kind==2);
 // Reopening the same unchanged WAV must not synchronously rescan its full
 // contents or cancel a waveform job already in progress. Cache ownership stays
 // with the sample library; a changed/unverifiable revision takes the slow path.
 FileRevision before{};bool before_valid=fs_.revision(path,before);WavView existing=library_.lookup(path);
 bool replay=(action==FileAction::Open || action==FileAction::Preview) && browser_preview_revision_valid_ && before_valid && same_file_revision(before,browser_preview_revision_) && !__builtin_strcmp(path,browser_preview_revision_path_) && !__builtin_strcmp(path,b.preview_path) && (b.preview.data || b.preview.reader) && existing.data==b.preview.data && existing.reader==b.preview.reader;
 WavView view=replay?b.preview:WavView{};if(!replay && library_.supports_incremental_load()){uint32_t crc=0;for(const auto& asset:project_.assets)if(asset.path[0] && !__builtin_strcmp(asset.path,path)){crc=asset.metadata.content_crc;break;}if(!library_.begin_incremental_load(path,crc)){browser_preview_revision_valid_=false;b.missing=true;copy_text(b.status,sizeof b.status,"Missing or unsupported audio file");return false;}browser_load_active_=true;browser_load_soundfont_=false;browser_load_action_=action;copy_text(browser_load_path_,sizeof browser_load_path_,path);browser_load_before_=before;browser_load_before_valid_=before_valid;copy_text(b.status,sizeof b.status,"Loading sample / please wait");ui_.mark_dirty();return true;}if(!replay && !library_.load(path,view)){browser_preview_revision_valid_=false;b.missing=true;copy_text(b.status,sizeof b.status,"Missing or unsupported audio file");return false;}return finish_browser_file(action,path,view,replay,before,before_valid);
 }
};
}
