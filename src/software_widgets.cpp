// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "ui/widgets.h"
namespace groove::ui {
auto Widgets::label(unsigned side,int x,int y,const char* text,uint32_t color,FontRole role,int w,bool numeric) -> void{
  if(mapped_layout() && (x==24 || x==16 || x==14) && y>=82 && y<=88){aligned_band(side,24,82,w,18,text,color,role,TextAlign::Left,TextKind::Section,numeric,true);return;}
  SoftwareCanvas::label(side,x,y,text,color,role,w,numeric);
 }
auto Widgets::button(unsigned side,int x,int y,int w,const char* text,bool selected,bool enabled) -> void{fill(side,x,y,w,24,selected?theme.raised:theme.surface);if(selected)selected_mark(side,x,y,w,24);centered_label(side,x+7,y,w-14,24,text,enabled?(selected?theme.accent:theme.secondary):theme.disabled);}
auto Widgets::pending_sweep(unsigned side,int x,int y,int w,int h,float phase,uint32_t color) -> void{int perimeter=2*(w+h-2),head=int(clamp01(phase)*perimeter);for(int n=0;n<head;++n){int px,py;if(n<w){px=x+n;py=y;}else if(n<w+h-1){px=x+w-1;py=y+n-w+1;}else if(n<2*w+h-2){px=x+2*w+h-3-n;py=y+h-1;}else{px=x;py=y+perimeter-n;}fill(side,px,py,1,1,color);}}
auto Widgets::rotary(unsigned side,float x,float y,float r,float amount,uint32_t color) -> void{float previous_x=0,previous_y=0;for(unsigned n=0;n<=24;++n){float angle=2.35f+float(n)/24*4.72f,px=x+cosf(angle)*r,py=y+sinf(angle)*r;if(n)line(side,previous_x,previous_y,px,py,float(n)/24<=clamp01(amount)?color:theme.border,2);previous_x=px;previous_y=py;}}
auto Widgets::arc270(unsigned side,int cx,int cy,int r,float amount,bool bipolar,uint32_t color) -> void{
  auto segment=[&](float a,float b,uint32_t col,float width){float a0=2.35619f+a*4.71239f,a1=2.35619f+b*4.71239f;line(side,cx+cosf(a0)*r,cy+sinf(a0)*r,cx+cosf(a1)*r,cy+sinf(a1)*r,col,width);};
  for(unsigned n=0;n<40;++n)segment(float(n)/40,float(n+1)/40,theme.border,2);
  float end=clamp01(amount),start=bipolar?.5f:0,lo=start<end?start:end,hi=start<end?end:start;
  for(unsigned n=0;n<40;++n){float a=float(n)/40,b=float(n+1)/40;if(b>lo && a<hi)segment(a<lo?lo:a,b>hi?hi:b,color,3);}
 }
auto Widgets::meter(unsigned side,int x,int y,int w,int h,float level,uint32_t source,bool horizontal) -> void{rounded(side,x,y,w,h,3,theme.raised);level=clamp01(level);for(unsigned n=0;n<12;++n){if(float(n)/12>=level)break;uint32_t color=meter_segment_color(theme,source,n);if(horizontal)rounded(side,x+int(n)*w/12,y,w/12-2,h,1,color);else rounded(side,x,y+h-(int(n)+1)*h/12,w,h/12-2,1,color);}}
auto Widgets::step_attributes(unsigned side,int x,int y,bool slide,bool accent,bool enabled,bool compact) -> void{
  uint32_t color=enabled?theme.teal:theme.disabled;
  if(compact){if(accent){line(side,x,y+3,x+2,y,color);line(side,x+2,y,x+4,y+3,color);line(side,x,y+3,x+4,y+3,color);}if(slide){line(side,x+7,y+3,x+9,y+3,color);line(side,x+9,y+3,x+12,y,color);line(side,x+12,y,x+14,y,color);}return;}
  if(accent){line(side,x,y+3,x+3,y,color);line(side,x+3,y,x+6,y+3,color);line(side,x,y+3,x+6,y+3,color);}
  if(slide){line(side,x,y+10,x+3,y+10,color);line(side,x+3,y+10,x+6,y+6,color);line(side,x+6,y+6,x+8,y+6,color);}
 }
auto Widgets::step_cell(unsigned side,int x,int y,int w,int h,unsigned step,bool enabled,bool playing,bool selected,bool locked,bool available,uint32_t color) -> void{fill(side,x,y,w,h,!available?theme.background:selected?theme.raised:theme.surface);if(enabled && available)fill(side,x+3,y+h-6,3,3,color);if(playing)fill(side,x+2,y,w-4,3,theme.motion);if(selected)selected_mark(side,x,y,w,h);char v[24];format_value(v,float(step),0);aligned_band(side,x,y,w,h>30?24:h,v,!available?theme.disabled:enabled?theme.text:theme.secondary,FontRole::Small,TextAlign::Center,TextKind::Cell,true);if(locked)fill(side,x+w-5,y+5,2,2,theme.mod);}
auto Widgets::graph_grid(unsigned side,int x,int y,int w,int h) -> void{for(unsigned i=1;i<4;++i)line(side,float(x),float(y+int(i)*h/4),float(x+w),float(y+int(i)*h/4),theme.border,1,65);for(unsigned i=1;i<8;++i)line(side,float(x+int(i)*w/8),float(y),float(x+int(i)*w/8),float(y+h),theme.border,1,45);}
auto Widgets::polyline(unsigned side,int x,int y,int w,int h,const float* samples,unsigned count,uint32_t color,float gain) -> void{if(count<2)return;for(unsigned n=1;n<count;++n){float a=clamp01(0.5f-samples[n-1]*gain*0.5f),b=clamp01(0.5f-samples[n]*gain*0.5f);line(side,float(x)+float(n-1)*w/(count-1),y+a*h,float(x)+float(n)*w/(count-1),y+b*h,color,2);}}
auto Widgets::envelope(unsigned side,int x,int y,int w,int h,float attack,float decay,float sustain,float release,uint32_t color) -> void{float xs[]={0,attack,attack+decay,1-release,1},ys[]={1,0,1-clamp01(sustain),1-clamp01(sustain),1};for(unsigned i=1;i<5;++i)line(side,x+xs[i-1]*w,y+ys[i-1]*h,x+xs[i]*w,y+ys[i]*h,color,2);}
auto Widgets::parameter_popup(unsigned side,const char* name,float number,const char* unit,unsigned precision) -> void{rounded(side,168,56,302,20,Geometry::button,theme.raised);label(side,177,58,name,theme.text,FontRole::Small,157);value(side,337,56,91,number,theme.teal,FontRole::Body,precision);if(*unit)label(side,437,58,unit,theme.secondary,FontRole::Small,27);}
auto Widgets::dialog(unsigned side,const char* title,const char* message,float amount) -> void{rounded(side,27,59,426,144,Geometry::dialog,theme.background,240);outline(side,27,59,426,144,Geometry::dialog,theme.border);label(side,43,71,title,theme.text,FontRole::Heading,394);label(side,43,111,message,theme.secondary,FontRole::Body,394);if(amount>=0)progress(side,43,165,394,6,amount,theme.teal);}
}
