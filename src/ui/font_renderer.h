#pragma once
#include "font_atlas.h"
namespace groove { namespace ui {
inline uint32_t next_codepoint(const char*& p){unsigned a=uint8_t(*p++);if(a<128)return a;unsigned n=a>=0xf0?3:a>=0xe0?2:a>=0xc0?1:0;uint32_t cp=a&((1u<<(6-n))-1);if(!n)return '?';while(n--){unsigned c=uint8_t(*p);if((c&0xc0)!=0x80)return '?';++p;cp=(cp<<6)|(c&63);}return cp;}
inline const Glyph& glyph(const FontFace& font,uint32_t cp){unsigned lo=0,hi=font.count;while(lo<hi){unsigned mid=(lo+hi)/2;if(font.glyphs[mid].code<cp)lo=mid+1;else hi=mid;}if(lo<font.count && font.glyphs[lo].code==cp)return font.glyphs[lo];return cp=='?'?font.glyphs[0]:glyph(font,'?');}
inline int kerning(const FontFace& f,uint32_t a,uint32_t b){if(a>127 || b>127)return 0;unsigned key=(a<<8)|b,lo=0,hi=f.kern_count;while(lo<hi){unsigned mid=(lo+hi)/2;if(f.kern[mid].pair<key)lo=mid+1;else hi=mid;}return lo<f.kern_count && f.kern[lo].pair==key?f.kern[lo].shift:0;}
inline bool figure(uint32_t cp){return cp>='0' && cp<='9';}
inline bool figure_symbol(uint32_t cp){return cp=='+' || cp=='-' || cp=='.' || cp==',' || cp==':' || cp=='%' || cp=='/' || cp==0xb0 || cp==0xd7 || cp==0x2212;}
inline const FontFace& text_face(FontRole role,uint32_t cp,bool numeric,uint32_t previous=0,uint32_t next=0){(void)cp;(void)numeric;(void)previous;(void)next;return font_face(role);}
inline int baseline(const FontFace& f,int top){return top+(int(f.line_height)-f.cap_height)/2+f.cap_height;}
inline int cap_top(const FontFace& f,int top){return baseline(f,top)-f.cap_height;}
inline int centered_text_y(FontRole role,int y,int height){return y+(height-font_face(role).line_height)/2;}
inline int advance(const FontFace& f,const Glyph& g,bool){return g.advance+f.tracking*64;}
inline int text_width(const char* text,FontRole role=FontRole::Body,bool numeric=false){int width=0;uint32_t previous=0;const FontFace* old=nullptr;while(*text){uint32_t cp=next_codepoint(text);const auto& f=text_face(role,cp,numeric,previous,uint8_t(*text));const auto& g=glyph(f,cp);width+=advance(f,g,numeric)+(old==&f && !figure(previous) && !figure(cp)?kerning(f,previous,cp):0);previous=cp;old=&f;}return (width+63)/64;}
template<class Pixel> inline void raster_text(Pixel pixel,int x,int y,const char* text,FontRole role,int maximum_width,bool numeric=false){
 int pen=0;uint32_t previous=0;const FontFace* old=nullptr;
 while(*text){uint32_t cp=next_codepoint(text);const FontFace* f=&text_face(role,cp,numeric,previous,uint8_t(*text));const auto* g=&glyph(*f,cp);int shift=old==f && !figure(previous) && !figure(cp)?kerning(*f,previous,cp):0,width=advance(*f,*g,numeric);
  if((pen+shift+width+63)/64>maximum_width)break;
  bool ellipsis=*text && (pen+shift+width+glyph(font_face(role),0x2026).advance+63)/64>maximum_width && text_width(text,role,numeric)>(maximum_width-(pen+shift+width)/64);
  if(ellipsis){cp=0x2026;f=&font_face(role);g=&glyph(*f,cp);shift=0;}
  pen+=shift;int origin=x+(pen+32)/64+g->x,base=baseline(*f,y);
  for(unsigned j=0;j<g->h;++j)for(unsigned i=0;i<g->w;++i){unsigned n=j*g->w+i,packed=f->coverage[g->offset+n/2],alpha=((n&1)?packed&15:packed>>4)*17;if(alpha && origin+int(i)>=x && origin+int(i)<x+maximum_width)pixel(origin+int(i),base+g->y+int(j),alpha);}
  if(ellipsis)break;
  pen+=width;previous=cp;old=f;
 }
}
}}
