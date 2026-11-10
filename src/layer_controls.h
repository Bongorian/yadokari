#pragma once
#include "project.h"
namespace groove {
inline constexpr const char* kLayerEditPages[]={"MIX","RANGE","FX 1","FX 2","SENDS"};
struct LayerKnobControl {const char* label="";float low=0,high=1,step=.02f;};
inline LayerKnobControl layer_knob_control(unsigned page,unsigned knob,const LayerPart* part){
 if(knob<3 || knob>6)return {};
 if(page==0){if(knob==3)return {"LEVEL",0,1,.02f};if(knob==4)return {"PAN",-1,1,.02f};if(knob==5)return {"DETUNE ct",-1200,1200,1};return {};}
 if(page==1){static const char* names[]={"KEY LOW","KEY HIGH","VEL LOW","VEL HIGH"};return {names[knob-3],knob<5?0.f:1.f,127,1};}
 if(page==2 || page==3){if(knob==3)return {"TYPE",0,float(kFxCount-1),1};if(knob==6)return {"WET",0,1,.02f};unsigned type=part?unsigned(part->fx.inserts[(page-2)*4]):0;const auto* fx=fx_descriptor(type);const auto* control=fx?&fx->controls[knob-4]:nullptr;return {control?control->name:"",0,1,control && control->count>1?1.f/(control->count-1):.02f};}
 if(page==4){if(knob==3)return {"DELAY SEND",0,1,.02f};if(knob==4)return {"REVERB SEND",0,1,.02f};if(knob==5)return {"LEAF FX",0,1,1};}
 return {};
}
inline float layer_knob_value(unsigned page,unsigned knob,const LayerPart& p){
 if(page==0)return knob==3?p.level:knob==4?p.pan:knob==5?p.detune:0;
 if(page==1)return knob==3?p.key_low:knob==4?p.key_high:knob==5?p.velocity_low:knob==6?p.velocity_high:0;
 if((page==2 || page==3) && knob>=3 && knob<=6)return p.fx.inserts[(page-2)*4+knob-3];
 return page==4?(knob==3?p.fx.delay:knob==4?p.fx.reverb:knob==5?float(p.fx.present):0):0;
}
inline bool edit_layer_knob(unsigned page,unsigned knob,LayerPart& p,int change,bool shift){
 auto c=layer_knob_control(page,knob,&p);if(!*c.label)return false;
 float delta=change*c.step*(page==0 && knob==5 && shift?100:1);float value=fmaxf(c.low,fminf(c.high,layer_knob_value(page,knob,p)+delta));
 if(page==0){if(knob==3)p.level=value;if(knob==4)p.pan=value;if(knob==5)p.detune=value;}
 if(page==1){if(knob==3)p.key_low=uint8_t(fminf(value,p.key_high));if(knob==4)p.key_high=uint8_t(fmaxf(value,p.key_low));if(knob==5)p.velocity_low=uint8_t(fminf(value,p.velocity_high));if(knob==6)p.velocity_high=uint8_t(fmaxf(value,p.velocity_low));}
 if(page==2 || page==3){unsigned slot=page-2;p.fx.present=true;p.fx.inserts[slot*4+knob-3]=value;if(knob==3)p.fx.disabled&=uint8_t(~(1u<<slot));}
 if(page==4){if(knob==3){p.fx.delay=value;p.fx.present=true;}if(knob==4){p.fx.reverb=value;p.fx.present=true;}if(knob==5)p.fx.present=value>=.5f;}
 return true;
}
}
