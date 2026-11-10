#pragma once
#include "../software_canvas.h"
#include "pad_feedback.h"
#include "content.h"
#include "meter_colors.h"
#include "../visual_model.h"
namespace groove { namespace ui {
class Widgets:public SoftwareCanvas {
public:
 // Primary body headings share one native cap centre; dense editing matrices
 // keep their cell geometry. Header/title bands remain under the shared header.
 void label(unsigned side,int x,int y,const char* text,uint32_t color,FontRole role=FontRole::Body,int w=460,bool numeric=false);
 void centered_label(unsigned side,int x,int y,int w,int h,const char* text,uint32_t color,FontRole role=FontRole::Small){aligned_band(side,x,y,w,h,text,color,role,TextAlign::Center);}

 void selected_mark(unsigned side,int x,int y,int w,int h){fill(side,x+2,y+h-2,w-4,2,theme.accent);}
 void panel(unsigned side,int x,int y,int w,int h,bool selected=false,uint32_t=0){fill(side,x,y,w,h,selected?theme.raised:theme.surface);if(selected)selected_mark(side,x,y,w,h);}
 void badge(unsigned side,int x,int y,int w,const char* text,uint32_t color,bool active=true){fill(side,x,y,w,20,active?theme.raised:theme.surface);centered_label(side,x+7,y,w-14,20,text,active?color:theme.secondary);}
 void button(unsigned side,int x,int y,int w,const char* text,bool selected,bool enabled=true);
 void progress(unsigned side,int x,int y,int w,int h,float amount,uint32_t color){h=h>=6?6:h>=3?3:2;rounded(side,x,y,w,h,0,theme.raised);int filled=int(clamp01(amount)*w+0.5f);if(filled)rounded(side,x,y,filled,h,0,color);}
 void level_meter(unsigned side,int x,int y,int w,int h,float amount,uint32_t source){progress(side,x,y,w,h,amount,meter_level_color(theme,source,amount));}
 void pending_sweep(unsigned side,int x,int y,int w,int h,float phase,uint32_t color);
 void slider(unsigned side,int x,int y,int w,float amount,uint32_t color){progress(side,x,y,w,4,amount,color);int cursor=x+int(clamp01(amount)*(w-4));rounded(side,cursor,y-3,4,10,2,color);}
 void rotary(unsigned side,float x,float y,float r,float amount,uint32_t color);
 // SPEC 2.6 knob form: a 270-degree value arc (gap at the bottom),
 // 3px track in border, fill in the given colour from the minimum
 // (unipolar) or from the centre (bipolar, with a background detent
 // notch at the centre), min/max reference ticks in secondary, and
 // a white tip dot at the fill end. No dial outline, no needle.
 void arc270(unsigned side,int cx,int cy,int r,float amount,bool bipolar,uint32_t color);
 void meter(unsigned side,int x,int y,int w,int h,float level,uint32_t source,bool horizontal=false);
 void step_attributes(unsigned side,int x,int y,bool slide,bool accent,bool enabled,bool compact=false);
 void step_cell(unsigned side,int x,int y,int w,int h,unsigned step,bool enabled,bool playing,bool selected,bool locked,bool available,uint32_t color);
 void graph_grid(unsigned side,int x,int y,int w,int h);
 void polyline(unsigned side,int x,int y,int w,int h,const float* samples,unsigned count,uint32_t color,float gain=1);
 void envelope(unsigned side,int x,int y,int w,int h,float attack,float decay,float sustain,float release,uint32_t color);
 void parameter_popup(unsigned side,const char* name,float number,const char* unit,unsigned precision=0);
 void dialog(unsigned side,const char* title,const char* message,float amount);
#include "content_widgets.h"
#include "visual_widgets.h"
};
}}
