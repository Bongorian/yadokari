// UiRenderer member implementation. Diagrams show parameter intent and actual
// routing, never fabricated spectra, detector envelopes or modulation phase.
 void wire(unsigned side,int x,int y,int to,uint32_t color){line(side,x,y,to,y,color,2);line(side,to-5,y-3,to,y,color);line(side,to-5,y+3,to,y,color);}
 void junction(unsigned side,int x,int y,uint32_t color){rounded(side,x-3,y-3,6,6,3,color);}
 void flow_stage(unsigned side,int x,int y,const char* name,const char* detail,uint32_t color,bool selected=false){
  rounded(side,x-12,y-12,24,24,12,selected?theme.raised:theme.surface);if(selected)selected_mark(side,x-12,y-12,24,24);wire(side,x-6,y,x+6,color);
  centered_label(side,x-55,y-36,110,20,name,theme.secondary,Font::Small);
  centered_label(side,x-55,y+18,110,25,detail,color,Font::Small);
  if(selected)line(side,x-32,y+45,x+32,y+45,color,2);
 }
 // Parameter-space illustrations share the same meaning for track / pad / group FX.
 void insert_picture(unsigned side,unsigned type,float a,float b,float wet,bool bypass=false,unsigned rate=48000,const float* bands=nullptr,float activity=0,bool measured=false,const Performance* perf=nullptr,uint32_t meter_source=0){
  const auto* effect=fx_descriptor(type);if(!effect)return;auto declaration=effect->visual;VisualData data;
  if(declaration.source==VisualSource::Vocoder){data.available=bands && measured;data.live=data.available && !bypass;data.label=data.available?declaration.caption:"FOLLOWERS NOT PUBLISHED ON THIS ROUTE";data.count=16;if(data.live)for(unsigned i=0;i<16;++i)data.values[i]=ui::clamp01(bands[i]*12);}
  else{data=ui::response_visual(declaration.model,a,b,float(rate));data.live=measured && !bypass;data.level=meter_amount(activity);data.label=declaration.caption;}
  label(side,24,82,effect->name,bypass?theme.dim:theme.text,Font::Heading,310);if(bypass)label(side,355,88,"BYPASSED",theme.dim,Font::Small,100);
  if(type==20 || type==21){uint32_t color=bypass?theme.dim:theme.accent;wire(side,24,154,74,color);junction(side,74,154,color);
   if(type==20){line(side,74,154,102,124,color);wire(side,102,124,174,color);label(side,180,109,"SHIFT",theme.secondary,Font::Small,70);value(side,246,105,64,int(fx_choice_index(a,25))-12,color,Font::Body,0);label(side,318,109,"st",theme.secondary,Font::Small,35);wire(side,345,124,390,color);line(side,390,124,418,154,color);line(side,74,154,102,182,theme.dim);wire(side,102,182,390,theme.dim);line(side,390,182,418,154,theme.dim);label(side,180,164,"DRY / WET",theme.secondary,Font::Small,140);value(side,318,161,60,wet*100,color,Font::Small,0);}
   else{line(side,74,154,104,124,color);wire(side,104,124,190,color);line(side,74,154,104,177,theme.blue);wire(side,104,177,190,theme.blue);outline(side,196,111,26,26,13,color,2);outline(side,196,164,26,26,13,theme.blue,2);line(side,201,124,217,124,color,2);line(side,209,116,209,132,color,2);line(side,201,177,217,177,theme.blue,2);label(side,112,106,"HORN",theme.secondary,Font::Small,80);label(side,112,158,"DRUM",theme.secondary,Font::Small,80);wire(side,230,124,376,color);wire(side,230,177,376,theme.blue);line(side,376,124,410,144,color);line(side,376,177,410,164,theme.blue);label(side,278,131,fx_descriptor(21)->controls[0].values[fx_choice_index(a,3)],color,Font::Small,100);label(side,258,154,"RAMPED SPEED",theme.secondary,Font::Small,140);}
   junction(side,418,154,color);wire(side,418,154,454,color);if(measured && !bypass)level_meter(side,24,198,430,2,meter_amount(activity),meter_source?meter_source:theme.teal);return;}
  if(perf && perf->analysis() && (declaration.model==ResponseModel::LowPass || declaration.model==ResponseModel::HighPass || declaration.model==ResponseModel::BandPass || declaration.model==ResponseModel::Notch)){auto fft=filter_spectrum(*perf,rate);visual_spectrum(side,24,131,430,53,fft,theme.dim);data.label="RESPONSE MODEL + POST-BUS FFT";}
  if(perf)draw_visual(side,declaration,data,*perf,perf->visual().sample,24,108,430,91);else visual_element(side,24,108,430,91,declaration,data,bypass?theme.dim:theme.accent);if(measured && !bypass)level_meter(side,24,198,430,2,meter_amount(activity),meter_source?meter_source:theme.teal);(void)wet;
 }

 void routing(const ProjectState& p,const Performance& perf){
  const auto& r=p.routing;const auto t=track_at(p,p.active_pattern,perf.selected_track());const auto& g=r.groups[perf.group_cursor()];
  static const char* buses[]={"OFF","MAIN 1-2","CUE 3-4","MAIN + CUE"};
  if(perf.routing_fx_page()){
   label(0,24,83,perf.group_send_page()?"GROUP / SEND SPLIT":"GROUP / INSERT FLOW",theme.secondary,Font::Small,340);value(0,412,78,38,perf.group_cursor()+1,theme.text,Font::Body,0);
   float parallel=g.parallel;
   if(parallel>0){
    wire(0,24,143,86,theme.teal);junction(0,86,143,theme.teal);line(0,86,143,110,116,theme.teal);wire(0,110,116,211,theme.teal);
    line(0,86,143,110,174,theme.blue);wire(0,110,174,211,theme.blue);
    flow_stage(0,224,116,"INSERT 1",track_insert_name(unsigned(g.insert_fx[0])),theme.teal,perf.group_fx_slot()==0 && !perf.group_send_page());
    outline(0,212,162,24,24,12,theme.blue,perf.group_fx_slot()==1?2:1);wire(0,218,174,230,theme.blue);label(0,143,169,"FX2",theme.secondary,Font::Small,45);label(0,252,181,track_insert_name(unsigned(g.insert_fx[4])),theme.blue,Font::Small,180);
    wire(0,238,116,340,theme.teal);line(0,340,116,366,143,theme.teal);wire(0,238,174,340,theme.blue);line(0,340,174,366,143,theme.blue);junction(0,366,143,theme.text);wire(0,366,143,453,theme.text);
    if(parallel<0.999f){line(0,282,116,282,174,theme.border);label(0,24,194,"Blends serial feed with parallel branches",theme.secondary,Font::Small,430);}else label(0,24,194,"Independent branches / equal output blend",theme.secondary,Font::Small,430);
   }else{
    wire(0,24,136,85,theme.teal);flow_stage(0,98,136,"TRACK MIX","GROUP IN",theme.teal);wire(0,112,136,207,theme.teal);
    flow_stage(0,220,136,"INSERT 1",track_insert_name(unsigned(g.insert_fx[0])),theme.teal,perf.group_fx_slot()==0 && !perf.group_send_page());wire(0,234,136,317,theme.teal);flow_stage(0,330,136,"INSERT 2",track_insert_name(unsigned(g.insert_fx[4])),theme.teal,perf.group_fx_slot()==1 && !perf.group_send_page());wire(0,344,136,455,theme.teal);
    label(0,24,194,"Serial / output of Insert 1 feeds Insert 2",theme.secondary,Font::Small,430);
   }
   if(!perf.group_send_page()){unsigned slot=perf.group_fx_slot()*4;insert_picture(1,unsigned(g.insert_fx[slot]),g.insert_fx[slot+1],g.insert_fx[slot+2],g.insert_fx[slot+3],false,perf.visual().rate?perf.visual().rate:48000,perf.visual().group_vocoder_bands[perf.group_cursor()][perf.group_fx_slot()],perf.visual().group_insert_activity[perf.group_cursor()][perf.group_fx_slot()],perf.visual_fresh(),&perf);}
   else{label(1,24,83,"POST INSERT / GROUP GAIN / SENDS",theme.secondary,Font::Small,420);wire(1,24,134,130,theme.teal);flow_stage(1,144,134,"GAIN",buses[g.output],theme.teal);wire(1,158,134,450,theme.teal);junction(1,216,134,theme.teal);line(1,216,134,216,171,theme.blue);wire(1,216,171,450,theme.blue);label(1,262,113,"DELAY",theme.teal,Font::Small,90);progress(1,330,121,110,3,g.send[0],theme.teal);label(1,262,177,"REVERB",theme.blue,Font::Small,90);progress(1,330,185,110,3,g.send[1],theme.blue);label(1,24,194,"Track sends remain pre-group FX",theme.secondary,Font::Small,420);}
   return;
  }
  label(0,24,83,"OUTPUT / MONITOR PATHS",theme.secondary,Font::Small,420);
  const char* inputs[]={"TRACK MIX","PREVIEW","INPUT 1-2"};const char* outputs[]={"MAIN 1-2","CUE 3-4",buses[r.monitor_bus]};float levels[]={r.main,r.preview*r.cue,r.monitor_bus?r.monitor:0};
  for(unsigned n=0;n<3;++n){int y=118+int(n)*32;uint32_t c=levels[n]>0?(n==1?theme.blue:theme.teal):theme.disabled;label(0,24,y-7,inputs[n],theme.text,Font::Small,100);wire(0,122,y,202,c);outline(0,211,y-9,18,18,9,c);wire(0,238,y,347,c);label(0,358,y-7,outputs[n],c,Font::Small,100);progress(0,253,y+7,70,2,levels[n],c);}
  label(0,24,194,"Lines show connections / rails show gain",theme.secondary,Font::Small,430);
  unsigned assigned=8;for(unsigned n=0;n<8;++n)if(r.groups[n].members&(1u<<perf.selected_track())){assigned=n;break;}
  label(1,24,83,"SELECTED TRACK",theme.secondary,Font::Small,160);value(1,180,78,32,perf.selected_track()+1,theme.text,Font::Body,0);if(perf.shifted()){char selected_group[]="EDIT GROUP A";selected_group[11]=char('A'+perf.group_cursor());label(1,282,83,selected_group,theme.amber,Font::Small,165);}
  wire(1,24,132,107,t.output_bus?theme.teal:theme.disabled);flow_stage(1,120,132,"TRACK",t.output_bus?"POST FADER":"OUTPUT OFF",t.output_bus?theme.teal:theme.disabled);
  wire(1,134,132,259,t.output_bus?theme.teal:theme.disabled);
  char group_name[]="GROUP A";group_name[6]=char('A'+(assigned<8?assigned:0));flow_stage(1,272,132,assigned<8?group_name:"DIRECT",buses[assigned<8?r.groups[assigned].output:t.output_bus],t.output_bus?theme.teal:theme.disabled);wire(1,286,132,450,t.output_bus?theme.teal:theme.disabled);
  const auto take=perf.resample_view();auto destination=perf.recording_origin()!=RecordingOrigin::None?perf.recording_target():take.target;label(1,24,177,"CAPTURE",theme.secondary,Font::Small,75);label(1,104,177,capture_name(perf.recording_source()),theme.blue,Font::Small,90);unsigned ct=capture_track(perf.recording_source());if(ct<16 || perf.recording_source()>=53)value(1,196,172,24,ct<16?ct+1:perf.recording_source()-52,theme.blue,Font::Small,0);wire(1,228,184,279,theme.blue);label(1,292,177,"TAKE T",theme.secondary,Font::Small,58);value(1,352,172,30,destination.track+1,theme.text,Font::Small,0);label(1,390,177,perf.recording_origin()!=RecordingOrigin::None && !perf.resampling()?(perf.recording_quantized()?"BAR":"FREE"):resample_bar_label(take.bars),theme.secondary,Font::Small,70);
  label(1,24,194,perf.recording_status(),perf.live_recording()?theme.red:theme.secondary,Font::Small,430);
 }
 void effects(const ProjectState& p,const Performance& perf){
  const auto t=track_at(p,p.active_pattern,perf.selected_track());bool live=perf.visual().sample && perf.visual().pattern[perf.selected_track()]==p.active_pattern;
  const float* fx=live?perf.visual().track_insert[perf.selected_track()]:t.insert_fx;unsigned rate=perf.visual().rate?perf.visual().rate:48000;
  if(perf.fx_page()==3 || perf.fx_page()==5){
   bool chain=perf.fx_page()==5;label(0,24,83,"TRACK / SIGNAL ORDER",theme.secondary,Font::Small,420);
   static const char* names[]={"TONE","INSERT 1","INSERT 2","COMP"};static const unsigned order[]={0,3,1,2};ui::ContentNode nodes[4];for(unsigned i=0;i<4;++i){unsigned n=order[i];bool selected=chain?perf.chain_node()==n:n==perf.fx_focus_slot()+1;bool enabled=n==0 || (n==3?t.compressor[5]>.000001f:!(t.insert_disabled&(1u<<(n-1))) && fx[(n-1)*4]>0);float activity=n==0?perf.visual().node_activity[perf.selected_track()][2]:n==3?perf.visual().node_activity[perf.selected_track()][3]:perf.visual().insert_activity[perf.selected_track()][n-1];nodes[i]={names[n],enabled?"ON":"PASS",meter_amount(activity),selected,enabled};}signal_chain(0,24,110,430,nodes,4,ui::track_color(p,perf.selected_track(),theme));
   label(0,24,199,chain?"TURN: STAGE   PUSH: EDIT   TOP: BYPASS / SWAP":"TURN: SLOT   PUSH: EDIT   SHIFT + INSERTS: SWAP",theme.secondary,Font::Small,430);
   unsigned node=chain?perf.chain_node():perf.fx_focus_slot()+1;
   if(node==0)tone_picture(1,t,rate,&perf);
   else if(node<3){unsigned slot=(node-1)*4;insert_picture(1,unsigned(fx[slot]),fx[slot+1],fx[slot+2],fx[slot+3],(t.insert_disabled>>(node-1))&1,rate,perf.visual().vocoder_bands[perf.selected_track()][node-1],perf.visual().insert_activity[perf.selected_track()][node-1],perf.visual_fresh(),&perf,ui::track_color(p,perf.selected_track(),theme));}
   else compressor_picture(1,live?perf.visual().track_compressor[perf.selected_track()]:t.compressor,live?perf.visual().track_compressor_reduction[perf.selected_track()]:0,perf.visual_fresh()?perf.visual().track_compressor_input[perf.selected_track()]:-1);
   return;
  }
  if(perf.fx_page()==4){
   label(0,24,83,"DRUM PAD / VOICE PATH",theme.secondary,Font::Small,420);flow_stage(0,64,136,"SAMPLE","PAD VOICE",theme.secondary);wire(0,78,136,207,theme.secondary);flow_stage(0,220,136,"INSERT",track_insert_name(unsigned(perf.knob_value(1))),theme.teal,true);wire(0,234,136,357,theme.teal);flow_stage(0,370,136,"LEVEL / SEND","TO TRACK FX",theme.secondary);label(0,24,199,"TURN: PAD   PUSH: TYPE   SHIFT + KNOB: LOCK",theme.secondary,Font::Small,430);insert_picture(1,unsigned(perf.knob_value(1)),perf.knob_value(2),perf.knob_value(3),perf.knob_value(4),false,rate);return;
  }
  if(perf.fx_page()==1 || perf.fx_page()==2){bool track=perf.fx_page()==2;const float* c=track?(live?perf.visual().track_compressor[perf.selected_track()]:t.compressor):(perf.visual().sample?perf.visual().effects+6:p.effects.compressor);
   label(0,24,83,track?"TRACK / LINKED STEREO COMPRESSOR":"MASTER / LINKED STEREO COMPRESSOR",theme.secondary,Font::Small,430);
   wire(0,24,119,303,theme.teal);junction(0,84,119,theme.teal);outline(0,304,107,24,24,12,theme.teal);wire(0,310,119,322,theme.teal);wire(0,330,119,451,theme.teal);label(0,346,95,"AUDIO OUT",theme.secondary,Font::Small,105);
   if(c[6]>0){label(0,24,169,"KEY T",theme.blue,Font::Small,48);value(0,70,164,32,c[6],theme.blue,Font::Small,0);}else line(0,84,119,84,176,theme.blue);
   wire(0,102,176,183,theme.blue);outline(0,184,164,24,24,12,theme.blue);label(0,145,144,"DETECTOR",theme.secondary,Font::Small,100);wire(0,210,176,316,theme.blue);line(0,316,176,316,132,theme.blue);label(0,331,163,"GAIN CONTROL",theme.blue,Font::Small,120);
   label(0,24,194,"Audio path above / sidechain below",theme.secondary,Font::Small,430);compressor_picture(1,c,track?(live?perf.visual().track_compressor_reduction[perf.selected_track()]:0):perf.visual().compressor_reduction,perf.visual_fresh()?(track?perf.visual().track_compressor_input[perf.selected_track()]:perf.visual().compressor_input):-1);return;
  }
  float time=perf.visual().sample?perf.visual().effects[2]:p.effects.delay_ms,feedback=perf.visual().sample?perf.visual().effects[1]:p.effects.delay_feedback;
  const auto& delay=system_fx_visual(SystemFx::Delay).visual;VisualData repeats;repeats.available=true;repeats.count=128;repeats.label=delay.caption;repeats.live=perf.visual_fresh() && perf.visual().delay_input>.00001f;repeats.point=repeats.live;repeats.phase=0;repeats.position=ui::clamp01(perf.visual().delay_input);for(unsigned i=0;i<128;++i){float ms=2000*float(i)/127;unsigned echo=unsigned(ms/fmaxf(1,time));float local=ms-echo*time;repeats.values[i]=local<32?powf(feedback,float(echo))*(1-local/32):0;}visual_element(0,24,82,430,118,delay,repeats,theme.accent);
  const auto& reverb=system_fx_visual(SystemFx::Reverb).visual;auto tail=ui::response_visual(ResponseModel::Decay,perf.visual().sample?perf.visual().effects[4]:p.effects.reverb_decay,0);tail.label=reverb.caption;visual_element(1,24,82,430,118,reverb,tail,theme.mod);

 }
 void compressor_picture(unsigned side,const float* c,float reduction,float detector=-1){const auto& declaration=system_fx_visual(SystemFx::Compressor).visual;auto data=ui::response_visual(ResponseModel::Compressor,c[0],c[1]);data.live=detector>=0;data.amount=reduction;data.level=detector>=0?meter_amount(detector):0;data.point=detector>.000001f;data.phase=data.level;data.position=ui::clamp01((20*log10f(fmaxf(.00000001f,detector))+20*log10f(fmaxf(.0001f,1-reduction))+60)/60);data.label=declaration.caption;visual_element(side,24,82,430,118,declaration,data,theme.accent);}

 VisualData filter_spectrum(const Performance& perf,unsigned rate){auto source=ui::pcm_visual(perf,VisualSource::TrackSpectrum);VisualData projected;projected.available=source.available;projected.live=source.live;projected.count=128;for(unsigned i=0;i<128;++i){float hz=20*powf(fminf(rate*.45f,20000)/20,float(i)/127),bin=hz*512/rate;unsigned at=bin>=1?unsigned(logf(bin)/logf(256.f)*64):0;if(at>63)at=63;projected.values[i]=bin>=1?source.values[at]:0;}return projected;}
 void tone_picture(unsigned side,ConstTrackView t,unsigned rate,const Performance* perf=nullptr){const auto& declaration=system_fx_visual(SystemFx::Filter).visual;auto data=ui::response_visual(ResponseModel::LowPass,t.cutoff,0,rate);data.label=declaration.caption;
  if(perf && perf->analysis()){auto spectrum=filter_spectrum(*perf,rate);visual_spectrum(side,24,105,430,80,spectrum,theme.dim);data.live=true;}
  visual_element(side,24,82,430,118,declaration,data,theme.accent);
 }
