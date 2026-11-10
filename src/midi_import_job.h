#pragma once
#include "engine.h"
#include "midi_import.h"
#include "browser_state.h"
#include "sequence_commit.h"
namespace groove {
enum class MidiImportStage:uint8_t {Idle,Queued,Reading,WaitingCommit,Applied,Cancelled,Failed};
struct MidiImportOutcome {uint32_t ticket=0;MidiImportStage stage=MidiImportStage::Idle;unsigned notes=0,skipped=0;char message[96]{};};
// File-task owner. UI selection is not part of an accepted operation's identity.
// Each call does one bounded phase; cancellation is drained before service().
class MidiImportJob {
 IFileAccess& fs_;Engine& engine_;ProjectState& project_;
 MidiImport* parser_=nullptr;MidiImportRequest request_{};MidiImportOutcome result_{};
 uint32_t next_ticket_=0;bool result_ready_=false;
 void finish(MidiImportStage stage,const char* message){result_.stage=stage;if(parser_){result_.notes=parser_->notes();result_.skipped=parser_->skipped();}copy_text(result_.message,sizeof result_.message,message);delete parser_;parser_=nullptr;result_ready_=true;}
 template<class P> static auto invoke(P& p,const MidiImport& m,int)->decltype(p(m)){return p(m);}
 template<class P> static auto invoke(P& p,const MidiImport& m,long)->decltype(p(m.pattern(),m.track(),m.result(),m.cc_lane_mask(),m.nrpn_slot_mask())){if(m.spill_mask())return SequenceCommitResult::Invalid;return p(m.pattern(),m.track(),m.result(),m.cc_lane_mask(),m.nrpn_slot_mask());}
public:
 MidiImportJob(IFileAccess& fs,Engine& engine,ProjectState& project):fs_(fs),engine_(engine),project_(project){}
 ~MidiImportJob(){delete parser_;}
 MidiImportJob(const MidiImportJob&)=delete;MidiImportJob& operator=(const MidiImportJob&)=delete;
 bool busy()const{return result_.stage==MidiImportStage::Queued || result_.stage==MidiImportStage::Reading || result_.stage==MidiImportStage::WaitingCommit;}
 uint32_t ticket()const{return result_.ticket;}MidiImportStage stage()const{return result_.stage;}
 uint32_t accept(const MidiImportRequest& request){
  if(busy() || result_ready_)return 0;
  request_=request;result_={};if(!++next_ticket_)++next_ticket_;result_.ticket=next_ticket_;
  if(request.pattern>=kPatterns || request.track>=kTracks || request.channel>15 || !request.path[0] || project_.audio[request.track].kind!=TrackKind::Instrument){finish(MidiImportStage::Failed,"Select an instrument track for MIDI import");return ticket();}
  if(request.generation!=engine_.staged_generation() || project_.tracks[request.track].midi_channel!=request.channel){finish(MidiImportStage::Cancelled,"MIDI import cancelled / target changed");return ticket();}
  parser_=alloc_new<MidiImport>(fs_);
  if(!parser_){finish(MidiImportStage::Failed,"MIDI import out of memory");return ticket();}
  if(!parser_->prepare_target(project_,request.pattern,request.track)){finish(MidiImportStage::Failed,"MIDI import target unavailable");return ticket();}
  result_.stage=MidiImportStage::Queued;return ticket();
 }
 bool cancel(uint32_t expected){if(!busy() || !expected || expected!=ticket())return false;finish(MidiImportStage::Cancelled,"MIDI import cancelled / original kept");return true;}
 bool take_result(MidiImportOutcome& out){if(!result_ready_)return false;result_ready_=false;out=result_;return true;}
 template<class Publish> void service(Publish publish){
  if(!busy())return;
  if(request_.generation!=engine_.staged_generation() || !parser_->target_unchanged(project_)){finish(MidiImportStage::Cancelled,"MIDI import cancelled / target changed");return;}
  if(result_.stage==MidiImportStage::Queued){if(!parser_->open(request_.path)){finish(MidiImportStage::Failed,parser_->message());return;}result_.stage=MidiImportStage::Reading;return;}
  if(parser_->busy()){parser_->service();if(!parser_->busy()){if(!parser_->done())finish(MidiImportStage::Failed,parser_->message());else result_.stage=MidiImportStage::WaitingCommit;}return;}
  auto applied=invoke(publish,*parser_,0);
  switch(applied){
   case SequenceCommitResult::Busy:result_.stage=MidiImportStage::WaitingCommit;return;
   case SequenceCommitResult::Applied:finish(MidiImportStage::Applied,parser_->message());return;
   case SequenceCommitResult::NoMemory:finish(MidiImportStage::Failed,"MIDI import out of memory / original kept");return;
   case SequenceCommitResult::SyncLost:finish(MidiImportStage::Cancelled,"MIDI import cancelled / project sync lost");return;
   case SequenceCommitResult::Invalid:finish(MidiImportStage::Failed,"MIDI sequence invalid / original kept");return;
  }
 }
};
}
