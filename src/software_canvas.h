#pragma once
#include "ui/icons.h"
#include "ui/content.h"
#include <stdint.h>
#include <math.h>
#include "ui/theme.h"
#include "ui/font_renderer.h"
#include "ui/hints.h"
namespace groove {
class SoftwareCanvas {
 struct Marquee {uint64_t started=0;uint32_t hash=0,key=0;int overflow=0,offset=0,y=0;};
 int field_x_=0,field_w_=0,field_y_=-1,field_h_=0;ui::TextAlign field_align_=ui::TextAlign::Left;ui::TextKind field_kind_=ui::TextKind::Plain;
 unsigned brightness_=100;
 uint32_t lit(uint32_t color)const{if(brightness_==100)return color;return 0xff000000u|(((color>>16&255)*brightness_/100)<<16)|(((color>>8&255)*brightness_/100)<<8)|((color&255)*brightness_/100);}
 bool layout_active_=false;int layout_length_=1,layout_extent_=1,layout_from_=0,layout_to_=0;int clip_top_=0,clip_bottom_=272;
 float mapped_y(float y)const{return layout_active_?float(layout_to_)+(y-layout_from_)*float(layout_extent_)/layout_length_:y;}
 int mapped_y(int y)const{return layout_active_?layout_to_+int(lroundf(float((y-layout_from_)*layout_extent_)/layout_length_)):y;}
 int mapped_h(int h)const{return layout_active_?int((h*layout_extent_+layout_length_/2)/layout_length_):h;}
 Marquee marquees_[48]{};unsigned marquee_count_=0;uint64_t text_time_=0;
 static int marquee_offset(int overflow,uint64_t elapsed);
public:
 struct TextPlacement {unsigned side;int x,y,width;ui::FontRole role;bool numeric;const char* text;int band_y,band_h;int field_x,field_w;ui::TextAlign align;ui::TextKind kind;};
 using TextObserver=void(*)(const TextPlacement&,void*);TextObserver text_observer=nullptr;void* text_observer_context=nullptr;
 void observe_text(unsigned side,int x,int y,int width,ui::FontRole role,bool numeric,const char* text,int band_y=-1,int band_h=0);
 ui::PageAdvice page_advice;
 void advice(const char* text){page_advice.add(text);}
 // Map geometry anchors, never bitmap glyphs. Text stays at the native atlas size.
 void brightness(unsigned percent){brightness_=percent<20?20:percent>100?100:percent;}
 void layout_band(int from,int length,int to,int extent){layout_active_=true;layout_from_=from;layout_to_=to;layout_length_=length;layout_extent_=extent;}
 void layout_reset(){layout_active_=false;}
 bool mapped_layout()const{return layout_active_;}
 int native_y(int y)const{return mapped_y(y);}
 int native_h(int h)const{return mapped_h(h);}
 void clip_y(int first=0,int last=272){clip_top_=first;clip_bottom_=last;}
 void text_clock(uint64_t now,bool reset=false){text_time_=now;if(reset)marquee_count_=0;}
 bool scrolling_due(int first,int last)const;
 // Names retain both ends: 1.2 s dwell, 28 px/s travel, clipped to their field.
 // No allocation and no ellipsis; short labels use the ordinary raster path.
 void scrolling_label(unsigned side,int x,int y,const char* value,uint32_t color,ui::FontRole role=ui::FontRole::Body,int w=460);
 static constexpr unsigned width=480,height=272;
 uint32_t pixels[2][width*height]{};
 ui::Theme theme=ui::Theme::by_index(ui::Theme::kDefault);
 void fill(unsigned side,int x,int y,int w,int h,uint32_t color);
 __attribute__((always_inline)) void blend(unsigned side,int x,int y,uint32_t color,unsigned alpha){color=lit(color);if(side>1 || x<0 || x>=480 || y<clip_top_ || y>=clip_bottom_ || !alpha)return;auto& p=pixels[side][y*480+x];p=alpha>=255?color:ui::mix(p,color,alpha);}
 static float rounded_coverage(float x,float y,float w,float h,float r);
 void rounded(unsigned side,int x,int y,int w,int h,int radius,uint32_t color,unsigned alpha=255);
 void outline(unsigned side,int x,int y,int w,int h,int radius,uint32_t color,unsigned thickness=1,unsigned alpha=255);
 // Integer endpoints and raster steps: no half-pixel blur on rules or graphs.
 void line_pixels(unsigned side,int x0,int y0,int x1,int y1,uint32_t color,int thickness=1,unsigned alpha=255);
 void line(unsigned side,float x0,float y0,float x1,float y1,uint32_t color,float thickness=2,unsigned alpha=255);

 // Like glyphs, icons map their anchor once and retain native pixel geometry.
 void icon(unsigned side,int x,int y,uint8_t id,uint32_t color,bool filled=false){ui::icons::draw(*this,id,side,x,mapped_y(y),color,filled);}
 void label_pixels(unsigned side,int x,int y,const char* value,uint32_t color,ui::FontRole role,int max_width,bool numeric=false,int band_y=-1,int band_h=0){observe_text(side,x,y,max_width,role,numeric,value,band_y,band_h);ui::raster_text([&](int px,int py,unsigned a){blend(side,px,py,color,a);},x,y,value,role,max_width,numeric);}
 void label(unsigned side,int x,int y,const char* value,uint32_t color,ui::FontRole role=ui::FontRole::Body,int max_width=460,bool numeric=false){label_pixels(side,x,mapped_y(y),value,color,role,max_width,numeric);}
 void aligned_band(unsigned side,int x,int y,int w,int h,const char* value,uint32_t color,ui::FontRole role,ui::TextAlign align=ui::TextAlign::Left,ui::TextKind kind=ui::TextKind::Cell,bool numeric=false,bool scrolling=false);
 void label_band(unsigned side,int x,int y,int w,int h,const char* value,uint32_t color,ui::FontRole role=ui::FontRole::Small){aligned_band(side,x,y,w,h,value,color,role);}

 void text(unsigned side,unsigned x,unsigned y,const char* value,uint32_t color,unsigned scale=1){label(side,int(x),int(y),value,color,scale>1?ui::FontRole::Heading:ui::FontRole::Small,480-int(x));}
 void number(unsigned side,unsigned x,unsigned y,unsigned value,uint32_t color,unsigned scale=1);
 static void format_value(char out[24],float value,unsigned precision=1);

 void decimal(unsigned side,unsigned x,unsigned y,float value,uint32_t color){char text[24];format_value(text,value,2);label(side,int(x),int(y),text,color,ui::FontRole::Number,480-int(x),true);}
 void value(unsigned side,int x,int y,int w,float v,uint32_t color,ui::FontRole role=ui::FontRole::Number,unsigned precision=1);
};
}
