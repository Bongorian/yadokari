// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "performance.h"
namespace groove {
auto Performance::mutation_commit() -> bool{const auto& current=track_const(selected_);if(!current.mutation.amount)return false;auto* before=new PatternState(project_,project_.active_pattern);auto* after=new PatternState(*before);auto& target=after->tracks[selected_];for(unsigned i=0;i<target.length;++i)target.steps[i]=mutation_step(current,i);target.mutation.amount=0;target.mutation.frozen=false;unsigned pattern=project_.active_pattern;uint16_t mask=uint16_t(1u<<selected_);if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();return true;}
auto Performance::finish_note_gate(const EngineEvent& e) -> void{
 if(e.track>=kTracks || e.flags>=kPatterns || e.parameter<1 || e.parameter>384 || !e.recording_id ||
    !matches_record(e.flags,e.track,e.value,e.recording_id))return;
 auto& step=track_views_.at(e.flags,e.track).steps[e.value];
 if(step.enabled && step.gate!=e.parameter){
  Command gate{CommandType::SetGate,e.track,e.value,e.parameter};gate.pattern=e.flags;
  // Do not move a release across a rotation or snapshot that stored the old gate.
  History* onset=nullptr;
  for(unsigned i=history_cursor_;i>0 && history_cursor_==history_size_;--i){
   auto& entry=history_[i-1];const auto& edit=entry.after;
   if((edit.type==CommandType::ApplyPattern && edit.value==e.flags) || entry.block)break;
   if(entry.note_id==e.recording_id && edit.track==e.track && edit.pattern==e.flags && edit.step==e.value){onset=&entry;break;}
  }
  if(onset){
   if(send(gate)){mirror(gate);onset->after.recorded_step.gate=e.parameter;++edit_serial_;dirty_=true;}
   else notify(UiNotice::RecordLost);
  }else{
   Command before=gate;before.value=step.gate;
   if(edit(gate,before))history_[history_cursor_-1].take=e.recording_take;else notify(UiNotice::RecordLost);
  }
 }
 forget_record(e.flags,e.track,e.value);
}
auto Performance::undo_target()const -> const char*{
 if(!history_cursor_)return "EMPTY";
 const auto& entry=history_[history_cursor_-1];
 if(entry.block)return "BLOCK";
 switch(entry.after.type){
 case CommandType::SetRecordedStep:return entry.after.recorded_step.enabled?"NOTE":"ERASE";
 case CommandType::SetKitStep:return "KIT HIT";
 case CommandType::SetGate:return "GATE";
 case CommandType::SetParameter:case CommandType::SetEngineParameter:return "PARAMETER";
 case CommandType::SetStepLock:case CommandType::ClearStepLock:return "LOCK";
 case CommandType::SetMute:return "MUTE";
 case CommandType::SetSolo:return "SOLO";
 case CommandType::ApplyPattern:return entry.track_before?"TRACK":"PATTERN";
 default:return "EDIT";
 }
}
auto Performance::take_bounds(unsigned index,unsigned& first,unsigned& end) -> bool{
 const uint32_t take=history_[index].take;
 if(note_recording_ || note_finish_pending_)return composition_notice("Finish recording first",true);
 if(!take || take==trimmed_take_ || history_[index].incomplete_take)return composition_notice("Full note take unavailable",true);
 first=index;end=index+1;
 while(first && history_[first-1].take==take)--first;
 while(end<history_size_ && history_[end].take==take)++end;
 for(unsigned i=0;i<history_size_;++i)
  if(history_[i].take==take && (history_[i].incomplete_take || i<first || i>=end))return composition_notice("Take contains other edits",true);
 return true;
}
auto Performance::undo(bool whole_take) -> bool{if(!history_cursor_)return false;unsigned end=history_cursor_,start=end-1;if(whole_take){unsigned limit;if(!take_bounds(start,start,limit))return false;}
 else while(start && history_[start-1].group==history_[end-1].group)--start;
 unsigned sounds=0,morphs=0;for(unsigned i=start;i<end;++i){if(Engine::carries_sound(history_[i].before.type))++sounds;if(history_[i].before.restore_morph)++morphs;}if(engine_.command_space()<end-start || engine_.sound_command_space()<sounds || engine_.morph_command_space()<morphs)return false;for(unsigned i=end;i>start;--i){auto& h=history_[i-1];if(!apply_history(h,false)){if(i!=end){refresh_steps();notify(UiNotice::PartialApplied);}return false;}history_cursor_=uint8_t(i-1);}history_cursor_=uint8_t(start);refresh_steps();dirty_=true;return true;}
auto Performance::redo(bool whole_take) -> bool{if(history_cursor_>=history_size_)return false;unsigned start=history_cursor_,end=start+1;if(whole_take){unsigned first;if(!take_bounds(start,first,end))return false;}
 else while(end<history_size_ && history_[end].group==history_[start].group)++end;
 unsigned sounds=0,morphs=0;for(unsigned i=start;i<end;++i){if(Engine::carries_sound(history_[i].after.type))++sounds;if(history_[i].after.restore_morph)++morphs;}if(engine_.command_space()<end-start || engine_.sound_command_space()<sounds || engine_.morph_command_space()<morphs)return false;for(unsigned i=start;i<end;++i)if(!apply_history(history_[i],true))return false;history_cursor_=uint8_t(end);refresh_steps();dirty_=true;return true;}
auto Performance::send(const Command& c)  -> bool{uint8_t pattern=c.pattern==255?(project_.audio[c.track].kind==TrackKind::Instrument?project_.active_pattern:0):c.pattern;if(!device_.local && c.type==CommandType::Trigger && c.parameter>=2 && c.parameter<=4){Command out=c;out.type=CommandType::LocalPad;if(engine_.submit(out,pattern))return true;}else if(engine_.submit(c,pattern))return true;++dropped_;return false;}
auto Performance::commit_pattern(PatternState* after) -> bool{auto* before=new PatternState(project_,project_.active_pattern);if(!engine_.replace_pattern(project_.active_pattern,*after)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,0,0,project_.active_pattern};push_history(c,c,before,after);install_pattern(project_,project_.active_pattern,*after);refresh_steps();return true;}
auto Performance::push_history(Command before,Command after,PatternState* pbefore,PatternState* pafter,TrackHistory* tbefore,TrackHistory* tafter) -> void{++edit_serial_;if(after.type==CommandType::ApplyPattern){if((!pafter || !pafter->groove_only) && after.pattern_revision!=kRotationHistory)clear_record_ids(after.value,after.parameter?after.parameter:0xffff);if(pbefore && pafter && !pafter->groove_only)note_assignment_changes(after.value,*pbefore,*pafter,after.parameter?after.parameter:0xffff);}for(unsigned i=history_cursor_;i<history_size_;++i){
 const uint32_t discarded=history_[i].take;
 if(discarded)for(unsigned j=0;j<history_cursor_;++j)if(history_[j].take==discarded)history_[j].incomplete_take=true;
 dispose(history_[i]);
 }history_size_=history_cursor_;if(history_cursor_==64){
 if(history_trim_notice_==HistoryTrimNotice::Unused)history_trim_notice_=HistoryTrimNotice::Pending;
 unsigned drop=1;while(drop<64 && history_[drop].group==history_[0].group)++drop;for(unsigned i=0;i<drop;++i){if(history_[i].take)trimmed_take_=history_[i].take;dispose(history_[i]);}for(unsigned i=drop;i<64;++i)history_[i-drop]=history_[i];for(unsigned i=64-drop;i<64;++i)history_[i]=History{};history_cursor_=uint8_t(64-drop);}auto& h=history_[history_cursor_++];h={before,after,active_group_?active_group_:++next_group_,pbefore,pafter,tbefore,tafter};history_size_=history_cursor_;dirty_=true;}
auto Performance::apply_history(History& h,bool redo) -> bool{if(h.block)return install_block(*h.block,redo);const auto& c=redo?h.after:h.before;if(c.type==CommandType::ApplyPattern){if(h.track_before || h.track_after){const auto* state=redo?h.track_after:h.track_before;if(!state)return false;/* Scope is independent of c.step, which preserves playback phase for other edits too. */if(state->phrase_only){if(!engine_.replace_track_phrase(c.value,c.track,*state))return false;install_track_phrase(track_views_.at(c.value,c.track),*state);clear_record_ids(c.value,uint16_t(1u<<c.track));return true;}if(!engine_.replace_track(c.value,c.track,*state,c.step==1,nullptr,nullptr,nullptr,&state->morph,&state->install))return false;install_single_track(c.value,c.track,*state,c.frames!=0);mirror_track_install(state->install,c.track);restore_track_morph(project_,c.track,state->morph);return true;}const auto* pattern=redo?h.pattern_after:h.pattern_before;uint16_t mask=c.parameter?c.parameter:0xffff;bool rotation=pattern && c.pattern_revision==kRotationHistory && (c.scalar==1 || c.scalar==-1) && mask && !(mask&(mask-1)) && c.scalar2>=1 && c.scalar2<=kSteps;unsigned track_index=rotation?unsigned(__builtin_ctz(unsigned(mask))):0,length=rotation?unsigned(c.scalar2):0;if(rotation && (track_views_.at(c.value,track_index).length!=length || pattern->tracks[track_index].length!=length))rotation=false;uint32_t previous=rotation?engine_.pattern_revision(c.value,track_index):0;if(!pattern || !engine_.replace_pattern(c.value,*pattern,mask,c.step==1))return false;if(rotation)remember_rotation(c.value,track_index,previous,engine_.pattern_revision(c.value,track_index),length,redo?int(c.scalar):-int(c.scalar));else if(!pattern->groove_only)clear_record_ids(c.value,mask);if(!pattern->groove_only)note_assignment_changes(c.value,*pattern,mask);if(c.frames)for(unsigned t=0;t<kTracks;++t)if((mask&(1u<<t)) && track_views_.at(c.value,t).instrument==pattern->tracks[t].instrument && track_views_.at(c.value,t).sample_slot==pattern->tracks[t].sample_slot)++assignment_revision_[t];install_pattern(project_,c.value,*pattern,mask);return true;}if(!send(c))return false;mirror(c);return true;}
}
