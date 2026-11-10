#pragma once
#include "session_files.h"
#include "future_factory_presets.h"
namespace groove {
// Main-loop only. Exercise real storage + owner-queued mounts, without audio
// workload qualification. GROOVE/PresetCheck.mgxp is a reserved disposable
// probe file; a prior interrupted attempt may be removed. Never save the live song.
// Restore the whole in-memory project, asset bindings and unknown definitions: a
// v12 save/mount deliberately registers pool entries, including during this probe.
class PresetFunctionalCheck {
 char representative_[160]{};bool representative_mounted_=false;
 SessionFiles& files_;IFileAccess& fs_;Engine& engine_;ProjectState& project_;Performance& ui_;
 ProjectState* original_=nullptr;PreservedStore* original_store_=nullptr;WavView* original_bindings_=nullptr;TrackState* expected_=nullptr;unsigned stage_=0,which_=0,selected_=0,passed_=0,checkpoint_=0;bool failed_=false,ready_=false;UiMode mode_=UiMode::Pad;
 static constexpr const char* path_="GROOVE/PresetCheck.mgxp";
 static constexpr const char* checkpoint_path_="GROOVE/PresetCheck.chk";
 void release(){delete original_;delete original_store_;delete[] original_bindings_;delete expected_;original_=nullptr;original_store_=nullptr;original_bindings_=nullptr;expected_=nullptr;}
 void finish(){fs_.remove(checkpoint_path_);if(fs_.exists(path_))fs_.remove(path_);release();stage_=0;ready_=true;}
public:
 static constexpr unsigned kCases=12;
 PresetFunctionalCheck(SessionFiles& f,IFileAccess& fs,Engine& e,ProjectState& p,Performance& ui):files_(f),fs_(fs),engine_(e),project_(p),ui_(ui){}
 ~PresetFunctionalCheck(){release();}
 unsigned progress()const{return stage_?which_*16+stage_:0;}
 bool busy()const{return stage_!=0;}
 bool representative_mounted()const{return representative_mounted_;}
 bool start(const char* representative=nullptr){if(busy() || files_.recording_busy() || files_.project_save_busy() || files_.kit_load_busy() || ui_.file_action_pending() || ui_.browser_editing() || !engine_.project_snapshot_safe())return false;if(fs_.exists(path_) && !fs_.remove(path_))return false;representative_[0]=0;representative_mounted_=false;if(representative && !copy_text(representative_,sizeof representative_,representative))return false;selected_=ui_.selected_track();mode_=ui_.mode();original_=alloc_new<ProjectState>(project_);original_store_=alloc_new<PreservedStore>();original_bindings_=alloc_array<WavView>(kSampleAssets);expected_=alloc_new<TrackState>();if(!original_ || !original_store_ || !original_bindings_ || !expected_ || !files_.copy_preserved_to(*original_store_)){release();return false;}files_.copy_bindings(original_bindings_);if(!ui_.stop()){release();return false;}which_=passed_=0;failed_=ready_=false;stage_=1;checkpoint_=0;return true;}
 __attribute__((noinline,optimize("Os"))) void service(){if(!stage_ || !engine_.project_snapshot_safe())return;if(ui_.selected_track()!=selected_){failed_=true;ui_.select_track(selected_);stage_=8;}
  bool ok=true;
  if(checkpoint_!=progress()){checkpoint_=progress();uint32_t marker=0x57430000u|checkpoint_;if(!fs_.write(checkpoint_path_,0,&marker,sizeof marker,true)){failed_=true;stage_=8;return;}}
  switch(stage_){
   case 1:{if(which_==0 && representative_[0]){ok=files_.load_preset(representative_);stage_=2;break;}if(which_>=8){if(which_==10)ok=files_.load_preset("FACTORY/DEBUG/Test-Tone.mgxp") && ui_.set_instrument(Instrument::Granular);else{auto* candidate=alloc_new<ProjectState>();ok=candidate!=nullptr;if(ok){if(which_==11){ok=make_future_factory_preset(*candidate,4);candidate->tracks[0].insert_fx[0]=20;candidate->tracks[0].insert_fx[1]=19.f/24;candidate->tracks[0].insert_fx[4]=21;candidate->tracks[0].insert_fx[5]=.5f;candidate->tracks[0].delay_send=.3f;candidate->tracks[0].reverb_send=.4f;}else{make_empty_project(*candidate);candidate->tracks[0].instrument=Instrument::Formant;candidate->tracks[0].formant.values[0]=float(which_-8);}PresetFxSnapshot fx;if(which_==11)fx.capture(track_at(*candidate,0,0));ok=ok && ui_.apply_preset_track(track_at(*candidate,0,0),nullptr,nullptr,nullptr,&fx);}delete candidate;}stage_=2;break;}auto* candidate=alloc_new<ProjectState>();ok=candidate!=nullptr;if(ok){if(which_<2 || which_==3 || which_>=5)ok=make_future_factory_preset(*candidate,which_==5?11:which_==6?13:which_==7?15:which_==3?8:which_?4:0);else if(which_==4){ok=files_.load_preset("FACTORY/DEBUG/SF2-Sine.mgxp");}else{make_empty_project(*candidate);auto t=track_at(*candidate,0,0);/* Use a known WAV: the live song may have no assets or only SF2. */ok=files_.load_sample_path("FACTORY/DEBUG/440Hz.wav");unsigned slot=project_.tracks[selected_].sample_slot;ok=ok && slot<kSampleAssets;if(ok){t.instrument=Instrument::Sampler;t.sample_slot=uint8_t(slot);t.sample_edit.slice_count=t.sample_edit.zone_count=1;t.sample_edit.markers[0]=0;t.sample_edit.markers[1]=1;t.sample_edit.zones[0].key_low=0;t.sample_edit.zones[0].key_high=127;t.sample_edit.zones[0].velocity_low=1;t.sample_edit.zones[0].velocity_high=127;t.sample_edit.zones[0].root=60;}}if(ok && which_!=4)ok=ui_.apply_preset_track(track_at(*candidate,0,0));}delete candidate;stage_=2;break;}
   case 2:if(ui_.visual().attached_instrument[selected_]!=unsigned(project_.tracks[selected_].instrument))return;if(which_==0 && representative_[0]){LibraryEntry entry;representative_mounted_=read_library_entry(fs_,representative_,entry) && same_preset_revision(project_.tracks[selected_].preset_identity,entry.identity) && project_.tracks[selected_].instrument==entry.instrument;if(!representative_mounted_){failed_=true;stage_=8;break;}}ok=files_.save_preset(path_,selected_,which_==11);if(ok){*expected_=track_at(project_,project_.active_pattern,selected_);copy_text(expected_->preset_name,96,filename(path_));data_strip_mgx_suffix(expected_->preset_name);}stage_=3;break;
   case 3:ok=ui_.set_instrument(Instrument::Empty);if(which_==11)ok=ui_.set_parameter(94,0) && ui_.set_parameter(98,0) && ui_.set_parameter(5,0) && ui_.set_parameter(10,0) && ok;stage_=4;break;
   case 4:if(ui_.visual().attached_instrument[selected_]!=unsigned(Instrument::Empty))return;ok=files_.load_preset(path_);stage_=5;break;
   case 5:{if(ui_.visual().attached_instrument[selected_]!=unsigned(expected_->instrument))return;const auto t=track_at(project_,project_.active_pattern,selected_);ok=t.instrument==expected_->instrument;const auto* d=engine_descriptor(unsigned(t.instrument));if(ok && d)for(unsigned i=0;i<d->parameters.count;++i)ok=ok && engine_parameter_value(t,i)==engine_parameter_value(ConstTrackView(*expected_),i);if(ok && which_==2)ok=t.sample_edit.zone_count==1 && t.sample_edit.zones[0].root==60 && project_.assets[t.sample_slot].metadata.content_crc!=0;if(ok && which_==4)ok=project_.assets[t.sample_slot].metadata.content_crc!=0;if(ok && which_==7){ChunkBuffer expected,actual;ok=write_track_engn(expected,ConstTrackView(*expected_),nullptr) && write_track_engn(actual,t,nullptr) && expected.size==actual.size && !__builtin_memcmp(expected.data,actual.data,actual.size);}if(ok && which_==11){for(unsigned i=0;i<8;++i)ok=ok && t.insert_fx[i]==expected_->insert_fx[i];ok=ok && t.delay_send==expected_->delay_send && t.reverb_send==expected_->reverb_send;}if(ok)++passed_;stage_=6;break;}
   case 6:ok=fs_.remove(path_);stage_=7;break;
   case 7:if(++which_<kCases)stage_=1;else stage_=8;break;
   case 8:{ok=engine_.stage_project(*original_,original_bindings_);if(ok){project_=*original_;files_.set_bindings(original_bindings_);ui_.project_loaded();ok=files_.sync_preserved_from(*original_store_) && ui_.select_track(selected_);if(ok)stage_=9;}break;}
   case 9:if(ui_.visual().attached_instrument[selected_]!=unsigned(original_->tracks[selected_].instrument))return;ui_.set_mode(mode_);finish();return;
  }
  if(!ok){failed_=true;if(stage_!=8)stage_=8;}
 }
 bool result(unsigned& passed,bool& ok){if(!ready_)return false;ready_=false;passed=passed_;ok=!failed_ && passed_==kCases && !fs_.exists(path_) && !fs_.exists(checkpoint_path_) && !fs_.exists("GROOVE/PresetCheck.mgxp.tmp");return true;}
};
}
