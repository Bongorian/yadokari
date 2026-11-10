#pragma once
#include "browser_file_ops.h"
#include "ui/control_descriptor.h"
namespace groove {
// Context layers above the page providers: while one is up it replaces the
// page's top row wholesale. The browser edit layer keeps its own key
// handler (browser_edit_key); this provider owns its strip descriptors.
enum class RepeatAction:uint8_t {None,Rate};
using RepeatControl=ui::ControlDescriptor<RepeatAction>;
inline RepeatControl repeat_button_control(unsigned index,unsigned rate){
 static const char* rates[]={"1/4","1/8","1/16","1/32","1/64","1/8T","1/16T","1/32T"};
 if(index>=8)return {};
 RepeatControl d{rates[index],RepeatAction::Rate,true};d.selected=index==rate;return d;
}
enum class BrowserEditAction:uint8_t {None,Apply,Cancel,CharPrev,CharNext,Insert,Erase,Left,Right};
constexpr uint8_t icon_for(BrowserEditAction action){switch(action){case BrowserEditAction::Apply:return ui::icons::Confirm;case BrowserEditAction::Cancel:return ui::icons::Cancel;case BrowserEditAction::Erase:return ui::icons::Erase;default:return ui::icons::None;}}

using BrowserEditControl=ui::ControlDescriptor<BrowserEditAction>;
struct BrowserEditContext {
 BrowserOperation operation=BrowserOperation::None;bool confirmation=false;
};
inline BrowserEditControl browser_edit_button_control(unsigned index,const BrowserEditContext& c){
 static const char* names[]={"APPLY","CANCEL","CHAR -","CHAR +","INSERT","ERASE","LEFT","RIGHT"};
 static const BrowserEditAction actions[]={BrowserEditAction::Apply,BrowserEditAction::Cancel,BrowserEditAction::CharPrev,BrowserEditAction::CharNext,BrowserEditAction::Insert,BrowserEditAction::Erase,BrowserEditAction::Left,BrowserEditAction::Right};
 if(index>=8)return {};
 if(c.confirmation && index>1)return {"",BrowserEditAction::None,false};
 const char* label=names[index];
 if(index==0 && c.operation==BrowserOperation::Delete)label="DELETE";
 if(index==0 && c.operation==BrowserOperation::OverwritePreset)label="OVERWRITE";
 return {label,actions[index],true};
}
}
