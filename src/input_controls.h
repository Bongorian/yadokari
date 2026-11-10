#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class InputAction:uint8_t {None,ScalePrev,ScaleNext,OctaveDown,OctaveUp,ShapePrev,ShapeNext,InvertPrev,InvertNext,Sounds,ChordEdit,PreviousSound,NextSound,Browser,Undo,Redo};
constexpr ui::TopWork top_work(InputAction a){switch(a){case InputAction::None:return ui::TopWork::Empty;case InputAction::ScalePrev:case InputAction::ScaleNext:case InputAction::OctaveDown:case InputAction::OctaveUp:case InputAction::ShapePrev:case InputAction::ShapeNext:case InputAction::InvertPrev:case InputAction::InvertNext:return ui::TopWork::Selection;case InputAction::Sounds:case InputAction::ChordEdit:case InputAction::Browser:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(InputAction a){return a==InputAction::Undo?ui::icons::Undo:a==InputAction::Redo?ui::icons::Redo:ui::icons::None;}
inline ui::ControlDescriptor<InputAction> input_button(unsigned i,bool chord,bool shift,bool undo,bool redo,bool kit=true){
 using A=InputAction;using D=ui::ControlDescriptor<A>;
 if(i<4){if(chord && !shift){static const char* labels[]={"SHAPE -","SHAPE +","INVERT -","INVERT +"};static const A actions[]={A::ShapePrev,A::ShapeNext,A::InvertPrev,A::InvertNext};return {labels[i],actions[i],true};}static const char* labels[]={"SCALE -","SCALE +","OCTAVE -","OCTAVE +"};static const A actions[]={A::ScalePrev,A::ScaleNext,A::OctaveDown,A::OctaveUp};return {labels[i],actions[i],true};}
 if(i==4)return {shift?(kit?"PREV KIT":"PREV PRESET"):(kit?"KITS":"PRESETS"),shift?A::PreviousSound:A::Sounds,true};
 if(i==5)return {shift?(kit?"NEXT KIT":"NEXT PRESET"):"CHORD EDIT",shift?A::NextSound:A::ChordEdit,true};
 if(i==6)return {"UNDO",A::Undo,undo};
 if(i==7)return {"REDO",A::Redo,redo};
 return D{};
}
}
