#pragma once
#include "load_sf2_generator.h"
#include "../src/session_files.h"
#include "../src/future_factory_presets.h"
#include <circle/logger.h>
namespace groove {
// Diagnostic-only, main-loop owner. All audio mutations are queued. No fake HID/analog claims.
class FinalDeviceCheck {
 SessionFiles& files_;IFileAccess& fs_;Engine& engine_;ProjectState& project_;Performance& ui_;
 ProjectState* original_=nullptr;WavView bindings_[64]{};DeviceSettings device_{};LoadSf2Generator sf2_;
 unsigned stage_=0,index_=0,passed_=0,failed_=0;uint64_t deadline_=0,tone_=0,started_=0;uint32_t generation_=0;float baseline_[2]{};char take_[96]{};bool record_done_=false,record_ok_=false;UiMode mode_=UiMode::Pad;unsigned selected_=0;
 void result(const char* name,bool ok){if(ok)++passed_;else ++failed_;CLogger::Get()->Write("final",ok?LogNotice:LogWarning,"Final check %s ok %u",name,unsigned(ok));}
 void next(unsigned s,uint64_t now,unsigned delay=0){stage_=s;deadline_=now+delay;}
 bool mount(ProjectState& p){if(!engine_.stage_project(p))return false;project_=p;ui_.project_loaded();generation_=engine_.staged_generation();return true;}
 bool preferences(DeviceSettings d){return ui_.set_device_settings(d);}
public:
 FinalDeviceCheck(SessionFiles& f,IFileAccess& fs,Engine& e,ProjectState& p,Performance& u):files_(f),fs_(fs),engine_(e),project_(p),ui_(u){}
 ~FinalDeviceCheck(){delete original_;}
 bool busy()const{return stage_!=0;}
 bool start(uint64_t now){if(busy() || files_.recording_busy() || files_.project_save_busy() || files_.kit_load_busy() || ui_.file_action_pending() || !engine_.project_snapshot_safe())return false;original_=alloc_new<ProjectState>(project_);auto* candidate=alloc_new<ProjectState>();if(!original_ || !candidate){delete original_;delete candidate;original_=nullptr;return false;}sf2_.reset();device_=ui_.device_settings();mode_=ui_.mode();selected_=ui_.selected_track();files_.copy_bindings(bindings_);ui_.panic();make_future_factory_preset(*candidate,11);candidate->routing.monitor=0;candidate->tracks[0].root_note=69;bool ok=mount(*candidate);delete candidate;if(!ok){delete original_;original_=nullptr;return false;}ui_.select_track(0);started_=now;passed_=failed_=index_=0;take_[0]=0;record_done_=record_ok_=false;next(1,now);return true;}
 void recorded(const RecordingResult& r){if(stage_!=31 || !r.ready)return;record_done_=true;record_ok_=r.saved && r.verified && r.assigned && r.frames==48000u*300 && !r.dropped && r.peak_l_u>1000 && r.peak_r_u>1000;if(r.assigned && r.slot<64)copy_text(take_,sizeof take_,project_.assets[r.slot].path);else if(r.partial[0])copy_text(take_,sizeof take_,r.partial);CLogger::Get()->Write("final",LogNotice,"Final long-record frames %u saved %u verified %u assigned %u drops %u",r.frames,unsigned(r.saved),unsigned(r.verified),unsigned(r.assigned),r.dropped);}
 __attribute__((noinline,optimize("Os"))) void service(uint64_t now){if(!stage_)return;ui_.animate(now);
  if(now-started_>480000 && stage_<90){result("deadline",false);ui_.stop();engine_.stop_live_record();next(90,now);}
  if((stage_==2 || stage_==31) && now>=tone_){engine_.submit({CommandType::DiagnosticTone,0,0,2});tone_=now+500;}
  if(now<deadline_)return;
  switch(stage_){
   case 1:if(ui_.visual().generation!=generation_)return;{auto d=device_;d.input_db=0;d.local=true;d.screen=100;d.tuner_source=0;preferences(d);ui_.set_mode(UiMode::Settings);ui_.soft_key(4);index_=0;tone_=0;next(2,now,1500);}break;
   case 2:{const auto& v=ui_.visual();const auto& t=ui_.tuner();bool ok=v.input_lr[0]>.001f && v.input_lr[1]>.001f && v.input_lr[0]<.98f && v.input_lr[1]<.98f && t.note==69 && fabsf(t.cents)<10;if(index_%3==0){baseline_[0]=v.input_lr[0];baseline_[1]=v.input_lr[1];}else{float ratio=index_%3==1?1.995262f:.501187f;for(unsigned i=0;i<2;++i)ok=ok && fabsf(v.input_lr[i]/baseline_[i]-ratio)<ratio*.15f;}CLogger::Get()->Write("final",LogNotice,"Final input case %u gain %d peaks-u %u/%u note %d cents-milli %d",index_,int(ui_.device_settings().input_db),unsigned(v.input_lr[0]*1000000),unsigned(v.input_lr[1]*1000000),t.note,int(t.cents*1000));result("gain-tuner",ok);
    if(++index_<9){auto d=ui_.device_settings();d.input_db=index_%3==0?0:index_%3==1?6:-6;d.tuner_source=index_/3;preferences(d);next(2,now,1500);}else{engine_.submit({CommandType::DiagnosticTone,0,0,0});auto d=ui_.device_settings();d.input_db=0;d.local=false;d.screen=60;d.led=1;d.velocity=2;preferences(d);ui_.set_mode(UiMode::Keyboard);next(3,now,1300);}}break;
   case 3:{uint8_t bytes[16];unsigned got=0;DeviceSettings decoded;bool ok=ui_.device_saved() && fs_.read("GROOVE/DEVICE.CFG",0,bytes,16,got) && got==16 && decode_device(bytes,decoded) && decoded.screen==60 && decoded.led==1 && decoded.velocity==2 && !decoded.local && decoded.input_db==0;result("device-persistence",ok);ui_.pad(12,0x10,3000);next(4,now,300);}break;
   case 4:result("local-off-silent",ui_.visual().master_peak<.0001f);ui_.pad(12,0x20,0);{auto d=ui_.device_settings();d.local=true;preferences(d);}next(5,now,100);break;
   case 5:ui_.pad(12,0x10,3000);next(6,now,300);break;
   case 6:result("local-on-sounds",ui_.visual().master_peak>.001f);{auto d=ui_.device_settings();d.local=false;preferences(d);}next(7,now,800);break;
   case 7:result("local-toggle-releases",ui_.visual().master_peak<.0001f);ui_.pad(12,0x20,0);{Command c{CommandType::MidiInput,0,69,0x90};c.scalar=100;engine_.submit(c);}next(8,now,300);break;
   case 8:result("local-off-midi-input",ui_.visual().master_peak>.001f);engine_.submit({CommandType::MidiInput,0,69,0x80});{auto d=device_;d.input_db=0;d.local=true;preferences(d);}index_=0;next(10,now,1000);break;
   case 10:{unsigned theme=index_/19,mode=index_%19;ui_.set_mode(UiMode::Settings);ui_.soft_key(0);int delta=int(theme)-int(ui_.visual_settings().theme);if(delta)ui_.knob(0,delta*1000);ui_.set_mode(UiMode(mode));ui_.dispatch({ControllerInput::Type::KnobTouch,0,1});result("overlay-open",ui_.knob_overlay());next(11,now,160);}break;
   case 11:ui_.dispatch({ControllerInput::Type::KnobTouch,0,0});if(++index_<114)next(10,now,160);else next(12,now,1300);break;
   case 12:ui_.animate(now);{unsigned touched=0;for(unsigned i=0;i<8;++i)if(ui_.knob_touched(i))touched|=1u<<i;CLogger::Get()->Write("final",LogNotice,"Final overlay clock-ms %u touched %u editing %u",unsigned(ui_.ui_time()),touched,unsigned(ui_.browser_editing()));}result("overlay-expiry",!ui_.knob_overlay());ui_.set_mode(UiMode::Settings);ui_.soft_key(6);next(13,now,6000);break;
   case 13:result("data-free-space",ui_.storage_valid() && ui_.storage_free()>128ull*1024*1024);next(20,now);break;
   case 20:if(sf2_.service(fs_)){CLogger::Get()->Write("final",LogNotice,"Final generated SF2 bytes %u",unsigned(fs_.size("GROOVE/LOADTEST.SF2")));result("large-sf2-generated",true);next(30,now);}else if(sf2_.failed()){result("large-sf2-generated",false);next(30,now);}break;
   case 30:{auto* p=alloc_new<ProjectState>();if(!p){result("record-project",false);next(90,now);break;}make_empty_project(*p);p->tempo=120;p->routing.monitor=0;p->audio[0].kind=TrackKind::Looper;p->audio[0].length=LoopLength::Free;p->audio[0].source=1;bool ok=mount(*p);delete p;if(!ok)return;ui_.select_track(0);ui_.set_mode(UiMode::Perform);next(32,now,200);}break;
   case 32:if(ui_.visual().generation!=generation_)return;{bool ok=ui_.play() && ui_.request_file(FileAction::AudioRecord);result("long-record-start",ok);tone_=0;next(ok?31:90,now);}break;
   case 31:if(!record_done_)return;result("free-five-minute-record",record_ok_);ui_.stop();engine_.submit({CommandType::DiagnosticTone,0,0,0});next(90,now,1000);break;
   case 90:if(files_.recording_busy())return;if(!engine_.stage_project(*original_,bindings_))return;project_=*original_;files_.set_bindings(bindings_);ui_.project_loaded();ui_.select_track(selected_);ui_.set_mode(mode_);generation_=engine_.staged_generation();preferences(device_);delete original_;original_=nullptr;next(91,now,1000);break;
   case 91:if(ui_.visual().generation!=generation_ || !ui_.device_saved())return;result("take-cleanup",!take_[0] || fs_.remove(take_));CLogger::Get()->Write("final",LogNotice,"Final device completed passed %u failed %u restored 1 elapsed-ms %u",passed_,failed_,unsigned(now-started_));stage_=0;break;
  }
 }
};
}
