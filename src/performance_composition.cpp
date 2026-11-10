#include "performance.h"
#include "composition_operations.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("Os")
#endif
namespace groove {
auto Performance::composition_notice(const char* message,bool error) -> bool{copy_text(composition_message_,sizeof composition_message_,message);notify(error?UiNotice::CompositionFailed:UiNotice::CompositionChanged);dirty_=true;return !error;}
auto Performance::paste_pattern() -> bool{
 if(!pattern_copied_)return composition_notice("COPY A PHRASE OR PATTERN FIRST",true);
 if(clipboard_track_only_){if(!phrase_clipboard_)return false;return commit_composition_phrase(clipboard_track_,*phrase_clipboard_);}
 auto* after=alloc_new<PatternState>(*pattern_clipboard_);if(!after)return memory_refused();return commit_pattern(after);
}
auto Performance::commit_composition_phrase(unsigned track_index,const PhraseState& phrase) -> bool{
 if(track_index>=kTracks || !allocation_admitted(sizeof(TrackHistory),2))return memory_refused();
 auto* before=alloc_new<TrackHistory>(track_at(project_,project_.active_pattern,track_index));auto* after=before?alloc_new<TrackHistory>(*before):nullptr;if(!after){delete before;return memory_refused();}
 before->phrase_only=after->phrase_only=true;copy_phrase_fields(*after,phrase);
 unsigned pattern=project_.active_pattern;if(!engine_.replace_track_phrase(pattern,track_index,*after)){delete before;delete after;return composition_notice("PHRASE EDIT BUSY / ORIGINAL KEPT",true);}
 Command c{CommandType::ApplyPattern,uint8_t(track_index),1,uint16_t(pattern)};c.parameter=uint16_t(1u<<track_index);push_history(c,c,nullptr,nullptr,before,after);install_track_phrase(track_views_.at(pattern,track_index),*after);clear_record_ids(pattern,c.parameter);refresh_steps();return true;
}
auto Performance::duplicate_variation() -> bool{
 if(!variation_valid(variation_))return composition_notice("SELECT THIN / +/-1-12 ST / DROP TRACK",true);
 unsigned from=project_.active_pattern,target=kPatterns;for(unsigned offset=1;offset<kPatterns;++offset){unsigned i=(from+offset)%kPatterns;bool live=false;for(const auto& clip:project_.clips)live|=clip.playing && clip.pattern==i;if(!live && pattern_is_empty(i)){target=i;break;}}
 if(target>=kPatterns)return composition_notice("NO UNUSED EMPTY PATTERN / SOURCE KEPT",true);
 if(!allocation_admitted(sizeof(PatternState),2))return memory_refused();
 auto* before=alloc_new<PatternState>(project_,target);auto* after=before?alloc_new<PatternState>():nullptr;if(!after){delete before;return memory_refused();}
 vary_pattern(*after,project_,from,variation_);
 if(variation_.kind==PatternVariation::DropTrack){auto* empty=alloc_new<PhraseState>();if(!empty){delete before;delete after;return memory_refused();}copy_phrase_fields(after->tracks[variation_.drop_track],*empty);delete empty;}
 if(!engine_.replace_pattern(target,*after,0xffff,true)){delete before;delete after;return composition_notice("PATTERN EDIT BUSY / SOURCE KEPT",true);}
 Command c{CommandType::ApplyPattern,0,1,uint16_t(target)};push_history(c,c,before,after);install_pattern(project_,target,*after);select_pattern(target);return composition_notice("VARIATION COPIED / SOURCE KEPT");
}
auto Performance::begin_variation() -> bool{if(modal_active() || file_action_pending())return false;composition_modal_=1;dirty_=true;return true;}
auto Performance::composition_apply() -> bool{if(!composition_modal_)return false;bool ok=composition_modal_==1?duplicate_variation():duplicate_scene(composition_scene_source_,composition_scene_target_);if(ok)composition_modal_=0;return ok;}
auto Performance::composition_knob(unsigned index,int delta) -> bool{int change=delta/1000;if(!change && delta)change=delta>0?1:-1;if(!change)return false;
 if(composition_modal_==2){if(index!=0)return false;composition_scene_target_=uint8_t(bounded(int(composition_scene_target_)+change,0,15));dirty_=true;return true;}
 if(index==0)variation_.kind=PatternVariation(bounded(int(variation_.kind)+change,0,2));else if(index==1){if(variation_.kind==PatternVariation::Thin)variation_.density=uint8_t(bounded(int(variation_.density)+change,0,100));else if(variation_.kind==PatternVariation::Transpose){int value=bounded(int(variation_.transpose)+change,-12,12);if(!value)value=change>0?1:-1;variation_.transpose=int8_t(value);}else variation_.drop_track=uint8_t(bounded(int(variation_.drop_track)+change,0,15));}else return false;dirty_=true;return true;
}
auto Performance::harmony_generate() -> bool{
 unsigned target=selected_;if(harmony_source_==target)return composition_notice("HARMONY: CHOOSE DIFFERENT SOURCE / TARGET",true);
 if(project_.audio[harmony_source_].kind!=TrackKind::Instrument || project_.audio[target].kind!=TrackKind::Instrument || project_.tracks[harmony_source_].instrument==Instrument::DrumKit || project_.tracks[target].instrument==Instrument::DrumKit)return composition_notice("HARMONY NEEDS TWO MELODIC TRACKS",true);
 auto* next=alloc_new<TrackState>(track_const(target));if(!next)return memory_refused();bool ok=make_harmony(*next,track_const(harmony_source_),track_const(target),harmony_mode_);
 if(!ok){delete next;return composition_notice("SOURCE HAS NO CHORD STEPS / CHECK TARGET RANGE",true);}auto* phrase=alloc_new<PhraseState>();if(!phrase){delete next;return memory_refused();}copy_phrase_fields(*phrase,*next);delete next;ok=commit_composition_phrase(target,*phrase);delete phrase;if(ok)composition_notice("HARMONY WRITTEN / ONE UNDO");return ok;
}
auto Performance::commit_composition_pattern(PatternState* after) -> bool{
 auto* before=alloc_new<PatternState>(project_,project_.active_pattern);if(!before){delete after;return memory_refused();}
 before->include_settings=after->include_settings;before->groove_only=after->groove_only;
 if(!engine_.replace_pattern(project_.active_pattern,*after,0xffff,true)){delete before;delete after;return composition_notice("PATTERN EDIT BUSY / ORIGINAL KEPT",true);}
 Command c{CommandType::ApplyPattern,0,1,project_.active_pattern};push_history(c,c,before,after);install_pattern(project_,project_.active_pattern,*after);refresh_steps();return true;
}
auto Performance::apply_groove_all() -> bool{
 if(!allocation_admitted(sizeof(PatternState),2))return memory_refused();
 auto* after=alloc_new<PatternState>(project_,project_.active_pattern);if(!after)return memory_refused();
 after->groove_only=true;
 GrooveState groove=groove_copied_?groove_clipboard_:track_const(selected_).groove;bool limited=groove_copied_ && paste_groove(groove_clip_timing_,project_.patterns[project_.active_pattern].swing,groove);for(auto& track:after->tracks)track.groove=groove;
 bool ok=commit_composition_pattern(after);if(ok)composition_notice(limited?"GROOVE APPLIED ALL / TIMING LIMITED":"GROOVE APPLIED TO ALL 16 TRACKS");return ok;
}

auto Performance::song_skeleton() -> bool{
 if(project_.song.length)return composition_notice("SONG HAS SECTIONS / SKELETON NEEDS EMPTY SONG",true);
 if(scene_take_requested_)return composition_notice("END ARRANGEMENT TAKE FIRST",true);
 SongState next=project_.song;next.length=5;next.enabled=next.loop=true;const unsigned bars[]={4,8,8,8,4};for(unsigned i=0;i<5;++i){next.sections[i]=SongSection{};next.sections[i].scene=uint8_t(i);next.sections[i].bars=uint16_t(bars[i]);}
 if(!song_send(next))return composition_notice("SONG EDIT BUSY / ORIGINAL KEPT",true);
 song_cursor_=0;return composition_notice("SKELETON: 4 / 8 / 8 / 8 / 4 BARS");
}
auto Performance::scene_empty(unsigned slot)const -> bool{
 if(slot>=kScenes || slot==project_.active_scene || int(slot)==queued_scene_)return false;
 const auto& s=project_.scenes[slot];if(s.mute_mask)return false;for(auto pattern:s.pattern_for_track)if(pattern)return false;
 for(unsigned i=0;i<project_.chain.length;++i)if(project_.chain.scenes[i]==slot)return false;
 for(unsigned i=0;i<project_.song.length;++i)if(!project_.song.sections[i].ticks && project_.song.sections[i].scene==slot)return false;
 return true;
}
auto Performance::begin_scene_duplicate() -> bool{
 if(modal_active() || file_action_pending())return false;
 composition_scene_source_=scene_slot_;unsigned target=kScenes;for(unsigned i=1;i<kScenes;++i){unsigned slot=(scene_slot_+i)%kScenes;if(scene_empty(slot)){target=slot;break;}}
 if(target==kScenes)return composition_notice("NO EMPTY UNREFERENCED SCENE",true);
 composition_scene_target_=uint8_t(target);composition_modal_=2;dirty_=true;return true;
}
auto Performance::duplicate_scene(unsigned source,unsigned target) -> bool{
 if(source>=kScenes || source==target || !scene_empty(target))return composition_notice("DEST SCENE OCCUPIED / LIVE OR REFERENCED",true);
 Command after{CommandType::SetScene,0,0,uint16_t(target)},before=after;after.scene=project_.scenes[source];before.scene=project_.scenes[target];if(!edit(after,before))return composition_notice("SCENE EDIT BUSY / ORIGINAL KEPT",true);
 scene_slot_=uint8_t(target);return composition_notice("SCENE COPIED / NOT LAUNCHED / ONE UNDO");
}
}
