#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class ModalAction:uint8_t {None,Accept,Cancel};
constexpr uint8_t icon_for(ModalAction action){switch(action){case ModalAction::Accept:return ui::icons::Confirm;case ModalAction::Cancel:return ui::icons::Cancel;default:return ui::icons::None;}}

inline ui::ControlDescriptor<ModalAction> modal_button(unsigned index,bool progress=false){if(!index)return {progress?"CANCEL":"CONFIRM",progress?ModalAction::Cancel:ModalAction::Accept,true};if(index==1 && !progress)return {"CANCEL",ModalAction::Cancel,true};return {};}
}
