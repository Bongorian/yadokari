// Widgets member: the six bounded drawing primitives. Values are already resolved.
 void visual_scope(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  line(side,x,y+h/2,x+w,y+h/2,theme.border);unsigned count=d.count<128?d.count:128;if(!count || w<=0 || h<=0)return;
  for(int i=0;i<w;++i){unsigned n=unsigned(i)*count/unsigned(w);float hi=clamp01(.5f-d.values[n]*.5f),lo=clamp01(.5f-d.low[n]*.5f);int top=y+int(hi*h),bottom=y+int(lo*h);if(bottom<top){int swap=top;top=bottom;bottom=swap;}int centre=y+h/2;if(top>centre)top=centre;if(bottom<centre)bottom=centre;fill(side,x+i,top,1,bottom-top+1,mix(theme.surface,color,190));}
  if(d.point)line(side,x+clamp01(d.position)*w,y,x+clamp01(d.position)*w,y+h,theme.text);
 }
 void visual_spectrum(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  unsigned count=d.count<128?d.count:128;if(!count || w<=0 || h<=0)return;for(int i=0;i<w;++i){unsigned n=unsigned(i)*count/unsigned(w);float magnitude=clamp01((20*log10f(fmaxf(d.values[n],.0001f))+80)/80);int height=int(magnitude*h);if(height)fill(side,x+i,y+h-height,1,height,mix(theme.surface,color,200));}line(side,x,y+h,x+w,y+h,theme.border);
 }
 void visual_bars(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  unsigned count=d.count<128?d.count:128;if(!count || w<=0 || h<=0)return;
  if(count<=8 && d.names[0]){ // A control profile, not a substitute PCM trace.
   unsigned rows=(count+1)/2;int column=w/2,row_h=h/int(rows);
   for(unsigned n=0;n<count;++n){int cx=x+int(n/rows)*column,cy=y+int(n%rows)*row_h;char value[24];format_value(value,d.parameters[n],2);
    aligned_band(side,cx,cy,column-66,row_h-3,d.names[n],theme.secondary,FontRole::Small,TextAlign::Left,TextKind::ValueLabel);
    aligned_band(side,cx+column-62,cy,54,row_h-3,value,theme.text,FontRole::Small,TextAlign::Right,TextKind::ValueNumber,true,true);
    progress(side,cx,cy+row_h-3,column-10,2,d.values[n],color);
   }
   return;
  }
  for(unsigned n=0;n<count;++n){int left=x+int(n)*w/int(count),right=x+int(n+1)*w/int(count),height=int(clamp01(d.values[n])*h);line(side,left,y+h,right-4,y+h,theme.border);if(height){fill(side,left,y+h-height,right-left-4,height,mix(theme.surface,color,190));fill(side,left,y+h-height,right-left-4,2,color);}if(count<=16){char number[24];format_value(number,n+1,0);centered_label(side,left,y+h+3,right-left-2,14,number,theme.secondary,FontRole::Small);}}
 }
 void visual_ring(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  int top=native_y(y),height=native_h(h),cx=x+w/2,cy=top+height/2,r=(w<height?w:height)/2-5;unsigned layers=d.layers?d.layers:1;if(layers>4)layers=4;
  for(unsigned layer=0;layer<layers;++layer){int radius=r-int(layer)*10;if(radius<8)break;for(unsigned n=0;n<64;++n){float a=6.283185307f*n/64-1.5707963f,b=6.283185307f*(n+1)/64-1.5707963f;uint32_t ink=float(n)/64<d.recorded?mix(theme.surface,color,120):theme.border;if(layer==0 && float(n)/64<d.phase)ink=color;line_pixels(side,cx+int(cosf(a)*radius),cy+int(sinf(a)*radius),cx+int(cosf(b)*radius),cy+int(sinf(b)*radius),ink,7);}}
  if(d.point){float a=clamp01(d.phase)*6.283185307f-1.5707963f;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)if(dx*dx+dy*dy<=5)blend(side,cx+int(cosf(a)*r)+dx,cy+int(sinf(a)*r)+dy,theme.text,255);}
  if(d.recording){int height=int(clamp01(d.level)*h);fill(side,x+w-5,y+h-height,3,height,theme.danger);}
 }
 void visual_curve(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  unsigned count=d.count<128?d.count:128;if(w<=0 || h<=0)return;if(count>1)for(unsigned n=1;n<count;++n){float a=clamp01(d.values[n-1]),b=clamp01(d.values[n]);int x0=x+int(n-1)*w/int(count-1),x1=x+int(n)*w/int(count-1);for(int px=x0;px<x1;++px){float t=float(px-x0)/float(x1-x0);int point=y+int((1-(a+(b-a)*t))*h),base=d.bipolar?y+h/2:y+h,top=point<base?point:base,bottom=point>base?point:base;fill(side,px,top,1,bottom-top+1,mix(theme.surface,color,145));}line(side,x0,y+(1-a)*h,x1,y+(1-b)*h,color,2);}
  if(d.point){int px=x+int(clamp01(d.phase)*w),py=y+int((1-clamp01(d.position))*h);line(side,px,y,px,y+h,theme.border);rounded(side,px-3,py-3,6,6,3,theme.text);}
 }
 void visual_response(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){visual_curve(side,x,y,w,h,d,color);if(d.live && d.point && d.count){int at=x+int(clamp01(d.level)*w);line(side,at,y,at,y+h,theme.secondary);if(d.amount>0)progress(side,x,y+h+6,w,3,d.amount,theme.warn);}}
 void visual_element(unsigned side,int x,int y,int w,int h,const VisualDeclaration& declaration,const VisualData& d,uint32_t color){
  aligned_band(side,x,y,w,18,d.label,d.live?theme.accent:theme.secondary,FontRole::Small,TextAlign::Center,TextKind::Hero);y+=26;h-=44;if(d.presentation!=VisualElement::None){auto resolved=declaration;resolved.element=d.presentation;draw_visual_shape(side,x,y,w,h,resolved,d,color);return;}draw_visual_shape(side,x,y,w,h,declaration,d,color);
 }
 void draw_visual_shape(unsigned side,int x,int y,int w,int h,const VisualDeclaration& declaration,const VisualData& d,uint32_t color){
  switch(declaration.element){case VisualElement::Scope:visual_scope(side,x,y,w,h,d,color);break;case VisualElement::Spectrum:visual_spectrum(side,x,y,w,h,d,color);break;case VisualElement::Bars:visual_bars(side,x,y,w,h,d,color);break;case VisualElement::Ring:visual_ring(side,x,y,w,h,d,color);break;case VisualElement::Curve:if(d.layers==3 && d.bipolar){VisualData neighbor;neighbor.count=64;neighbor.bipolar=true;int table_w=w*2/3;for(unsigned layer=0;layer<2;++layer){for(unsigned i=0;i<64;++i)neighbor.values[i]=d.low[layer*64+i];visual_curve(side,x+24-int(layer)*10,y+int(layer)*8,table_w-32,h-22,neighbor,mix(theme.border,color,55+layer*25));}visual_curve(side,x,y+22,table_w-10,h-22,d,color);float total=d.parameters[0]+d.parameters[1]+.3f+d.parameters[3];envelope(side,x+table_w+12,y+16,w-table_w-12,h-20,d.parameters[0]/total,d.parameters[1]/total,d.parameters[2],d.parameters[3]/total,color);unsigned ticks=1+unsigned(clamp01(d.parameters[4]/20)*15);for(unsigned n=0;n<ticks;++n){int px=x+int(n)*w/int(ticks);line(side,px,y+h+7,px,y+h+15,theme.secondary);}}else if(d.layers==2 && d.bipolar){visual_curve(side,x,y,w,h*3/5,d,color);VisualData response;response.count=d.count;for(unsigned i=0;i<d.count;++i)response.values[i]=d.low[i];visual_response(side,x,y+h*2/3,w,h/3,response,mix(theme.secondary,color,140));}else visual_curve(side,x,y,w,h,d,color);if(d.range){fill(side,x,y+h+8,w,5,theme.border);int first=x+int(clamp01(d.range_low)*w),last=x+int(clamp01(d.range_high)*w);fill(side,first,y+h+8,last-first+1,5,color);fill(side,x+int(clamp01(d.position)*(w-3)),y+h+5,3,11,theme.text);}break;case VisualElement::Response:visual_response(side,x,y,w,h,d,color);break;case VisualElement::None:empty_state(side,x,y,w,"No audio visualization",declaration.caption);break;}
 }
