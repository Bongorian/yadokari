#pragma once
#include "../src/session_files.h"
#include "../src/benchmark_project.h"
#include <circle/logger.h>
namespace groove {
// Non-destructive diagnostic draft. Keeps the original RAM project and bindings,
// writes only a new named test song, and restores the original stopped.
class KitRecordCheck {
 SessionFiles& files_;IFileAccess& fs_;ISampleLibrary& library_;Engine& engine_;ProjectState& project_;Performance& ui_;
 inline static constexpr const char* suffixes_[3]={".new",".tmp",".bak"};
 ProjectState* original_=nullptr;WavView bindings_[64]{};unsigned stage_=0,pattern_=0,steps_[2]{},velocity_[2]{16,16};uint64_t due_=0,started_=0;uint32_t generation_=0;bool readback_=false;char path_[96]{};UiMode mode_=UiMode::Pad;unsigned selected_=0;
 static void suffix_path(const char* path,const char* suffix,char* out,unsigned capacity){copy_text(out,capacity,path);unsigned n=0;while(out[n])++n;copy_text(out+n,capacity-n,suffix);}
 void next(unsigned s,uint64_t now,unsigned wait=0){stage_=s;due_=now+wait;}
 bool mount(ProjectState& p,const WavView* views){if(!engine_.stage_project(p,views))return false;project_=p;files_.set_bindings(views);ui_.project_loaded();generation_=engine_.staged_generation();return true;}
 bool watch(){return engine_.watch_kit_hits(0,pattern_,steps_[0],steps_[1],velocity_[0],velocity_[1]);}
 void report(const char* context){CLogger::Get()->Write("kitrec",LogNotice,"Kit replay %s expected-pattern %u active-pattern %u chain %u notes %u/%u velocity-match %u/%u started %u/%u audio-frames %u/%u",context,pattern_,unsigned(project_.clips[0].pattern),unsigned(project_.chain.enabled),engine_.kit_watch_notes(0),engine_.kit_watch_notes(1),engine_.kit_watch_velocity(0),engine_.kit_watch_velocity(1),engine_.kit_watch_hits(0),engine_.kit_watch_hits(1),engine_.kit_watch_frames(0),engine_.kit_watch_frames(1));}
 bool name(){for(unsigned i=1;i<1000;++i){copy_text(path_,sizeof path_,"GROOVE/KITREC-000.mgxs");path_[14]=char('0'+i/100);path_[15]=char('0'+i/10%10);path_[16]=char('0'+i%10);bool exists=fs_.exists(path_);for(const char* suffix:suffixes_){char other[112];suffix_path(path_,suffix,other,sizeof other);exists|=fs_.exists(other);}if(!exists)return true;}return false;}
public:
 KitRecordCheck(SessionFiles& files,IFileAccess& fs,ISampleLibrary& library,Engine& engine,ProjectState& p,Performance& ui):files_(files),fs_(fs),library_(library),engine_(engine),project_(p),ui_(ui){}
 ~KitRecordCheck(){delete original_;}
 bool busy()const{return stage_!=0;}
 bool start(uint64_t now){if(busy() || files_.recording_busy() || files_.kit_load_busy() || files_.project_save_busy() || !engine_.project_snapshot_safe() || !allocation_admitted(sizeof(ProjectState),3) || !name())return false;original_=alloc_new<ProjectState>(project_);auto* draft=alloc_new<ProjectState>();if(!original_ || !draft){delete original_;delete draft;original_=nullptr;return false;}files_.copy_bindings(bindings_);mode_=ui_.mode();selected_=ui_.selected_track();ui_.panic();make_benchmark_project(*draft);WavView views[64]{};bool ok=true;for(unsigned i=0;i<64 && ok;++i)if(draft->assets[i].path[0])ok=library_.load(draft->assets[i].path,views[i]);if(ok)ok=mount(*draft,views);delete draft;if(!ok){delete original_;original_=nullptr;return false;}readback_=false;started_=now;next(1,now);return true;}
 void service(uint64_t now){if(!stage_)return;if(now-started_>60000 && stage_<90){CLogger::Get()->Write("kitrec",LogWarning,"Kit check timeout");next(90,now);}if(now<due_)return;
  switch(stage_){
   case 1:if(ui_.visual().generation!=generation_)return;ui_.select_track(0);if(!files_.start_kit_load("FACTORY/KITS/Techno-01.mgxk",false)){next(90,now);break;}next(2,now);break;
   case 2:if(files_.kit_load_busy())return;if(project_.tracks[0].instrument!=Instrument::DrumKit){next(90,now);break;}ui_.set_shift(false);ui_.set_mode(UiMode::Pad);ui_.play();next(3,now,900);break;
   case 3:{pattern_=project_.clips[0].pattern;ui_.select_pattern(pattern_);const auto kit=track_at(project_,pattern_,0);for(unsigned pad=0;pad<2;++pad){bool used[128]{};for(const auto& s:kit.kit.steps[pad])if(s.enabled)used[s.velocity]=true;velocity_[pad]=0;for(unsigned v=16;v<128;++v)if(!used[v]){velocity_[pad]=v;break;}}bool ok=velocity_[0] && velocity_[1] && ui_.arm_note_record(true);for(unsigned pad=0;pad<2 && ok;++pad){unsigned pressure=((velocity_[pad]-1)*4095+125)/126;ok=ui_.pad(12+pad,0x10,uint16_t(pressure)) && ui_.pad(12+pad,0x20,0);}ok=ui_.arm_note_record(false) && ok;CLogger::Get()->Write("kitrec",LogNotice,"Kit record probe queued %u pattern %u chain %u",unsigned(ok),pattern_,unsigned(project_.chain.enabled));next(ok?4:90,now,2000);}break;
   case 4:{steps_[0]=steps_[1]=256;const auto kit=track_at(project_,pattern_,0);for(unsigned pad=0;pad<2;++pad)for(unsigned s=0;s<kSteps;++s)if(kit.kit.steps[pad][s].enabled && kit.kit.steps[pad][s].velocity==velocity_[pad]){steps_[pad]=s;break;}CLogger::Get()->Write("kitrec",LogNotice,"Kit stored lanes steps %u/%u velocity %u/%u active-pattern %u chain %u",steps_[0],steps_[1],velocity_[0],velocity_[1],unsigned(project_.clips[0].pattern),unsigned(project_.chain.enabled));if(steps_[0]>=256 || steps_[1]>=256 || !watch()){next(90,now);break;}next(5,now,5000);}break;
   case 5:report("original-chain");engine_.stop_kit_watch();ui_.stop();next(6,now,100);break;
   case 6:{auto* copy=alloc_new<ProjectState>(project_);bool ok=copy && project_paths(*copy,path_,false) && save_recoverable_checked(fs_,path_,*copy,[&](){return engine_.project_snapshot_safe();});if(ok)ok=read_project_file(fs_,path_,*copy);if(ok){const auto kit=track_at(*copy,pattern_,0);for(unsigned pad=0;pad<2;++pad)ok=ok && kit.kit.steps[pad][steps_[pad]].enabled && kit.kit.steps[pad][steps_[pad]].velocity==velocity_[pad];}delete copy;readback_=ok;CLogger::Get()->Write("kitrec",LogNotice,"Kit saved-step readback ok %u path %s",unsigned(ok),path_);if(!ok){next(90,now);break;}Command c{CommandType::SetChainEnabled,0,0,0};engine_.submit(c);project_.chain.enabled=false;ui_.launch_clip(0,pattern_,false);ui_.play();next(7,now,200);}break;
   case 7:if(!watch()){next(90,now);break;}next(8,now,5000);break;
   case 8:{report("fixed-clip");bool ok=readback_ && engine_.kit_watch_notes(0) && engine_.kit_watch_notes(1) && engine_.kit_watch_hits(0) && engine_.kit_watch_hits(1) && engine_.kit_watch_frames(0) && engine_.kit_watch_frames(1);CLogger::Get()->Write("kitrec",ok?LogNotice:LogWarning,"Kit record playback hits %u/%u readback %u audio %u/%u ok %u",engine_.kit_watch_hits(0),engine_.kit_watch_hits(1),unsigned(readback_),engine_.kit_watch_frames(0),engine_.kit_watch_frames(1),unsigned(ok));next(90,now);}break;
   case 90:if(files_.kit_load_busy())return;engine_.stop_kit_watch();ui_.stop();if(!mount(*original_,bindings_))return;delete original_;original_=nullptr;ui_.select_track(selected_);ui_.set_mode(mode_);next(91,now,200);break;
   case 91:if(ui_.visual().generation!=generation_)return;{bool ok=fs_.remove(path_);for(const char* suffix:suffixes_){char other[112];suffix_path(path_,suffix,other,sizeof other);ok=fs_.remove(other) && ok;}CLogger::Get()->Write("kitrec",LogNotice,"Kit check restored stopped cleaned %u",unsigned(ok));stage_=0;}break;
  }
 }
};
}
