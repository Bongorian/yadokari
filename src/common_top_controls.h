#pragma once
#include "ui/control_descriptor.h"
namespace groove {
enum class CommonTopAction:uint8_t {None,Source,Modulation,Store,Recall,Audio,MountSource,CloseSource,TrackName};
constexpr ui::TopWork top_work(CommonTopAction a){switch(a){case CommonTopAction::None:return ui::TopWork::Empty;case CommonTopAction::Source:case CommonTopAction::Modulation:case CommonTopAction::Audio:case CommonTopAction::CloseSource:return ui::TopWork::Exit;default:return ui::TopWork::Execute;}}
constexpr uint8_t icon_for(CommonTopAction action){switch(action){case CommonTopAction::Source:return ui::icons::Source;case CommonTopAction::Modulation:return ui::icons::Modulation;case CommonTopAction::Store:return ui::icons::Store;case CommonTopAction::Recall:return ui::icons::Recall;case CommonTopAction::Audio:return ui::icons::Audio;case CommonTopAction::MountSource:return ui::icons::Confirm;case CommonTopAction::CloseSource:return ui::icons::Back;default:return ui::icons::None;}}

inline ui::ControlDescriptor<CommonTopAction> common_top_control(unsigned index,bool synth,bool store_slot,bool recall_slot,bool can_store,bool can_recall,bool audio_door=false){
 if(synth && index==4)return {"ENGINE",CommonTopAction::Source,true};
 if(synth && index==5)return {"MODULATION",CommonTopAction::Modulation,true};
 if(index==6 && store_slot)return {"STORE POOL",CommonTopAction::Store,can_store};
 if(index==7 && audio_door)return {"AUDIO",CommonTopAction::Audio,true};
 if(index==7 && recall_slot)return {"RECALL POOL",CommonTopAction::Recall,can_recall};
 return {};
}
}
