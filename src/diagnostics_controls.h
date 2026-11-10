#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class DiagnosticsAction:uint8_t {None,Test,Tone,Exit};
constexpr ui::TopWork top_work(DiagnosticsAction a){switch(a){case DiagnosticsAction::None:return ui::TopWork::Empty;case DiagnosticsAction::Exit:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(DiagnosticsAction action){switch(action){case DiagnosticsAction::Exit:return ui::icons::Back;default:return ui::icons::None;}}

using DiagnosticsControl=ui::ControlDescriptor<DiagnosticsAction>;
inline DiagnosticsControl diagnostics_button(unsigned index){
 if(index<4 || index>=8)return {};
 const char* names[]={"LED TEST","LCD TEST","TONE TEST","EXIT"};
 return {names[index-4],index==7?DiagnosticsAction::Exit:index==6?DiagnosticsAction::Tone:DiagnosticsAction::Test,true};
}
}
