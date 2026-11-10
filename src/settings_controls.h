#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class SettingsAction:uint8_t {None,Page,More,ConfirmUpdate,CancelUpdate,Routing,Midi,Diagnostics,Update,Transfer,CloseTransfer,TransferAP,TransferStation,StartTransfer,BrowseTransfer};
constexpr ui::TopWork top_work(SettingsAction a){switch(a){case SettingsAction::None:return ui::TopWork::Empty;case SettingsAction::Page:case SettingsAction::More:case SettingsAction::TransferAP:case SettingsAction::TransferStation:return ui::TopWork::Selection;case SettingsAction::Routing:case SettingsAction::Midi:case SettingsAction::Diagnostics:case SettingsAction::CloseTransfer:case SettingsAction::BrowseTransfer:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(SettingsAction action){switch(action){case SettingsAction::ConfirmUpdate:return ui::icons::Confirm;case SettingsAction::CancelUpdate:return ui::icons::Cancel;default:return ui::icons::None;}}

using SettingsControl=ui::ControlDescriptor<SettingsAction>;
inline SettingsControl settings_button(unsigned index,bool update_confirm,unsigned settings_page){
 if(update_confirm){if(index==0)return {"CONFIRM",SettingsAction::ConfirmUpdate,true};if(index==1)return {"CANCEL",SettingsAction::CancelUpdate,true};return {};}
 static const char* names[]={"APPEARANCE","DISPLAY","LIGHTS","PAD","AUDIO","MIDI","SYSTEM"};
 static constexpr uint8_t symbols[]={ui::icons::Appearance,ui::icons::Display,ui::icons::Lights,ui::icons::Pad,ui::icons::Audio,ui::icons::Midi,ui::icons::System};
 unsigned bank=settings_page/3*3;
 if(index<3){unsigned page=bank+index;if(page>=7)return {};SettingsControl d{names[page],SettingsAction::Page,true,int(page),int(page)};d.selected=page==settings_page;d.icon=symbols[page];return d;}
 if(index==3)return {"MORE",SettingsAction::More,true};
 if(index==4 && settings_page==6)return {"FILE TRANSFER",SettingsAction::Transfer,true};
 if(index==4)return settings_page==4?SettingsControl{"ROUTING",SettingsAction::Routing,true}:SettingsControl{};
 if(index==5)return settings_page==5?SettingsControl{"MIDI ROUTES",SettingsAction::Midi,true}:SettingsControl{};
 if(index==6)return settings_page==6?SettingsControl{"DIAGNOSTICS",SettingsAction::Diagnostics,true}:SettingsControl{};
 if(index==7)return settings_page==6?SettingsControl{"UPDATE",SettingsAction::Update,true}:SettingsControl{};
 return {};
}
}
