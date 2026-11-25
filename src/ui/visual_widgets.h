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
   if(i){
    for(int col=last_x+1;col<px;++col){
     int t=last_top+(top-last_top)*(col-last_x)/(px-last_x);
     int b=last_bottom+(bottom-last_bottom)*(col-last_x)/(px-last_x);
     if(t<b)fill(side,col,t,1,b-t+1,mix(theme.surface,color,120));
     else if((col-x)%4==0){int mid=y+(h-1)/2;fill(side,col,t<mid?t:mid,1,t<mid?mid-t:t-mid,mix(theme.surface,color,65));}
    }
    line(side,last_x,last_top,px,top,color,1);line(side,last_x,last_bottom,px,bottom,color,1);
   }
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
 void visual_ring(unsigned side,int x,int y,int w,int h,const VisualData& d,uint32_t color,bool loop){
  const int top=native_y(y),height=native_h(h);
  if(w<20 || height<20)return;
  // Quarter-wave samples for both projected axes. The upper face carries time,
  // the front wall has fixed thickness and never implies recorded content.
  static constexpr uint8_t sine[]={0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126,127};
  auto unit=[](unsigned n){unsigned i=n%32;int v=sine[i<=16?i:32-i];return n%64>=32?-v:v;};
  const int ry=height*3/8,rx=w/2-6<ry*3?w/2-6:ry*3,depth=ry/5,cx=x+w/2,cy=top+(height-depth)/2;
  const uint32_t recorded=mix(theme.raised,color,135);
  const int inner_rx=rx*3/4,inner_ry=ry*2/3;
  for(int row=-ry;row<=ry;++row){
   float v=float(row)/ry;
   int outer=int(rx*sqrtf(fmaxf(0,1-v*v)));
   v=float(row)/inner_ry;
   int inner=int(inner_rx*sqrtf(fmaxf(0,1-v*v)));
   line_pixels(side,cx-outer,cy+row,cx-inner,cy+row,theme.raised);
   line_pixels(side,cx+inner,cy+row,cx+outer,cy+row,theme.raised);
  }
  for(unsigned n=0;n<64;++n){
   const int x0=cx+rx*unit(n)/127,y0=cy-ry*unit(n+16)/127;
   const int x1=cx+rx*unit(n+1)/127,y1=cy-ry*unit(n+17)/127;
   if(n>=16 && n<48){
    for(int j=1;j<=depth;++j)line_pixels(side,x0,y0+j,x1,y1+j,j==depth?theme.border:theme.raised);
   }
   uint32_t ink=d.point && float(n)/64<d.phase?color:float(n)/64<d.recorded?recorded:theme.border;
   line_pixels(side,x0,y0,x1,y1,ink,2);
   if(n%2==0){
    int tick=n%8==0?7:3;
    line_pixels(side,x0,y0,cx+(rx-tick)*unit(n)/127,cy-(ry-tick)*unit(n+16)/127,n%8?theme.dim:theme.secondary);
   }
   if(d.layers>1)line_pixels(side,cx+(rx-12)*unit(n)/127,cy-(ry-9)*unit(n+16)/127,cx+(rx-12)*unit(n+1)/127,cy-(ry-9)*unit(n+17)/127,recorded,2);
  }
  if(d.point){unsigned pos=unsigned(clamp01(d.phase)*64+.5f);int px=cx+rx*unit(pos)/127,py=cy-ry*unit(pos+16)/127;line_pixels(side,px-2,py-2,px+2,py-2,theme.text,5);}
  if(loop)centered_label(side,x,y,w,h-4,d.recording?"REC":!d.available?"EMPTY":d.point?"PLAY":"READY",d.recording?theme.danger:theme.text,FontRole::Bold);
  if(d.recording){int level=int(clamp01(d.level)*h);fill(side,x+w-5,y+h-level,3,level,theme.danger);}
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
   // A shaded edge belongs to this model curve and stays inside its value field.
   if(h>=30 && w>=80){
    const int bottom=y+h-1;
    for(int depth=3;depth>0;--depth){
     const int a=y0+depth<bottom?y0+depth:bottom,b=y1+depth<bottom?y1+depth:bottom;
     line(side,x0,a,x1,b,mix(theme.surface,color,depth==1?130:65),1);
    }
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
  aligned_band(side,x,y,w,18,d.label,d.live?theme.accent:theme.secondary,FontRole::Small,TextAlign::Center,TextKind::Hero);y+=26;h-=44;if(w>=80 && h>=20 && declaration.element!=VisualElement::Ring){fill(side,x,y,w,1,theme.border);fill(side,x,y,1,h,theme.border);fill(side,x+w-1,y,1,h,theme.raised);fill(side,x,y+h-1,w,1,theme.raised);}if(d.presentation!=VisualElement::None){auto resolved=declaration;resolved.element=d.presentation;draw_visual_shape(side,x,y,w,h,resolved,d,color);return;}draw_visual_shape(side,x,y,w,h,declaration,d,color);
 }
 void draw_visual_shape(unsigned side,int x,int y,int w,int h,const VisualDeclaration& declaration,const VisualData& d,uint32_t color){
  switch(declaration.element){case VisualElement::Scope:visual_scope(side,x,y,w,h,d,color);break;case VisualElement::Spectrum:visual_spectrum(side,x,y,w,h,d,color);break;case VisualElement::Bars:visual_bars(side,x,y,w,h,d,color);break;case VisualElement::Ring:visual_ring(side,x,y,w,h,d,color,declaration.source==VisualSource::Loop);break;case VisualElement::Curve:if(d.layers==3 && d.bipolar){VisualData neighbor;neighbor.count=64;neighbor.bipolar=true;int table_w=w*2/3;for(unsigned layer=0;layer<2;++layer){for(unsigned i=0;i<64;++i)neighbor.values[i]=d.low[layer*64+i];visual_curve(side,x+24-int(layer)*10,y+int(layer)*8,table_w-32,h-22,neighbor,mix(theme.border,color,55+layer*25));}visual_curve(side,x,y+22,table_w-10,h-22,d,color);float total=d.parameters[0]+d.parameters[1]+.3f+d.parameters[3];envelope(side,x+table_w+12,y+16,w-table_w-12,h-20,d.parameters[0]/total,d.parameters[1]/total,d.parameters[2],d.parameters[3]/total,color);unsigned ticks=1+unsigned(clamp01(d.parameters[4]/20)*15);for(unsigned n=0;n<ticks;++n){int px=x+int(n)*w/int(ticks);line(side,px,y+h+7,px,y+h+15,theme.secondary);}}else if(d.layers==2 && d.bipolar){visual_curve(side,x,y,w,h*3/5,d,color);VisualData response;response.count=d.count;for(unsigned i=0;i<d.count;++i)response.values[i]=d.low[i];visual_response(side,x,y+h*2/3,w,h/3,response,mix(theme.secondary,color,140));}else visual_curve(side,x,y,w,h,d,color);if(d.range){fill(side,x,y+h+8,w,5,theme.border);int first=x+int(clamp01(d.range_low)*(w-1)),last=x+int(clamp01(d.range_high)*(w-1));fill(side,first,y+h+8,last-first+1,5,color);fill(side,x+int(clamp01(d.position)*(w-3)),y+h+5,3,11,theme.text);}break;case VisualElement::Response:visual_response(side,x,y,w,h,d,color);break;case VisualElement::None:empty_state(side,x,y,w,"No audio visualization",declaration.caption);break;}
 }
