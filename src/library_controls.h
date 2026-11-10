#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class LibraryAction:uint8_t {None,Kind,Origin,Previous,Next,Preview,StopPreview,Load,Files,Back,StoreFile,SaveAs,Duplicate};
constexpr ui::TopWork top_work(LibraryAction a){using A=LibraryAction;switch(a){case A::None:return ui::TopWork::Empty;case A::Kind:case A::Origin:case A::Previous:case A::Next:return ui::TopWork::Selection;case A::Files:case A::Back:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(LibraryAction a){using A=LibraryAction;return a==A::Preview?ui::icons::Play:a==A::StopPreview?ui::icons::Stop:a==A::Back?ui::icons::Back:a==A::StoreFile?ui::icons::Store:a==A::Duplicate?ui::icons::Copy:ui::icons::None;}
inline ui::ControlDescriptor<LibraryAction> library_button(unsigned i,bool shift,bool kits,bool originals,bool preview,bool rows){
 using A=LibraryAction;if(shift){switch(i){case 4:return {"STORE FILE",A::StoreFile,true};case 5:return {"STORE AS",A::SaveAs,true};case 6:return {"DUPLICATE",A::Duplicate,true};default:return {};}}
 switch(i){case 0:return {kits?"KIND: KIT":"KIND: SOUND",A::Kind,true};case 1:return {originals?"ORIGINALS":"MY VERSIONS",A::Origin,true};case 2:return {"PREV",A::Previous,rows};case 3:return {"NEXT",A::Next,rows};case 4:return {preview?"STOP PREVIEW":"PREVIEW",preview?A::StopPreview:A::Preview,rows,0,1,1,preview};case 5:return {"LOAD",A::Load,rows};case 6:return {"FILES",A::Files,true};case 7:return {"BACK",A::Back,true};default:return {};}
}
}
