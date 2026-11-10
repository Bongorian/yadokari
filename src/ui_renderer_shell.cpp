// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "ui_renderer.h"
#include "layer_controls.h"
namespace groove {
auto UiRenderer::audio_input_name(unsigned source,char* out) -> const char*{unsigned t=capture_track(source);if(t<16){out[0]='T';out[1]=char('0'+(t+1)/10);out[2]=char('0'+(t+1)%10);out[3]=' ';copy_text(out+4,28,capture_name(source));return out;}if(source>=53){copy_text(out,32,"Group 0");out[6]=char('0'+source-52);return out;}return source==1?"Line In":capture_name(source);}
auto UiRenderer::pattern(const ProjectState& p,const Performance& perf) -> void{
  const auto t=track_at(p,p.active_pattern,perf.selected_track());uint32_t color=ui::track_color(p,perf.selected_track(),theme);
  bool overview=perf.mode()!=UiMode::Step || perf.step_overview();unsigned first=overview?(perf.page()/4)*64:perf.page()*16,count=overview?64:16;
  if(t.instrument==Instrument::DrumKit && perf.mode()==UiMode::Step && perf.select_held()){for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){unsigned n=(3-row)*4+col,slot=t.kit.pads[n].sample[0]<kSampleAssets?t.kit.pads[n].sample[0]:t.kit.pads[n].sample[1];int x=10+int(col)*116,y=81+int(row)*27;rounded(0,x,y,112,24,ui::Geometry::cell,n==perf.selected_kit_pad()?theme.raised:theme.surface);value(0,x+4,y+2,24,n+1,theme.teal,Font::Small,0);scrolling_label(0,x+30,y+6,slot<kSampleAssets && p.assets[slot].path[0]?filename(p.assets[slot].path):"Empty",slot<kSampleAssets?theme.text:theme.disabled,Font::Small,78);}advice("SELECT + PAD: CHOOSE SOUND");return;}
  if(t.instrument==Instrument::DrumKit){unsigned pad=perf.selected_kit_pad();for(unsigned n=0;n<count;++n){unsigned i=first+n;const auto& s=t.kit.steps[pad][i];bool locked=s.sample_lock<kSampleAssets || s.start_lock<255 || s.pitch_lock || s.reverse_lock<2 || s.filter_lock<255 || s.level_lock<128 || s.delay_lock<128 || s.reverb_lock<128;for(const auto& lock:t.steps[i].locks)locked|=lock.enabled;bool playing=perf.playing() && p.clips[perf.selected_track()].playing && p.active_pattern==p.clips[perf.selected_track()].pattern && i==perf.position(perf.selected_track());if(overview)step_cell(0,10+int(n%16)*29,81+int(n/16)*26,25,22,i+1,s.enabled,playing,i==perf.selected_step(),locked,i<t.length,color);else{int x=10+int(n%8)*58,y=81+int(n/8)*54;step_cell(0,x,y,52,47,i+1,s.enabled,playing,i==perf.selected_step(),locked,i<t.length,color);value(0,x+5,y+24,42,s.velocity,s.enabled?theme.text:theme.disabled,Font::Body,0);}}label(0,14,190,"KIT PAD",theme.secondary,Font::Small,70);value(0,84,185,34,pad+1,theme.teal,Font::Body,0);advice("SELECT + PAD: SOUND / TURN: STEP");return;}
  for(unsigned n=0;n<count;++n){unsigned i=first+n;bool locked=false;for(const auto& lock:t.steps[i].locks)locked|=lock.enabled;bool playing=perf.playing() && p.clips[perf.selected_track()].playing && p.active_pattern==p.clips[perf.selected_track()].pattern && i==perf.position(perf.selected_track());if(overview)step_cell(0,10+int(n%16)*29,81+int(n/16)*26,25,22,i+1,t.steps[i].enabled || t.steps[i].lock_only,playing,i==perf.selected_step(),locked,i<t.length,color);else{int x=10+int(n%8)*58,y=81+int(n/8)*54;step_cell(0,x,y,52,47,i+1,t.steps[i].enabled || t.steps[i].lock_only,playing,i==perf.selected_step(),locked,i<t.length,color);if(t.steps[i].lock_only && !t.steps[i].enabled)label(0,x+5,y+26,"LOCK",theme.amber,Font::Small,42);else value(0,x+5,y+24,42,t.steps[i].velocity,t.steps[i].enabled?theme.text:theme.disabled,Font::Body,0);}{int x=overview?10+int(n%16)*29:10+int(n%8)*58,y=overview?81+int(n/16)*26:81+int(n/8)*54;const auto& s=t.steps[i];step_attributes(0,x+(overview?9:6),y+(overview?17:25),s.slide(),s.accent(),s.enabled && i<t.length,overview);}
   unsigned extra=0;for(uint8_t note:t.steps[i].chord)extra+=note<128;if(extra){int x=overview?10+int(n%16)*29:10+int(n%8)*58;int y=overview?81+int(n/16)*26:81+int(n/8)*54;for(unsigned c=0;c<(overview && extra>2?2:extra);++c)rounded(0,x+3+int(c)*4,y+(overview?19:41),overview?2:3,overview?2:3,1,t.steps[i].enabled?theme.teal:theme.disabled);}}
  for(unsigned page=0;page<4;++page)rounded(0,10+int(page)*20,196,14,3,1,page==perf.page()%4?color:theme.border);
  if(perf.mode()!=UiMode::Keyboard || !perf.shifted()){label(0,104,189,"Length",theme.secondary,Font::Small,50);value(0,156,187,29,t.length,theme.text,Font::Body,0);label(0,213,189,"Selected step",theme.secondary,Font::Small,89);value(0,302,187,31,perf.selected_step()+1,theme.text,Font::Body,0);}
  if(perf.mode()==UiMode::Keyboard && perf.shifted()){advice("SHIFT + PADS: EDIT CHORD");}else if(perf.mode()==UiMode::Step){if(perf.note_recording() && p.ui.note_replace && track_at(p,p.clips[perf.note_record_track()].pattern,perf.note_record_track()).instrument!=Instrument::DrumKit){label(0,363,189,"REC TRACK",theme.red,Font::Small,76);value(0,439,187,31,perf.note_record_track()+1,theme.red,Font::Body,0);}else label(0,363,189,overview?"64-step view":"Velocity / 16",theme.secondary,Font::Small,107);}
 }
auto UiRenderer::pads(const ProjectState& p,const Performance& perf) -> void{
  const auto selected=track_at(p,p.active_pattern,perf.selected_track());bool kit=perf.pad_role()==ui::PadRole::Kit || perf.pad_role()==ui::PadRole::KitSelect || perf.pad_role()==ui::PadRole::KitMute || perf.pad_role()==ui::PadRole::KitSolo;
  if(perf.mode()==UiMode::Keyboard){note_grid(p,perf,false);return;}
  for(unsigned n=0;n<16;++n){int x=10+int(n%4)*117,y=81+int(mk3::pad_logical(n)/4)*30;const auto t=track_at(p,p.clips[n].pattern,n);bool active=kit?n==perf.selected_kit_pad():n==perf.selected_track();rounded(0,x,y,109,26,ui::Geometry::panel,active?theme.raised:theme.surface);if(active)selected_mark(0,x,y,109,26);value(0,x+5,y,24,n+1,theme.secondary,Font::Small,0);unsigned slot=kit?(selected.kit.pads[n].sample[0]<kSampleAssets?selected.kit.pads[n].sample[0]:selected.kit.pads[n].sample[1]):t.sample_slot;const char* name=kit?(slot<kSampleAssets && p.assets[slot].path[0]?filename(p.assets[slot].path):"EMPTY"):track_display_name(t,p.audio[n].kind);const auto kind=kit?ui::track_engine_kind(p,perf.selected_track()):ui::track_engine_kind(p,n);kind_chip(0,x+35,y+1,69,kind);scrolling_label(0,x+5,y+14,name,(kit?selected.kit.pads[n].muted:perf.audible_muted(n))?theme.disabled:theme.text,Font::Small,99);}
 }
auto UiRenderer::kit_panel(const ProjectState& p,const Performance& perf) -> void{
  const auto t=track_at(p,p.active_pattern,perf.selected_track());unsigned index=perf.selected_kit_pad();const auto& pad=t.kit.pads[index];const auto& step=t.kit.steps[index][perf.selected_step()];
  panel(1,10,79,460,128);label(1,24,86,"DRUM KIT / PAD",theme.secondary,Font::Small,180);value(1,215,77,55,index+1,theme.teal,Font::Number,0);badge(1,320,83,125,pad.muted?"MUTED":"READY",pad.muted?theme.red:theme.teal);
  const char* name=pad.sample[0]<kSampleAssets && p.assets[pad.sample[0]].path[0]?filename(p.assets[pad.sample[0]].path):"No sample";
  scrolling_label(1,24,121,name,theme.text,Font::Body,420);label(1,24,143,"Level",theme.secondary,Font::Small,58);value(1,83,137,72,pad.level*100,theme.text,Font::Body,0);label(1,169,143,"Pan",theme.secondary,Font::Small,44);value(1,213,137,63,pad.pan*100,theme.text,Font::Body,0);label(1,292,143,"Choke",theme.secondary,Font::Small,60);value(1,357,137,45,pad.edit.choke,theme.text,Font::Body,0);
  label(1,24,167,"Step",theme.secondary,Font::Small,45);value(1,70,161,45,perf.selected_step()+1,theme.teal,Font::Body,0);label(1,136,167,"Sample lock",theme.secondary,Font::Small,96);if(step.sample_lock<kSampleAssets)value(1,238,161,49,step.sample_lock+1,theme.amber,Font::Body,0);else label(1,238,167,"OFF",theme.secondary,Font::Small,49);label(1,309,167,"Ratchet",theme.secondary,Font::Small,63);value(1,378,161,45,step.ratchet,theme.text,Font::Body,0);
  advice("PAD: PLAY / SELECT   STEP: PROGRAM   MUTE: PAD");
 }
auto UiRenderer::library_catalog(const ProjectState& p,const Performance& perf) -> void{
 const auto& b=perf.browser();unsigned first=b.cursor/6*6;
 for(unsigned i=first;i<b.count && i<first+6;++i){int y=81+int(i-first)*20;bool selected=i==b.cursor;
  list_row(0,10,y,460,19,{"","","",selected,true,0});
  scrolling_label(0,20,y+4,filename(b.entries[i].name),selected?theme.text:theme.secondary,Font::Small,275);
  kind_chip(0,306,y+2,74,ui::engine_kind(b.library_engine[i]));label(0,390,y+4,b.library_factory[i]?"FACTORY":"USER",theme.secondary,Font::Small,70);
 }
 if(!b.count)empty_state(0,20,90,440,b.library_kits?"NO KITS":"NO SOUNDS FOR THIS ENGINE","CHANGE KIND, ENGINE OR ORIGIN / FILES");
 scrolling_label(1,20,86,track_display_name(p.tracks[perf.selected_track()],p.audio[perf.selected_track()].kind),theme.secondary,Font::Small,440);
 if(b.cursor<b.count){scrolling_label(1,20,125,b.entries[b.cursor].name,theme.text,Font::Body,440);label(1,20,160,b.library_factory[b.cursor]?"FACTORY":"USER",theme.secondary,Font::Small,150);label(1,350,139,"TRACK",theme.secondary,Font::Small,100);value(1,351,153,100,perf.selected_track()+1,theme.teal,Font::Body,0);label(1,180,160,"REV",theme.secondary,Font::Small,50);value(1,230,155,100,b.library_revision[b.cursor],theme.text,Font::Small,0);if(b.previewing)badge(1,20,183,150,"PREVIEW",theme.teal);}
 advice(ui::FileToast::instruction(b.status)?b.status:"");
}
auto UiRenderer::browser(const ProjectState& p,const Performance& perf) -> void{const auto& b=perf.browser();if(b.new_project_list){panel(0,10,79,460,128);panel(1,10,79,460,128);label(0,24,85,"NEW PROJECT / EMPTY + TEMPLATES",theme.secondary,Font::Small,430);unsigned first=b.cursor/4*4;for(unsigned i=0;i<4 && first+i<b.count;++i){unsigned row=first+i;int y=111+int(i)*22;if(row==b.cursor)fill(0,20,y-2,438,21,theme.raised);label(0,26,y,b.entries[row].name,row==b.cursor?theme.text:theme.secondary,Font::Body,425);}label(1,24,85,b.folder_offset+b.cursor?"TEMPLATE > UNTITLED":"EMPTY PROJECT",theme.text,Font::Heading,430);label(1,24,126,"FIRST SAVE: SAVE AS",theme.secondary,Font::Body,430);label(1,24,158,"FACTORY SOURCE IS KEPT",theme.secondary,Font::Body,430);label(1,24,185,"TOP 1 OPEN / TOP 2 CANCEL",theme.secondary,Font::Small,430);return;}if(perf.browser_editing()){const auto& e=b.edit;const char* title=e.operation==BrowserOperation::RenameTrack?"RENAME TRACK":e.operation==BrowserOperation::SavePreset?(e.duplicate?"DUPLICATE PRESET":"SAVE PRESET AS"):e.operation==BrowserOperation::OverwritePreset?"OVERWRITE PRESET":e.operation==BrowserOperation::SaveProject?"SAVE PROJECT AS":e.operation==BrowserOperation::Delete?"DELETE":e.operation==BrowserOperation::Rename?"RENAME":"NEW FOLDER";panel(0,10,79,460,128);panel(1,10,79,460,128);if(perf.preset_fx_editing()){label(0,24,174,"KNOB 3 / INCLUDE FX",theme.secondary,Font::Small,300);badge(0,350,169,100,e.include_fx?"ON":"OFF",e.include_fx?theme.teal:theme.dim);label(0,24,192,e.duplicate?"KNOB 4 / NEW GUID":"KNOB 4 / SAME LINEAGE",theme.secondary,Font::Small,420);}label(0,24,88,title,theme.text,Font::Body,420);label(0,24,119,e.source[0]?filename(e.source):b.directory,theme.secondary,Font::Body,420);advice("APPLY: TOP 1   CANCEL: TOP 2");advice(ui::FileToast::instruction(b.status)?b.status:"");if(e.operation==BrowserOperation::OverwritePreset){label(1,24,88,"Replace this saved preset?",theme.warn,Font::Body,420);label(1,24,126,e.name,theme.text,Font::Body,420);advice("TOP 1: OVERWRITE   TOP 2: CANCEL");}else if(e.operation==BrowserOperation::Delete){label(1,24,88,"Permanently remove this entry?",theme.warn,Font::Body,420);label(1,24,126,"Referenced or uncertain files are kept",theme.secondary,Font::Small,420);}else{char shown[25]{},prefix[25]{};unsigned first=(e.cursor/24)*24,i=0;while(i<24 && e.name[first+i]){shown[i]=e.name[first+i];if(i<e.cursor-first)prefix[i]=shown[i];++i;}label(1,24,88,"NAME",theme.secondary,Font::Small,420);label(1,24,118,shown,theme.text,Font::Body,420);label(1,24+ui::text_width(prefix,Font::Body,false),140,"^",theme.teal,Font::Small,20);advice("TURN: CHARACTER   KNOB 1: CURSOR");advice("TOP 5: INSERT   TOP 6: ERASE");}return;}const auto& meta=perf.preview_metadata();bool favorite=b.preview_path[0]?perf.sample_favorite(b.preview_path):meta.favorite;label(0,14,80,b.directory[0]?b.directory:"DATA / root",theme.secondary,Font::Small,330);if(b.favorites_only)badge(0,360,78,110,"FAVORITES",theme.teal);if(!b.count)empty_state(0,24,115,430,"No entries",b.favorites_only?"No favorites in this folder":"Open a folder or add content");unsigned first=(b.cursor/4)*4;for(unsigned i=0;i<4 && first+i<b.count;++i){unsigned index=first+i;const auto& e=b.entries[index];int y=98+int(i)*22;bool selected=index==b.cursor;list_row(0,10,y,460,20,{e.name,"",e.directory?"DIR":e.unsupported?"UNSUPPORTED":is_recording_part(e.name)?"PART":is_soundfont_file(e.name)?"SF2":is_dx7_file(e.name)?"DX7":is_midi_file(e.name)?"MIDI":e.content_kind==1?"SONG":e.content_kind==2?"KIT":e.content_kind==3?"PATCH":browser_project_kind(e.name)?"MGX":"WAV",selected,true,0});}
  label(0,14,190,b.status,b.missing?theme.red:theme.secondary,Font::Small,345);value(0,365,186,42,b.count?b.folder_offset+b.cursor+1:0,theme.text,Font::Small,0);label(0,410,190,"/",theme.secondary,Font::Small,8);value(0,422,186,48,b.folder_total,theme.secondary,Font::Small,0);panel(1,10,79,460,128);bool sf2=b.cursor<b.count && !b.entries[b.cursor].directory && is_soundfont_file(b.entries[b.cursor].name);bool dx7=b.cursor<b.count && !b.entries[b.cursor].directory && is_dx7_file(b.entries[b.cursor].name);bool midi=b.cursor<b.count && !b.entries[b.cursor].directory && is_midi_file(b.entries[b.cursor].name);scrolling_label(1,23,85,(midi||dx7||sf2)?b.entries[b.cursor].name:b.selected_name[0]?b.selected_name:"Select a sample",theme.text,Font::Body,midi||!b.preview.frames?434:280);if(sf2){label(1,23,120,"SOUNDFONT BANK",theme.text,Font::Body,420);label(1,23,148,"OPEN: ASSIGN TO TRACK",theme.text,Font::Body,420);label(1,23,176,"SYNTH: BANK / PROGRAM",theme.secondary,Font::Small,420);}else if(dx7){label(1,23,120,"DX7 / 32 VOICES",theme.text,Font::Body,420);label(1,23,148,"OPEN: IMPORT PRESET FILES",theme.text,Font::Body,420);label(1,23,176,"Bank kept / pool unchanged",theme.secondary,Font::Small,420);}else if(midi){label(1,24,120,"Source channel",theme.secondary,Font::Body,180);value(1,210,112,55,track_at(p,p.active_pattern,perf.selected_track()).midi_channel+1,theme.teal,Font::Number,0);advice(perf.midi_import_confirmation()?(perf.modal_progress()?"IMPORTING   LEFT: CANCEL":"PUSH: IMPORT   LEFT: CANCEL"):"SHIFT + IMPORT: REVIEW STEPS");label(1,24,180,track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::ExternalMidi?"RPN / NRPN may transmit on import":"Select MIDI OUT for RPN / NRPN",theme.amber,Font::Small,420);}else if(b.preview.frames){{SampleEdit whole{};sample_wave(1,24,115,420,44,perf,whole,perf.visual().preview_active && perf.previewing_displayed() && perf.preview_frame_current(),perf.visual().preview_position,0,1,false);if(b.previewing && perf.previewing_displayed()){auto pcm=ui::pcm_visual(perf,VisualSource::CapturePcm);visual_scope(1,24,160,420,12,pcm,theme.mod);}}value(1,24,174,73,float(b.preview.frames)/b.preview.rate,theme.text,Font::Body,2);label(1,100,183,"s",theme.secondary,Font::Small,16);value(1,120,174,48,b.preview.rate/1000.0f,theme.text,Font::Body,1);label(1,171,183,"kHz",theme.secondary,Font::Small,30);label(1,218,183,"Root",theme.secondary,Font::Small,34);static const char* pitches[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};label(1,255,181,pitches[meta.root%12],theme.text,Font::Small,18);value(1,275,175,17,int(meta.root/12)-1,theme.text,Font::Body,0);label(1,315,183,"BPM",theme.secondary,Font::Small,34);if(meta.bpm)value(1,356,175,54,meta.bpm,theme.text,Font::Body,0);else label(1,358,181,"--",theme.secondary,Font::Body,30);badge(1,320,84,131,b.analysis_pending && b.analysis_pending_source==(b.preview.reader?static_cast<const void*>(b.preview.reader):static_cast<const void*>(b.preview.data))?"ANALYZING":b.previewing?(perf.previewing_displayed()?"PREVIEW / CUE":"OTHER / CUE"):favorite?"FAVORITE":"Ready",b.analysis_pending && b.analysis_pending_source==(b.preview.reader?static_cast<const void*>(b.preview.reader):static_cast<const void*>(b.preview.data))?theme.amber:b.previewing?(perf.previewing_displayed()?theme.blue:theme.amber):favorite?theme.amber:theme.secondary);}
  (void)p;
 }
auto UiRenderer::diagnostics(const Performance& perf) -> void{
  const auto& d=perf.diagnostics();
  section(0,24,77,430,"USB");key_value(0,24,99,430,"LINK",d.connected?"CONNECTED":"DISCONNECTED");
  char text[40];format_value(text,d.endpoint_count,0);key_value(0,24,118,200,"ENDPOINTS",text);format_value(text,d.transfer_errors,0);key_value(0,252,118,200,"ERRORS",text);
  section(0,24,139,430,"HID");format_value(text,d.reports,0);key_value(0,24,160,200,"REPORTS",text);
  for(unsigned i=0;i<6 && i<d.last_hid_size;++i)value(0,252+int(i)*33,160,30,d.last_hid[i],theme.secondary,Font::Small,0);
  section(0,24,181,430,"AUDIO");label(0,24,203,d.audio_drain_stalled?"DRAIN STALLED":d.audio_active?"USB AUDIO ACTIVE":"USB AUDIO UNAVAILABLE",d.audio_drain_stalled?theme.danger:theme.secondary,Font::Small,260);
  format_value(text,d.tx_channels,0);key_value(0,24,221,200,"TX CH",text);format_value(text,d.rx_channels,0);key_value(0,252,221,200,"RX CH",text);
  section(1,24,77,430,"MIDI");format_value(text,d.midi_in,0);key_value(1,24,99,200,"IN",text);format_value(text,d.midi_out,0);key_value(1,252,99,200,"OUT",text);
  format_value(text,d.midi_dropped,0);key_value(1,24,118,200,"DROPPED",text);label(1,252,118,d.midi_status==MidiDiagnosticStatus::Ready?"READY":"NOT READY",theme.secondary,Font::Small,200);
  section(1,24,139,430,"LOG");unsigned offset=perf.diagnostic_offset()<d.log_count?perf.diagnostic_offset():0;
  for(unsigned i=0;i<3 && i+offset<d.log_count;++i){const auto& e=d.log[(d.log_cursor+15-i-offset)%16];int y=162+int(i)*23;label(1,24,y,e.kind==DiagnosticKind::TransferError?"TRANSFER ERROR":e.kind==DiagnosticKind::Hid?"HID":e.kind==DiagnosticKind::MidiIn?"MIDI IN":"MIDI OUT",e.kind==DiagnosticKind::TransferError?theme.danger:theme.secondary,Font::Small,170);value(1,204,y,170,float(e.timestamp/1000),theme.text,Font::Small,0);label(1,390,y,"ms",theme.secondary,Font::Small,40);}
  advice("TURN: LOG SCROLL / EVENTS: UNDO");
 }
auto UiRenderer::audio_lane_page(const ProjectState& p,const Performance& perf) -> void{
  unsigned t=perf.selected_track();const auto& a=p.audio[t];bool loop=a.kind==TrackKind::Looper;unsigned selected=loop?0:perf.audio_clip_cursor();const auto& clip=a.clips[selected];uint32_t color=ui::track_color(p,t,theme);
  label(0,16,82,loop?"LOOPER / SIGNAL FLOW":a.kind==TrackKind::Audio?"AUDIO / SONG TIMELINE":"SELECT LOOPER OR AUDIO ABOVE",theme.text,Font::Bold,450);
  if(loop){
   for(unsigned i=0;i<8;++i){if(!*perf.knob_label(i))continue;auto shown=formatted_knob(p,perf,i);value_row(0,24+int(i/4)*224,104+int(i%4)*25,206,23,{perf.knob_label(i),shown.text,perf.knob_normalized(i),ui::ValueShape::Bar,0,true,perf.knob_touched(i)});}
   VisualData data;data.available=clip.asset<64;data.point=perf.visual().sample_active[t];data.phase=perf.visual().sample_position[t];data.layers=data.available?(perf.audio_undo_available(t)?2:1):0;data.recording=perf.live_recording();data.recorded=data.recording?ui::clamp01(float(perf.visual().record_columns)/512):data.available?1:0;data.live=data.point || data.recording;data.label=data.available?filename(p.assets[clip.asset].path):"EMPTY / RECORD A LOOP";draw_visual(1,kLoopVisual,data,perf,perf.visual().sample,24,82,430,118);return;
  }else{uint32_t extent=3840*8;for(const auto& c:a.clips)if(c.asset<64 && c.tick+3840>extent)extent=c.tick+3840;for(unsigned n=0;n<9;++n){int x=18+n*54;line(0,x,113,x,184,theme.border,1);}for(unsigned n=0;n<kAudioClips;++n){const auto& c=a.clips[n];if(c.asset>=64)continue;unsigned rate=p.assets[c.asset].metadata.rate;float ticks=rate?float(c.frames)*p.tempo*16/rate:3840;int x=18+int(float(c.tick)/extent*432),w=int(ticks/extent*432);if(w<3)w=3;if(w>450-x)w=450-x;fill(0,x,120+int(n%3)*20,w,16,n==selected?theme.raised:theme.surface);fill(0,x,120+int(n%3)*20,w,3,color);if(n==selected)selected_mark(0,x,120+int(n%3)*20,w,16);}label(0,18,190,"K3 MOVE / K4-5 TRIM / Top6 DUPLICATE",theme.secondary,Font::Small,435);}
  label(1,18,82,perf.live_recording()?recording_hint(perf.recording_controls(),perf.record_state()):!a.enabled?"STOPPED":clip.asset>=64?"EMPTY / RECORD OR BROWSER LOAD":"READY / REAL-TIME AUDIO",perf.live_recording()?theme.red:theme.teal,Font::Small,440);
  scrolling_label(1,18,106,clip.asset<64?filename(p.assets[clip.asset].path):"No audio asset",theme.text,Font::Body,440);
  auto signal=ui::pcm_visual(perf,VisualSource::CapturePcm);draw_visual(1,kCaptureVisual,signal,perf,perf.analysis()?perf.analysis()->sample:perf.visual().sample,18,127,440,64);
  advice("PADS: TRACK / SHIFT Top2: SONG");
 }
auto UiRenderer::mutation(const ProjectState& p,const Performance& perf) -> void{const auto t=track_at(p,p.active_pattern,perf.selected_track());panel(0,10,79,460,128);label(0,24,85,"Original / live mutation",theme.secondary,Font::Small,320);label(0,360,85,"Page",theme.secondary,Font::Small,40);value(0,409,81,35,perf.page()+1,theme.text,Font::Body,0);for(unsigned i=0;i<16;++i){unsigned cell=perf.page()*16+i;auto step=mutation_step(t,cell);int x=24+int(i)*27;rounded(0,x,111,22,26,ui::Geometry::cell,cell>=t.length?theme.border:t.steps[cell].enabled?theme.blue:theme.raised);rounded(0,x,146,22,26,ui::Geometry::cell,cell>=t.length?theme.border:step.enabled?theme.teal:theme.raised);}label(0,24,183,"Blue: source    Teal: transformed",theme.secondary,Font::Small,420);panel(1,10,79,460,128);label(1,24,85,t.mutation.frozen?"Frozen mutation target":"Seeded pattern mutation",t.mutation.frozen?theme.amber:theme.secondary,Font::Small,400);value(1,24,108,110,t.mutation.amount,theme.text,Font::Number,0);label(1,138,126,"%",theme.secondary,Font::Small,25);progress(1,24,156,430,6,float(t.mutation.amount)/100,theme.teal);advice("Strip: amount   Shift + Generate: Freeze");}
auto UiRenderer::module_picture(const ProjectState& p,const Performance& perf) -> void{
  for(unsigned side=0;side<2;++side){label(side,24,81,"MIDI CHANNEL > TRACK / PRESET",theme.secondary,Font::Small,430);for(unsigned row=0;row<8;++row){unsigned t=side*8+row;int y=101+int(row)*11;auto color=t==perf.selected_track()?theme.teal:theme.secondary;value(side,24,y,24,float(t+1),color,Font::Small,0);line(side,55,y+5,85,y+5,color,1);value(side,93,y,24,float(t+1),color,Font::Small,0);const auto& track=p.tracks[t];scrolling_label(side,128,y,p.audio[t].kind!=TrackKind::Instrument?"AUDIO / MIDI DISABLED":track_display_name(track,p.audio[t].kind),color,Font::Small,290);progress(side,426,y+4,27,3,perf.visual().peak[t],theme.teal);}}
  label(0,24,194,"BANK: CC0=0 / CC32=0 OR 1   PC=1-128",theme.secondary,Font::Small,430);
  static const char* status[]={"SEQUENCER STOPPED / TOP 5: EXIT MODULE","BANK OUT OF RANGE / SOUND KEPT","EMPTY POOL SLOT / SOUND KEPT","UNKNOWN ENGINE / SOUND KEPT","UNAVAILABLE OR BUSY / SOUND KEPT","AUDIO TRACK / SOUND KEPT"};unsigned reason=perf.visual().module_reason[perf.selected_track()];label(1,24,194,status[reason<6?reason:4],reason?theme.warn:theme.secondary,Font::Small,430);
 }
auto UiRenderer::project(const ProjectState& p,const Performance& perf) -> void{
  if(perf.browser_editing()){browser(p,perf);return;}

  const char* names[]={"PROJECT FILES","STORE POOL","RECALL POOL"};
  for(unsigned i=0;i<3;++i)list_row(0,16,91+int(i)*29,448,26,{names[i],"","",perf.project_list_cursor()==i,true,0});
  advice("TURN: ACTION / PUSH: APPLY / K4: POOL SLOT");
  scrolling_label(1,24,85,perf.bake_collect_path()?filename(perf.bake_collect_path()):perf.project_name(),theme.text,Font::Body,430);if(perf.bake_collect_path())advice("TOP 5: COLLECT PRESET + SAMPLE");
  if(perf.project_list_cursor()){const auto& sound=p.sounds[perf.sound_slot()];scrolling_label(1,24,116,sound.preset_name[0]?sound.preset_name:instrument_descriptor(unsigned(sound.instrument))->name,theme.secondary,Font::Heading,430);label(1,24,154,"POOL SLOT",theme.secondary,Font::Small,150);value(1,190,149,80,perf.sound_slot()+1,theme.text,Font::Body,0);}
  else {unsigned start=perf.asset_cursor()>1?perf.asset_cursor()-1:0;if(start>61)start=61;for(unsigned i=0;i<3;++i){unsigned n=start+i;int y=116+int(i)*23;value(1,24,y,28,n+1,theme.secondary,Font::Small,0);scrolling_label(1,67,y,p.assets[n].path[0]?p.assets[n].path:"Empty asset",p.assets[n].path[0]?theme.text:theme.dim,Font::Body,385);}}
 }
auto UiRenderer::settings(const Performance& perf) -> void{
  static const char* headings[]={"APPEARANCE","LCD OUTPUT","PAD / STRIP LIGHTS","PAD RESPONSE","AUDIO INPUT / TUNER","MIDI CONTROL","DATA / SYSTEM"};unsigned page=perf.settings_page();
  label(0,24,82,headings[page],theme.secondary,Font::Small,420);
  for(unsigned i=0;i<perf.settings_count();++i){unsigned id=perf.setting_control(i);value_row(0,24,(perf.settings_count()>3?102+int(i)*24:105+int(i)*27),430,perf.settings_count()>3?22:25,{perf.setting_name(i),perf.setting_text(i),perf.setting_row_normalized(i),ui::settings_value_shape(id),id==6?3u:0u,true,i==perf.settings_cursor()});}
  advice(perf.device_saved()?"TURN: SELECT ITEM / K1: VALUE":"DEVICE SETTINGS NOT YET SAVED");
  if(page==0){label(1,24,85,ui::Theme::name(perf.visual_settings().theme),theme.text,Font::Heading,430);label(1,24,119,"Aa 0123456789",theme.text,Font::Number,420);uint32_t colors[]={theme.teal,theme.amber,theme.red,theme.blue,theme.text,theme.secondary};for(unsigned i=0;i<6;++i)rounded(1,24+int(i)*72,161,60,7,3,colors[i]);label(1,24,191,"Shared type, color and motion on every page",theme.secondary,Font::Small,430);}
  else if(page==1 && perf.setting_control(perf.settings_cursor())==1){label(1,24,82,"THEME / DEVICE",theme.secondary,Font::Small,430);for(unsigned i=0;i<ui::Theme::kCount;++i){int x=24+int(i%3)*146,y=110+int(i/3)*31;bool selected=i==perf.visual_settings().theme;fill(1,x,y,138,27,selected?theme.raised:theme.surface);if(selected)selected_mark(1,x,y,138,27);centered_label(1,x,y,138,27,ui::Theme::name(i),selected?theme.accent:theme.secondary,Font::Body);}}
  else if(page==1){label(1,24,85,"Software screen level",theme.text,Font::Heading,430);uint32_t colors[]={0xffff0000,0xff00ff00,0xff0000ff};for(unsigned i=0;i<3;++i){rounded(1,24+int(i)*144,124,128,32,0,colors[i]);label(1,24+int(i)*144,164,i==0?"RED":i==1?"GREEN":"BLUE",theme.secondary,Font::Small,128);}label(1,24,191,"Pixel intensity / does not change LCD backlight",theme.secondary,Font::Small,430);}

  else if(page==2){
   label(1,24,82,"PAD INTENT / WIRE PREVIEW",theme.text,Font::Small,430);
   using S=ui::LedState;const S states[]={S::Muted,S::Assigned,S::Selected,S::Playing,S::Queued,S::Recording};const char* names[]={"DIM / 18%","NORMAL / 55%","FOCUS / 100%","MOTION / PLAY","WARN / WAIT","DANGER / REC"};
   for(unsigned i=0;i<6;++i){int x=24+int(i%3)*148,y=102+int(i/3)*18;fill(1,x,y,10,10,ui::PadPalette::rgb(states[i],theme,{perf.selected_engine_kind()},0,ui::Motion::Off));label(1,x+15,y,names[i],theme.secondary,Font::Small,130);}
   label(1,24,140,"ENGINE KIND / FIXED COLORS",theme.secondary,Font::Small,430);
   for(unsigned i=0;i<7;++i)kind_chip(1,24+int(i%4)*110,158+int(i/4)*18,94,ui::EngineKind(i));
   label(1,24,197,"Indexed wire profile / physical calibration pending",theme.secondary,Font::Small,430);
   ui::LedFrame leds;perf.led_frame(leds);for(unsigned i=0;i<16;++i)fill(0,24+int(i)*26,170,20,6,leds.pad_rgb[mk3::pad_logical(i)]);
  }

  else if(page==3){label(1,24,85,"Strike pressure > velocity",theme.text,Font::Heading,430);graph_grid(1,40,112,390,67);for(unsigned n=1;n<=64;++n)line(1,40+(n-1)*390.f/64,179-pad_velocity((n-1)*4095/64,perf.device_settings().velocity)*67.f/127,40+n*390.f/64,179-pad_velocity(n*4095/64,perf.device_settings().velocity)*67.f/127,theme.accent,2);label(1,24,191,"Initial strike only / FIXED VEL takes priority",theme.secondary,Font::Small,430);}
  else if(page==4){label(0,24,165,"L",theme.secondary,Font::Small,16);level_meter(0,50,166,174,5,meter_amount(perf.visual().input_lr[0]),theme.teal);label(0,246,165,"R",theme.secondary,Font::Small,16);level_meter(0,270,166,174,5,meter_amount(perf.visual().input_lr[1]),theme.teal);const auto& t=perf.tuner();label(1,24,85,"TUNER / A4 = 440 Hz",theme.secondary,Font::Small,420);if(t.note<0)label(1,24,118,"Play a steady single note",theme.text,Font::Heading,420);else{static const char* pitches[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};char note[8];copy_text(note,sizeof note,pitches[t.note%12]);unsigned n=unsigned(__builtin_strlen(note));note[n++]=char('0'+t.note/12-1);note[n]=0;label(1,24,112,note,theme.text,Font::Number,90);value(1,145,115,100,t.hz,theme.text,Font::Body,1);label(1,252,118,"Hz",theme.secondary,Font::Small,25);value(1,295,115,88,t.cents,fabsf(t.cents)<5?theme.teal:theme.amber,Font::Body,1);label(1,390,118,"ct",theme.secondary,Font::Small,28);}line(1,40,161,440,161,theme.border);for(int ct=-50;ct<=50;ct+=25){int x=240+ct*4;line(1,x,155,x,167,ct?theme.secondary:theme.accent);}if(t.note>=0){float x=240+t.cents*4;line(1,x,149,x,173,theme.accent,2);}label(1,26,174,"-50",theme.secondary,Font::Small,38);label(1,235,174,"0",theme.secondary,Font::Small,20);label(1,421,174,"+50",theme.secondary,Font::Small,38);label(1,24,191,"Digital gain / tuner does not enable monitoring",theme.secondary,Font::Small,430);}
  else if(page==5){label(1,24,85,perf.device_settings().local?"PADS > INTERNAL SOUND":"PADS > MIDI OUT",theme.text,Font::Heading,430);label(1,24,122,"MIDI IN > TRACKS / always received",theme.secondary,Font::Body,430);label(1,24,150,perf.multitimbral()?"Module: Ch1-16 > Track1-16":"Normal: configured input routes",theme.accent,Font::Body,430);advice("Top6: routes / Module exit stays stopped");}
  else{label(1,24,85,"DATA FREE",theme.text,Font::Heading,430);if(perf.storage_valid()){value(1,24,119,280,float(double(perf.storage_free())/(perf.system_space_unit()?1048576.:1073741824.)),theme.text,Font::Number,2);label(1,330,132,perf.system_space_unit()?"MiB":"GiB",theme.secondary,Font::Body,110);progress(1,24,165,420,10,perf.storage_total()?float(double(perf.storage_free())/perf.storage_total()):0,theme.accent);}else label(1,24,119,"Unavailable",theme.warn,Font::Body,420);label(1,24,191,"No format / no automatic media erase",theme.secondary,Font::Small,430);advice("Top7: diagnostics / Top8: update");}

 }
auto UiRenderer::gesture_page(const ProjectState& p,const Performance& perf) -> void{
  panel(0,10,79,460,128);label(0,24,85,"Gesture / one-bar loop",theme.secondary,Font::Body,400);for(unsigned n=0;n<8;++n)line(0,24,110+n*10,453,110+n*10,theme.border,1,100);for(unsigned i=0;i<p.gesture.count;++i){const auto& e=p.gesture.points[i];int x=24+int(float(e.tick)/p.gesture.ticks*429),y=110+(e.target==GestureTarget::Morph?7:e.target==GestureTarget::Macro?e.parameter%8:e.parameter%7)*10;rounded(0,x,y,3,6,1,e.target==GestureTarget::Morph?theme.amber:theme.teal);}if(perf.gesture_recording() || perf.gesture_playing())line(0,24+perf.visual().gesture_phase*429,108,24+perf.visual().gesture_phase*429,184,theme.amber,1.5f);label(0,24,190,perf.gesture_recording()?"Recording knobs + Strip":perf.gesture_playing()?"Playing / non-destructive layer":"REC starts a new gesture",perf.gesture_recording()?theme.red:theme.teal,Font::Body,420);
  panel(1,10,79,460,128);label(1,24,85,"Recorded movements",theme.secondary,Font::Body,250);value(1,24,108,109,p.gesture.count,theme.text,Font::Number,0);label(1,140,122,"/ 128 points",theme.secondary,Font::Body,190);label(1,24,154,"Record on any page / 96 PPQN",theme.text,Font::Body,430);advice("Shift + CLEAR confirms erase");
 }
auto UiRenderer::source_list(const ProjectState& p,const Performance& perf) -> void{
  panel(0,10,79,460,128);
  const auto t=track_at(p,p.active_pattern,perf.selected_track());
  label(0,24,85,"ENGINE",theme.secondary,Font::Small,200);
  if(t.instrument==Instrument::Unknown)label(0,200,85,"CURRENT: UNKNOWN ENGINE",theme.warn,Font::Small,250);
  static const char* families[]={"DRUM","SYNTH","SAMPLER","MACRO","FM","CREATIVE","MIDI","KIT","PHYSICAL"};
  struct Row{bool header;unsigned family;unsigned engine;};
  Row rows[40];unsigned count=0,cursor_row=0;
  for(unsigned f=0;f<9;++f){bool head=false;for(unsigned e=0;e<instrument_count();++e){if(!instrument_descriptor(e))continue;const char* want=families[f];const char* have=engine_family_name(Instrument(e));unsigned n=0;while(want[n] && want[n]==have[n])++n;if(want[n]!=have[n])continue;if(!head){rows[count]={true,f,e};++count;head=true;}if(e==perf.source_cursor())cursor_row=count;rows[count]={false,f,e};++count;}}
  unsigned start=cursor_row>3?cursor_row-3:0;if(start+8>count)start=count>8?count-8:0;
  for(unsigned i=0;i<8 && start+i<count;++i){const Row& r=rows[start+i];int y=100+int(i)*12;
   if(r.header){const auto family=engine_family_control(Instrument(r.engine));icon(0,22,y-1,family.icon,theme.secondary);label(0,42,y,families[r.family],theme.secondary,Font::Small,182);continue;}
   const auto* d=instrument_descriptor(r.engine);bool cursor=r.engine==perf.source_cursor();bool current=Instrument(r.engine)==t.instrument;
   if(cursor)rounded(0,18,y-1,424,11,3,theme.raised);
   if(current)rounded(0,25,y+2,5,5,2,theme.accent);
   scrolling_label(0,38,y,d->name,cursor || current?theme.text:theme.secondary,Font::Small,300);
   if(d->requires_48k)label(0,392,y,"48KHZ",theme.warn,Font::Small,60);
   else if(r.engine==unsigned(Instrument::Sampler))label(0,392,y,"ASSET",theme.secondary,Font::Small,60);
  }
 }
auto UiRenderer::source_detail(const ProjectState& p,const Performance& perf) -> void{
 const auto& b=perf.browser();const auto* d=instrument_descriptor(perf.source_cursor());if(!d)return;
 scrolling_label(1,20,82,d->name,theme.text,Font::Body,440);
 label(1,20,105,b.library_kits?"KITS / PUSH TO CHOOSE":"SOUNDS / PUSH TO CHOOSE",theme.secondary,Font::Small,440);
 if(!b.count){const auto action=perf.pending_file_action();const bool reading=action==FileAction::LibraryInspect || action==FileAction::LibraryRestore;empty_state(1,20,132,440,reading?"READING SOUNDS":"NO SOUNDS FOR THIS ENGINE",reading?"ENGINE LIST REMAINS AVAILABLE":"TOP 5: LOAD ENGINE FOR DEFAULT SOUND");return;}
 const unsigned first=b.cursor/4*4;
 for(unsigned i=first;i<b.count && i<first+4;++i){int y=128+int(i-first)*18;list_row(1,10,y,460,17,{"","","",i==b.cursor,true,0});scrolling_label(1,20,y+3,b.entries[i].name,i==b.cursor?theme.text:theme.secondary,Font::Small,340);label(1,390,y+3,b.library_factory[i]?"FACTORY":"USER",theme.secondary,Font::Small,70);}
 if(track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::Unknown)advice("UNKNOWN PAYLOAD KEPT UNTIL LOAD IS CONFIRMED");
}
auto UiRenderer::draw_visual(unsigned side,const VisualDeclaration& declaration,const VisualData& data,const Performance& perf,uint64_t token,int x,int y,int w,int h) -> void{unsigned key=(unsigned(declaration.element)<<12)|(unsigned(declaration.source)<<6)|unsigned(declaration.model);key^=(perf.selected_track()<<16)^unsigned(perf.visual().generation*16777619u)^(perf.editing_pattern()<<24);key^=perf.visual_source()<<20;auto shown=visual_motion_[side].update(key,token,perf.ui_time(),data,perf.visual_settings().motion!=ui::Motion::Off && data.live,declaration.element==VisualElement::Ring || declaration.source==VisualSource::WavetableScan);visual_element(side,x,y,w,h,declaration,shown,theme.accent);}
auto UiRenderer::toast(const char* text,uint32_t color) -> void{rounded(1,14,74,452,34,ui::Geometry::button,theme.background,250);outline(1,14,74,452,34,ui::Geometry::button,theme.border);fill(1,19,81,3,20,color);label(1,30,ui::centered_text_y(Font::Small,74,34),text,color,Font::Small,424);}
auto UiRenderer::hints(const Performance& perf) -> void{
  ui::PageAdvice lines;lines.add(perf.encoder_hint());
  for(unsigned i=0;i<page_advice.count;++i)lines.add(page_advice.lines[i]);
  if(perf.mode()==UiMode::Routing && perf.routing_fx_page())lines.add("A-H: Select group bus");
  else{lines.add("A-H: Select track / SHIFT + A-H: Switch sticky layer");lines.add("MUTE / SOLO + A-H: Mute / solo track");lines.add("HOLD A-H: Mixer peek / RELEASE: Return");}
  lines.add("EVENTS: Undo / SHIFT + EVENTS: Redo");
  bool reset=false;for(unsigned i=0;i<8;++i)reset|=perf.knob_reset_available(i);if(reset)lines.add("TOUCH KNOB + ERASE: Reset / EVENTS: Undo reset");
  lines.add("HOLD BOTH PAGE ARROWS: Home / return");
  for(unsigned side=0;side<2;++side){rounded(side,6,68,468,174,ui::Geometry::dialog,theme.background,250);outline(side,6,68,468,174,ui::Geometry::dialog,theme.border);label(side,20,73,"PAGE HELP",theme.text,Font::Heading,190);label(side,210,77,"RELEASE PAGE BUTTON TO RETURN",theme.secondary,Font::Small,250);}
  unsigned row=0;for(unsigned item=0;item<lines.count && row<20;++item){const char* src=lines.lines[item];while(*src && row<20){char line[160]{};unsigned n=0,last_space=0;while(src[n] && n<150){line[n]=src[n];line[n+1]=0;if(ui::text_width(line,Font::Small)>438){line[n]=0;break;}if(src[n]==' ')last_space=n;++n;}if(src[n] && last_space>8){n=last_space;line[n]=0;}if(!n)break;label(row/10,20,103+int(row%10)*14,line,theme.secondary,Font::Small,438);src+=n;while(*src==' ')++src;++row;}}
 }
auto UiRenderer::knob_legend(const ProjectState& p,const Performance& perf) -> void{
  for(unsigned side=0;side<2;++side){
   for(unsigned col=0;col<4;++col){unsigned i=side*4+col;int x=int(col)*120+4;
    // An unchanged open cell owns its whole rectangle; the static legend must
    // not paint over its lower half on a frame that skips the ARC redraw.
    if(perf.knob_overlay_mask()&(1u<<i)){fill(side,x-4,ui::Layout::legend_y,4,30,theme.surface);fill(side,x+112,ui::Layout::legend_y,4,30,theme.surface);fill(side,x,270,112,2,theme.surface);continue;}
    fill(side,x-4,ui::Layout::legend_y,120,ui::Layout::legend_h,theme.surface);const char* name=perf.knob_label(i);if(!*name)continue;
    if(ui::text_width(name,Font::Small)<=112)centered_label(side,x,ui::Layout::legend_y,112,14,name,theme.secondary,Font::Small);else scrolling_label(side,x,ui::centered_text_y(Font::Small,ui::Layout::legend_y,14),name,theme.secondary,Font::Small,112);
    auto shown=formatted_knob(p,perf,i);int width=ui::text_width(shown.text,Font::Small);int left=width<108?x+(112-width)/2:x+2;
    scrolling_label(side,left,ui::centered_text_y(Font::Small,ui::Layout::legend_y+14,14),shown.text,theme.text,Font::Small,108);
    if(!(perf.knob_overlay_mask()&(1u<<i)))progress(side,x+4,270,104,2,perf.knob_display_normalized(i),theme.accent);
   }
  }
 }
auto UiRenderer::knobs(const ProjectState& p,const Performance& perf,uint64_t now) -> void{
  unsigned mask=perf.knob_overlay_mask();
  for(unsigned i=0;i<8;++i)if(mask&(1u<<i)){
   unsigned side=i/4;int x=int(i%4)*120+4;float amount=perf.knob_normalized(i),dt=float(now-last_ms_);
   if(!initialized_ || !context_matches_ || !(overlay_mask_was_&(1u<<i)) || perf.visual_settings().motion==ui::Motion::Off)smooth_[i]=amount;
   else{smooth_[i]+=(amount-smooth_[i])*ui::clamp01(dt/60);if(fabsf(smooth_[i]-amount)<.0001f)smooth_[i]=amount;}
   uint16_t parameter=perf.knob_parameter(i);const auto* pd=parameter?target_descriptor(parameter):nullptr;
   float live=smooth_[i];if(pd && perf.visual().sample && ((parameter>=64 && parameter<=66) || perf.visual().pattern[perf.selected_track()]==p.active_pattern))live=perf.knob_display_normalized(i);
   fill(side,x,ui::Layout::overlay_y,112,ui::Layout::overlay_h,theme.surface);
   centered_label(side,x+2,ui::Layout::overlay_y,108,13,perf.knob_label(i),theme.secondary,Font::Small);
   auto shown=formatted_knob(p,perf,i);const bool numeric=shown.digits[0];const char* value=numeric?shown.digits:shown.text;Font face=numeric?Font::Knob:Font::Body;
   int width=ui::text_width(value,face);if(!numeric && width>77)width=77;Font unit_face=Font::Body;int unit_width=numeric?ui::text_width(shown.unit,unit_face):0;
   int radius=12;if(width+unit_width+31>108){unit_face=Font::Small;unit_width=numeric?ui::text_width(shown.unit,unit_face):0;}if(width+unit_width+31>108)unit_width=0;
   if(width+29>108)radius=width+19<=108?8:5;
   int group_width=radius*2+5+width+(unit_width?unit_width+2:0),left=x+(112-group_width)/2;
   uint32_t arc=(perf.knob_turning_mask()&(1u<<i))?theme.motion:theme.accent;
   arc270(side,left+radius,ui::Layout::overlay_y+30,radius,live,pd && pd->minimum<0,arc);
   int vx=left+radius*2+5;
   if(numeric)label_band(side,vx,ui::Layout::overlay_y+14,width,31,value,theme.text,face);
   else scrolling_label(side,vx,ui::centered_text_y(face,ui::Layout::overlay_y+14,31),value,theme.text,face,width);
   if(unit_width)label_band(side,vx+width+2,ui::Layout::overlay_y+20,unit_width,23,shown.unit,theme.secondary,unit_face);

  }
 }
auto UiRenderer::selection_signature(const Performance& perf, ConstTrackView t, unsigned pattern) const  -> uint64_t{
  uint64_t h=1469598103934665603ull;
  h=fnv(h,perf.selected_track()); h=fnv(h,perf.selected_kit_pad()); h=fnv(h,pattern); h=fnv(h,unsigned(t.instrument));
  h=fnv(h,perf.selected_step()); h=fnv(h,unsigned(perf.shifted())); h=fnv(h,unsigned(perf.control_all()));
  h=fnv(h,perf.note_repeat_mode()); h=fnv(h,perf.visual().generation);
  return h;
 }
auto UiRenderer::start_transition(uint64_t now,int direction) -> void{
  for(unsigned side=0;side<2;++side)for(int y=0;y<ui::Layout::body_h;++y)for(unsigned x=0;x<480;++x)body_snapshot_[side][y*480+x]=pixels[side][(ui::Layout::body_y+y)*480+x];
  transition_.begin(now,direction);transition_pending_=true;
 }
auto UiRenderer::composite_transition(uint64_t now) -> void{
  if(transition_pending_){unsigned mask=0;for(unsigned side=0;side<2;++side){bool changed=false;for(int y=0;y<ui::Layout::body_h && !changed;++y)for(unsigned x=0;x<480;++x)if(body_snapshot_[side][y*480+x]!=pixels[side][(ui::Layout::body_y+y)*480+x]){changed=true;break;}if(changed)mask|=1u<<side;}transition_.changed_sides(mask);transition_pending_=false;}
  int shift=transition_.offset(now);if(!shift)return;
  for(unsigned side=0;side<2;++side)if(transition_.sides()&(1u<<side))for(int y=0;y<ui::Layout::body_h;++y){auto* row=&pixels[side][(ui::Layout::body_y+y)*480];for(int x=0;x<480;++x)body_scratch_[x]=row[x];for(int x=0;x<480;++x){int src=x-shift;row[x]=src>=0 && src<480?body_scratch_[src]:body_snapshot_[side][y*480+x];}}
 }
auto UiRenderer::knobs_active(const Performance& perf,uint64_t now) const  -> bool{
  for(unsigned i=0;i<8;++i){
   if(!(perf.knob_overlay_mask()&(1u<<i)))continue;
   if(perf.knob_value(i)!=previous_[i]) return true;
   if(smooth_[i]!=perf.knob_normalized(i)) return true;

  }
  return false;
 }
auto UiRenderer::draw(const ProjectState& p,const Performance& perf,unsigned peak,unsigned load,WavView sample) -> void{
  (void)peak;(void)load;theme=ui::Theme::by_index(perf.visual_settings().theme);brightness(perf.device_settings().screen);uint64_t now=perf.ui_time();if(last_mode_!=int(perf.mode()))page_ms_=now;
  if(perf.transfer_view().active){const auto& v=perf.transfer_view();layout_reset();clip_y();transition_.cancel();text_clock(now,false);for(unsigned side=0;side<2;++side)fill(side,0,0,480,272,theme.background);
   for(unsigned i=0;i<8;++i){unsigned side=i/4;int x=8+int(i%4)*117;auto control=perf.soft_presentation(i);bool selected=perf.soft_selected(i);rounded(side,x,2,113,18,0,selected?theme.raised:theme.surface);if(selected)selected_mark(side,x,2,113,18);label_band(side,x+3,2,107,18,control.label,perf.soft_enabled(i)?theme.accent:theme.disabled,Font::Small);}label_band(0,24,30,430,28,"FILE TRANSFER",theme.text,Font::Heading);label_band(1,24,30,430,28,v.usb?"USB-C / FILES":"WI-FI / FILES",theme.text,Font::Heading);
   label_band(0,24,78,430,22,v.usb?"TRANSPORT":"SSID",theme.secondary,Font::Small);label_band(0,24,100,430,34,v.ssid,theme.text,Font::Heading);
   label_band(0,24,151,430,22,v.usb?"HOST TOOL":"CONNECTION CODE",theme.secondary,Font::Small);label_band(0,24,174,430,40,v.usb?"tools/files/usb_files.py":v.code,theme.accent,v.usb?Font::Body:Font::Number);
   label_band(1,24,78,430,22,v.usb?"SESSION":"ADDRESS",theme.secondary,Font::Small);label_band(1,24,100,430,30,v.usb?"PHYSICAL CABLE / NO CODE":v.address,theme.text,Font::Heading);label_band(1,24,130,430,18,v.discovery,theme.secondary,Font::Small);label_band(1,24,155,430,24,v.status,theme.secondary,Font::Small);
   fill(1,24,188,430,4,theme.surface);if(v.length)fill(1,24,188,int(430.0*double(v.received)/double(v.length)),4,theme.accent);char remaining[24];format_value(remaining,float(v.remaining),0);label_band(1,24,222,220,24,v.usb?"IDLE TIMEOUT":"CLOSES IN",theme.secondary,Font::Small);label_band(1,252,222,202,24,remaining,theme.text,Font::Heading);
   initialized_=false;return;
  }
  if(meter_generation_!=perf.visual().generation || now<last_ms_){for(auto& m:meters_)m=ui::MeterBallistics{};meter_generation_=perf.visual().generation;}
  for(unsigned t=0;t<16;++t)meters_[t].update(perf.visual_fresh()?perf.visual().peak[t]:0,now,now>=last_ms_?now-last_ms_:0);
  const auto t=track_at(p,p.active_pattern,perf.selected_track());
  if(perf.mode()==UiMode::Sampler || perf.mode()==UiMode::Browser || (t.instrument==Instrument::Sampler && (perf.mode()==UiMode::Synth || perf.mode()==UiMode::Pad || perf.mode()==UiMode::Step || perf.mode()==UiMode::Keyboard || perf.mode()==UiMode::Chord))){
   WavView source=perf.mode()==UiMode::Browser?perf.browser().preview:sample;
   if(wave_cache_.update(source,perf.browser(),perf.visual().generation) || wave_cache_.pending())body_invalidated_=true;
  }
  uint64_t context=unsigned(perf.mode());const unsigned contexts[]={unsigned(perf.tracker_view()),unsigned(perf.step_overview()),unsigned(perf.follow_display()),perf.selected_track(),perf.selected_kit_pad(),p.active_pattern,unsigned(t.instrument),perf.instrument_view(),perf.generate_page(),unsigned(perf.wave_profile()),unsigned(perf.wave_clock()),unsigned(perf.control_all()),perf.selected_step(),perf.generate_lane(),unsigned(perf.shifted()),unsigned(perf.select_held()),perf.modulation_page(),perf.performance_page(),perf.sampler_page(),perf.slice_cursor(),unsigned(perf.zone_vel()),perf.fx_page(),perf.settings_page(),perf.settings_cursor(),perf.group_cursor(),perf.fx_focus_slot(),perf.chain_node(),perf.song_cursor(),unsigned(perf.source_select()),perf.source_cursor(),unsigned(perf.source_arm_unknown()),unsigned(perf.routing_fx_page()),unsigned(perf.group_send_page()),perf.group_fx_slot(),perf.note_repeat_mode(),unsigned(perf.clip_follow_page()),unsigned(t.clip_follow.action),t.clip_follow.cycles,t.clip_follow.target,t.clip_follow.probability,unsigned(perf.scene_take_requested()),perf.visual().generation};for(unsigned key:contexts)context=(context^key)*1099511628211ull;if(perf.mode()==UiMode::Song){auto key=[&](uint32_t v){context=(context^v)*1099511628211ull;};key(p.song.length);key(p.song.enabled);key(p.song.loop);for(unsigned i=0;i<p.song.length;++i){const auto& sec=p.song.sections[i];key(sec.scene);key(sec.bars);key(sec.ticks);key(sec.mute_mask);key(sec.playing_mask);key(sec.restart_mask);key(uint8_t(sec.transpose));for(auto pattern:sec.patterns)key(pattern);}}
  if(perf.mode()==UiMode::Settings){auto key=[&](uint64_t value){context=(context^value)*1099511628211ull;};key(perf.device_saved());if(perf.settings_page()==4)key(perf.visual().sample);if(perf.settings_page()==6){key(perf.storage_valid());key(perf.storage_free());key(perf.storage_total());}}
  for(unsigned n=0;n<16;++n)context=(context^unsigned(ui::track_engine_kind(p,n)))*1099511628211ull;
  for(const char* name=t.track_name;*name;++name)context=(context^uint8_t(*name))*1099511628211ull;
  for(const char* name=t.preset_name;*name;++name)context=(context^uint8_t(*name))*1099511628211ull;
  if(perf.mode()==UiMode::Project){for(const char* n=perf.project_path();*n;++n)context=(context^uint8_t(*n))*1099511628211ull;context=(context^perf.asset_cursor())*1099511628211ull;context=(context^perf.midi_capture_state())*1099511628211ull;for(const char* n=perf.midi_capture_path();*n;++n)context=(context^uint8_t(*n))*1099511628211ull;for(const auto& asset:p.assets)for(const char* n=asset.path;*n;++n)context=(context^uint8_t(*n))*1099511628211ull;}
  if(perf.mode()==UiMode::Browser || perf.mode()==UiMode::Library || perf.source_select() || perf.browser_editing()){const auto& b=perf.browser();auto key=[&](unsigned v){context=(context^v)*1099511628211ull;};auto text_key=[&](const char* text){for(;*text;++text)key(uint8_t(*text));key(0);};key(b.library_list);key(unsigned(perf.pending_file_action()));key(b.library_offset);key(b.library_total);key(b.folder_offset);key(b.folder_total);key(b.library_kits);key(b.library_originals);key(b.edit.duplicate);key(b.cursor);key(b.count);key(unsigned(b.edit.operation));key(b.edit.cursor);key(b.edit.include_fx);key(b.previewing);text_key(b.directory);text_key(b.status);text_key(b.edit.name);for(unsigned i=0;i<b.count;++i){text_key(b.entries[i].name);key(b.entries[i].directory);key(unsigned(b.library_engine[i]));key(b.library_factory[i]);key(b.library_revision[i]);}}
  if(perf.mode()==UiMode::Sampler || (perf.mode()==UiMode::Routing && !perf.routing_fx_page())){const auto target=perf.recording_target();const auto take=perf.resample_view();const unsigned state[]={target.track,target.pattern,target.kit_pad,perf.recording_source(),unsigned(perf.recording_origin()),unsigned(perf.recording_controls().stopping),unsigned(perf.recording_quantized()),unsigned(perf.pending_file_action()),unsigned(perf.recording_file_busy()),unsigned(perf.resampling()),unsigned(perf.resample_cancel_pending()),take.source,take.bars,take.target.track,take.target.pattern,take.target.kit_pad,unsigned(perf.audio_available()),unsigned(perf.scene_take_requested())};for(unsigned v:state)context=(context^v)*1099511628211ull;}
  if(perf.mode()==UiMode::Sampler || perf.mode()==UiMode::Browser){const auto& b=perf.browser();context=(context^uintptr_t(b.analysis_source))*1099511628211ull;context=(context^uintptr_t(sample.reader?static_cast<const void*>(sample.reader):static_cast<const void*>(sample.data)))*1099511628211ull;context=(context^sample.frames)*1099511628211ull;context=(context^unsigned(b.analysis_pending))*1099511628211ull;}
  context_matches_=context==last_context_;text_clock(now,!context_matches_);
  const char* toast_text=perf.notice()!=UiNotice::None?perf.notice_text():perf.file_notice_text();
  uint32_t toast_hash=2166136261u;for(const char* ch=toast_text;*ch;++ch)toast_hash=(toast_hash^uint8_t(*ch))*16777619u;
  bool toast_active=*toast_text!=0,toast_changed=toast_active!=toast_was_ || toast_hash!=toast_hash_;
  // Partial redraw (SPEC 2.5 / 9-4, Stage 1 prerequisite): the canvas
  // is persistent, so a band whose inputs did not change keeps its
  // pixels and its draw calls are skipped. The header band (soft
  // keys, titles, meters, popups at y<76) always repaints. The body
  // band repaints on a context change (mode, subpages, selection,
  // project generation), while the transport plays or recording runs
  // (playheads / visualizer / waveform), while a dialog is or was on
  // screen, while meters or live peaks still carry energy, on a
  // theme change, or on explicit invalidate_body(). The knob band
  // repaints while any knob value, easing, physical knob touch
  // is live. The section 2.5 transition engine will plug its band
  // invalidations into invalidate_body().
  ui::LedFrame pad_output;perf.led_frame(pad_output);bool pad_changed=false;
  for(unsigned i=0;i<16;++i){pad_changed|=last_pad_output_[i]!=pad_output.pads[i];last_pad_output_[i]=pad_output.pads[i];}
  const bool pad_body=perf.mode()==UiMode::Pad || perf.mode()==UiMode::Keyboard || perf.mode()==UiMode::Step || perf.mode()==UiMode::Clip || perf.mode()==UiMode::Scene || (perf.mode()==UiMode::Synth && t.instrument==Instrument::DrumKit) || (perf.mode()==UiMode::Settings && perf.settings_page()==2);
  bool theme_changed=!theme_seen_ || theme.background!=last_theme_bg_ || last_brightness_!=perf.device_settings().screen;last_brightness_=perf.device_settings().screen; theme_seen_=true; last_theme_bg_=theme.background;
  const bool visual_page=(perf.mode()==UiMode::Synth && !perf.source_select()) || perf.mode()==UiMode::Sampler || perf.mode()==UiMode::Browser || perf.mode()==UiMode::Library || perf.mode()==UiMode::Mixer || perf.mode()==UiMode::Effects || perf.mode()==UiMode::Perform || (perf.mode()==UiMode::Routing && perf.routing_fx_page());
  uint64_t live_sig=1469598103934665603ull;auto hash_float=[&](float value){uint32_t bits;__builtin_memcpy(&bits,&value,sizeof bits);live_sig=fnv(live_sig,bits);};const auto* measured=perf.analysis();live_sig=fnv(live_sig,measured?1:0);
  if(visual_page)for(unsigned i=0;i<8;++i)hash_float(perf.knob_display_value(i));
   if(visual_page && measured)for(unsigned tap=0;tap<3;++tap){for(unsigned i=0;i<kVisualScope;++i){hash_float(measured->scope_low[tap][i]);hash_float(measured->scope_high[tap][i]);}for(unsigned i=0;i<kVisualBands;++i)hash_float(measured->spectrum[tap][i]);}
  unsigned selected=perf.selected_track();hash_float(perf.visual().voice_level[selected]);hash_float(perf.visual().wavetable_phase[selected]);hash_float(perf.visual().wavetable_position[selected]);hash_float(perf.visual().sample_position[selected]);hash_float(perf.visual().morph_position);for(unsigned slot=0;slot<2;++slot)for(unsigned i=0;i<16;++i)hash_float(perf.visual().vocoder_bands[selected][slot][i]);
  hash_float(perf.visual().compressor_reduction);hash_float(perf.visual().track_compressor_reduction[selected]);for(unsigned i=0;i<6;++i)hash_float(perf.visual().node_activity[selected][i]);if(perf.visual().master_peak>.00001f)hash_float(perf.visual().performance_fx_phase);for(unsigned slot=0;slot<2;++slot)for(unsigned i=0;i<16;++i)hash_float(perf.visual().group_vocoder_bands[perf.group_cursor()][slot][i]);
  if(visual_page && (live_sig!=live_signature_ || visual_motion_[0].active(now) || visual_motion_[1].active(now))){body_invalidated_=true;}live_signature_=live_sig;
  bool dialog_up=perf.browser_editing() || perf.pending_project()!=Performance::ProjectAction::None || perf.update_confirm_pending() || perf.stretch_modal()!=0 || perf.freeze_modal()!=0;
  bool meters=false; for(unsigned m=0;m<16;++m) if(meters_[m].live>0.001f || meters_[m].held>0) meters=true;
  const uint8_t overlay_mask=perf.knob_overlay_mask();const bool overlay=overlay_mask!=0,overlay_changed=overlay_mask!=overlay_mask_was_ || perf.knob_turning_mask()!=turning_mask_was_;
  bool controls_changed=false;for(unsigned i=0;i<8;++i)if(perf.knob_value(i)!=previous_[i])controls_changed=true;
  // Phrase attributes may change without a note/velocity/LED change (Undo,
  // paste, or a SHIFT gesture completed between display frames).
  uint64_t attributes=1469598103934665603ull;
  if(perf.mode()==UiMode::Step){
   if(perf.tracker_view())for(unsigned column=0;column<8;++column){unsigned ti=perf.selected_track()/8*8+column;const auto phrase=track_at(p,p.active_pattern,ti);if(p.audio[ti].kind!=TrackKind::Instrument || phrase.instrument==Instrument::DrumKit)continue;bool following=perf.playing() && perf.follow_display() && p.clips[ti].playing && p.clips[ti].pattern==p.active_pattern;unsigned center=following?perf.position(ti):perf.selected_step();for(int row=0;row<ui::Layout::tracker_rows;++row)attributes=fnv(attributes,phrase.steps[tracker_row(center,row-ui::Layout::tracker_rows/2,phrase.length)].attributes);}
   else if(t.instrument!=Instrument::DrumKit){unsigned first=perf.step_overview()?(perf.page()/4)*64:perf.page()*16,count=perf.step_overview()?64:16;for(unsigned i=0;i<count;++i)attributes=fnv(attributes,t.steps[first+i].attributes);}
  }
  const bool attributes_changed=attributes!=phrase_attributes_;phrase_attributes_=attributes;
  const bool static_body=(perf.mode()==UiMode::Settings && perf.settings_page()!=4 && perf.settings_page()!=6) || perf.mode()==UiMode::Project || (perf.mode()==UiMode::Synth && perf.source_select());
  const bool live_body=!static_body && (perf.playing() || perf.live_recording() || perf.mode()==UiMode::Diagnostics || meters || perf.visual().master_peak>0.001f || perf.visual().input_peak>0.001f);
  repaint_body_=attributes_changed || overlay_changed || scrolling_due(64,272) || (pad_body && pad_changed) || controls_changed || !initialized_ || !context_matches_ || theme_changed || dialog_up || prev_dialog_ || help_was_!=perf.help_visible() || toast_changed || live_body || body_invalidated_;
  body_invalidated_=false;

  repaint_knobs_=overlay && (repaint_body_ || scrolling_due(207,272) || !initialized_ || !context_matches_ || theme_changed || knobs_active(perf,now));
  // View indices are shared by signature and direction; no parallel index lists.
  const unsigned sub[]={perf.instrument_view(),perf.generate_page(),unsigned(perf.wave_clock()),perf.modulation_page(),perf.performance_page(),perf.sampler_page(),perf.fx_page(),perf.settings_page(),unsigned(perf.source_select()),unsigned(perf.routing_fx_page()),unsigned(perf.group_send_page()),perf.group_fx_slot(),unsigned(perf.step_overview())+2*unsigned(perf.tracker_view())};
  uint64_t sub_sig=1469598103934665603ull;for(unsigned v:sub)sub_sig=fnv(sub_sig,v);
  uint64_t sel_sig=selection_signature(perf,t,p.active_pattern);
  if(dialog_up || perf.modal_active() || perf.help_visible() || theme_changed || perf.visual_settings().motion==ui::Motion::Off)transition_.cancel();
  else if(initialized_ && last_mode_>=0){
   if(int(perf.mode())!=last_mode_)start_transition(now,int(perf.mode())>last_mode_?1:-1);
   else if(signatures_valid_ && sub_sig!=last_subpage_sig_ && sel_sig==last_selection_sig_){int dir=1;for(unsigned i=0;i<13;++i)if(sub[i]!=last_sub_values_[i]){dir=sub[i]>last_sub_values_[i]?1:-1;break;}start_transition(now,dir);}
  }
  last_subpage_sig_=sub_sig;last_selection_sig_=sel_sig;signatures_valid_=true;for(unsigned i=0;i<13;++i)last_sub_values_[i]=sub[i];
  if(transition_.active())repaint_body_=true;
  for(unsigned side=0;side<2;++side){fill(side,0,0,480,64,theme.background);if(repaint_body_)fill(side,0,64,480,ui::Layout::body_h,theme.background);}
  for(unsigned i=0;i<8;++i){unsigned side=i/4;int x=8+int(i%4)*117;bool chosen=perf.soft_selected(i);rounded(side,x,2,113,18,0,chosen?theme.raised:theme.surface);if(chosen)fill(side,x+6,18,101,2,theme.accent);const auto control=perf.soft_presentation(i);uint32_t ink=!perf.soft_enabled(i)?theme.disabled:chosen?theme.accent:theme.secondary;int tw=ui::text_width(control.label,Font::Small),group=tw+(control.icon?18:0);int left=x+3+(107-(group>107?107:group))/2;if(control.icon)icon(side,left,4,control.icon,ink);label_band(side,left+(control.icon?18:0),2,107-(control.icon?18:0),18,control.label,ink,Font::Small); }
  const char* right=perf.audio_page()?"Audio / looper":perf.mode()==UiMode::Library?(perf.browser().library_kits?"Kit":"Preset"):perf.mode()==UiMode::Browser?(perf.browser().new_project_list?"Template / empty":perf.midi_browser()?"MIDI import":"Waveform / audition"):perf.mode()==UiMode::Routing?(perf.routing_fx_page()?(perf.group_send_page()?"Group sends":"Group insert"):"Audio buses"):perf.mode()==UiMode::Diagnostics?"Hardware monitor":perf.mode()==UiMode::Mixer?"Channel / routing":perf.mode()==UiMode::Effects?(perf.fx_page()==5?"FX chain":perf.fx_page()==4?"Pad insert":perf.fx_page()==3?"Track inserts":perf.fx_page()==2?"Track compressor":perf.fx_page()==1?"Master compressor":"Delay / Reverb"):perf.mode()==UiMode::Sampler?(perf.sampler_page()==1?"Trim / envelope":perf.sampler_page()==2?(t.instrument==Instrument::DrumKit?"Kit layers":"Slice mapping"):"Sample playback"):perf.mode()==UiMode::Perform?(perf.performance_page()==4?"Live FX":perf.performance_page()==3?"Pattern mutation":perf.performance_page()==2?"Gesture recorder":perf.performance_page()==1?"Performance macros":perf.control_all()?"Control All":"A/B morph"):perf.mode()==UiMode::Clip?"Clip detail":perf.mode()==UiMode::Scene?"Scene follow":perf.mode()==UiMode::Settings?"Device settings":perf.mode()==UiMode::Project?"Project assets":perf.mode()==UiMode::Modulation?(perf.modulation_page()==0?"LFO / pressure":perf.modulation_page()==1?"Automation":perf.modulation_page()==2?"Macro routing":perf.modulation_page()==4?"RPN / NRPN":"MIDI routing"):perf.mode()==UiMode::Generate?"Generative tools":track_display_name(t,p.audio[perf.selected_track()].kind);
  header(0,perf.twin_dialog()?"TWIN":perf.composition_dialog()?"DUPLICATE":perf.page_title(),perf,p);header(1,perf.twin_dialog()?"Prepare instrument":perf.composition_dialog()==2?"Scene destination":perf.composition_dialog()?"Pattern variation":right,perf,p);
  layout_band(76,131,64,ui::Layout::body_h);clip_y(ui::Layout::body_y,ui::Layout::legend_y);
  if(repaint_body_)page_advice.clear();
  if(repaint_body_)switch(perf.mode()){
   case UiMode::Clip:clips(p,perf);break;    case UiMode::Song:song(p,perf);break;
   case UiMode::Scene:scenes(p,perf);break;
   case UiMode::Library:library_catalog(p,perf);break;
   case UiMode::Browser:browser(p,perf);break;
   case UiMode::Routing:routing(p,perf);break;
   case UiMode::Diagnostics:layout_reset();clip_y(ui::Layout::body_y,ui::Layout::legend_y);diagnostics(perf);break;
   case UiMode::Sampler:{unsigned slot=t.instrument==Instrument::DrumKit?t.kit.pads[perf.selected_kit_pad()].sample[0]:t.sample_slot;sampler(t,perf,sample,slot<kSampleAssets?filename(p.assets[slot].path):"No sample");break;}
   case UiMode::Mixer:mixer(p,perf);break;
   case UiMode::Effects:effects(p,perf);break;
   case UiMode::Modulation:modulation_editor(p,perf);break;
   case UiMode::Perform:if(perf.audio_page())audio_lane_page(p,perf);else if(perf.performance_page()==4)performance_fx_page(perf);else if(perf.performance_page()==3)mutation(p,perf);else if(perf.performance_page()==2)gesture_page(p,perf);else if(perf.performance_page()==1)macro_performance(p);else performance(p,perf);break;
   case UiMode::Generate:generate(p,perf);break;
   case UiMode::Project:project(p,perf);break;
   case UiMode::Settings:settings(perf);break;
   case UiMode::Step:if(perf.tracker_view()){layout_reset();tracker(p,perf);break;}pattern(p,perf);if(t.instrument==Instrument::DrumKit)kit_panel(p,perf);else source_visual(t,perf);break;
   case UiMode::Chord:chords(p,perf);source_visual(t,perf);break;
   case UiMode::Keyboard:pads(p,perf);if(perf.step_chord_editor())step_chord_detail(p,perf);else source_visual(t,perf);break;
   case UiMode::Synth:if(perf.source_select()){source_list(p,perf);source_detail(p,perf);}else if(perf.layer_page()){layer_picture(t,perf);}else if(perf.kit_edit_page()){pads(p,perf);kit_panel(p,perf);}else{synth_signal(p,t,perf);synth(t,perf);}break;
   default:pads(p,perf);if(t.instrument==Instrument::DrumKit)kit_panel(p,perf);else source_visual(t,perf);break;
  }
  layout_reset();clip_y();
  if(overlay_changed)transition_.cancel();
  if(transition_.active()) composite_transition(now);

  // Transient spatial picker: only where SELECT really selects a track.
  // Musical pad grids and ordinary reading-order tables keep their meanings.
  if(repaint_body_ && perf.select_held() && perf.pad_role()==ui::PadRole::TrackSelect && !perf.modal_active()){
   fill(0,0,76,480,131,theme.background);pads(p,perf);
  }
  if(repaint_body_ && overlay)repaint_knobs_=true;
  knob_legend(p,perf);
  if(repaint_body_)for(unsigned side=0;side<2;++side)for(int y=0;y<ui::Layout::overlay_h;++y)for(int x=0;x<480;++x)overlay_underlay_[side][y*480+x]=pixels[side][(ui::Layout::overlay_y+y)*480+x];
  if(repaint_knobs_){for(unsigned i=0;i<8;++i)if(overlay_mask&(1u<<i)){unsigned side=i/4;int x=int(i%4)*120+4;for(int y=ui::Layout::overlay_y;y<ui::Layout::overlay_y+ui::Layout::overlay_h;++y)for(int j=x;j<x+112;++j)pixels[side][y*480+j]=overlay_underlay_[side][(y-ui::Layout::overlay_y)*480+j];}clip_y(ui::Layout::overlay_y,ui::Layout::overlay_y+ui::Layout::overlay_h);knobs(p,perf,now);clip_y();}if(perf.help_visible() && repaint_body_)hints(perf);help_was_=perf.help_visible();if(toast_active && !perf.modal_active() && !perf.help_visible() && repaint_body_)toast(toast_text,perf.notice_error()?theme.red:theme.secondary);toast_was_=toast_active;toast_hash_=toast_hash;if(perf.modal_active() && !perf.twin_dialog() && !perf.composition_dialog() && !perf.browser_editing() && !perf.stretch_modal() && !perf.freeze_modal() && perf.pending_project()==Performance::ProjectAction::None)dialog(0,perf.confirmation_heading(),"Top1: Confirm / Top2: Cancel",-1);if(perf.pending_project()!=Performance::ProjectAction::None)dialog(0,perf.pending_project()==Performance::ProjectAction::New?"Create empty project?":"Load saved project?","Unsaved edits will be replaced. Confirm / Cancel",-1);if(perf.stretch_modal())dialog(0,"TIME STRETCH",perf.stretch_text(),perf.stretch_modal()==2?perf.stretch_progress_pct()/100.0f:-1.0f);if(perf.freeze_modal())dialog(0,"TRACK FREEZE",perf.freeze_text(),perf.freeze_progress_pct()/100.0f);if(perf.twin_dialog())twin_panel(perf);if(perf.composition_dialog())composition_panel(p,perf);for(unsigned i=0;i<8;++i)previous_[i]=perf.knob_value(i);overlay_mask_was_=overlay_mask;turning_mask_was_=perf.knob_turning_mask();initialized_=true;prev_dialog_=dialog_up;last_ms_=now;last_mode_=int(perf.mode());last_track_=int(perf.selected_track());last_context_=context;
 }
auto UiRenderer::formatted_knob(const ProjectState& p,const Performance& perf,unsigned i) -> KnobValue{
  KnobValue out;const char* name=perf.knob_label(i);float raw=perf.knob_display_value(i);uint16_t parameter=perf.knob_parameter(i);
  if(perf.composition_dialog()==2){if(i==0)out.number(raw,0);return out;}
  if(perf.composition_dialog()){if(i==0){static const char* names[]={"THIN","TRANSPOSE","DROP TRACK"};out.append(names[unsigned(perf.variation_options().kind)]);}else if(i==1){out.number(raw,0);if(perf.variation_options().kind==PatternVariation::Thin)out.append("%");else if(perf.variation_options().kind==PatternVariation::Transpose)out.append("st");}return out;}
  if(perf.mode()==UiMode::Clip && !perf.clip_follow_page() && perf.shifted() && i==7){out.append(perf.copy_track_only()?"TRACK ONLY":"ALL");return out;}
  if(perf.mode()==UiMode::Generate && perf.generate_page()==5){if(i==2)out.append(perf.harmony_mode()==HarmonyMode::Bass?"BASS":"ARP");else if(i<2)out.number(raw,0);return out;}
  if(perf.twin_dialog()){if(perf.twin_dialog()!=1 || i>=6)return out;if(i<2){char note[12];pitch_name(unsigned(raw),note);out.append(note);}else{out.number(raw,0);if(i>=4)out.append("ms");else if(i==2)out.append("st");}return out;}
  if(parameter==11){out.append(raw>=.5f?"ON":"OFF");return out;}
  if(parameter==12){out.append(kLfoDivisions[unsigned(raw)<12?unsigned(raw):6]);return out;}
  if(perf.layer_page()){
   unsigned page=perf.layer_edit_page();if(i==7){out.append(kLayerEditPages[page]);return out;}
   if(page==4 && i==5){out.append(raw>=.5f?"ON":"OFF");return out;}
   const auto& layer=track_at(p,p.active_pattern,perf.selected_track()).layer;
   if((page==2 || page==3) && perf.layer_cursor()<layer.count){unsigned type=unsigned(layer.parts[perf.layer_cursor()].fx.inserts[(page-2)*4]);if(i==3){const auto* fx=fx_descriptor(unsigned(raw));out.append(fx?fx->name:"UNKNOWN");return out;}if(i==4 || i==5){const char* choice=fx_choice_name(type,i-3,raw);if(choice && *choice){out.append(choice);return out;}}}
  }
  if(perf.step_attribute_knob(i)){out.append(raw>=.5f?"ON":"OFF");return out;}
  if(perf.input_values()){if(i==4 && !perf.step_chord_editor()){out.number(raw*100,0);out.append("%");return out;}if(i==1){char pitch[12];pitch_name(unsigned(raw),pitch);out.append(pitch);}else if(i==2)out.append(scale_name(unsigned(raw)));else if(i==6)out.append(chord_shape_definition(unsigned(raw)).name);else if(i==7){static const char* inversions[]={"ROOT","INV 1","INV 2","INV 3"};out.append(inversions[unsigned(raw)%4]);}else{out.number(raw,0);if(i==0)out.append("bpm");if(i==5)out.append("t");}return out;}
  if(perf.mode()==UiMode::Settings){out.append(perf.setting_text(perf.settings_cursor()));return out;}
  if(perf.browser_editing()){if(perf.preset_fx_editing() && (i==2 || i==3)){out.append((i==2?perf.browser().edit.include_fx:perf.browser().edit.duplicate)?"ON":"OFF");return out;}const auto& edit=perf.browser().edit;if(!i)out.number(edit.cursor+1,0);else{char ch[2]={edit.name[edit.cursor],0};out.append(!ch[0]?"End":ch[0]==' '?"Space":ch);}return out;}
   if(engine_target(parameter)){const auto* d=engine_parameter(target_engine(parameter),target_local(parameter));if(d && d->choice_count){out.append(d->choices[unsigned(raw)<d->choice_count?unsigned(raw):0]);return out;}}
   if(perf.native_synth_page() && parameter==engine_target(Instrument::Texture,0)){static const char* modes[]={"GRANULAR","STRETCH","LOOP DELAY","SPECTRAL"};out.append(modes[unsigned(raw)%4]);return out;}
   if(perf.native_synth_page() && parameter==engine_target(Instrument::Texture,11)){static const char* quality[]={"32k 16b ST","32k 16b MO","16k 8b ST","16k 8b MO"};out.append(quality[unsigned(raw)%4]);return out;}
   if(perf.native_synth_page() && parameter==632){unsigned source=unsigned(raw),track=capture_track(source);if(source>=53){char group[]={ 'G','r','o','u','p',' ',char('A'+source-53),0};out.append(group);}else{out.append(source==1?"Line In":capture_name(source));if(track<16)out.number(track+1,0);}return out;}
   if(perf.native_synth_page() && (parameter==engine_target(Instrument::Texture,10) || parameter==engine_target(Instrument::Texture,12) || parameter==engine_target(Instrument::Texture,13))){out.append(raw>=.5f?"ON":"OFF");return out;}
   if(perf.native_synth_page() && perf.core_parameter(i)==engine_target(Instrument::Resonator,0)){static const char* names[]={"MODAL","SYMPATHETIC","INHARMONIC"};out.append(names[unsigned(raw)<3?unsigned(raw):0]);return out;}
   if(perf.native_synth_page() && perf.core_parameter(i)==engine_target(Instrument::Resonator,1)){out.number(float(1u<<unsigned(raw)),0);return out;}
   if(perf.mode()==UiMode::Clip && perf.clip_follow_page() && i==0){out.append(clip_follow_name(ClipFollowAction(unsigned(raw))));return out;}
   if(perf.mode()==UiMode::Clip && perf.clip_follow_page() && i==4 && raw==0){out.append("NONE");return out;}
   float displayed=raw;const char* unit="";unsigned precision=0;bool normalized=false;
   const bool kit=track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::DrumKit;
   bool creative_lab=perf.mode()==UiMode::Synth && perf.instrument_view()==3 && (unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)>=12 && unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)<=16);
   if(perf.audio_page()){unit="";normalized=false;precision=(i>=3 && i<=6)?2:0;}
   else if(perf.native_synth_page()){const auto* d=target_descriptor(parameter);if(d){unit=d->unit;precision=d->precision;normalized=d->minimum==0 && d->maximum<=1;}}
   else if(perf.mode()==UiMode::Clip && perf.clip_follow_page()){unit=i==3?"%":"";}
   else if(perf.note_repeat_mode()){unit=i==2?"%":"";}
   else if(creative_lab){bool wave=track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::WaveSequence;unit=(wave?i==7:i==4)?"ct":((wave?i==4:i==1) || (!wave && i>=5))?"%":"";normalized=!wave && (i>=5 || (i==2 && (track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::ChipClock || track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::WestCoast)));if(normalized)unit="%";precision=0;}
   else if(perf.mode()==UiMode::Synth && perf.instrument_view()==0 && (unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)>=12 && unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)<=16) && i==1 && !perf.shifted()){displayed=raw*100;unit="%";}
   else if(perf.mode()==UiMode::Synth && perf.instrument_view()==0 && (unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)>=12 && unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)<=16) && i>=2 && i<=6){auto d=instrument_control_display(track_at(p,p.active_pattern,perf.selected_track()).instrument,i-2,raw);displayed=d.value;unit=d.unit;precision=d.precision;}
   else if(perf.mode()==UiMode::Perform && perf.control_all() && perf.shifted() && i==0){unit="";}
   else if(kit && (perf.mode()==UiMode::Pad || perf.kit_edit_page())){normalized=i>=2 && i<=6;unit=normalized?"%":"";}
   else if(kit && perf.mode()==UiMode::Sampler && !perf.sampler_page()){normalized=i==2 || i==3;unit=normalized?"%":i==4?"x":"";precision=i==4?2:0;}
   else if(perf.mode()==UiMode::Step){unit=kit?(i==4 && !perf.shifted()?"%":i==3?(perf.shifted()?"t":"st"):""):(i==4 || (i==6 && perf.shifted())?"%":i==3 || i==6?"t":"");}
   else if(perf.mode()==UiMode::Perform && perf.performance_page()==4){normalized=i==0 || i==1 || i==4;unit=normalized?"%":"";}
   else if(perf.mode()==UiMode::Perform && perf.performance_page()==1){normalized=true;unit="%";}
   else if(perf.mode()==UiMode::Perform && perf.performance_page()==3 && i==0)unit="%";
   else if(perf.mode()==UiMode::Modulation && perf.modulation_page()){unsigned page=perf.modulation_page();if((page==1 && i==3) || (page==2 && (i==3 || i==4))){uint16_t id=page==1?track_at(p,p.active_pattern,perf.selected_track()).automation[perf.automation_lane()].parameter:p.macros[perf.macro_index()].routes[perf.macro_route()].parameter;auto mode=page==1?track_at(p,p.active_pattern,perf.selected_track()).automation[perf.automation_lane()].mode:p.macros[perf.macro_index()].routes[perf.macro_route()].mode;const auto* d=target_descriptor(id);unit=mode==LayerMode::Multiply?"x":d->unit;precision=mode==LayerMode::Multiply?2:d->precision;normalized=mode!=LayerMode::Multiply && id<128 && id!=8 && id!=66;}if(page==3 && i==4){normalized=true;unit="%";}}
   else if(perf.mode()==UiMode::Sampler && perf.sampler_page()==1){unit=i==0 || i==1?"%":i==2?"x":i<7?"ms":"";normalized=i<2;precision=i<3?2:0;}
   else if(perf.mode()==UiMode::Sampler && perf.sampler_page()==2){normalized=track_at(p,p.active_pattern,perf.selected_track()).instrument!=Instrument::DrumKit && (i==1 || i==2);unit=normalized?"%":"";}    else if(perf.mode()==UiMode::Sampler && perf.sampler_page()==3){unit=i==0 || i==1 || i==4 || i==5?"%":i==3?"x":i>=6?"ms":"";normalized=i==0 || i==1 || i==4 || i==5;precision=i<2 || i==3?2:0;}    else if(perf.mode()==UiMode::Sampler && perf.sampler_page()==4){normalized=i==1 || i==2;unit=normalized?"%":"";}
   else if(perf.mode()==UiMode::Routing || (perf.mode()==UiMode::Browser && i>=4 && i<=6)){normalized=perf.mode()==UiMode::Browser || (perf.routing_fx_page()?(perf.group_send_page()?(i==1 || i==2 || i==3 || i==7):(i>=2 && i<=5)):(perf.shifted()?i==1:i<4));unit=normalized?"%":"";}
   else if(perf.mode()==UiMode::Sampler && i>=5){normalized=true;unit="%";}
   else if(perf.mode()==UiMode::Effects && perf.fx_page()==4){normalized=i>=2;unit=normalized?"%":"";}
   else if(perf.mode()==UiMode::Effects && perf.fx_page()==3){normalized=i%4!=0;unit=normalized?"%":"";}
   else if(perf.mode()==UiMode::Effects && perf.fx_page()==5){if(i==4 && perf.knob_parameter(i)){const auto* d=target_descriptor(perf.knob_parameter(i));unit=d->unit;precision=d->precision;normalized=true;}}
   else if(perf.mode()==UiMode::Effects && perf.fx_page() && i<7){const auto* d=target_descriptor(uint16_t((perf.fx_page()==2?87:70)+i));unit=d->unit;precision=d->precision;normalized=i==0 || i==5;}
   else if(perf.mode()==UiMode::Effects){normalized=i!=2;unit=i==2?"ms":"%";}
   else if(perf.layer_page()){unsigned page=perf.layer_edit_page();precision=page==0?(i==3 || i==4?2:0):page==1?0:2;}
   else if(perf.source_select()){if(i==2){displayed=raw;unit="";precision=0;}else if(i==4){displayed=raw;unit="";precision=0;}else if(i==5 || i==6){displayed=raw*100;unit="%";precision=0;}}
   else if(engine_target(parameter) && target_engine(parameter)==Instrument::FM){displayed=target_local(parameter)==0?fm_ratio(raw):fm_index(raw);unit=target_local(parameter)==0?"x":"rad";precision=2;}
   else if(engine_target(parameter) && engine_parameter(target_engine(parameter),target_local(parameter)) && engine_parameter(target_engine(parameter),target_local(parameter))->display==EngineDisplay::Native){const auto* d=target_descriptor(parameter);unit=d->unit;precision=d->precision;normalized=target_engine(parameter)==Instrument::Sampler && (target_local(parameter)==1 || target_local(parameter)==2);}
   else if(perf.core_parameter(i)){const auto* d=target_descriptor(perf.core_parameter(i));unit=d->unit;precision=d->precision;normalized=perf.core_parameter(i)!=8;}
   else if(perf.synth_parameter(i)){const auto* d=target_descriptor(perf.synth_parameter(i));unit=d->unit;precision=d->precision;normalized=perf.synth_parameter(i)!=8;}
   else if(perf.mode()==UiMode::Mixer || perf.mode()==UiMode::Perform || perf.mode()==UiMode::Modulation){unsigned id=i+1;if(perf.mode()==UiMode::Modulation){id=modulation_parameter_id(i,track_at(p,p.active_pattern,perf.selected_track()).lfo_sync>=.5f);}if(perf.mode()==UiMode::Perform && i==7 && name[0]=='P')id=9;const auto* d=target_descriptor(uint16_t(id));unit=d->unit;precision=d->precision;normalized=id!=8;}
   else if(perf.mode()==UiMode::Synth || perf.mode()==UiMode::Pad || perf.mode()==UiMode::Keyboard || perf.mode()==UiMode::Chord || perf.mode()==UiMode::Step){const auto t=track_at(p,p.active_pattern,perf.selected_track());if(i>=2 && i<=6 && !perf.wave_profile()){if(t.instrument==Instrument::FM){displayed=i==2?fm_ratio(raw):fm_index(raw);unit=i==2?"x":"rad";precision=2;}else{normalized=true;unit="%";}}}
   if(!perf.note_repeat_mode() && perf.control_all() && perf.mode()==UiMode::Perform && i+1==perf.control_all_parameter()){const auto* d=target_descriptor(uint16_t(i+1));displayed=raw*(d->maximum-d->minimum);}
   if(normalized)displayed*=100;

   bool target_name=!perf.note_repeat_mode() && perf.mode()==UiMode::Modulation && ((perf.modulation_page()==1 && i==1) || (perf.modulation_page()==2 && i==2));
   bool lab=!perf.note_repeat_mode() && perf.mode()==UiMode::Synth && perf.instrument_view()==3 && (unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)>=12 && unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)<=16);
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Modulation && perf.modulation_page()==2 && i==2 && perf.shifted()){unsigned n=unsigned(raw);out.append(mod_source_name(ModSource(n<=unsigned(ModSource::Velocity)?n:0)));return out;}
   if(target_name){out.append(perf.parameter_name(uint16_t(raw)));return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Modulation && perf.modulation_page()==2 && i==5){static const char* modes[]={"Replace","Add","Multiply"};static const char* curves[]={"Linear","Slow","Fast","Smooth"};unsigned n=unsigned(raw);out.append(perf.shifted()?curves[n<4?n:0]:modes[n<3?n:0]);return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Modulation && perf.modulation_page()==2 && i==6){out.append(perf.shifted()?(raw>0?"Included":"Excluded"):(raw>0?"All tracks":"Track mask"));return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Modulation && perf.modulation_page()==2 && i==7 && perf.shifted()){out.append(raw>0?"Descending":"Ascending");return out;}
   if(perf.audio_page() && perf.audio_lane().kind!=TrackKind::Audio && i==2){static const char* lengths[]={"Bars","Pattern","Free"};out.append(lengths[unsigned(perf.audio_lane().length)]);return out;}
   if(perf.audio_page() && i==(perf.audio_lane().kind==TrackKind::Audio?7u:1u)){char input_name[32];out.append(audio_input_name(perf.audio_lane().source,input_name));return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Perform && perf.performance_page()==4 && i==2){static const char* rates[]={"1/8","1/16","1/32","1/64"};out.append(rates[perf.live_repeat_rate()%4]);return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Perform && perf.performance_page()==4 && i==3){static const char* names[]={"Off","Filter","Gate","Crush","Tape stop","Reverse","Tremolo","Isolator","Ring","Delay throw","Reverb throw","Freeze","Pitch dive","Pitch rise"};unsigned selected=unsigned(raw);out.append(names[selected<14?selected:0]);return out;}
   if(lab && track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::WaveSequence && (i==0 || i==6)){static const char* lanes[]={"Sample","Pitch","Timing","Gate","Shape","Step mod"};static const char* notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};out.append(i==0?lanes[unsigned(raw)%6]:notes[unsigned(raw)%12]);return out;}
   if(!perf.note_repeat_mode() && !lab && perf.mode()==UiMode::Synth && perf.instrument_view()==0 && ((track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::ChipClock && i==3) || (track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::SpectralFreeze && i==2))){out.append(raw>=0.5f?(i==3?"Follow":"Hold"):(i==3?"Raw":"Track"));return out;}
   if(lab && unsigned(track_at(p,p.active_pattern,perf.selected_track()).instrument)>12 && (i==0 || i==3)){static const char* notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};char source[16];source[0]='T';format_value(source+1,raw,0);out.append(i==3?notes[unsigned(raw)%12]:raw?source:"Master");return out;}
   if(!perf.note_repeat_mode() && kit && perf.mode()==UiMode::Step && (i==1 || i==6 || i==7)){if(raw==0)out.append("Base");else if(i==7 && perf.shifted())out.append(raw==2?"Reverse":"Forward");else if(i==6 || i==7){out.number((raw-1)*100/(i==6?254:127),0);out.append("%");}else out.number(raw,0);return out;}
   if(!perf.note_repeat_mode() && !kit && perf.mode()==UiMode::Step && perf.shifted() && i==1 && (track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::Sampler?raw==0:raw==255)){out.append("Base");return out;}
   {auto choice=perf.fx_knob_control(i);if(choice.count){out.append(choice.values[fx_choice_index(raw,choice.count)]);return out;}}
   bool note_control=(name[0]=='N' && name[1]=='O' && name[2]=='T' && name[3]=='E' && !name[4]) || (perf.mode()==UiMode::Sampler && !kit && !perf.sampler_page() && i==0) || (perf.mode()==UiMode::Browser && i==2 && !perf.midi_browser());
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Effects && ((perf.fx_page()==3 && i%4==0) || (perf.fx_page()==4 && i==1))){out.append(track_insert_name(unsigned(raw)));return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Step && perf.shifted() && track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::DrumKit && (i==2 || i==4 || i==5)){if(raw==0)out.append("Base");else{out.number((raw-1)*100/127,0);out.append("%");}return out;}
   if(!perf.note_repeat_mode() && perf.mode()==UiMode::Sampler && perf.sampler_page()==2 && i==3 && track_at(p,p.active_pattern,perf.selected_track()).instrument==Instrument::DrumKit){const char* names[]={"Single","Velocity","Round robin","Stack"};out.append(names[unsigned(raw)%4]);return out;}
   if(perf.mode()==UiMode::Modulation && perf.modulation_page()==3 && perf.shifted() && i==1 && raw==0){out.append("Off");}
   else if(perf.note_repeat_mode() && i==3){out.append(raw>0?"On":"Off");}
   else if(perf.note_repeat_mode()==2 && i==0){const char* names[]={"Up","Down","Up/down","Random"};out.append(names[unsigned(raw)%4]);}
   else if(perf.note_repeat_mode()){out.number(displayed,precision);if(*unit)out.append(unit);}
   else if(kit && perf.mode()==UiMode::Sampler && ((!perf.sampler_page() && i==5) || (perf.sampler_page()==2 && (i==5 || i==6)))){out.append(raw>0?"On":"Off");}
   else if(kit && (perf.mode()==UiMode::Pad || perf.kit_edit_page()) && perf.shifted() && i==7){const char* modes[]={"Single","Velocity","Round robin","Stack"};out.append(modes[unsigned(raw)%4]);}
   else if(perf.mode()==UiMode::Sampler && (perf.sampler_page()==1 || (kit && !perf.sampler_page())) && i==7){const char* names[]={"One-shot","Gate","Loop","Ping-pong"};out.append(names[unsigned(raw)%4]);}
   else if(note_control){static const char* notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};unsigned note=unsigned(raw);char spelling[8];const char* pitch=notes[note%12];unsigned n=0;while(*pitch)spelling[n++]=*pitch++;int octave=int(note/12)-1;if(octave<0){spelling[n++]='-';octave=-octave;}spelling[n++]=char('0'+octave);spelling[n]=0;out.append(spelling);}
   else if((perf.mode()==UiMode::Sampler && !kit && !perf.sampler_page() && (i==3 || i==4)) || (perf.mode()==UiMode::Scene && i==2)){out.append(raw>0?"On":"Off");}
   else if(perf.mode()==UiMode::Routing && perf.routing_fx_page() && perf.group_send_page() && (i==0 || i==4 || i==5)){static const char* groups[]={"A","B","C","D","E","F","G","H"};static const char* outputs[]={"Off","Main","Cue","Both"};out.append(i==0?groups[perf.group_cursor()]:i==4?outputs[unsigned(raw)%4]:raw>0?"Assigned":"None");}
   else if(perf.mode()==UiMode::Routing && perf.routing_fx_page() && !perf.group_send_page() && (i==0 || i==1 || i==6 || i==7)){static const char* groups[]={"A","B","C","D","E","F","G","H"};static const char* outputs[]={"Off","Main","Cue","Both"};out.append(i==0?groups[perf.group_cursor()]:i==1?track_insert_name(unsigned(raw)):i==6?outputs[unsigned(raw)%4]:raw>0?"Assigned":"None");}
   else if(perf.mode()==UiMode::Routing && !perf.routing_fx_page() && perf.shifted() && (i==0 || i==2 || i==3)){static const char* groups[]={"A","B","C","D","E","F","G","H"};static const char* outputs[]={"Off","Main","Cue","Both"};out.append(i==0?groups[perf.group_cursor()]:i==2?outputs[unsigned(raw)%4]:raw>0?"Assigned":"None");}
   else if(perf.mode()==UiMode::Routing && !perf.routing_fx_page() && i>=4 && i<=6){const char* buses[]={"Off","Main","Cue","Both"};if(i==6){unsigned source=unsigned(raw),track=capture_track(source);out.append(capture_name(source));if(track<16 || source>=53)out.number(source>=53?source-52:track+1,0);}else out.append(buses[unsigned(raw)%4]);}
   else if(perf.mode()==UiMode::Scene && i==3){out.append(p.chain.mode==FollowMode::Sequential?"Sequence":p.chain.mode==FollowMode::Random?"Random":"No repeat");}
   else if(perf.mode()==UiMode::Scene && ((i==6 && !perf.shifted() && raw==0) || (i==7 && !perf.shifted() && raw<0))){out.append("Global");}
   else if(perf.mode()==UiMode::Scene && i==7 && perf.shifted()){const char* conditions[]={"Always","Fill","Not fill"};out.append(conditions[unsigned(raw)%3]);}
   else if(perf.mode()==UiMode::Effects && (perf.fx_page()==1 || perf.fx_page()==2) && i==6){if(raw<0.5f)out.append(perf.fx_page()==2?"Self":"Master");else{out.append("Track");out.number(raw,0);}}
   else if(perf.mode()==UiMode::Generate && perf.generate_page()==4 && i==0){const char* modes[]={"Off","Up","Down","Up/down","Random"};out.append(modes[unsigned(raw)%5]);}
   else if(perf.mode()==UiMode::Generate && perf.generate_page()==4 && i==1){out.number(float(1u<<unsigned(raw)),0);out.append("x");}
   else if(perf.mode()==UiMode::Generate && perf.generate_page()==1 && i==3){const char* modes[]={"Wrap","Bounce","Clamp"};out.append(modes[unsigned(track_at(p,p.active_pattern,perf.selected_track()).accumulator.mode)]);}
   else if(perf.mode()==UiMode::Modulation && ((perf.modulation_page()==1 && i==5) || (perf.modulation_page()==2 && i==5))){static const char* names[]={"Replace","Add","Multiply"};out.append(names[unsigned(raw)]);}
   else if(perf.mode()==UiMode::Modulation && perf.modulation_page()==4 && i==5){out.append(raw>0?"RPN":"NRPN");}
   else if(perf.mode()==UiMode::Modulation && ((perf.modulation_page()==1 && i==6) || (perf.modulation_page()==2 && i>=6) || (perf.modulation_page()==3 && i==7 && !perf.shifted()))){out.append(raw>0?"On":"Off");}
   else if(perf.mode()==UiMode::Modulation && perf.modulation_page()==3 && (i==5 || i==6)){static const char* notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};out.append(i==5?notes[unsigned(raw)%12]:scale_name(unsigned(raw)));}
   else if(perf.mode()==UiMode::Settings){out.append(perf.setting_text(i));}

   else if(perf.source_select() && i==0){out.append(instrument_descriptor(perf.source_cursor())->family);}
   else if(perf.source_select() && i==1){out.append(instrument_descriptor(perf.source_cursor())->name);}
   else if(perf.library_catalog_controls() && i==2){const auto* engine=instrument_descriptor(unsigned(perf.browser().library_instrument));out.append(engine?engine->name:"UNAVAILABLE");}else if(perf.library_catalog_controls() && i==3){out.append(perf.browser().library_originals?"ORIGINALS":"MY VERSIONS");}
   else if(perf.mode()==UiMode::Effects && perf.fx_page()==5 && i==0){static const char* nodes[]={"TONE","INSERT 1","INSERT 2","COMP"};out.append(nodes[perf.chain_node()<4?perf.chain_node():0]);}

   else{out.number(displayed,precision);if(*unit)out.append(unit);}

  return out;
 }

}

namespace groove {
auto UiRenderer::composition_panel(const ProjectState& p,const Performance& perf) -> void{
 for(unsigned side=0;side<2;++side)fill(side,0,ui::Layout::body_y,480,ui::Layout::body_h,theme.background);
 label(0,24,85,perf.composition_dialog()==2?"DUPLICATE SCENE":"DUPLICATE + VARIATION",theme.text,Font::Heading,430);
 for(unsigned i=0;i<(perf.composition_dialog()==2?1u:2u);++i){auto shown=formatted_knob(p,perf,i);value_row(0,24,122+int(i)*34,430,30,{perf.knob_label(i),shown.text,perf.knob_normalized(i),ui::ValueShape::Bar});}
 label(1,24,85,perf.composition_dialog()==2?"SELECTED SCENE > EMPTY SLOT":"EDITED PATTERN > NEXT EMPTY",theme.secondary,Font::Small,430);
 value(1,24,112,70,perf.composition_dialog()==2?perf.scene_duplicate_source()+1:p.active_pattern+1,theme.text,Font::Number,0);
 label(1,24,155,"SOURCE KEPT / ONE UNDO",theme.secondary,Font::Body,430);
 label(1,24,184,perf.notice_error()?perf.notice_text():"TOP 1 APPLY / TOP 2 CANCEL",perf.notice_error()?theme.warn:theme.secondary,Font::Small,430);
}
auto UiRenderer::twin_panel(const Performance& perf) -> void{
 const auto& config=perf.twin_config();bool running=perf.twin_dialog()==2;
 for(unsigned side=0;side<2;++side){fill(side,0,64,480,ui::Layout::overlay_y-64,theme.background);panel(side,10,79,460,128);}
 label(0,24,85,"TWIN / PREPARE",theme.text,Font::Heading,420);
 label(1,24,85,running?"AUTO SAMPLING / MAIN ISOLATED":"SYNTH < > SAMPLE / NEXT BAR",theme.secondary,Font::Small,430);
 label(0,24,111,config.valid()?"RANGE / UP TO 16 ZONES":"INVALID RANGE / MAX 16 NOTES",config.valid()?theme.secondary:theme.warn,Font::Small,350);
 value(0,390,111,55,config.count(),config.valid()?theme.text:theme.warn,Font::Small,0);
 unsigned count=config.count();if(count>16)count=16;
 for(unsigned i=0;i<count;++i){int x=24+int(i)*25;bool current=running && i==perf.twin_note_index();fill(0,x,133,20,8,current?theme.accent:running && i<perf.twin_note_index()?theme.teal:theme.dim);}
 label(1,24,111,running?"NOTE":"DRY TAP / TRACK FX STAY LIVE",theme.secondary,Font::Small,420);
 if(running){value(1,82,111,50,float(perf.twin_note_index()<config.count()?perf.twin_note_index()+1:config.count()),theme.text,Font::Small,0);value(1,180,111,50,float(config.count()),theme.secondary,Font::Small,0);level_meter(1,24,134,410,6,float(perf.twin_progress())/100,theme.teal);}
 static const char* labels[]={"LOW NOTE","HIGH NOTE","INTERVAL","VELOCITY","HOLD MS","LIMIT MS"};
 for(unsigned i=0;i<6;++i){unsigned side=i/4;int x=24+int(i%4)*113;label(side,x,157,labels[i],theme.secondary,Font::Small,105);value(side,x,177,95,perf.knob_value(i),theme.text,Font::Body,0);}
 label(1,250,157,running?"TOP 1 CANCEL":"TOP 1 PREPARE",theme.text,Font::Small,200);label(1,250,180,"SOURCE KEPT UNTIL COMMIT",theme.secondary,Font::Small,200);
 }
}
