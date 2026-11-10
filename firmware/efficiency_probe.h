#pragma once
#include "mk3_audio.h"
#include "cpu_memory.h"
#include "project_storage.h"
#include "update_runtime.h"
#include "../src/session_files.h"
#include "../src/update/sha256.h"
#include <circle/logger.h>
// Private, fixed-input audit. The task owns files and snapshots; DSP owns its
// counters. No arbitrary file command, DATA backup or audio-thread allocation.
class CEfficiencyProbe {
 enum Phase {Idle,Network,Prepare,Settle,Arm,Play,FilePlay,Stop,Dump,Drain,Restore,Restoring};
 Phase phase_=Idle;unsigned index_=0,started_=0,row_=0,row_start_=0,limit_=0,dump_=0,due_=0,restore_generation_=0;
 bool failed_=false,used_=false;unsigned restore_started_=0,progress_due_=0;groove::CoreWorkMeter& work_;groove::EfficiencyAudioMeter meter_;
 groove::CoreWorkMeter::Snapshot work_start_[4]{};bool save_started_=false,save_known_=false,save_ok_=false;unsigned save_at_=0;
 static constexpr const char* SavePath="GROOVE/EFF-R3-SAVE.mgxs";
 groove::ProjectState* original_=nullptr;groove::PreservedOwner original_store_;
 groove::WavView original_views_[groove::kSampleAssets]{};
 groove::UiMode original_mode_=groove::UiMode::Pad;CMk3Audio* audio_=nullptr;
 static unsigned Millis(){return unsigned(CTimer::GetClockTicks64()/1000);}
 static uint32_t Clock(){uint64_t ticks;asm volatile("isb\n\tmrs %0, cntpct_el0":"=r"(ticks)::"memory");return uint32_t(ticks);}
 static unsigned Micros(){return CTimer::GetClockTicks();}
 static unsigned Frequency(){uint64_t f;asm volatile("mrs %0,cntfrq_el0":"=r"(f));return unsigned(f);}
 static bool Identity(CProjectStorage& storage,unsigned index,const char* path){
  const char* hashes[]={"58d18152a43c0aef3373846f267644eaf31a1e4cf7ebf29dea077c1f31f55ab8","a561c459f81a9b4822fa3a72374b903fe10b22fffd01eea30639638880da5421","779db17e4c33167231fc5825fb0c981cf726987a76d724a174f92389fff51c28","1985347683f6807341d39de35bde441dc5052bfc72356b3b1239e25276d41b4e","6b2bde7262b5176e4f953ada51c20498b493c98f6ffe25fa863cddc4c27af4a9","d5da2c2097ece1ce7f0ff614e0e982c0c992e6f5c6e7b8541259df77902f0452","3b7639fe9edb67857dd8dd50023cad8674d699f01eaa7138559ad496862949ec"};
  groove::update::Sha256 hash;uint8_t bytes[4096];uint64_t size=storage.size(path),at=0;
  if(!size || size>1024*1024)return false;
  while(at<size){unsigned wanted=unsigned(size-at>sizeof bytes?sizeof bytes:size-at),got=0;if(!storage.read(path,at,bytes,wanted,got) || got!=wanted)return false;hash.add(bytes,got);at+=got;}
  uint8_t digest[32];hash.finish(digest);const char hex[]="0123456789abcdef";
  for(unsigned i=0;i<32;++i)if(hex[digest[i]>>4]!=hashes[index][i*2] || hex[digest[i]&15]!=hashes[index][i*2+1])return false;
  return true;
 }
 void mark(unsigned phase){row_=index_*8+phase;row_start_=CTimer::GetClockTicks();for(unsigned c=0;c<4;++c)work_start_[c]=work_.snapshot(c);meter_.request(row_);}
 void report(unsigned previous,unsigned at,unsigned until,const groove::CoreWorkMeter::Snapshot* before,const groove::CoreWorkMeter::Snapshot* after){
  const auto& s=meter_.sealed(previous);unsigned duration=until-at;
  CLogger::Get()->Write("eff",LogNotice,"EFF row %u wall-us %u blocks %u total-us %lu max-us %u over500 %u missing %u pool32-blocks %u pool32-missing-lag1 %u main %u poly %u va %u sampler %u native %u refused %u",previous,duration,s.blocks,(unsigned long)s.us,s.max_us,s.over500,s.last_missing-s.first_missing,s.pool32_blocks,s.pool32_missing,s.main_peak,s.poly_peak,s.va_peak,s.sampler_peak,s.native_seen,s.refused_seen);
  const auto& a=s.assist;if(a.mismatch)failed_=true;CLogger::Get()->Write("eff",a.mismatch?LogError:LogNotice,"EFF assist row %u submitted %lu used %lu late %lu busy %lu checked %lu mismatch %lu cloned %lu reused %lu",previous,(unsigned long)a.submitted,(unsigned long)a.used,(unsigned long)a.late,(unsigned long)a.busy,(unsigned long)a.checked,(unsigned long)a.mismatch,(unsigned long)a.cloned,(unsigned long)a.reused);
  CLogger::Get()->Write("eff",LogNotice,"EFF pool row %u owned %u va-main %u va-poly %u released %u triggers %lu steal-release %lu steal-held %lu refused %lu",previous,s.owned_peak,s.va_main_peak,s.va_poly_peak,s.released_peak,(unsigned long)s.steals.triggered,(unsigned long)s.steals.released,(unsigned long)s.steals.held,(unsigned long)s.steals.refused);
  const auto& e=s.event_work.us;CLogger::Get()->Write("eff",LogNotice,"EFF event row %u validate %u mount %u publish %u clip-reset %u step %u trigger %u",previous,e[0],e[1],e[2],e[3],e[4],e[5]);
  const auto& peak=s.peak;CLogger::Get()->Write("eff",LogNotice,"EFF peak row %u block %u render-us %u control %u events %u tracks %u output %u commands %u top-kind %u top-track %u top-us %u visual %u",previous,s.peak_block,peak.total,peak.control,peak.events,peak.tracks,peak.output,peak.command_count,peak.top_kind,peak.top_track,peak.top_us,peak.visual);
  CLogger::Get()->Write("eff",LogNotice,"EFF reset row %u performance %u group %u delay %u reverb %u track %u snapshot %u",previous,peak.reset[0],peak.reset[1],peak.reset[2],peak.reset[3],peak.reset[4],peak.reset[5]);
  for(unsigned c=0;c<4;++c){auto end=after[c];CLogger::Get()->Write("eff",LogNotice,"EFF core row %u core %u busy-us %lu calls %u lifetime-max-job %u",previous,c,(unsigned long)(end.busy-before[c].busy),end.calls-before[c].calls,end.max_us);}
 }
 // Seal previous row after the next phase has been observed by the DSP.
 unsigned pending_row_=groove::EfficiencyAudioMeter::off,pending_at_=0,pending_end_=0;groove::CoreWorkMeter::Snapshot pending_work_[4]{},pending_work_end_[4]{};
 void change(unsigned phase){pending_row_=row_;pending_at_=row_start_;pending_end_=CTimer::GetClockTicks();for(unsigned c=0;c<4;++c){pending_work_[c]=work_start_[c];pending_work_end_[c]=work_.snapshot(c);}mark(phase);}
 void drain_row(){if(pending_row_!=groove::EfficiencyAudioMeter::off && meter_.acknowledged()==row_){report(pending_row_,pending_at_,pending_end_,pending_work_,pending_work_end_);pending_row_=groove::EfficiencyAudioMeter::off;}}
public:
 explicit CEfficiencyProbe(groove::CoreWorkMeter& work):work_(work){}
 bool busy()const{return phase_!=Idle;}
 bool save_result(bool saved,bool probe){if(!probe || !save_started_ || save_known_ || !busy())return false;save_known_=true;save_ok_=saved;return true;}
 bool start(groove::Engine& engine,groove::ProjectState& project,groove::Performance& ui,groove::SessionFiles& files,CMk3Audio* audio,unsigned first=0){
  if(first>6 || used_ || busy() || !audio || !audio->operational() || (files.transfer_busy() || files.recording_busy()))return false;
  original_=groove::alloc_cpu_new<groove::ProjectState>(project);
  if(!original_ || !original_store_ || !files.copy_preserved_to(*original_store_)){delete original_;original_=nullptr;return false;}
  files.copy_bindings(original_views_);original_mode_=ui.mode();audio_=audio;audio_->EfficiencyMeter(&meter_);used_=true;failed_=false;index_=first;started_=Millis();phase_=Network;engine.begin_stage_profile(Micros,1);ui.stop();return true;
 }
 void service(groove::Engine& engine,groove::ProjectState& project,groove::Performance& ui,groove::SessionFiles& files,CProjectStorage& storage,CMk3Audio* audio,CUpdateRuntime& updater){
  if(!busy())return;
  unsigned ms=Millis();
  if(audio!=audio_ || !audio || !audio->operational()){failed_=true;phase_=Restore;}
  drain_row();
  if(phase_==Network){CString status;updater.RemoteStatus(status);if(!__builtin_strstr(status,"ip=none") && !__builtin_strstr(status,"ip=0.0.0.0")){work_.enable(true);mark(7);phase_=Prepare;CLogger::Get()->Write("eff",LogNotice,"EFF revision 3 begin freq %u rate 48000 quantum 24 bars 48 core-duty excludes deliberate idle; core0 spans include nested task waits; per-native clocks stride127",Frequency());}else if(ms-started_>15000){failed_=true;phase_=Restore;}return;}

  if(phase_==Prepare){
   if(index_==7){phase_=Restore;return;}
   if(pending_row_!=groove::EfficiencyAudioMeter::off)return;
   const char* paths[]={"PROJECTS/DSP-EFF-T1.mgxs","PROJECTS/DSP-EFF-T2.mgxs","PROJECTS/DSP-EFF-T3.mgxs","PROJECTS/DSP-EFF-T4.mgxs","PROJECTS/DSP-EFF-T5.mgxs","PROJECTS/DSP-EFF-T6.mgxs","PROJECTS/DSP-EFF-T7.mgxs"};const char* path=paths[index_];
   change(0);bool ok=Identity(storage,index_,path) && engine.dsp_profile().begin(Clock,engine.staged_generation()+1) && files.load(path,false,false);
   CLogger::Get()->Write("eff",ok?LogNotice:LogError,"EFF song %u load %u path %s tempo %u",index_+1,unsigned(ok),path,project.tempo);
   if(!ok){failed_=true;phase_=Restore;return;}
   {auto stream=storage.stream_counters();CLogger::Get()->Write("eff",LogNotice,"EFF stream song %u readers %u reads %lu bytes %lu failures %lu misses %lu",index_+1,stream.readers,(unsigned long)stream.reads,(unsigned long)stream.bytes,(unsigned long)stream.failures,(unsigned long)stream.misses);}
   limit_=unsigned((uint64_t(48000)*60*4*48/project.tempo+23)/24);started_=Millis();phase_=Settle;return;
  }
  if(phase_==Settle){if(ms-started_<1000 || pending_row_!=groove::EfficiencyAudioMeter::off)return;change(1);started_=ms;phase_=Arm;return;}
  if(phase_==Arm){if(pending_row_!=groove::EfficiencyAudioMeter::off || ms-started_<1000)return;meter_.cap(index_*8+2,limit_);engine.dsp_profile().begin_measurement();change(2);phase_=Play;started_=0;progress_due_=0;return;}
  if(phase_==Play){
   if(!started_){if(pending_row_!=groove::EfficiencyAudioMeter::off)return;if(!ui.play()){failed_=true;phase_=Restore;return;}started_=ms;}
   if(ms-progress_due_>=5000){progress_due_=ms;CLogger::Get()->Write("eff",LogNotice,"EFF progress song %u blocks %u/%u generation %u playing %u",index_+1,meter_.blocks(row_),limit_,engine.staged_generation(),unsigned(ui.playing()));}
   if(meter_.blocks(row_)>=limit_ || ms-started_>limit_/2*3+30000){if(meter_.blocks(row_)<limit_)failed_=true;if(index_==0 && !failed_){meter_.cap(6,24000);change(6);started_=ms;phase_=FilePlay;}else{change(3);ui.stop();started_=ms;phase_=Stop;}}return;
  }
  if(phase_==FilePlay){
   if(pending_row_!=groove::EfficiencyAudioMeter::off)return;
   if(!save_started_){save_at_=ms;bool clear=!storage.exists(SavePath) && !storage.exists("GROOVE/EFF-R3-SAVE.mgxs.tmp");save_started_=true;bool ok=clear && files.start_project_save(SavePath,true);if(!ok){save_known_=true;save_ok_=false;}CLogger::Get()->Write("eff",ok?LogNotice:LogError,"EFF fileplay request %u playing %u",unsigned(ok),unsigned(ui.playing()));}
   if((meter_.blocks(6)>=24000 && save_known_) || ms-started_>30000){bool clean=save_known_ && save_ok_ && ui.playing() && !files.project_save_busy() && !storage.exists(SavePath) && !storage.exists("GROOVE/EFF-R3-SAVE.mgxs.tmp");if(!clean)failed_=true;CLogger::Get()->Write("eff",clean?LogNotice:LogError,"EFF fileplay saved %u known %u cleaned %u elapsed-ms %u blocks %u",unsigned(save_ok_),unsigned(save_known_),unsigned(clean),ms-save_at_,meter_.blocks(6));change(3);ui.stop();started_=ms;phase_=Stop;}return;
  }
  if(phase_==Stop){if(ms-started_<1000 || pending_row_!=groove::EfficiencyAudioMeter::off)return;engine.dsp_profile().stop();change(4);dump_=0;due_=ms;phase_=Dump;return;}
  if(phase_==Dump){
   if(!engine.dsp_profile().ready() || pending_row_!=groove::EfficiencyAudioMeter::off || int32_t(ms-due_)<0)return;
   due_=ms+20;auto& profile=engine.dsp_profile();
   while(dump_<groove::DspProfile::bins){unsigned bin=dump_++;const auto& c=profile.counter(bin);if(!c.calls)continue;CLogger::Get()->Write("eff",LogNotice,"EFF bin song %u id %u calls %lu frames %lu ticks %lu active %lu max-block %u",index_+1,bin,(unsigned long)c.calls,(unsigned long)c.frames,(unsigned long)c.ticks,(unsigned long)c.active_ticks,c.max_block_ticks);return;}
   if(dump_<groove::DspProfile::bins+16){unsigned t=dump_++-groove::DspProfile::bins;const auto& c=profile.native_counter(t);if(c.frames)CLogger::Get()->Write("eff",LogNotice,"EFF native song %u track %u bin %u frames %lu ticks %lu sampled-blocks %u max24-ticks %u",index_+1,t+1,c.bin,(unsigned long)c.frames,(unsigned long)c.ticks,c.sampled_blocks,c.max_block_ticks);return;}
   {auto stream=storage.stream_counters();CLogger::Get()->Write("eff",LogNotice,"EFF stream-end song %u readers %u reads %lu bytes %lu failures %lu misses %lu",index_+1,stream.readers,(unsigned long)stream.reads,(unsigned long)stream.bytes,(unsigned long)stream.failures,(unsigned long)stream.misses);}
   {auto p=engine.prepared_sound_counts();CLogger::Get()->Write("eff",LogNotice,"EFF payload song %u prepared-bytes %lu chunks %lu completed %lu warm %lu cold %lu max-chunk %u",index_+1,(unsigned long)p.prepared_bytes,(unsigned long)p.chunks,(unsigned long)p.completed,(unsigned long)p.warm,(unsigned long)p.cold,p.max_chunk);}
   CLogger::Get()->Write("eff",LogNotice,"EFF song %u done failed %u",index_+1,unsigned(failed_));started_=ms;phase_=Drain;return;
  }
  if(phase_==Drain){if(ms-started_<1000)return;char pending;bool unread=CLogger::Get()->Read(&pending,1,FALSE)>0;if(unread && ms-started_<30000)return;if(unread){failed_=true;phase_=Restore;}else{++index_;phase_=Prepare;}return;}
  if(phase_==Restore){if(!restore_started_)restore_started_=ms;if(ms-restore_started_>15000){work_.enable(false);meter_.request(groove::EfficiencyAudioMeter::off);engine.end_stage_profile();if(audio_)audio_->EfficiencyMeter(nullptr);phase_=Idle;CLogger::Get()->Write("eff",LogError,"EFF revision 3 completed failed 1 restored 0 RAM snapshot retained");return;}if(files.project_save_busy())return;engine.dsp_profile().stop();if(!engine.dsp_profile().quiescent())return;ui.stop();if(pending_row_!=groove::EfficiencyAudioMeter::off)return;change(5);if(original_ && files.restore_probe_snapshot(*original_,original_views_,*original_store_)){ui.set_mode(original_mode_);restore_generation_=engine.staged_generation();started_=ms;phase_=Restoring;}return;}
  if(phase_==Restoring && ui.visual().generation==restore_generation_ && ms-started_>=1000 && pending_row_==groove::EfficiencyAudioMeter::off){meter_.request(groove::EfficiencyAudioMeter::off);if(meter_.acknowledged()!=groove::EfficiencyAudioMeter::off)return;groove::CoreWorkMeter::Snapshot end[4];for(unsigned c=0;c<4;++c)end[c]=work_.snapshot(c);report(row_,row_start_,CTimer::GetClockTicks(),work_start_,end);work_.enable(false);engine.end_stage_profile();if(audio_)audio_->EfficiencyMeter(nullptr);delete original_;original_=nullptr;original_store_->clear();phase_=Idle;CLogger::Get()->Write("eff",LogNotice,"EFF revision 3 completed failed %u restored 1",unsigned(failed_));}
 }
};
