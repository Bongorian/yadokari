#pragma once
#include "engine.h"
#include "project_files.h"
#include "twin_digest.h"
#include "wav_writer.h"
namespace groove {
// File-owner job. Capture uses the existing bounded freeze slab and dry tap;
// publication is one acknowledged audio-owner transaction, never partial mount.
class TwinPrepare {
 IFileAccess& fs_;ISampleLibrary& library_;Engine& engine_;ProjectState& project_;
 TwinInstall* draft_=nullptr;bool published_=false,moved_=false,owns_capture_=false,owns_temporary_=false;FileWriter* writer_=nullptr;
 TwinConfig config_{};unsigned note_=0,written_=0,first_=0,end_=0,total_=0;uint32_t offsets_[17]{};bool writing_=false,revision_set_=false;
 float write_buffer_[2048]{};char temporary_[160]{},path_[160]{};const char* error_=nullptr;bool done_=false,ok_=false;unsigned asset_=64;WavView result_view_{};
 void fail(const char* error){error_=error;if(published_){__atomic_store_n(&draft_->cancel,1u,__ATOMIC_RELEASE);return;}delete writer_;writer_=nullptr;if(owns_capture_){engine_.freeze_capture_abort();engine_.freeze_capture_release();owns_capture_=false;}if(owns_temporary_)fs_.remove(temporary_);if(moved_)fs_.remove(path_);delete draft_;draft_=nullptr;done_=true;ok_=false;}
 bool unchanged(){auto* sound=alloc_new<SoundState>();if(!sound)return false;snapshot_sound(*sound,track_at(project_,project_.active_pattern,draft_->track));uint8_t digest[32];bool same=twin_content_digest(*sound,project_,draft_->track,digest) && !__builtin_memcmp(digest,draft_->twin.digest,32);delete sound;return same;}
 bool capture(){if(!unchanged())return false;owns_capture_=engine_.twin_capture_begin(draft_->track,config_.note(note_),config_.velocity,config_.hold_ms,config_.max_ms);return owns_capture_;}
public:
 TwinPrepare(IFileAccess& fs,ISampleLibrary& library,Engine& engine,ProjectState& project):fs_(fs),library_(library),engine_(engine),project_(project){}
 ~TwinPrepare(){delete writer_;if(draft_){if(published_)__atomic_store_n(&draft_->cancel,1u,__ATOMIC_RELEASE);else delete draft_;}if(!ok_){if(owns_temporary_)fs_.remove(temporary_);if(moved_ && !published_)fs_.remove(path_);}if(owns_capture_)engine_.freeze_capture_release();}
 bool busy()const{return draft_!=nullptr;}bool done()const{return done_;}bool ok()const{return ok_;}const char* error()const{return error_?error_:"TWIN PREPARE FAILED";}
 unsigned asset_slot()const{return asset_;}const char* path()const{return path_;}unsigned note_index()const{return note_;}unsigned note_count()const{return config_.count();}
 unsigned progress()const{return !busy()?100:unsigned((note_*100+engine_.freeze_capture_progress()*100)/config_.count());}
 #if defined(__GNUC__) && !defined(__clang__)
 __attribute__((optimize("Os"),noinline))
 #endif
 bool begin(unsigned track,const TwinConfig& config,unsigned asset,unsigned source_slot,unsigned sampled_slot,const PresetIdentity& source_id,const PresetIdentity& sampled_id,const char* folder){
  if(busy() || engine_.freeze_capture_state()!=0 || !config.valid() || track>=kTracks || asset>=kSampleAssets || source_slot>=kPresetSlots || sampled_slot>=kPresetSlots || source_slot==sampled_slot)return false;
  done_=ok_=moved_=published_=writing_=revision_set_=owns_temporary_=false;error_=nullptr;config_=config;note_=written_=first_=end_=total_=0;for(auto& n:offsets_)n=0;
  draft_=alloc_new<TwinInstall>();if(!draft_)return false;draft_->track=track;draft_->asset=asset;asset_=asset;draft_->generation=engine_.staged_generation();draft_->twin.source=uint16_t(source_slot);draft_->twin.sampled=uint16_t(sampled_slot);
  snapshot_sound(draft_->source,track_at(project_,project_.active_pattern,track));if(!twin_content_digest(draft_->source,project_,track,draft_->twin.digest)){fail("TWIN DIGEST FAILED");return false;}draft_->source.identity=source_id;draft_->sampled.identity=sampled_id;
  bool available=false;for(unsigned i=1;i<10000;++i){char name[32]="Twin-0000.wav";for(unsigned d=0,n=i;d<4;++d,n/=10)name[8-d]=char('0'+n%10);if(!join_path(path_,sizeof path_,folder,name))break;unsigned n=0;while(path_[n])++n;if(n+6>=sizeof temporary_)break;copy_text(temporary_,sizeof temporary_,path_);copy_text(temporary_+n,sizeof temporary_-n,".part");if(!fs_.exists(path_) && !fs_.exists(temporary_)){available=true;break;}}
  if(!available || !copy_text(draft_->material.path,96,path_)){fail("TWIN MATERIAL PATH UNAVAILABLE");return false;}
  if(!fs_.directory(folder) || !(writer_=alloc_new<FileWriter>(fs_,temporary_))){fail("TWIN STORAGE OPEN FAILED");return false;}
  owns_temporary_=true;uint8_t header[44];if(!wav_header_float32(header,0,engine_.sample_rate()) || !writer_->write(header,44)){fail("TWIN STORAGE WRITE FAILED");return false;}
  if(!capture()){fail("TWIN NEEDS STOPPED OR SILENT TRACK / FREE MEMORY");return false;}return true;
 }
 void cancel(){if(busy())fail("TWIN CANCELLED / SOURCE KEPT");}
 #if defined(__GNUC__) && !defined(__clang__)
 __attribute__((optimize("Os"),noinline))
 #endif
 void service(){
  if(!busy())return;
  if(published_){auto* result=engine_.twin_install_result();if(!result)return;if(result!=draft_)return;
   if(result->status==1 && result->generation==engine_.staged_generation()){project_.sounds[result->twin.source]=result->source;project_.sounds[result->twin.sampled]=result->sampled;project_.assets[result->asset]=result->material;project_.twins[result->track]=result->twin;result_view_=result->view;ok_=true;}
   else{error_="TWIN NOT COMMITTED / SOURCE KEPT";if(moved_)fs_.remove(path_);}
   engine_.release_twin_install();draft_=nullptr;published_=false;done_=true;return;
  }
  if(engine_.staged_generation()!=draft_->generation){fail("TWIN CANCELLED / PROJECT CHANGED");return;}
  unsigned state=engine_.freeze_capture_state();if(state==1 || state==2 || state==5)return;if(state!=3){fail("TWIN CAPTURE REFUSED OR SILENT");return;}
  const float* pcm=engine_.twin_capture_data();unsigned frames=engine_.freeze_capture_frames();if(!pcm || !frames){fail("TWIN CAPTURE EMPTY");return;}
  if(!writing_){uint32_t revision=engine_.twin_capture_revision();if(revision_set_ && revision!=draft_->revision){fail("TWIN SOURCE CHANGED");return;}draft_->revision=revision;revision_set_=true;
   // Keep a short preroll and tail around the first/last audible sample.
   auto audible=[&](unsigned i){return fabsf(pcm[i*2])>0.000061f || fabsf(pcm[i*2+1])>0.000061f;};first_=0;while(first_<frames && !audible(first_))++first_;end_=frames;while(end_>first_ && !audible(end_-1))--end_;if(first_==end_){fail("TWIN NOTE IS SILENT");return;}first_=first_>16?first_-16:0;end_=end_+32<frames?end_+32:frames;written_=first_;writing_=true;
  }
  unsigned count=end_-written_;if(count>1024)count=1024;const float scale=127.f/config_.velocity;for(unsigned i=0;i<count*2;++i){float sample=pcm[written_*2+i]*scale;if(!isfinite(sample) || fabsf(sample)>1){fail("TWIN DRY LEVEL TOO HIGH / LOWER ENGINE LEVEL");return;}unsigned frame=written_+i/2;unsigned tail=engine_.sample_rate()/200;if(tail>(end_-first_)/2)tail=(end_-first_)/2;if(tail && frame>=end_-tail)sample*=float(end_-1-frame)/float(tail);write_buffer_[i]=sample;}
  if(count && !writer_->write(reinterpret_cast<const uint8_t*>(write_buffer_),count*8)){fail("TWIN STORAGE WRITE FAILED");return;}written_+=count;if(written_<end_)return;
  total_+=end_-first_;offsets_[++note_]=total_;engine_.freeze_capture_release();owns_capture_=false;writing_=false;
  if(note_<config_.count()){if(!capture())fail("TWIN SOURCE CHANGED / CAPTURE REFUSED");return;}
  uint8_t header[44];bool saved=writer_->finish();delete writer_;writer_=nullptr;if(!saved || !wav_header_float32(header,total_,engine_.sample_rate()) || !fs_.write(temporary_,0,header,44,false) || !fs_.move(temporary_,path_)){fail("TWIN WAV FINALIZE FAILED");return;}moved_=true;owns_temporary_=false;
  WavView view;uint32_t crc=0;if(!library_.load_twin(path_,view) || !view.data || view.reader || !file_checksum(fs_,path_,crc) || !unchanged()){fail("TWIN MATERIAL LOAD FAILED / SOURCE KEPT");return;}
  auto& sample=draft_->sampled;sample.instrument=Instrument::Sampler;sample.sample_slot=uint8_t(draft_->asset);sample.sample_slices=uint8_t(config_.count());sample.output_trim=draft_->source.output_trim;copy_text(sample.preset_name,96,"TWIN Sample");
  auto& edit=sample.sample_edit;edit.mode=SampleMode::Gate;edit.fade_ms=0;edit.attack_ms=0;edit.release_ms=12;edit.crossfade_ms=0;edit.slice_count=edit.zone_count=uint8_t(config_.count());
  for(unsigned i=0;i<=config_.count();++i)edit.markers[i]=float(offsets_[i])/float(total_);
  for(unsigned i=0;i<16;++i)edit.pads[i]=uint8_t(i%config_.count());
  for(unsigned i=0;i<config_.count();++i){auto& zone=edit.zones[i];zone.root=uint8_t(config_.note(i));zone.key_low=uint8_t(i?(config_.note(i-1)+config_.note(i))/2+1:0);zone.key_high=uint8_t(i+1<config_.count()?(config_.note(i)+config_.note(i+1))/2:127);}
  draft_->view=view;copy_text(draft_->material.path,96,path_);auto& m=draft_->material.metadata;m.rate=view.rate;m.frames=view.frames;m.bits=view.bits;m.channels=uint8_t(view.channels);m.content_crc=crc;m.root=config_.low;
  if(!valid_sound(sample) || !engine_.submit_twin_install(draft_)){fail("TWIN COMMIT BUSY / SOURCE KEPT");return;}published_=true;
 }
 WavView material_view()const{return result_view_;}
};
}
