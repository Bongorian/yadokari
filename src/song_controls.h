#pragma once
#include "project.h"
#include "ui/control_descriptor.h"
namespace groove {
// Shared Song / captured Arrangement grammar. Labels, dispatch and limits
// come from the same descriptor on Desktop and Circle.
enum class SongAction:uint8_t { None,Add,Delete,Import,Clear,Record,Cancel,Section,Loop,Enable,Scene,Bars,Repeats,Transpose,Track,Ticks,Pattern,Skeleton };
constexpr uint8_t icon_for(SongAction action){switch(action){case SongAction::Delete:return ui::icons::Erase;case SongAction::Import:return ui::icons::Open;case SongAction::Clear:return ui::icons::Erase;case SongAction::Record:return ui::icons::Record;case SongAction::Cancel:return ui::icons::Cancel;default:return ui::icons::None;}}

using SongControl=ui::ControlDescriptor<SongAction>;
struct SongControlContext {
 unsigned length=0,chain_length=0,cursor=0;
 bool captured=false,recording=false,playing=false,counting_in=false,import_armed=false,clear_armed=false;
};
inline SongControl song_button_control(unsigned index,const SongControlContext& c){
 switch(index){
  case 0:return {"ADD",SongAction::Add,!c.recording && c.length<32};
  case 1:return {"DELETE",SongAction::Delete,!c.recording && c.cursor<c.length};
  case 2:return {c.import_armed?"SURE?":"IMPORT",SongAction::Import,!c.recording && !c.length && c.chain_length>0};
  case 3:return {c.clear_armed?"SURE?":"CLEAR",SongAction::Clear,!c.recording && c.length>0};
  case 4:return {c.recording?"END TAKE":"REC ARRANGE",SongAction::Record,c.recording || (!c.playing && !c.counting_in)};
  case 5:return c.recording?SongControl{"CANCEL TAKE",SongAction::Cancel,true}:SongControl{"SKELETON",SongAction::Skeleton,true};
  default:return {};
 }
}
inline SongControl song_knob_control(unsigned index,const SongControlContext& c){
 unsigned count=c.length?c.length:c.chain_length;
 bool editable=!c.recording && c.cursor<c.length;
 switch(index){
  case 0:return {"SECTION",SongAction::Section,count>0,1,int(count?count:1)};
  case 1:return {"LOOP",SongAction::Loop,!c.recording,0,1};
  case 2:return {"ENABLE",SongAction::Enable,!c.recording,0,1};
  case 4:return c.captured?SongControl{"TRACK",SongAction::Track,c.cursor<c.length,1,16}:SongControl{"SCENE",SongAction::Scene,editable,1,16};
  case 5:return c.captured?SongControl{"TICKS",SongAction::Ticks,editable,1,int(kSongMaxTicks),int(kSongTicksPerBeat)}:SongControl{"BARS",SongAction::Bars,editable,1,1024};
  case 6:return c.captured?SongControl{"PATTERN",SongAction::Pattern,editable,1,16}:SongControl{"REPEATS",SongAction::Repeats,editable,1,64};
  case 7:return {"TRANSPOSE",SongAction::Transpose,editable,-24,24};
  default:return {};
 }
}
}
