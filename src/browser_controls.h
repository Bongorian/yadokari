#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class BrowserAction:uint8_t {None,Open,Up,Preview,StopPreview,Load,MidiImport,Favorite,Duplicate,ToggleFavorites,SaveKit,Project,Factory,Rename,Delete,NewFolder};
constexpr ui::TopWork top_work(BrowserAction a){switch(a){case BrowserAction::None:return ui::TopWork::Empty;case BrowserAction::Project:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(BrowserAction action){switch(action){case BrowserAction::Open:return ui::icons::Open;case BrowserAction::Preview:return ui::icons::Play;case BrowserAction::StopPreview:return ui::icons::Stop;case BrowserAction::MidiImport:return ui::icons::Midi;case BrowserAction::Duplicate:return ui::icons::Copy;case BrowserAction::Project:return ui::icons::Open;case BrowserAction::Delete:return ui::icons::Erase;case BrowserAction::NewFolder:return ui::icons::New;default:return ui::icons::None;}}

using BrowserControl=ui::ControlDescriptor<BrowserAction>;
struct BrowserContext {
 bool shift=false,midi=false,previewing=false,other_preview=false,part=false,midi_file=false,new_project=false;
};
inline BrowserControl browser_button_control(unsigned index,const BrowserContext& c){
 if(c.new_project){if(index==0)return {"OPEN",BrowserAction::Open,true};if(index==1)return {"CANCEL",BrowserAction::Project,true};return {};}
 switch(index){
 case 0:return c.shift?BrowserControl{"FAVORITES",BrowserAction::ToggleFavorites,true}:BrowserControl{"OPEN",BrowserAction::Open,true};
 case 1:return {"UP",BrowserAction::Up,true};
 case 2:{
  if(c.midi && !c.previewing)return {};
  BrowserControl d{c.previewing && !c.other_preview?"STOP PREVIEW":"PREVIEW",c.previewing && !c.other_preview?BrowserAction::StopPreview:BrowserAction::Preview,true};
  d.selected=c.previewing && !c.other_preview;return d;}
 case 3:{
  if(c.midi && !c.shift)return {"HOLD SHIFT",BrowserAction::None,false};
  return {c.shift && c.midi?"IMPORT MIDI":"LOAD",c.shift && c.midi_file?BrowserAction::MidiImport:BrowserAction::Load,true};}
 case 4:{
  if(c.midi && !c.shift)return {};
  return {c.shift?(c.part?"RECOVER":"DUPLICATE"):"FAVORITE",c.shift?BrowserAction::Duplicate:BrowserAction::Favorite,true};}
 case 5:return c.shift?BrowserControl{"RENAME",BrowserAction::Rename,true}:BrowserControl{"SAVE PRESET",BrowserAction::SaveKit,true};
 case 6:return c.shift?BrowserControl{"DELETE",BrowserAction::Delete,true}:BrowserControl{"PROJECT",BrowserAction::Project,true};
 case 7:return c.shift?BrowserControl{"NEW FOLDER",BrowserAction::NewFolder,true}:BrowserControl{"FACTORY KIT",BrowserAction::Factory,true};
 }
 return {};
}
}
