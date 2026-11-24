// Widgets member: the six bounded drawing primitives. Values are already resolved.
 void plot_axis(unsigned side,int x,int y,int w,int h,bool bipolar){
  int baseline=bipolar?(h-1)/2:h-1;
  fill(side,x,y+baseline,w,1,theme.border);
  if(w>=80 && h>=18){
   for(int q=1;q<4;++q){
    int px=x+(w-1)*q/4;
    fill(side,px,y+baseline-2,1,bipolar?5:3,theme.secondary);
   }
  }
 }
 void visual_scope(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  if(w<=0 || h<=0)return;
  plot_axis(side,x,y,w,h,true);
  const unsigned count=d.count<128?d.count:128;
  int last_x=x,last_top=0,last_bottom=0;
  for(unsigned i=0;i<count;++i){
   const int px=x+int(i)*(w-1)/int(count>1?count-1:1);
   const int top=y+int(clamp01(.5f-.5f*fmaxf(d.values[i],d.low[i]))*(h-1));
   const int bottom=y+int(clamp01(.5f-.5f*fminf(d.values[i],d.low[i]))*(h-1));
   fill(side,px,top,1,bottom-top+1,color);
   if(i){line(side,last_x,last_top,px,top,color,1);line(side,last_x,last_bottom,px,bottom,color,1);}
   last_x=px;last_top=top;last_bottom=bottom;
  }
  if(d.point)fill(side,x+int(clamp01(d.position)*(w-1)),y,1,h,theme.text);
 }
 void visual_spectrum(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  const unsigned count=d.count<128?d.count:128;
  if(!count || w<=0 || h<=0)return;
  plot_axis(side,x,y,w,h,false);
  const uint32_t body=mix(theme.surface,color,130);
  for(unsigned n=0;n<count;++n){
   const int start=int(n)*w/int(count),end=int(n+1)*w/int(count);
   if(start==end)continue;
   const float magnitude=clamp01((20*log10f(fmaxf(d.values[n],.0001f))+80)/80);
   const int height=int(magnitude*h),bar=end-start-(end-start>=4?1:0);
   if(height){
    fill(side,x+start,y+h-height,bar,height,body);
    fill(side,x+start,y+h-height,bar,1,color);
   }
  }
 }
 void visual_bars(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  unsigned count=d.count<128?d.count:128;if(!count || w<=0 || h<=0)return;
  if(count<=8 && d.names[0]){ // A control profile, not a substitute PCM trace.
   unsigned rows=(count+1)/2;int column=w/2,row_h=h/int(rows);
   for(unsigned n=0;n<count;++n){int cx=x+int(n/rows)*column,cy=y+int(n%rows)*row_h;char value[24];format_value(value,clamp01(d.values[n])*100,0);
    unsigned size=unsigned(__builtin_strlen(value));value[size]='%';value[size+1]=0;
    aligned_band(side,cx,cy,column-66,row_h-3,d.names[n],theme.secondary,FontRole::Small,TextAlign::Left,TextKind::ValueLabel);
    aligned_band(side,cx+column-62,cy,54,row_h-3,value,theme.text,FontRole::Small,TextAlign::Right,TextKind::ValueNumber,true,true);
    progress(side,cx,cy+row_h-3,column-10,2,d.values[n],color);
   }
   return;
  }
  const int chart_h=count<=16 && h>28?h-15:h;
  const uint32_t ink=mix(theme.surface,color,145);
  for(unsigned n=0;n<count;++n){
   int left=x+int(n)*w/int(count),span=x+int(n+1)*w/int(count)-left;
   if(!span)continue;
   int width=span-(span>=6?3:span>=3?1:0);
   int height=int(clamp01(d.values[n])*chart_h);
   fill(side,left,y+chart_h-1,width,1,theme.border);
   if(height){
    fill(side,left,y+chart_h-height,width,height,ink);
    fill(side,left,y+chart_h-height,width,1,color);
   }
   if(chart_h<h){char number[24];format_value(number,n+1,0);centered_label(side,left,y+chart_h+1,span,14,number,theme.secondary,FontRole::Small);}
  }
 }
 void visual_ring(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  int top=native_y(y),height=native_h(h),cx=x+w/2,cy=top+height/2,r=(w<height?w:height)/2-5;unsigned layers=d.layers?d.layers:1;if(layers>4)layers=4;
  for(unsigned layer=0;layer<layers;++layer){int radius=r-int(layer)*10;if(radius<8)break;for(unsigned n=0;n<64;++n){float a=6.283185307f*n/64-1.5707963f,b=6.283185307f*(n+1)/64-1.5707963f;uint32_t ink=float(n)/64<d.recorded?mix(theme.surface,color,120):theme.border;if(layer==0 && float(n)/64<d.phase)ink=color;line_pixels(side,cx+int(cosf(a)*radius),cy+int(sinf(a)*radius),cx+int(cosf(b)*radius),cy+int(sinf(b)*radius),ink,7);}}
  if(d.point){float a=clamp01(d.phase)*6.283185307f-1.5707963f;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)if(dx*dx+dy*dy<=5)blend(side,cx+int(cosf(a)*r)+dx,cy+int(sinf(a)*r)+dy,theme.text,255);}
  if(d.recording){int height=int(clamp01(d.level)*h);fill(side,x+w-5,y+h-height,3,height,theme.danger);}
 }
 void visual_curve(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){
  const unsigned count=d.count<128?d.count:128;
  if(w<=0 || h<=0)return;
  plot_axis(side,x,y,w,h,d.bipolar);
  const int base=y+(d.bipolar?(h-1)/2:h-1);
  const uint32_t shade=mix(theme.surface,color,68);
  for(unsigned n=1;n<count;++n){
   const int x0=x+int(n-1)*(w-1)/int(count-1),x1=x+int(n)*(w-1)/int(count-1);
   const int y0=y+int((1-clamp01(d.values[n-1]))*(h-1)),y1=y+int((1-clamp01(d.values[n]))*(h-1));
   // Sparse stems keep the scale visible under overlapping wave layers.
   for(int px=x0;px<x1;++px)if((px-x)%4==0){
    int point=y0+(y1-y0)*(px-x0)/(x1-x0),top=point<base?point:base;
    fill(side,px,top,1,(point<base?base-point:point-base),shade);
   }
   line(side,x0,y0,x1,y1,color,1);
  }
  if(d.point){
   const int px=x+int(clamp01(d.phase)*(w-1)),py=y+int((1-clamp01(d.position))*(h-1));
   fill(side,px,y,1,h,theme.secondary);
   const int left=px-2<x?x:px-2,right=px+2>=x+w?x+w-1:px+2;
   fill(side,left,py,right-left+1,1,theme.text);
  }
 }
 void visual_response(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color){visual_curve(side,x,y,w,h,d,color);if(d.live && d.point && d.count){int at=x+int(clamp01(d.level)*(w-1));fill(side,at,y,1,h,theme.secondary);if(d.amount>0)progress(side,x,y+h+6,w,3,d.amount,theme.warn);}}
 void visual_element(unsigned side,int x,int y,int w,int h,const VisualDeclaration& declaration,const VisualData& d,uint32_t color){
  aligned_band(side,x,y,w,18,d.label,d.live?theme.accent:theme.secondary,FontRole::Small,TextAlign::Center,TextKind::Hero);y+=26;h-=44;if(d.presentation!=VisualElement::None){auto resolved=declaration;resolved.element=d.presentation;draw_visual_shape(side,x,y,w,h,resolved,d,color);return;}draw_visual_shape(side,x,y,w,h,declaration,d,color);
 }
 void draw_visual_shape(unsigned side,int x,int y,int w,int h,const VisualDeclaration& declaration,const VisualData& d,uint32_t color){
  switch(declaration.element){case VisualElement::Scope:visual_scope(side,x,y,w,h,d,color);break;case VisualElement::Spectrum:visual_spectrum(side,x,y,w,h,d,color);break;case VisualElement::Bars:visual_bars(side,x,y,w,h,d,color);break;case VisualElement::Ring:visual_ring(side,x,y,w,h,d,color);break;case VisualElement::Curve:if(d.layers==3 && d.bipolar){VisualData neighbor;neighbor.count=64;neighbor.bipolar=true;int table_w=w*2/3;for(unsigned layer=0;layer<2;++layer){for(unsigned i=0;i<64;++i)neighbor.values[i]=d.low[layer*64+i];visual_curve(side,x+24-int(layer)*10,y+int(layer)*8,table_w-32,h-22,neighbor,mix(theme.border,color,55+layer*25));}visual_curve(side,x,y+22,table_w-10,h-22,d,color);float total=d.parameters[0]+d.parameters[1]+.3f+d.parameters[3];envelope(side,x+table_w+12,y+16,w-table_w-12,h-20,d.parameters[0]/total,d.parameters[1]/total,d.parameters[2],d.parameters[3]/total,color);unsigned ticks=1+unsigned(clamp01(d.parameters[4]/20)*15);for(unsigned n=0;n<ticks;++n){int px=x+int(n)*w/int(ticks);line(side,px,y+h+7,px,y+h+15,theme.secondary);}}else if(d.layers==2 && d.bipolar){visual_curve(side,x,y,w,h*3/5,d,color);VisualData response;response.count=d.count;for(unsigned i=0;i<d.count;++i)response.values[i]=d.low[i];visual_response(side,x,y+h*2/3,w,h/3,response,mix(theme.secondary,color,140));}else visual_curve(side,x,y,w,h,d,color);if(d.range){fill(side,x,y+h+8,w,5,theme.border);int first=x+int(clamp01(d.range_low)*(w-1)),last=x+int(clamp01(d.range_high)*(w-1));fill(side,first,y+h+8,last-first+1,5,color);fill(side,x+int(clamp01(d.position)*(w-3)),y+h+5,3,11,theme.text);}break;case VisualElement::Response:visual_response(side,x,y,w,h,d,color);break;case VisualElement::None:empty_state(side,x,y,w,"No audio visualization",declaration.caption);break;}
 }
