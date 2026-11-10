#pragma once
#include "../src/session_files.h"
#include "../src/ui_renderer.h"
#if !defined(__linux__)
#include "cpu_memory.h"
#endif

namespace groove {
// Task-owned, explicitly requested functional qualification. Never renders audio,
// runs a soak, writes PROJECTS or changes CURRENT.TXT. The normal audio worker
// consumes every command and publishes the observations used here.
class DeploymentCheck {
public:
 using Report=void(*)(void*,const char*,bool);
private:
 Engine& engine_;ProjectState& project_;Performance& ui_;SessionFiles& files_;IFileAccess& fs_;
 struct State {
  ProjectState original,candidate;PreservedStore preserved;WavView bindings[kSampleAssets]{};
  NavigationLocation navigation{};TwinConfig twin{};UiRenderer renderer;
  char project_path[160]{},fixture[96]{},materials[4][96]{};unsigned material_count=0;
  uint32_t generation=0;unsigned stage=0,edits=0,passed=0,failed=0;
  uint64_t deadline=0,after=0;float isolation_peak=0,sample_peak=0;
  Report report=nullptr;void* context=nullptr;
  __attribute__((noinline,optimize("Os"))) explicit State(const ProjectState& source):original(source),candidate(source){}
 };
 inline static constexpr const char* suffixes_[]={".tmp",".new",".bak"};
 State* state_=nullptr;bool ready_=false,success_=false;unsigned passed_=0;
 bool check(const char* name,bool ok){auto& s=*state_;if(ok)++s.passed;else ++s.failed;if(s.report)s.report(s.context,name,ok);return ok;}
 void next(unsigned stage,uint64_t now,unsigned delay=0){state_->stage=stage;state_->after=now+delay;state_->deadline=now+30000;}
 void restore(uint64_t now){ui_.set_shift(false);ui_.cancel_project();ui_.cancel_browser_edit();ui_.browser().new_project_list=false;ui_.stop();if(files_.recording_busy())ui_.request_file(FileAction::CancelTwin);next(90,now);}
 bool mount(){auto& s=*state_;if(!engine_.stage_project(s.candidate))return false;project_=s.candidate;ui_.project_loaded();ui_.select_track(0);s.generation=engine_.staged_generation();return true;}
 bool synced()const{return ui_.visual().generation==state_->generation;}
 bool visible(const char* text){auto& s=*state_;struct Find{const char* text;bool found=false;} find{text};s.renderer.text_observer=[](const SoftwareCanvas::TextPlacement& p,void* context){auto& f=*static_cast<Find*>(context);if(__builtin_strstr(p.text,f.text))f.found=true;};s.renderer.text_observer_context=&find;s.renderer.invalidate();s.renderer.draw(project_,ui_);s.renderer.text_observer=nullptr;s.renderer.text_observer_context=nullptr;return find.found;}
 bool remember_material(){auto& s=*state_;const auto& pair=project_.twins[0];if(!pair.prepared())return false;unsigned slot=project_.sounds[pair.sampled].sample_slot;if(slot>=kSampleAssets)return false;const char* path=project_.assets[slot].path;for(unsigned i=0;i<s.material_count;++i)if(!__builtin_strcmp(path,s.materials[i]))return true;return s.material_count<4 && copy_text(s.materials[s.material_count++],96,path);}
 bool zones(){const auto& t=project_.sounds[project_.twins[0].sampled];if(t.sample_edit.zone_count!=3 || t.sample_edit.slice_count!=3)return false;WavView views[kSampleAssets]{};files_.copy_bindings(views);const auto& wav=views[t.sample_slot];if(!wav.data || wav.reader || wav.format!=3 || wav.bits!=32)return false;uint32_t sums[3]{};
  for(unsigned z=0;z<3;++z){if(t.sample_edit.zones[z].root!=48+12*z)return false;unsigned a=unsigned(t.sample_edit.markers[z]*wav.frames+.5f),b=unsigned(t.sample_edit.markers[z+1]*wav.frames+.5f);double energy=0;uint32_t hash=2166136261u;if(b<=a+500 || b>wav.frames)return false;for(unsigned f=a;f<b;++f){float v=wav_sample(wav,f,0);uint32_t bits;__builtin_memcpy(&bits,&v,4);energy+=double(v)*v;hash=(hash^bits)*16777619u;}if(energy<1e-5)return false;sums[z]=hash;}return sums[0]!=sums[1] && sums[1]!=sums[2] && t.sample_edit.zones[0].key_low==0 && t.sample_edit.zones[2].key_high==127;
 }
 bool save_fixture(){auto& s=*state_;s.candidate=project_;return project_paths(s.candidate,s.fixture,false) && save_recoverable_checked(fs_,s.fixture,s.candidate,[&](){return engine_.project_snapshot_safe();});}
public:
 DeploymentCheck(Engine& e,ProjectState& p,Performance& u,SessionFiles& f,IFileAccess& fs):engine_(e),project_(p),ui_(u),files_(f),fs_(fs){}
 ~DeploymentCheck(){delete state_;}
 bool busy()const{return state_!=nullptr;}
 bool result(unsigned& passed,bool& ok){if(!ready_)return false;ready_=false;passed=passed_;ok=success_;return true;}
 __attribute__((noinline,optimize("Os"))) bool start(uint64_t now,Report report,void* context){
  if(busy() || files_.recording_busy() || files_.project_save_busy() || files_.kit_load_busy() || ui_.file_action_pending() || ui_.modal_active() || !engine_.project_snapshot_safe())return false;
  #if defined(__linux__)
  auto* s=alloc_new<State>(project_);
#else
  auto* s=alloc_cpu_new<State>(project_);
#endif
  if(!s){return false;}state_=s;s->report=report;s->context=context;s->navigation=ui_.navigation();s->twin=ui_.twin_config();copy_text(s->project_path,160,ui_.project_path());files_.copy_bindings(s->bindings);
  if(!files_.copy_preserved_to(s->preserved) || !fs_.directory("GROOVE")){delete state_;state_=nullptr;return false;}
  bool fresh=false;for(unsigned i=1;i<1000 && !fresh;++i){copy_text(s->fixture,96,"GROOVE/Deploy-000.mgxs");s->fixture[14]=char('0'+i/100);s->fixture[15]=char('0'+i/10%10);s->fixture[16]=char('0'+i%10);fresh=!fs_.exists(s->fixture);for(const char* suffix:suffixes_){char path[112];copy_text(path,112,s->fixture);copy_text(path+__builtin_strlen(path),112-__builtin_strlen(path),suffix);fresh=fresh && !fs_.exists(path);}}
  if(!fresh){delete state_;state_=nullptr;return false;}
  ui_.panic();ui_.restore_navigation({});make_empty_project(s->candidate);s->candidate.tempo=300;s->candidate.routing.monitor=0;auto t=track_at(s->candidate,0,0);t.instrument=Instrument::FM;t.length=256;t.level=.18f;for(unsigned n=0;n<256;++n){t.steps[n].enabled=true;t.steps[n].note=uint8_t(48+n%24);t.steps[n].velocity=80;}s->candidate.song.enabled=true;s->candidate.song.length=1;s->candidate.song.sections[0].bars=16;
  const char* supplied="PROJECTS/Deploy-LongPhrase.mgxs";bool external=fs_.exists(supplied);bool ok=external?files_.load(supplied,false,false):(project_paths(s->candidate,s->fixture,false) && save_recoverable(fs_,s->fixture,s->candidate) && files_.load(s->fixture,false,false));ready_=false;if(external)check("external-midi-converter-fixture",ok && track_at(project_,0,0).length==256);
  if(!check("long-song-current-format-load",ok)){restore(now);return true;}s->generation=engine_.staged_generation();ui_.select_track(0);next(1,now,100);return true;
 }
 __attribute__((noinline,optimize("Os"))) void service(uint64_t now){
  if(!state_){return;}auto& s=*state_;if(now>s.deadline && s.stage<90){check("deadline",false);restore(now);return;}if(now<s.after)return;
  bool ok=true;
  switch(s.stage){
   case 1:if(!synced())return;ui_.set_mode(UiMode::Generate);for(unsigned n=0;n<8 && ui_.generate_page();++n)ui_.encoder_direction(0x04);ok=ui_.generate_page()==0 && ui_.play();next(2,now,20);break;
   case 2:ok=ui_.knob(s.edits%2,(s.edits%4)<2?1000:-1000) && ui_.playing() && track_at(project_,0,0).length==256;if(++s.edits<80){s.after=now+10;break;}check("variation-80-live-pulse-rotation",ok && ui_.visual().playing);ui_.set_mode(UiMode::Step);ui_.set_shift(true);ok=ok && ui_.knob(7,-1000);next(3,now,30);break;
   case 3:ok=track_at(project_,0,0).length==255 && ui_.playing() && ui_.knob(7,1000);next(4,now,30);break;
   case 4:ok=track_at(project_,0,0).length==256 && ui_.visual().playing && ui_.stop();ui_.set_shift(false);next(5,now,50);break;
   case 5:ok=!ui_.visual().playing && ui_.play();next(6,now,50);break;
   case 6:ok=check("live-length-stop-play",ui_.visual().playing && ui_.playing()) && ui_.stop();make_empty_project(s.candidate);s.candidate.tempo=300;s.candidate.routing.monitor=0;{auto t=track_at(s.candidate,0,0);t.instrument=Instrument::Layer;t.level=.5f;t.layer.count=2;t.steps[0].enabled=true;t.steps[0].note=60;t.steps[0].velocity=100;for(unsigned i=0;i<2;++i){auto& part=t.layer.parts[i];part.sound.instrument=Instrument::VirtualAnalog;part.level=.2f;auto& v=part.sound.virtual_analog.values;v[0]=float(i);v[1]=float(i+1);v[4]=800+float(i)*700;v[11]=.001f;v[12]=.02f;v[13]=.65f;v[14]=.03f;}}ok=ok && mount();next(10,now,200);break;
   case 10:if(!synced())return;ui_.set_mode(UiMode::Mixer);ui_.twin_config()={};ui_.twin_config().low=48;ui_.twin_config().high=72;ui_.twin_config().interval=12;ui_.twin_config().hold_ms=80;ui_.twin_config().max_ms=600;ok=ui_.soft_key(5);next(11,now,10);break;
   case 11:s.isolation_peak=fmaxf(s.isolation_peak,fmaxf(ui_.visual().master_peak,ui_.visual().cue_peak));if(files_.recording_busy() || ui_.file_action_pending() || ui_.twin_dialog()==2)return;ok=check("twin-ready-header",ui_.twin_status()==TwinStatus::Ready && visible("TWIN READY")) && remember_material();ok=check("twin-three-root-distinct-material",ok && zones()) && ok;ok=check("twin-prepare-main-cue-isolated",s.isolation_peak<1e-7f) && ok;next(12,now,200);break;
   case 12:ok=ui_.play();next(13,now,50);break;
   case 13:ok=ui_.playing() && ui_.soft_key(5);next(14,now);break;
   case 14:if(ui_.twin_status()!=TwinStatus::Active || ui_.visual().attached_instrument[0]!=unsigned(Instrument::Sampler))return;ok=check("twin-active-one-operation-live",ui_.playing() && visible("TWIN ACTIVE"));s.sample_peak=0;next(15,now,150);break;
   case 15:s.sample_peak=fmaxf(s.sample_peak,ui_.visual().peak[0]);if(s.sample_peak<1e-6f && now<s.after+800)return;ok=check("twin-sampler-audio",s.sample_peak>1e-6f) && ui_.soft_key(5);next(16,now);break;
   case 16:if(ui_.twin_status()!=TwinStatus::Ready || ui_.visual().attached_instrument[0]!=unsigned(Instrument::Layer))return;ok=check("twin-source-return-one-operation-live",ui_.playing()) && ui_.stop() && ui_.layer_knob(3,-1);next(17,now,250);break;
   case 17:ok=check("twin-stale-header",ui_.twin_status()==TwinStatus::Stale && visible("TWIN STALE")) && ui_.soft_key(5);next(18,now,10);break;
   case 18:if(files_.recording_busy() || ui_.file_action_pending() || ui_.twin_dialog()==2)return;ok=check("twin-reprepare-ready",ui_.twin_status()==TwinStatus::Ready) && remember_material() && ui_.soft_key(5);next(19,now,100);break;
   case 19:if(ui_.twin_status()!=TwinStatus::Active)return;ok=save_fixture() && files_.load(s.fixture,false,false);s.generation=engine_.staged_generation();ui_.select_track(0);ui_.set_mode(UiMode::Mixer);next(20,now,100);break;
   case 20:if(!synced())return;ok=check("twin-active-song-reload",ui_.twin_status()==TwinStatus::Active) && ui_.soft_key(5);next(21,now,100);break;
   case 21:if(ui_.twin_status()!=TwinStatus::Ready)return;ok=check("twin-reloaded-source-return",project_.tracks[0].instrument==Instrument::Layer);make_empty_project(s.candidate);ok=ok && mount();next(25,now,100);break;
   case 25:if(!synced())return;ui_.set_mode(UiMode::Song);ok=ui_.soft_key(5);next(26,now,100);break;
   case 26:{const unsigned bars[]={4,8,8,8,4};ok=project_.song.length==5 && project_.song.loop;for(unsigned i=0;i<5;++i)ok=ok && project_.song.sections[i].bars==bars[i] && project_.song.sections[i].scene==i;ok=check("song-skeleton-five-sections",ok);ui_.request_project(Performance::ProjectAction::New);next(27,now,100);break;}
   case 27:if(ui_.file_action_pending())return;ok=check("project-new-template-or-empty",ui_.browser().new_project_list?(ui_.browser().count && !__builtin_strcmp(ui_.browser().entries[0].name,"EMPTY")):ui_.pending_project()==Performance::ProjectAction::New);ui_.cancel_project();ui_.browser().new_project_list=false;ui_.set_mode(UiMode::Pad);ok=ok && ui_.set_multitimbral(true);ok=check("module-enter-notification",ok && ui_.notice()==UiNotice::ModuleEntered);next(28,now,150);break;
   case 28:if(!ui_.multitimbral())return;ok=check("module-persistent-header",visible("MODULE") && visible("MIDI IN > TRACKS"));ok=check("module-play-exit-guidance",!ui_.play() && ui_.notice()==UiNotice::ModuleTransport && __builtin_strstr(ui_.notice_text(),"SETTINGS > MIDI")) && ok;ok=ui_.set_multitimbral(false) && ok;next(29,now,100);break;
   case 29:if(ui_.multitimbral())return;make_empty_project(s.candidate);s.candidate.tracks[0].instrument=Instrument::Layer;s.candidate.tracks[0].layer.count=4;for(auto& part:s.candidate.tracks[0].layer.parts){part.sound.instrument=Instrument::PlaitsMacro;part.sound.model=0;}ok=mount();next(30,now,200);break;
   case 30:if(!synced())return;ui_.set_mode(UiMode::Pad);ok=check("layer-refusal-common-header",ui_.layer_warning()==LayerReason::Voices && visible("LAYER REFUSED") && visible(layer_short_reason(LayerReason::Voices)));restore(now);break;
   case 90:if(files_.recording_busy() || ui_.file_action_pending())return;ui_.cancel_project();project_=s.original;if(!files_.sync_preserved_from(s.preserved))return;if(!engine_.stage_project(project_,s.bindings))return;files_.set_bindings(s.bindings);ui_.project_loaded();ui_.project_path(s.project_path);ui_.twin_config()=s.twin;s.generation=engine_.staged_generation();next(91,now,100);break;
   case 91:if(!synced())return;ui_.restore_navigation(s.navigation);{bool clean=fs_.remove(s.fixture);for(const char* suffix:suffixes_){char path[112];copy_text(path,112,s.fixture);copy_text(path+__builtin_strlen(path),112-__builtin_strlen(path),suffix);clean=fs_.remove(path) && clean;}for(unsigned i=0;i<s.material_count;++i)clean=fs_.remove(s.materials[i]) && clean;check("restored-owned-files-cleaned",clean);passed_=s.passed;success_=!s.failed;ready_=true;delete state_;state_=nullptr;}return;
   default:ok=false;break;
  }
  if(!ok && state_ && state_->stage<90){check("operation",false);restore(now);}
 }
};
}
