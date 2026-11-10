#pragma once
#include "composition_operations.h"
#include "ui/control_descriptor.h"
namespace groove {
enum class CompositionAction:uint8_t {None,Kind,Amount,Source,Target,Harmony,CopyScope,Destination};
using CompositionControl=ui::ControlDescriptor<CompositionAction>;
inline CompositionControl variation_knob(unsigned i,const VariationOptions& o){
 if(i==0)return {"VARIATION",CompositionAction::Kind,true,0,2};
 if(i!=1)return {};
 if(o.kind==PatternVariation::Thin)return {"DENSITY",CompositionAction::Amount,true,0,100};
 if(o.kind==PatternVariation::Transpose)return {"TRANSPOSE",CompositionAction::Amount,true,-12,12};
 return {"DROP TRACK",CompositionAction::Amount,true,1,16};
}
inline CompositionControl harmony_knob(unsigned i){
 if(i==0)return {"SOURCE TRACK",CompositionAction::Source,true,1,16};
 if(i==1)return {"TARGET TRACK",CompositionAction::Target,true,1,16};
 if(i==2)return {"MODE",CompositionAction::Harmony,true,0,1};
 return {};
}
inline CompositionControl scene_destination_knob(unsigned i){return i==0?CompositionControl{"DEST SCENE",CompositionAction::Destination,true,1,16}:CompositionControl{};}
inline CompositionControl copy_scope_knob(){return {"COPY SCOPE",CompositionAction::CopyScope,true,0,1};}
}
