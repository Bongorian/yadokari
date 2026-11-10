// Widgets member implementation: fixed value fields, shared hierarchy and shape.
 void section(unsigned side,int x,int y,int w,const char* title){aligned_band(side,x,y,w,18,title,theme.secondary,FontRole::Small,TextAlign::Left,TextKind::Section,false,true);line(side,x,y+19,x+w,y+19,theme.border);}
 void key_value(unsigned side,int x,int y,int w,const char* key,const char* text){aligned_band(side,x,y,w/2,18,key,theme.secondary,FontRole::Body,TextAlign::Left,TextKind::ValueLabel);aligned_band(side,x+w/2,y,w/2,18,text,theme.text,FontRole::Body,TextAlign::Right,TextKind::ValueNumber,false,true);}
 void empty_state(unsigned side,int x,int y,int w,const char* title,const char* detail){label_band(side,x,y,w,24,title,theme.secondary,FontRole::Heading);label_band(side,x,y+28,w,18,detail,theme.dim,FontRole::Small);}
 void value_row(unsigned side,int x,int y,int w,int h,const ContentValue& item){
  uint32_t ink=item.enabled?theme.text:theme.dim,accent=item.enabled?(item.selected?theme.value_edit:theme.accent):theme.dim;int field=w>300?190:w/2;
  if(item.selected){fill(side,x,y,w,h,theme.raised);selected_mark(side,x,y,w,h);}
  aligned_band(side,x,y,w-field-8,h-6,item.name,item.enabled?theme.secondary:theme.dim,FontRole::Body,TextAlign::Left,TextKind::ValueLabel,false,true);
  int tw=text_width(item.text,FontRole::Body);if(tw>field)tw=field;int tx=x+w-tw;
  if(item.shape==ValueShape::Chip){rounded(side,tx-5,y+1,tw+10,h-8,3,theme.surface);}
  aligned_band(side,x+w-field,y,field,h-6,item.text,ink,FontRole::Body,TextAlign::Right,TextKind::ValueNumber,false,true);
  int rail=y+h-4;float n=clamp01(item.normalized);
  if(item.shape==ValueShape::Bar)progress(side,x,rail,w,2,n,accent);
  else if(item.shape==ValueShape::Bipolar){fill(side,x,rail,w,2,theme.border);int a=x+w/2,b=x+int(n*w);fill(side,a<b?a:b,rail,a<b?b-a:a-b,2,accent);fill(side,x+w/2,rail-2,1,6,theme.secondary);}
  else if(item.shape==ValueShape::Segments){unsigned count=item.steps>1?item.steps:8;if(count>24)count=24;for(unsigned i=0;i<count;++i){int a=x+int(i)*w/int(count),b=x+int(i+1)*w/int(count);fill(side,a,rail,b-a-2,2,float(i)/float(count-1)<=n?accent:theme.border);}}
 }
 void list_row(unsigned side,int x,int y,int w,int h,const ContentRow& row){
  fill(side,x,y,w,h,row.selected?theme.raised:theme.surface);if(row.selected)selected_mark(side,x,y,w,h);
  int inset=row.icon?22:8;if(row.icon)icon(side,x+5,y+3,row.icon,row.enabled?theme.secondary:theme.dim);
  int state_width=*row.state?40:0,available=w-inset-state_width-8;
  aligned_band(side,x+inset,y,available,*row.secondary?h/2:h,row.primary,row.enabled?theme.text:theme.dim,FontRole::Body,TextAlign::Left,TextKind::Cell,false,true);
  if(*row.secondary)label_band(side,x+inset,y+h/2,available,h/2,row.secondary,theme.secondary,FontRole::Small);
  if(state_width)label_band(side,x+w-state_width,y,state_width-3,h,row.state,theme.secondary,FontRole::Small);
 }
 void mixer_strip(unsigned side,int x,int y,int w,int h,const char* name,float level,float pan,float peak,const char* state,bool selected,uint32_t color,float held=-1){
  fill(side,x,y,w,h,selected?theme.raised:theme.surface);if(selected)selected_mark(side,x,y,w,h);label_band(side,x+4,y+3,w-8,18,name,selected?theme.text:theme.secondary,FontRole::Small);
  int mx=x+w/2-3,my=y+24,mh=h-54;
  meter(side,mx,my,6,mh,peak,color);
  line(side,mx-5,my,mx-2,my,theme.secondary);line(side,mx+8,my,mx+11,my,theme.secondary); // 0 dBFS
  if(held>0){unsigned segment=unsigned(ceilf(clamp01(held)*12));if(!segment)segment=1;int py=my+mh-int(segment)*mh/12;fill(side,mx-2,py,10,2,meter_peak_color(theme,held));}
  progress(side,x+5,y+h-25,w-10,3,level,color);
  fill(side,x+5+int(clamp01((pan+1)*.5f)*(w-12)),y+h-28,2,8,theme.secondary);label_band(side,x+3,y+h-18,w-6,16,state,theme.secondary,FontRole::Small);
 }

 void signal_chain(unsigned side,int x,int y,int w,const ContentNode* nodes,unsigned count,uint32_t source){if(!count || count>8)return;int width=w/int(count);for(unsigned i=0;i<count;++i){int left=x+int(i)*width,cx=left+width/2;const auto& n=nodes[i];uint32_t ink=n.selected?theme.accent:n.enabled?theme.secondary:theme.dim;centered_label(side,left,y,width,18,n.name,ink,FontRole::Small);if(i+1<count){line(side,cx+5,y+27,cx+width-5,y+27,theme.border);line(side,cx+width-9,y+24,cx+width-5,y+27,theme.border);}rounded(side,cx-4,y+23,8,8,4,ink);centered_label(side,left,y+37,width,14,n.state,ink,FontRole::Small);level_meter(side,left+7,y+60,width-14,3,n.activity,source);}}
