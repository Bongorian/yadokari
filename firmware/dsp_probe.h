#pragma once
#include "mk3_audio.h"
#include "project_storage.h"
#include "../src/session_files.h"
#include "../src/dsp_probe_workload.h"
#include "../src/alloc_policy.h"
#include <circle/logger.h>
#include <circle/timer.h>
// Primary/task owner only. The audio core only sees Engine's fixed profiler.
class CDspProbe {
 enum Phase {Idle,Prepare,Warmup,Measure,Closing,Restore,Restoring};Phase phase_=Idle;
 groove::ProjectState* original_=nullptr;groove::ProjectState* candidate_=nullptr;
 groove::WavView original_views_[64]{},views_[64]{};
 uint8_t pcm_[512*2]{};bool optimization_=false;bool sf_ready_=false,aborted_=false,triggered_=false,measured_=false;unsigned bank_=0,program_=0,case_=0,window_=0,case_missing_=0,suite_missing_=0,last_missing_=0;
 unsigned start_ms_=0,window_ms_=0,next_hit_ms_=0,next_tone_ms_=0,suite_ms_=0,restore_generation_=0;
 unsigned dump_position_=0,dump_due_=0,measured_ms_=0,window_missing_=0;bool dump_started_=false;
 CMk3Audio::WorkerWindow worker_window_{};CMk3Audio::TimingWindow timing_window_{};
 CMk3Audio* audio_=nullptr;groove::UiMode original_mode_=groove::UiMode::Pad;
 groove::DspCondition condition()const{return optimization_?(case_==0 || case_==2?groove::DspCondition{groove::DspCondition::Voices,groove::Instrument::VirtualAnalog,0,case_==0?4u:2u,8}:groove::DspCondition{groove::DspCondition::Native,groove::Instrument::Texture,0,case_==1?16u:4u,1}):groove::dsp_condition(case_);}
 unsigned end_case()const{return optimization_?4:groove::dsp_case_count();}
 static uint32_t Clock(){uint64_t ticks;asm volatile("isb\n\tmrs %0, cntpct_el0":"=r"(ticks)::"memory");return uint32_t(ticks);}
 static unsigned Frequency(){uint64_t frequency;asm volatile("mrs %0, cntfrq_el0":"=r"(frequency));return unsigned(frequency);}
 static unsigned Millis(){return unsigned(CTimer::GetClockTicks64()/1000);}
 void tone(groove::Engine& engine,unsigned ms){if(int32_t(ms-next_tone_ms_)>=0){engine.submit({groove::CommandType::DiagnosticTone,0,0,1});next_tone_ms_=ms+900;}}
 void trigger(groove::Engine& engine,const groove::DspCondition& c,unsigned ms){
  if(c.kind==groove::DspCondition::Idle || c.kind==groove::DspCondition::Chain)return;
  if(int32_t(ms-next_hit_ms_)<0)return;
  if(triggered_ && !groove::dsp_retrigger(c.engine))return;
  for(unsigned t=0;t<c.tracks;++t)for(unsigned n=0;n<c.notes;++n){groove::Command cmd{groove::CommandType::Trigger,uint8_t(t),uint8_t(n),100};cmd.parameter=groove::dsp_retrigger(c.engine)?0:1;cmd.scalar=float(60+3*n);engine.submit(cmd);}
  triggered_=true;next_hit_ms_=ms+500;
 }
 // One line per task interval keeps the CDC log ring bounded. Results remain
 // immutable while draining; timing windows close BEFORE the drain begins.
 static bool dump(groove::Engine& engine,unsigned index,unsigned& position){
  auto& profile=engine.dsp_profile();const auto& s=profile.snapshot();
  if(!position){position=1;CLogger::Get()->Write("dsp",LogNotice,"DSP profile case %u freq %u blocks %u sampled %u main-peak %u poly-peak %u control-calls %u control-ticks %lu control-max %u snapshot-max %u",index,Frequency(),s.blocks,s.sampled,s.main_peak,s.poly_peak,s.control_calls,(unsigned long)s.control_ticks,s.control_max,s.snapshot_control_max);CLogger::Get()->Write("dsp",LogNotice,"DSP VA case %u triggers %u ticks %lu max %u params %u ticks %lu max %u",index,s.va_trigger_calls,(unsigned long)s.va_trigger_ticks,s.va_trigger_max,s.va_parameter_calls,(unsigned long)s.va_parameter_ticks,s.va_parameter_max);CLogger::Get()->Write("dsp",LogNotice,"DSP peak case %u total %u visual %u trigger %u params %u visual-max %u visual-ticks %lu",index,s.all_block_max,s.peak_visual,s.peak_va_trigger,s.peak_va_parameters,s.visual_block_max,(unsigned long)s.visual_ticks);return false;}
  while(position<=groove::DspProfile::bins){unsigned i=position++-1;const auto& c=profile.counter(i);if(c.calls){CLogger::Get()->Write("dsp",LogNotice,"DSP bin case %u id %u calls %lu frames %lu ticks %lu active %lu max-block %u max-voice-q16 %lu",index,i,(unsigned long)c.calls,(unsigned long)c.frames,(unsigned long)c.ticks,(unsigned long)c.active_ticks,c.max_block_ticks,(unsigned long)c.max_voice_q16);return false;}}
  while(position<=groove::DspProfile::bins+16){unsigned t=position++-groove::DspProfile::bins-1;if(s.layer_reasons[t]){CLogger::Get()->Write("dsp",LogNotice,"DSP layer case %u track %u reason %u",index,t,unsigned(s.layer_reasons[t]));return false;}}
  return true;
 }
public:
 bool busy()const{return phase_!=Idle;}
 bool start(groove::Engine& engine,groove::ProjectState& project,groove::Performance& ui,groove::SessionFiles& files,CMk3Audio* audio,bool stress_only,bool optimization_only=false){
  if(busy() || !audio || !audio->operational() || files.recording_busy())return false;
  audio_=audio;suite_missing_=last_missing_=audio->worker_missing();suite_ms_=Millis();
  original_=groove::alloc_new<groove::ProjectState>(project);candidate_=groove::alloc_new<groove::ProjectState>();
  if(!original_ || !candidate_){delete original_;delete candidate_;original_=candidate_=nullptr;return false;}
  files.copy_bindings(original_views_);for(auto& view:views_)view={};for(unsigned i=0;i<512;++i){int16_t value=(i%128)<64?8192:-8192;pcm_[2*i]=uint8_t(value);pcm_[2*i+1]=uint8_t(uint16_t(value)>>8);}views_[0]={pcm_,512,48000,1,16,1};
  optimization_=optimization_only;original_mode_=ui.mode();case_=optimization_?0:stress_only?groove::dsp_stress_start():0;aborted_=sf_ready_=false;phase_=Prepare;
  unsigned low=~0u,high=0;uint64_t sum=0;for(unsigned i=0;i<256;++i){unsigned a=Clock(),elapsed=Clock()-a;if(elapsed<low)low=elapsed;if(elapsed>high)high=elapsed;sum+=elapsed;}
  CLogger::Get()->Write("dsp",LogNotice,"DSP suite revision %u begin first %u end %u freq %u clock-pair-min %u avg %u max %u deadline-us 500 seed 0x57313001",optimization_?groove::kDspProbeRevision:groove::kWave4ProbeRevision,case_,end_case(),Frequency(),low,unsigned(sum/256),high);
  ui.panic();return true;
 }
 void service(groove::Engine& engine,groove::ProjectState& project,groove::Performance& ui,groove::SessionFiles& files,CProjectStorage& storage,CMk3Audio* audio,bool stop){
  if(!busy()){return;}unsigned ms=Millis();
  if(audio==audio_ && audio)last_missing_=audio->worker_missing();
  if((stop || audio!=audio_ || !audio || !audio->operational()) && phase_!=Restore && phase_!=Restoring){aborted_=true;engine.dsp_profile().stop();phase_=Restore;CLogger::Get()->Write("dsp",LogWarning,"DSP suite abort: STOP or audio binding changed");}
  if(phase_==Prepare){
   if(case_>=end_case()){phase_=Restore;return;}
   const auto condition=this->condition();case_missing_=last_missing_;measured_=dump_started_=false;window_=window_ms_=0;
   // Begin before load/stage: coarse control cost includes ApplySnapshot.
   if(!engine.dsp_profile().begin(Clock,engine.staged_generation()+1)){CLogger::Get()->Write("dsp",LogError,"DSP profiler busy case %u",case_);aborted_=true;phase_=Restore;return;}
   groove::make_dsp_probe_project(*candidate_,condition);
   groove::copy_text(candidate_->assets[0].path,96,"GROOVE/DSP-RESIDENT.WAV");candidate_->assets[0].metadata.frames=512;candidate_->assets[0].metadata.rate=48000;candidate_->assets[0].metadata.channels=1;candidate_->assets[0].metadata.bits=16;
   if(condition.engine==groove::Instrument::SoundFont && !sf_ready_){uint32_t crc=0;const char* path="GROOVE/LOADTEST.SF2";sf_ready_=storage.load(path,views_[4]) && views_[4].soundfont && groove::soundfont_first(views_[4].soundfont,bank_,program_) && groove::file_checksum(storage,path,crc);CLogger::Get()->Write("dsp",LogNotice,"DSP SF2 available %u bytes %u crc %08x bank %u program %u",unsigned(sf_ready_),unsigned(storage.size(path)),crc,bank_,program_);}
   if(condition.engine==groove::Instrument::SoundFont){if(!sf_ready_){engine.dsp_profile().stop();phase_=Closing;window_=0;CLogger::Get()->Write("dsp",LogWarning,"DSP case %u SKIPPED missing valid SoundFont",case_);return;}for(auto& t:candidate_->tracks)if(t.instrument==groove::Instrument::SoundFont){t.soundfont.values[0]=float(bank_);t.soundfont.values[1]=float(program_);}groove::copy_text(candidate_->assets[4].path,96,"GROOVE/LOADTEST.SF2");}
   bool ok=engine.stage_project(*candidate_,views_);if(!ok){engine.dsp_profile().stop();phase_=Closing;window_=0;CLogger::Get()->Write("dsp",LogWarning,"DSP case %u REJECTED invalid project/resource",case_);return;}
   project=*candidate_;files.set_bindings(views_);ui.project_loaded();if(condition.kind==groove::DspCondition::Chain)ui.play();
   start_ms_=Millis();next_hit_ms_=next_tone_ms_=start_ms_;triggered_=measured_=false;phase_=Warmup;window_=window_ms_=0;
   CLogger::Get()->Write("dsp",LogNotice,"DSP case %u start kind %u engine %u variant %u requested-tracks %u notes %u generation %u",case_,unsigned(condition.kind),unsigned(condition.engine),condition.variant,condition.tracks,condition.notes,engine.staged_generation());return;
  }
  if(phase_==Warmup || phase_==Measure){const auto condition=this->condition();tone(engine,ms);trigger(engine,condition,ms);
   if(phase_==Warmup && uint32_t(ms-start_ms_)>=2000){audio->BeginWorkerWindow();audio->BeginTimingWindow();engine.dsp_profile().begin_measurement();window_ms_=ms;window_=last_missing_;measured_=true;phase_=Measure;CLogger::Get()->Write("dsp",LogNotice,"DSP case %u warmup-missing %u",case_,last_missing_-case_missing_);}
   if(phase_==Measure && uint32_t(ms-window_ms_)>=6000){engine.dsp_profile().stop();phase_=Closing;}
   return;
  }
  if(phase_==Closing){
   if(!engine.dsp_profile().ready()){return;}
   if(!dump_started_){dump_started_=true;dump_position_=0;dump_due_=ms;if(measured_){worker_window_=audio->EndWorkerWindow();timing_window_=audio->EndTimingWindow();measured_ms_=ms-window_ms_;window_missing_=last_missing_-window_;}}
   if(int32_t(ms-dump_due_)<0)return;
   dump_due_=ms+100;if(!dump(engine,case_,dump_position_))return;
   if(measured_){auto w=worker_window_;auto a=timing_window_;CLogger::Get()->Write("dsp",LogNotice,"DSP result case %u window-ms %u blocks %u avg-us %u max-us %u over500 %u window-missing %u condition-missing %u native-active %u refused %u callbacks %u callback-max %u bad %u",case_,measured_ms_,w.blocks,w.blocks?unsigned(w.total_us/w.blocks):0,w.max_us,w.over500,window_missing_,last_missing_-case_missing_,engine.native_active_mask(),engine.native_refused_mask(),a.callbacks,a.max_us,a.bad_chunks);}
   ui.stop();engine.submit({groove::CommandType::Panic,0,0,0});++case_;window_=window_ms_=0;phase_=Prepare;return;
  }
  if(phase_==Restore){
   engine.dsp_profile().stop();ui.stop();engine.submit({groove::CommandType::DiagnosticTone,0,0,0});if(!engine.dsp_profile().quiescent())return;
   if(engine.stage_project(*original_,original_views_)){project=*original_;files.set_bindings(original_views_);ui.project_loaded();ui.set_mode(original_mode_);restore_generation_=engine.staged_generation();phase_=Restoring;}return;
  }
  if(phase_==Restoring && ui.visual().generation==restore_generation_){delete original_;delete candidate_;original_=candidate_=nullptr;phase_=Idle;CLogger::Get()->Write("dsp",LogNotice,"DSP suite revision %u completed aborted %u elapsed-ms %u total-missing %u restored 1",optimization_?groove::kDspProbeRevision:groove::kWave4ProbeRevision,unsigned(aborted_),uint32_t(ms-suite_ms_),last_missing_-suite_missing_);}
 }
};
