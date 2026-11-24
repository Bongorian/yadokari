// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "ui_renderer.h"
#include "wavetable_warp.h"
namespace groove {
auto UiRenderer::sampler(ConstTrackView t,const Performance& perf,WavView sample,const char* name) -> void{
  sampler_body(t,perf,sample,name);
  if(!perf.shifted() && !perf.live_recording() && !perf.recording_request_pending() && !perf.recording_file_busy())return;
  const auto target=perf.recording_target();unsigned source=perf.recording_source(),track=capture_track(source);
  rounded(1,14,178,452,28,0,theme.surface);
  label(1,24,181,perf.sampler_recording_status(),perf.live_recording()?theme.red:theme.secondary,Font::Small,430);
  label(1,24,196,"TAKE T",theme.secondary,Font::Small,50);value(1,78,192,25,target.track+1,theme.teal,Font::Small,0);
  label(1,112,196,"P",theme.secondary,Font::Small,12);value(1,128,192,25,target.pattern+1,theme.teal,Font::Small,0);
  if(target.kit_pad<16){label(1,166,196,"PAD",theme.secondary,Font::Small,33);value(1,186,192,25,target.kit_pad+1,theme.teal,Font::Small,0);}
  label(1,244,196,capture_name(source),theme.teal,Font::Small,82);
  if(track<16 || source>=53)value(1,334,192,25,source>=53?source-52:track+1,theme.teal,Font::Small,0);
  label(1,376,196,perf.resampling()?resample_bar_label(perf.resample_view().bars):perf.recording_quantized()?"BAR":"FREE",theme.secondary,Font::Small,76);
 }
auto UiRenderer::sample_wave(unsigned side,int x,int y,int w,int h,const Performance& perf,const SampleEdit& edit,bool point,float position,float first,float last,bool markers) -> void{
  const auto* peaks=wave_cache_.peaks();float span=fmaxf(.000001f,last-first);
  auto at=[&](float pos){return x+int(ui::clamp01((pos-first)/span)*(w-1));};
  int start=at(edit.start),end=at(edit.end);fill(side,x,y,w,h,theme.inset);if(end>start)fill(side,start,y,end-start+1,h,theme.raised);line(side,x,y+h/2,x+w-1,y+h/2,theme.border);
  if(peaks)for(int px=0;px<w;++px){float a=first+span*float(px)/w,b=first+span*float(px+1)/w;unsigned lo=unsigned(ui::clamp01(a)*kSampleWaveBins),hi=unsigned(ceilf(ui::clamp01(b)*kSampleWaveBins));if(lo>=kSampleWaveBins)lo=kSampleWaveBins-1;if(hi>kSampleWaveBins)hi=kSampleWaveBins;if(hi<=lo)hi=lo+1;float low=1,high=-1;for(unsigned n=lo;n<hi;++n){low=fminf(low,peaks->low[n]);high=fmaxf(high,peaks->high[n]);}int top=y+int(ui::clamp01(.5f-high*.5f)*(h-1)),bottom=y+int(ui::clamp01(.5f-low*.5f)*(h-1));fill(side,x+px,top,1,bottom-top+1,x+px>=start && x+px<=end?theme.accent:ui::mix(theme.inset,theme.accent,70));}
  else label(side,x+6,y+h/2-6,perf.browser().analysis_pending || wave_cache_.pending()?"ANALYZING WAVEFORM":"WAVEFORM NOT AVAILABLE",theme.secondary,Font::Small,w-12);
  if(markers){bool start_edit=false,end_edit=false;for(unsigned i=0;i<8;++i)if(perf.knob_touched(i) || (perf.knob_overlay_mask()&(1u<<i))){uint16_t active=perf.knob_parameter(i);
   if(engine_target(active)){const auto* d=engine_parameter(target_engine(active),target_local(active));if(d && d->target==EngineValueTarget::Sample)active=uint16_t(80+d->offset);}
   start_edit|=active==81;end_edit|=active==82;}
   uint32_t start_ink=start_edit?theme.accent:theme.secondary,end_ink=end_edit?theme.accent:theme.secondary;
   line(side,start,y-3,start,y+h,start_ink,start_edit?2:1);line(side,end,y-3,end,y+h,end_ink,end_edit?2:1);
   int sx=bounded(start, x,x+w-90),ex=bounded(end-32,sx+48,x+w-36);label(side,sx,y-15,"START",start_ink,Font::Small,44);label(side,ex,y-15,"END",end_ink,Font::Small,36);
  }
  if(point)line(side,at(position),y,at(position),y+h,theme.text,2);
 }
auto UiRenderer::sampler_body(ConstTrackView t,const Performance& perf,WavView sample,const char* name) -> void{
  bool kit=t.instrument==Instrument::DrumKit;const auto& pad=t.kit.pads[perf.selected_kit_pad()];const auto& edit=kit?pad.edit:t.sample_edit;unsigned sample_slice=kit?0:t.sample_slice;unsigned sample_slices=kit?(edit.slice_count?edit.slice_count:1):t.sample_slices;bool sample_loop=kit?edit.mode==SampleMode::Loop:t.sample_loop;bool sample_reverse=kit?pad.reverse:t.sample_reverse;unsigned sample_slot=kit?pad.sample[0]:t.sample_slot;unsigned root_note=kit?60:t.root_note;
  panel(0,10,79,460,128);scrolling_label(0,23,84,(sample.data || sample.reader)?name:"No sample loaded",theme.secondary,Font::Small,300);badge(0,368,83,88,perf.live_recording()?(perf.record_state()==4?"ARMED":perf.record_state()==5?"STOP ARMED":"REC"):sample_loop || edit.mode==SampleMode::Loop?"Loop":edit.mode==SampleMode::Gate?"Gate":edit.mode==SampleMode::PingPong?"Ping-pong":edit.mode==SampleMode::Stretch?"Stretch":"One-shot",perf.live_recording()?theme.red:theme.teal);
  if(perf.live_recording()){
   const auto& vis=perf.visual();auto signal=ui::pcm_visual(perf,VisualSource::CapturePcm);draw_visual(0,kCaptureVisual,signal,perf,perf.analysis()?perf.analysis()->sample:vis.sample,24,109,430,66);
   char elapsed[16];unsigned secs10=unsigned(vis.record_columns*1024*10/(vis.rate?vis.rate:48000));unsigned mm=secs10/600,ss=(secs10/10)%60,tt=secs10%10;elapsed[0]=char('0'+mm/10);elapsed[1]=char('0'+mm%10);elapsed[2]=':';elapsed[3]=char('0'+ss/10);elapsed[4]=char('0'+ss%10);elapsed[5]='.';elapsed[6]=char('0'+tt);elapsed[7]=0;
   label(0,24,186,"TAKE",theme.secondary,Font::Small,34);label(0,58,186,elapsed,theme.text,Font::Small,60);
  }
  else if((sample.data || sample.reader) && sample.frames){
   const auto& e=edit;unsigned count=e.slice_count?e.slice_count:sample_slices,index=sample_slice%count;float a=e.slice_count?e.markers[index]:float(index)/count,b=e.slice_count?e.markers[index+1]:float(index+1)/count;
   bool playing=perf.visual_fresh() && perf.visual().sample_active[perf.selected_track()] && perf.visual().sample_slot[perf.selected_track()]==sample_slot && perf.visual().instrument[perf.selected_track()]==unsigned(t.instrument) && perf.visual().pattern[perf.selected_track()]==perf.editing_pattern();
   sample_wave(0,20,119,440,44,perf,e,playing,perf.visual().sample_position[perf.selected_track()]);
   progress(0,20+int(440*(e.start+(e.end-e.start)*a)),165,int(440*(e.end-e.start)*(b-a)),2,1,theme.mod);
   for(unsigned i=0;i<=count;++i){float marker=e.start+(e.end-e.start)*(e.slice_count?e.markers[i]:float(i)/count);int x=20+int(440*marker);if(i && i<count)line(0,float(x),121,float(x),162,theme.border);}
   if(count>1){int first=20+int(439*(e.start+(e.end-e.start)*a)),last=20+int(439*(e.start+(e.end-e.start)*b));if(last<=first)last=first+1;if(last>459)last=459;line(0,first,121,first,162,theme.mod,1.5f);line(0,last,121,last,162,theme.mod,1.5f);}

  }
  if(!perf.live_recording() && !sample.frames){auto input=ui::pcm_visual(perf,VisualSource::CapturePcm);draw_visual(0,kCaptureVisual,input,perf,perf.analysis()?perf.analysis()->sample:perf.visual().sample,24,108,430,66);}
  if(!wave_cache_.ready() && (sample.data || sample.reader) && perf.browser().analysis_pending)label(0,24,185,"ANALYZING / source overview pending",theme.secondary,Font::Small,430);
  if(sample.reader && sample.reader->underruns()){label(0,300,112,"UNDERRUN",theme.amber,Font::Small,80);value(0,390,107,60,sample.reader->underruns(),theme.amber,Font::Body,0);}
  label(0,23,169,sample_reverse?"Reverse":"Forward",theme.secondary,Font::Small,100);label(0,128,169,"Slice",theme.secondary,Font::Small,40);value(0,172,164,29,sample_slice+1,theme.amber,Font::Body,0);if(sample.data || sample.reader){char format[48];format_value(format,float(sample.rate)/1000,1);unsigned fn=0;while(format[fn])++fn;copy_text(format+fn,sizeof format-fn,sample.reader?"k / SD stream":sample.format==3?"k / Float32":sample.bits==32?"k / PCM32":sample.bits==24?"k / PCM24":sample.bits==8?"k / PCM8":"k / PCM16");label(0,291,169,format,theme.secondary,Font::Small,163);}if(perf.live_recording()){advice("REC: STOP RECORDING");}else{unsigned source=perf.capture_source(),track=capture_track(source);label(0,23,190,"REC SOURCE:",theme.secondary,Font::Small,95);label(0,122,190,capture_name(source),theme.teal,Font::Small,70);if(track<16)value(0,198,190,24,track+1,theme.teal,Font::Small,0);advice("REC: SOURCE / SHIFT + REC: INPUT");}
  if(perf.sampler_page()==1){panel(1,10,79,460,128);label(1,24,85,"Attack / hold / release",theme.secondary,Font::Small,420);float duration=sample.frames && sample.rate?float(sample.frames)*1000/sample.rate*(edit.end-edit.start)/edit.speed:1000;float total=duration+edit.release_ms;float attack=fminf(0.8f,edit.attack_ms/fmaxf(total,1));float release=fminf(0.8f,edit.release_ms/fmaxf(total,1));graph_grid(1,24,112,420,58);envelope(1,24,112,420,58,attack,0,1,release,theme.teal);label(1,24,184,"Duration",theme.secondary,Font::Small,92);value(1,118,177,94,duration,theme.text,Font::Body,1);label(1,218,184,"ms",theme.secondary,Font::Small,28);badge(1,304,180,142,edit.mode==SampleMode::Loop?"LOOP":edit.mode==SampleMode::Gate?"GATE":edit.mode==SampleMode::PingPong?"PING-PONG":edit.mode==SampleMode::Stretch?"STRETCH":"ONE-SHOT",theme.teal);return;}
  if(perf.sampler_page()==2){static const char* layer_names[]={"LAYER: SINGLE","LAYER: VELOCITY","LAYER: ROUND ROBIN","LAYER: STACK"};panel(1,10,79,460,128);label(1,24,85,kit?layer_names[pad.layer_mode]:"16 pads / slice mapping",theme.secondary,Font::Small,420);for(unsigned i=0;i<16;++i){int x=24+int(i%8)*53,y=110+int(i/8)*35;rounded(1,x,y,46,29,4,kit && i==perf.selected_kit_pad()?theme.raised:theme.surface);unsigned slot=kit?t.kit.pads[i].sample[0]:edit.pads[i];value(1,x+4,y+2,37,kit?(slot<kSampleAssets?slot+1:0):slot+1,theme.teal,Font::Body,0);}advice(kit?"SELECT + PAD: SELECT   SHIFT + SELECT + PAD: SWAP":"Map Pads replaces tracks / Undo available");return;}   if(perf.sampler_page()==3){panel(1,10,79,460,128);label(1,24,85,"Zoom / current slice",theme.secondary,Font::Small,240);value(1,330,79,60,sample_slice+1,theme.text,Font::Number,0);unsigned count=edit.slice_count?edit.slice_count:sample_slices,index=sample_slice%count;float a=edit.slice_count?edit.markers[index]:float(index)/count,b=edit.slice_count?edit.markers[index+1]:float(index+1)/count;graph_grid(1,24,104,420,52);sample_wave(1,24,104,420,52,perf,edit,false,0,edit.start+(edit.end-edit.start)*a,edit.start+(edit.end-edit.start)*b,false);float duration=sample.frames && sample.rate?float(sample.frames)*1000/sample.rate*(edit.end-edit.start)/edit.speed:1000;float total=duration*(b-a)+edit.release_ms;float attack=fminf(0.8f,edit.attack_ms/fmaxf(total,1));float release=fminf(0.8f,edit.release_ms/fmaxf(total,1));graph_grid(1,24,164,420,26);envelope(1,24,164,420,26,attack,0,1,release,theme.amber);label(1,24,196,"SPEED: LIVE   START/END/ATTACK/RELEASE: NEXT TRIGGER",theme.secondary,Font::Small,425);return;}   if(perf.sampler_page()==4){panel(0,10,79,460,128);label(0,24,85,"Slices / pad assignment",theme.secondary,Font::Small,300);unsigned lcount=edit.slice_count?edit.slice_count:sample_slices;for(unsigned i=0;i<lcount && i<6;++i){int y=104+int(i)*15;bool cur=i==perf.slice_cursor();float sa=edit.slice_count?edit.markers[i]:float(i)/lcount,sb=edit.slice_count?edit.markers[i+1]:float(i+1)/lcount;value(0,24,y-6,30,i+1,cur?theme.amber:theme.text,Font::Body,0);value(0,60,y-6,52,sa*100,theme.secondary,Font::Body,1);label(0,116,y,"-",theme.secondary,Font::Small,10);value(0,128,y-6,52,sb*100,theme.secondary,Font::Body,1);label(0,184,y,"%",theme.secondary,Font::Small,12);int pad=-1;for(unsigned q=0;q<16;++q)if(edit.pads[q]==i){pad=int(q);break;}if(pad>=0){label(0,230,y,"PAD",theme.secondary,Font::Small,34);value(0,268,y-6,40,pad+1,theme.teal,Font::Body,0);}else label(0,230,y,"-",theme.secondary,Font::Small,20);if(edit.slice_warp[i])label(0,330,y,"WARP",theme.amber,Font::Small,44);}if(lcount>6)label(0,330,196,"...",theme.secondary,Font::Small,30);panel(1,10,79,460,128);label(1,24,85,edit.zone_count?"Zones / nearest zone wins":"Zones",theme.secondary,Font::Small,300);if(edit.slice_warp[lcount?perf.slice_cursor()%lcount:0])label(1,338,85,"WARP SYNC",theme.amber,Font::Small,120);if(!edit.zone_count){label(1,24,118,"Single sample / root only",theme.text,Font::Body,300);label(1,24,140,"Turn KEY LOW, KEY HIGH or ROOT to create one zone per slice.",theme.secondary,Font::Small,420);}else{for(unsigned i=0;i<edit.zone_count;++i){const auto& z=edit.zones[i];int x0=24+int(z.key_low)*432/128,x1=24+(int(z.key_high)+1)*432/128,y0=188-(int(z.velocity_high)+1)*88/128,y1=188-int(z.velocity_low)*88/128;fill(1,x0,y0,x1-x0,y1-y0,i==perf.slice_cursor()?theme.raised:theme.surface);if(i==perf.slice_cursor())selected_mark(1,x0,y0,x1-x0,y1-y0);int rx=24+int(z.root)*432/128,ry=(y0+y1)/2;rounded(1,rx-2,ry-2,5,5,2,i==perf.slice_cursor()?theme.amber:theme.teal);}}advice(perf.zone_vel()?"Push: KEY range   K6/K7: VELOCITY":"Push: VELOCITY range   K6/K7: KEY");return;}   panel(1,10,79,460,128);label(1,24,85,perf.sampler_page()==1?"Trim / envelope":perf.sampler_page()==2?"Slices / pad assignment":"Playback region",theme.secondary,Font::Small,240);value(1,24,104,104,sample_slice+1,theme.text,Font::Number,0);label(1,143,121,"of",theme.secondary,Font::Body,20);value(1,164,104,54,edit.slice_count?edit.slice_count:sample_slices,theme.text,Font::Number,0);
  label(1,24,153,"Pitch ratio",theme.secondary,Font::Body,125);value(1,150,145,91,edit.speed*powf(2,float(int(root_note)-60)/12),theme.teal,Font::Number,2);badge(1,296,116,150,sample_reverse?"REVERSE":"FORWARD",theme.blue);badge(1,296,153,150,sample_loop || edit.mode==SampleMode::Loop?"LOOP":edit.mode==SampleMode::Gate?"GATE":edit.mode==SampleMode::PingPong?"PING-PONG":edit.mode==SampleMode::Stretch?"STRETCH":"ONE-SHOT",theme.teal);
   }
static unsigned synth_edit_knobs(const Performance& perf){
 unsigned touched=0;
 for(unsigned i=0;i<8;++i)if(perf.knob_touched(i) && *perf.knob_label(i))touched|=1u<<i;
 return touched?touched:perf.knob_overlay_mask();
}
auto UiRenderer::synth_signal(const ProjectState& p,ConstTrackView t,const Performance& perf) -> void{
  if(t.instrument==Instrument::Empty){label(0,24,88,"EMPTY TRACK",theme.secondary,Font::Heading,430);advice("TOP 5: ENGINE / choose an instrument");advice("BROWSE: Load a sample, kit or preset");return;}
  if(t.instrument==Instrument::ExternalMidi){label(0,24,86,"Notes / expression / clock",theme.text,Font::Heading,430);label(0,24,124,"SEQUENCER",theme.blue,Font::Small,126);line(0,142,145,288,145,theme.blue,2);line(0,280,140,288,145,theme.blue,2);line(0,280,150,288,145,theme.blue,2);label(0,308,124,"MIDI OUT",theme.teal,Font::Small,145);advice("TOP 4: MIDI / TOP 5: ENGINE");label(0,24,197,"External audio is not measured here",theme.secondary,Font::Small,430);return;}
  const unsigned view=perf.instrument_view();
  const bool articulation=t.instrument==Instrument::Chimera && (view==2 || view==4);
  const auto* details=synth_detail_bank(t.instrument,view);
  const char* page=details?details->name:articulation?(view==2?"ARTICULATION 1/2":"ARTICULATION 2/2"):view<4?perf.soft_label(view):"";
  scrolling_label(0,24,82,track_display_name(t),theme.text,Font::Body,242);
  aligned_band(0,270,80,184,16,page,theme.secondary,Font::Small,ui::TextAlign::Right);
  line(0,24,98,454,98,theme.border);

  const unsigned active=synth_edit_knobs(perf);
  if(active && !(active&(active-1))){
   unsigned i=unsigned(__builtin_ctz(active));
   char key[]={'K',char('1'+i),0};
   label(0,24,108,key,theme.accent,Font::Body,25);
   scrolling_label(0,55,105,perf.knob_label(i),theme.text,Font::Heading,395);
   auto shown=formatted_knob(p,perf,i);
   scrolling_label(0,24,129,shown.text,theme.text,Font::Number,264);
   const float position=ui::clamp01(perf.knob_display_normalized(i));
   const auto id=perf.knob_parameter(i);
   const auto* d=id?target_descriptor(id):nullptr;
   const int x=24,w=430,rail=165,point=x+int(position*(w-1));
   fill(0,x,rail,w,3,theme.border);
   int start=x;
   if(d && d->minimum<0 && d->maximum>0)
    start+=int((-d->minimum)/(d->maximum-d->minimum)*(w-1));
   fill(0,point<start?point:start,rail,1+__builtin_abs(point-start),3,theme.accent);
   line(0,point,rail-3,point,rail+5,theme.text,2);
   if(d && d->maximum>d->minimum && perf.knob_reset_available(i)){
    const float initial=ui::clamp01((d->initial-d->minimum)/(d->maximum-d->minimum));
    const int mark=x+int(initial*(w-1));
    line(0,mark,rail+5,mark,rail+9,theme.secondary);
    label(0,bounded(mark-21,x,x+w-52),rail+11,"DEFAULT",theme.secondary,Font::Small,52);
    aligned_band(0,288,136,166,16,"TOUCH + ERASE: RESET",theme.secondary,Font::Small,ui::TextAlign::Right);
   }
   if(perf.synth_fine_target(i))
    aligned_band(0,225,149,229,13,"TOUCH + TURN 4-D FOR FINE",theme.secondary,Font::Small,ui::TextAlign::Right);
   if(d && d->apply_timing==ApplyNextTrigger)
    aligned_band(0,285,116,169,14,"NEXT TRIGGER",theme.secondary,Font::Small,ui::TextAlign::Right);
   return;
  }

  for(unsigned i=0;i<8;++i){
   const char* name=perf.knob_label(i);if(!*name)continue;
   uint16_t id=perf.knob_parameter(i);const auto* d=id?target_descriptor(id):nullptr;
   ui::ValueShape shape=d && d->minimum<0?ui::ValueShape::Bipolar:d && d->interpolation==Interpolation::Discrete?ui::ValueShape::Segments:ui::ValueShape::Bar;
   if(engine_target(id)){
    const auto* declaration=engine_parameter(target_engine(id),target_local(id));
    if((declaration && declaration->choice_count) || id==engine_target(Instrument::Chimera,ChimeraSceneSpeak) ||
       id==engine_target(Instrument::Chimera,ChimeraTalkOutput))shape=ui::ValueShape::Chip;
   }
   const bool selected=active&(1u<<i);
   const int x=24+int(i/4)*224,y=102+int(i%4)*24;
   char key[]={'K',char('1'+i),0};
   label_band(0,x,y,21,17,key,selected?theme.accent:theme.secondary,Font::Small);
   auto shown=formatted_knob(p,perf,i);
   value_row(0,x+23,y,183,21,{name,shown.text,perf.knob_display_normalized(i),shape,
    d && d->interpolation==Interpolation::Discrete?unsigned(d->maximum-d->minimum+1):0u,true,selected});
  }
  if(details)advice("TOP 4: NEXT DETAIL PAGE");
  if(articulation)advice(view==2?"TOP 3: ARTICULATION 2/2":"TOP 3: ARTICULATION 1/2 / SHIFT: SCENE SPEAK & TALK TO TRACK");
 }
auto UiRenderer::layer_picture(ConstTrackView t,const Performance& perf) -> void{
  label(0,24,82,"ONE NOTE / PARALLEL CHILD VOICES",theme.secondary,Font::Small,430);
  unsigned cost=0;layer_status(t.layer,&cost);auto reason=perf.layer_warning();if(reason==LayerReason::Ready && perf.visual().layer_failure[perf.selected_track()])reason=LayerReason(perf.visual().layer_failure[perf.selected_track()]);
  for(unsigned i=0;i<4;++i){int y=109+int(i)*23;bool used=i<t.layer.count;auto color=i==perf.layer_cursor()?theme.teal:theme.secondary;value(0,24,y,18,float(i+1),color,Font::Small,0);const char* name=used?(t.layer.parts[i].sound.preset_name[0]?t.layer.parts[i].sound.preset_name:layer_engine_name(t.layer.parts[i].sound.instrument)):"EMPTY / USE CHILD";scrolling_label(0,50,y,name,color,Font::Small,224);line(0,282,y+6,312,y+6,color,1);progress(0,320,y+4,55,4,used?t.layer.parts[i].level:0,color);line(0,380,y+6,408,y+6,color,1);line(0,408,y+6,408,154,color,1);}
  line(0,408,154,453,154,theme.teal,2);label(0,417,167,"SUM",theme.teal,Font::Small,40);
  if(perf.synth_monitor()!=Performance::SynthMonitor::Engine)return;
  label(1,24,82,layer_reason(reason),reason==LayerReason::Ready?theme.teal:theme.warn,Font::Small,430);
  advice("POOL SLOT / TOP 3: USE CHILD");const auto& candidate=perf.layer_candidate();scrolling_label(1,24,125,candidate.preset_name[0]?candidate.preset_name:layer_engine_name(candidate.instrument),theme.text,Font::Body,430);
  if(perf.layer_cursor()<t.layer.count){const auto& part=t.layer.parts[perf.layer_cursor()];char ranges[96];unsigned n=0;auto append=[&](const char* text){while(*text && n+1<sizeof ranges)ranges[n++]=*text++;ranges[n]=0;};auto number=[&](unsigned v){char text[16];format_value(text,float(v),0);append(text);};append("KEY ");number(part.key_low);append("-");number(part.key_high);append("   VEL ");number(part.velocity_low);append("-");number(part.velocity_high);append(part.fx.present?"   FX ON":"   FX OFF");label(1,24,153,ranges,theme.secondary,Font::Small,430);}
  label(1,24,172,"LEAF FX > SUM > TRACK FX > SENDS",theme.secondary,Font::Small,430);label(1,24,187,"NOTE COST",theme.secondary,Font::Small,100);value(1,132,187,38,float(cost),theme.teal,Font::Small,0);label(1,174,187,"/ 12   POOL IS NOT EXPANDED",theme.secondary,Font::Small,275);
 }
auto UiRenderer::sample_picture(ConstTrackView t,const Performance& perf) -> void{
   label(1,24,82,"SAMPLE WAVEFORM / TRIM & POSITION",theme.secondary,Font::Small,430);
   bool playing=perf.visual_fresh() && perf.visual().sample_active[perf.selected_track()] && perf.visual().sample_slot[perf.selected_track()]==t.sample_slot && perf.visual().instrument[perf.selected_track()]==unsigned(Instrument::Sampler) && perf.visual().pattern[perf.selected_track()]==perf.editing_pattern();
   sample_wave(1,24,119,430,64,perf,t.sample_edit,playing,perf.visual().sample_position[perf.selected_track()]);
   rounded(1,76,189,93,16,ui::Geometry::cell,theme.raised);char speed[24];format_value(speed,t.sample_edit.speed,2);label(1,24,192,"SPEED",theme.secondary,Font::Small,50);label(1,80,192,speed,theme.text,Font::Small,70);label(1,151,192,"x",theme.secondary,Font::Small,16);return;
 }
auto UiRenderer::chimera_picture(const Performance& perf) -> void{
  panel(1,10,79,460,128);
  label(1,24,86,"VOCAL TRACT",theme.secondary,Font::Small,130);
  const auto& frame=perf.visual();
  if(!perf.visual_fresh() || frame.taps.track!=perf.selected_track())return;
  if(!frame.chimera_available){label(1,24,139,"48 KHZ ENGINE UNAVAILABLE",theme.warn,Font::Small,400);return;}
  const auto& tract=frame.chimera;
  if(tract.model>=3 && tract.model<7){
   label(1,155,86,kChimeraSpecies[tract.model-3],theme.text,Font::Small,130);
   for(unsigned i=0;i<5 && tract.animal[i].name[0];++i){
    const auto& tube=tract.animal[i];
    int y=111+int(i)*20;
    label(1,24,y-5,tube.name,theme.secondary,Font::Small,94);
    const int width=bounded(int(30+sqrtf(tube.length)*23),35,320);
    int previous=0;
    for(unsigned j=0;j<ChimeraGeometry::sections;++j){
     int x=120+int(j)*width/int(ChimeraGeometry::sections-1);
     int radius=bounded(int(1+sqrtf(tube.radius[j])*3.2f),1,8);
     unsigned strength=unsigned(ui::clamp01(fabsf(tube.pressure[j]))*255);
     auto ink=ui::mix(theme.border,tube.pressure[j]<0?theme.teal:theme.warn,strength);
     line(1,x,y-radius,x,y+radius,ink,2);
     if(j){
      int back=120+int(j-1)*width/int(ChimeraGeometry::sections-1);
      line(1,back,y-previous,x,y-radius,theme.text,1);
      line(1,back,y+previous,x,y+radius,theme.text,1);
     }
     previous=radius;
    }
   }
   return;
  }
  // Face to the right. Frame coordinates are centimetres; normalize the
  // diagram to the selected body so the outline fits all three human models.
  using Point=ChimeraGeometry::Point;
  const float scale=tract.model==1?.90f:tract.model==2?.74f:1.f;
  auto x=[&](Point p){return bounded(int(117+14*p.x/scale),20,245);};
  auto y=[&](Point p){return bounded(int(194-6.7f*p.y/scale),95,204);};
  auto model_point=[&](float a,float b){return Point{a*scale,b*scale};};
  auto stroke=[&](const Point* pts,unsigned n,uint32_t ink,int width){
   for(unsigned i=1;i<n;++i)line(1,x(pts[i-1]),y(pts[i-1]),x(pts[i]),y(pts[i]),ink,width);
  };
  // Fill a closed sagittal contour with an even/odd scan. Use native LCD
  // rows to avoid gaps under the vertical layout transform.
  auto solid=[&](const Point* pts,unsigned n,uint32_t ink){
   constexpr unsigned limit=ChimeraGeometry::points;
   int xs[limit],ys[limit],top=272,bottom=0;
   for(unsigned i=0;i<n;++i){xs[i]=x(pts[i]);ys[i]=native_y(y(pts[i]));top=ys[i]<top?ys[i]:top;bottom=ys[i]>bottom?ys[i]:bottom;}
   for(int row=top;row<bottom;++row){
    int hits[limit];unsigned count=0;
    for(unsigned i=0,j=n-1;i<n;j=i++){
     if((ys[i]>row)==(ys[j]>row))continue;
     const int hit=xs[i]+(xs[j]-xs[i])*(row-ys[i])/(ys[j]-ys[i]);
     unsigned k=count++;while(k && hits[k-1]>hit){hits[k]=hits[k-1];--k;}hits[k]=hit;
    }
    for(unsigned k=1;k<count;k+=2)line_pixels(1,hits[k-1],row,hits[k],row,ink,1);
   }
  };
  constexpr unsigned n=ChimeraGeometry::points;
  const Point upper=tract.outer[n-1],lower=tract.inner[n-1];
  Point head[]={{-3.6f,-1.1f},{-3.7f,2.2f},{-4.2f,4.0f},{-5.6f,6.1f},
   {-6.1f,8.6f},{-5.7f,11.1f},{-4.4f,12.9f},{-2.5f,14.0f},{.1f,14.5f},{2.7f,14.1f},
   {4.8f,13.1f},{6.1f,11.9f},{6.5f,11.0f},{6.35f,10.6f},{6.7f,10.0f},{8.2f,9.0f},
   {8.25f,8.7f},{7.75f,8.55f},{7.15f,8.62f},{7.0f,8.2f}};
  for(auto& p:head)p=model_point(p.x,p.y);
  const Point chin[]={lower,{lower.x+.12f*scale,lower.y-.30f*scale},
   {lower.x-.32f*scale,lower.y-.72f*scale},{lower.x-.13f*scale,lower.y-1.27f*scale},
   {lower.x-.55f*scale,lower.y-1.65f*scale},model_point(4.9f,4.2f),model_point(2.6f,3.2f),
   model_point(1.8f,1.3f),model_point(1.8f,-1.1f)};
  static_assert(sizeof head/sizeof *head+1+sizeof chin/sizeof *chin<=n,"head contour capacity");
  Point silhouette[n];unsigned count=0;
  for(auto p:head)silhouette[count++]=p;
  silhouette[count++]=upper;for(auto p:chin)silhouette[count++]=p;
  solid(silhouette,count,ui::mix(theme.surface,theme.secondary,42));
  stroke(head,sizeof head/sizeof *head,theme.secondary,2);
  stroke(chin,sizeof chin/sizeof *chin,theme.secondary,2);
  const Point nose_to_lip[]={head[19],upper};stroke(nose_to_lip,2,theme.secondary,2);
  const Point brow[]={model_point(4.0f,11.75f),model_point(4.9f,12.0f),model_point(5.8f,11.8f)};
  const Point eye[]={model_point(4.35f,11.52f),model_point(5.1f,11.40f),model_point(5.55f,11.53f)};
  stroke(brow,3,theme.secondary,1);stroke(eye,3,theme.text,1);
  const auto nostril=model_point(7.65f,8.85f);
  rounded(1,x(nostril)-2,y(nostril)-1,5,2,1,theme.background);
  // The air column, tongue, palate and lips follow the audio model.
  for(unsigned i=0;i<ChimeraGeometry::sections-1;++i){
   const Point cell[]={tract.section_outer[i],tract.section_outer[i+1],tract.section_inner[i+1],tract.section_inner[i]};
   const float pressure=tract.pressure[i];const auto ink=ui::mix(theme.background,pressure<0?theme.teal:theme.warn,unsigned(ui::clamp01(fabsf(pressure))*210));
   solid(cell,4,ink);
  }
  solid(tract.tongue,n,ui::mix(theme.surface,theme.teal,140));
  stroke(tract.inner,n,theme.secondary,1);stroke(tract.outer,n,theme.text,1);
  const auto nasal_ink=ui::mix(theme.blue,theme.text,90);
  stroke(tract.nasal,n,nasal_ink,2);stroke(tract.velum,n,nasal_ink,1);stroke(tract.tongue,n,theme.teal,2);
  stroke(tract.outer+n-3,3,theme.warn,2);stroke(tract.inner+n-3,3,theme.warn,2);
  auto tag=[&](Point p,int row,const char* name,uint32_t color){
   int px=x(p),py=y(p);
   line(1,px,py,242,row,theme.border,1);line(1,242,row,249,row,color,1);
   rounded(1,px-2,py-2,5,4,2,color);label(1,254,row-4,name,color,Font::Small,85);
  };
  tag(tract.nasal[21],105,"NASAL",nasal_ink);
  tag(tract.tongue[17],129,"TONGUE",theme.teal);
  tag(upper,151,"LIPS",theme.warn);
  tag(tract.glottis,184,"GLOTTIS",theme.text);
  label(1,254,84,frame.chimera_live?"LIVE":"REST SHAPE",theme.secondary,Font::Small,90);
  aligned_band(1,350,81,102,15,kChimeraBodies[tract.model<3?tract.model:0],theme.secondary,Font::Small,ui::TextAlign::Right);
  label(1,347,111,"AIR PRESSURE",theme.secondary,Font::Small,107);
  const int axis=148;
  line(1,348,axis,451,axis,theme.border,1);
  for(unsigned i=0;i<ChimeraGeometry::sections;++i){
   int height=int(22*tract.pressure[i]);int px=349+int(i)*6;
   if(height)line(1,px,axis,px,axis-height,height>0?theme.warn:theme.teal,3);
  }
  label(1,347,175,"THROAT > LIPS",theme.secondary,Font::Small,108);
  for(unsigned i=0;i<ChimeraGeometry::sections;++i){
   const Point a=tract.section_inner[i],b=tract.section_outer[i];
   const float dx=a.x-b.x,dy=a.y-b.y;
   if(dx*dx+dy*dy>0.02f*0.02f)continue;
   fill(1,x(a)-2,y(a)-2,5,5,theme.warn);
   label(1,347,192,"CONTACT",theme.warn,Font::Small,108);
   break;
  }
 }
void UiRenderer::synth_monitor_picture(const Performance& perf){
 const bool scope=perf.synth_monitor()==Performance::SynthMonitor::Scope;
 const auto& frame=perf.visual();
 const auto& tap=frame.taps;
 const auto* fft=scope?nullptr:perf.analysis();
 const bool current=scope?(perf.visual_fresh() && tap.sample && tap.generation==frame.generation &&
   tap.track==perf.selected_track() && tap.source==255):fft!=nullptr;
 panel(1,10,79,460,128);
 label(1,24,84,scope?"OUTPUT WAVEFORM":"OUTPUT SPECTRUM",theme.text,Font::Body,212);
 aligned_band(1,240,83,214,16,"TRACK  /  POST FADER",theme.secondary,Font::Small,ui::TextAlign::Right);
 if(!current){
  label(1,24,140,scope?"WAITING FOR TRACK PCM":"WAITING FOR TRACK FFT",theme.secondary,Font::Small,400);
  return;
 }
 constexpr int left=52,top=110,width=400,height=65;
 const unsigned rate=scope?tap.rate:fft->rate;
 for(int i=0;i<=4;++i){int px=left+i*(width-1)/4;line(1,px,top,px,top+height-1,theme.border);}
 char unit[24];
 if(scope){
  // Start at a rising zero crossing when the buffer contains one. A fixed
  // time window makes the period comparable between notes.
  constexpr unsigned samples=kVisualPcm/2;
  unsigned first=0;
  for(unsigned i=1;i<=kVisualPcm-samples;++i)if(tap.pcm[1][i-1]<0 && tap.pcm[1][i]>=0){first=i;break;}
  VisualData wave;wave.count=samples/2;
  float peak=0;
  for(unsigned i=0;i<wave.count;++i){
   const float a=tap.pcm[1][first+2*i],b=tap.pcm[1][first+2*i+1];
   wave.low[i]=fminf(a,b);wave.values[i]=fmaxf(a,b);
   peak=fmaxf(peak,fmaxf(fabsf(a),fabsf(b)));
  }
  unsigned gain=1;
  if(peak>0)while(gain<16 && peak*gain<=.45f)gain*=2;
  for(unsigned i=0;i<wave.count;++i){wave.low[i]*=gain;wave.values[i]*=gain;}
  const char* span=gain==16?"1/16":gain==8?"1/8":gain==4?"1/4":gain==2?"1/2":"1";
  char scale[8]={'+',0};copy_text(scale+1,sizeof scale-1,span);
  label(1,12,top-3,scale,theme.secondary,Font::Small,38);
  label(1,24,top+height/2-4,"0",theme.secondary,Font::Small,25);
  scale[0]='-';label(1,12,top+height-8,scale,theme.secondary,Font::Small,38);
  visual_scope(1,left,top,width,height,wave,theme.teal);
  label(1,left,184,"0 ms",theme.secondary,Font::Small,51);
  if(peak>0){format_value(unit,20*log10f(peak),1);label(1,187,184,"PEAK",theme.secondary,Font::Small,40);aligned_band(1,226,181,51,15,unit,theme.text,Font::Small,ui::TextAlign::Right);label(1,281,184,"dBFS",theme.secondary,Font::Small,42);}
  else label(1,211,184,"SILENCE",theme.secondary,Font::Small,85);
  format_value(unit,1000.f*samples/(rate?rate:48000),2);
  aligned_band(1,364,181,65,15,unit,theme.secondary,Font::Small,ui::TextAlign::Right);
  label(1,434,184,"ms",theme.secondary,Font::Small,24);
 }else{
  VisualData bands;bands.count=kVisualBands;
  for(unsigned i=0;i<bands.count;++i)bands.values[i]=fft->spectrum[1][i];
  visual_spectrum(1,left,top,width,height,bands,theme.accent);
  label(1,18,top-4,"0 dB",theme.secondary,Font::Small,34);
  label(1,18,top+height-8,"-80",theme.secondary,Font::Small,32);
  for(unsigned i=0;i<3;++i){
   const float hz=float(rate)*(1u<<(4*i))/kVisualPcm;
   format_value(unit,hz>=1000?hz/1000:hz,hz>=1000 && hz<10000?1:0);
   unsigned n=unsigned(__builtin_strlen(unit));copy_text(unit+n,sizeof unit-n,hz>=1000?" kHz":" Hz");
   aligned_band(1,left+int(i)*(width-80)/2,181,80,15,unit,theme.secondary,Font::Small,
    i==0?ui::TextAlign::Left:i==1?ui::TextAlign::Center:ui::TextAlign::Right);
  }
 }
}
auto UiRenderer::synth(ConstTrackView t,const Performance& perf) -> void{

  if(perf.instrument_view()==3 && (unsigned(t.instrument)>=unsigned(Instrument::WaveSequence) && unsigned(t.instrument)<=unsigned(Instrument::SpectralFreeze))){panel(1,10,79,460,128);static const char* lanes[]={"Sample","Pitch","Timing","Gate","Shape","Step mod"};label(1,24,85,"Engine lab / independent lanes",theme.secondary,Font::Body,270);label(1,306,85,t.creative.follower_source?"FOLLOW TRK":"FOLLOW MASTER",theme.secondary,Font::Body,104);if(t.creative.follower_source)value(1,414,85,20,t.creative.follower_source,theme.teal,Font::Body,0);value(1,436,85,34,t.creative.follower_amount,theme.teal,Font::Body,0);for(unsigned l=0;l<6;++l){int y=111+int(l)*14;label(1,24,y,lanes[l],l==perf.creative_lane()?theme.teal:theme.secondary,Font::Small,80);for(unsigned n=0;n<8;++n){int x=111+int(n)*41;rounded(1,x,y,35,11,3,n<t.creative.length[l]?theme.raised:theme.background);if(l==perf.creative_lane() && n==perf.creative_cell())fill(1,x+3,y+2,9,2,theme.amber);progress(1,x+3,y+7,29,2,float(t.creative.value[l][n])/127,theme.teal);}}return;}

  const auto* engine=engine_descriptor(unsigned(t.instrument));if(!engine)return;auto declaration=engine->visual;
  if(perf.instrument_view()==1 && !perf.wave_profile()){tone_picture(1,t,perf.visual().rate,&perf);return;}
  if(engine->page.hero==HeroVisual::VocalTract){chimera_picture(perf);return;}
  if(perf.instrument_view()==2 && (engine->instrument.envelope&EnvelopeEditable))declaration={VisualElement::Curve,VisualSource::Envelope,VisualCadence::Audio,VisualFallback::Parameters,ResponseModel::None,"AMPLITUDE ENVELOPE"};
  if(t.instrument==Instrument::Fm6 && (perf.instrument_view()==0 || synth_detail_bank(t.instrument,perf.instrument_view()))){
   const auto* bank=synth_detail_bank(t.instrument,perf.instrument_view());
   const bool detune=bank && bank->locals[0]==11;
   label(1,24,83,detune?"OPERATOR DETUNE":"OPERATOR LEVELS",theme.text,Font::Body,300);
   aligned_band(1,308,82,145,16,detune?"-50 ... +50 ct":"0 ... 2.00 x",theme.secondary,Font::Small,ui::TextAlign::Right);
   const unsigned active=synth_edit_knobs(perf);
   for(unsigned op=0;op<6;++op){
    const int x=28+int(op)*71,baseline=detune?136:166;
    const unsigned local=(detune?11:5)+op;
    const float setting=ui::recipe_value(t,local,&perf);
    const int end=detune?baseline-int(setting*.55f):baseline-int(setting*28);
    const bool focus=bank && bank->locals[op]==local && (active&(1u<<op));
    const auto ink=focus?theme.accent:theme.teal;
    fill(1,x,108,52,59,theme.inset);line(1,x,baseline,x+51,baseline,theme.secondary);
    fill(1,x+4,end<baseline?end:baseline,44,1+__builtin_abs(end-baseline),ink);
    char number[24];format_value(number,setting,(detune?1:2)+(focus?1:0));
    centered_label(1,x-3,168,58,14,number,theme.text,Font::Small);
    char name[]={'O','P',' ',char('6'-op),0};centered_label(1,x-3,184,58,14,name,ink,Font::Small);
   }
   return;
  }
  if(t.instrument==Instrument::Wavetable && perf.instrument_view()==kFirstSynthDetailView){
   if(!wavetable_bank_ready()){label(1,24,140,"WAVETABLE BANK UNAVAILABLE",theme.secondary,Font::Body,430);return;}
   VisualData cycle;cycle.available=true;cycle.bipolar=true;cycle.count=128;
   const unsigned track=perf.selected_track();
   const auto& frame=perf.visual();
   const bool sounding=perf.visual_fresh() && frame.instrument[track]==unsigned(Instrument::Wavetable)
    && frame.pattern[track]==perf.editing_pattern() && frame.envelope[track]>=.00001f;
   cycle.label=sounding?"BASE TABLE MODEL / PLAYED PITCH":"BASE TABLE MODEL / C4";
   const unsigned selection=unsigned(t.wavetable.values[WavetableTable]);
   if(selection>=kWavetableFirstAsset){
    label(1,24,118,"USER TABLE",theme.text,Font::Body,430);
    label(1,24,147,"4-D: VIEW LIVE WAVEFORM",theme.secondary,Font::Small,430);return;
   }
   const auto* bank=factory_wavetable(selection);
   const float control=sounding?frame.wavetable_position[track]*127.f:ui::recipe_value(t,0,&perf);
   const float position=bank?bank->position(control):control;
   const unsigned a=unsigned(position),b=a+1<(bank?bank->frames:kWavetableWaves)?a+1:a;
   const unsigned mode=unsigned(ui::recipe_value(t,WavetableWarpMode,&perf));
   const float warp=ui::recipe_value(t,WavetableWarpAmount,&perf);
   const float ratio=ui::recipe_value(t,WavetablePmRatio,&perf);
   const float depth=ui::recipe_value(t,WavetablePmAmount,&perf);
   const auto effective=sounding?frame.wavetable_map[track]:
    wavetable_phase_map(mode,warp,ratio,depth,261.6256f/float(frame.rate?frame.rate:48000),ui::recipe_value(t,7,&perf));
   const auto levels=wavetable_extended_levels(effective);
   auto sample=[&](float phase){
    const float at=wavetable_wrap(phase)*kWavetableSamples;
    const unsigned left=unsigned(at)%kWavetableSamples,right=(left+1)%kWavetableSamples;
    auto read=[&](unsigned wave,unsigned mip){
     const auto* data=bank?bank->wave(wave,mip):kWavetableData[wave][mip];
     return data[left]+(data[right]-data[left])*(at-float(left));
    };
    auto table=[&](unsigned mip){const float first=read(a,mip);return first+(read(b,mip)-first)*(position-a);};
    const float hi=table(levels.mip);
    return (hi+(table(levels.next)-hi)*levels.blend)/32767.f;
   };
   for(unsigned i=0;i<cycle.count;++i){
    const float phase=float(i)/float(cycle.count-1);
    float shape;
    if(effective.spectral)shape=wavetable_spectral_sample(effective,effective.step,phase,phase*effective.ratio,position,levels.mip,levels.blend,bank);
    else {
     const auto map=wavetable_read_phases(phase,effective.mode,effective.warp,phase*effective.ratio,effective.pm);
     const float first=sample(map.first);shape=first+(sample(map.second)-first)*map.blend;
    }
    cycle.values[i]=ui::clamp01(.5f+.5f*shape);
   }
   visual_element(1,24,82,430,118,{VisualElement::Curve},cycle,theme.teal);
   return;
  }
  if(t.instrument==Instrument::Formant && perf.instrument_view()==0){
   float frequencies[2];formant_frequencies(4*ui::recipe_value(t,1,&perf),exp2f(ui::recipe_value(t,2,&perf)/12),frequencies[0],frequencies[1]);
   label(1,24,83,"VOWEL FORMANTS",theme.text,Font::Body,225);
   aligned_band(1,260,82,192,16,"BASE FREQUENCIES",theme.secondary,Font::Small,ui::TextAlign::Right);
   for(unsigned i=0;i<2;++i){
    const int row=115+int(i)*37;
    label(1,24,row,i?"F2":"F1",theme.secondary,Font::Body,36);
    const uint32_t ink=i?theme.accent:theme.teal;
    const float pos=ui::clamp01(log2f(frequencies[i]/64)/8);
    const int marker=70+int(pos*282);
    line(1,70,row+7,352,row+7,theme.border,2);
    line(1,marker,row+2,marker,row+12,ink,3);
    char number[24];format_value(number,frequencies[i],0);
    aligned_band(1,367,row-2,58,19,number,theme.text,Font::Body,ui::TextAlign::Right);
    label(1,432,row+2,"Hz",theme.secondary,Font::Small,22);
   }
   label(1,70,181,"64 Hz",theme.secondary,Font::Small,60);
   aligned_band(1,260,179,94,16,"16 kHz",theme.secondary,Font::Small,ui::TextAlign::Right);
   return;
  }
  if(t.instrument==Instrument::PhaseDist && perf.instrument_view()==0){
   VisualData cycle;cycle.available=true;cycle.count=128;cycle.bipolar=true;
   const unsigned shape=unsigned(ui::recipe_value(t,0,&perf));
   const float depth=ui::recipe_value(t,1,&perf);
   cycle.label="OSCILLATOR CYCLE  /  BASE DISTORTION";
   for(unsigned i=0;i<cycle.count;++i)cycle.values[i]=.5f+.5f*sinf(6.2831853f*phase_distortion_map(float(i)/127,shape,depth));
   visual_element(1,24,82,430,118,{VisualElement::Curve},cycle,theme.teal);
   return;
  }
  if(declaration.source==VisualSource::SamplePeaks){sample_picture(t,perf);return;}
  if(perf.instrument_view()==0 && declaration.recipe!=VisualRecipe::Controls){
   auto model=ui::control_recipe(t,false,&perf);
   if(model.presentation==VisualElement::Curve){
    model.label=declaration.recipe==VisualRecipe::Fm2?"FM CYCLE  /  FILTER MODEL":"OSCILLATOR MIX  /  FILTER MODEL";
    visual_element(1,24,82,430,118,declaration,model,theme.teal);
    return;
   }
  }
  auto data=ui::engine_visual_data(declaration,t,perf);draw_visual(1,declaration,data,perf,perf.analysis()?perf.analysis()->sample:perf.visual().sample);

 }
}
