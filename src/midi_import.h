#pragma once
#include "file_access.h"
#include "drum_gm.h"
#include "sequence_operations.h"
namespace groove {
// Bounded, task-owned SMF note importer. It never exposes a partial edit to audio.
class MidiImport {
 char source_track_name_[96]{},imported_track_name_[96]{};bool source_track_has_channel_=false;
 struct Event {uint64_t tick;uint32_t order;uint8_t status,note,velocity;};
 static constexpr unsigned kMaxBytes=256*1024,kMaxEvents=4096;
public:
 MidiImport(IFileAccess& fs):fs_(fs){}
 ~MidiImport(){delete[] data_;delete[] events_;delete[] scratch_;delete kit_before_;delete kit_after_;}
 MidiImport(const MidiImport&)=delete;MidiImport& operator=(const MidiImport&)=delete;
 bool prepare_target(const ProjectState& project,unsigned pattern,unsigned track){
  if(busy_ || pattern>=kPatterns || track>=kTracks)return false;
  delete kit_before_;delete kit_after_;kit_before_=kit_after_=nullptr;spill_mask_=folded_=unmapped_=dropped_=0;spare_mask_=0;
  if(project.tracks[track].instrument==Instrument::DrumKit){kit_before_=alloc_new<PatternState>(project,pattern);kit_after_=kit_before_?alloc_new<PatternState>(*kit_before_):nullptr;if(!kit_after_)return false;for(unsigned t=0;t<16;++t)if(t!=track && project.tracks[t].instrument==Instrument::Empty && project.audio[t].kind==TrackKind::Instrument){bool empty=true;for(unsigned p=0;p<kPatterns && empty;++p)empty=sequence_empty(track_at(project,p,t));if(empty)spare_mask_|=uint16_t(1u<<t);}}
  source_track_name_[0]=imported_track_name_[0]=0;source_track_has_channel_=false;baseline_=track_at(project,pattern,track);result_=baseline_;pattern_=pattern;track_=track;
  tempo_=project.tempo;channel_=baseline_.midi_channel;kind_=project.audio[track].kind;
  cc_lane_mask_=nrpn_slot_mask_=nrpn_seen_mask_=0;division_=0;prepared_=true;done_=false;return true;
 }
 bool open(const char* path){
  if(busy_ || !prepared_ || !copy_text(path_,sizeof path_,path))return false;
  done_=false;uint64_t size=fs_.size(path);if(size<14 || size>kMaxBytes){error_="MIDI file size unsupported";return false;}
  delete[] data_;delete[] events_;delete[] scratch_;
  size_=unsigned(size);data_=alloc_array<uint8_t>(size_);events_=alloc_array<Event>(kMaxEvents);scratch_=alloc_array<Event>(kMaxEvents);
  if(!data_ || !events_ || !scratch_){error_="MIDI import out of memory";return false;}
  revision_known_=fs_.revision(path_,revision_);read_=verify_=position_=event_count_=skipped_=note_count_=tracks_left_=0;
  verified_=parsing_=track_open_=false;busy_=true;error_="Reading MIDI...";return true;
 }
 bool start(const char* path,const ProjectState& project,unsigned pattern,unsigned track){return prepare_target(project,pattern,track) && open(path);}
 bool busy()const{return busy_;}bool done()const{return done_;}const char* message()const{return error_;}unsigned skipped()const{return skipped_;}unsigned notes()const{return note_count_;}unsigned cc_lane_mask()const{return cc_lane_mask_;}unsigned nrpn_slot_mask()const{return nrpn_slot_mask_;}unsigned pattern()const{return pattern_;}unsigned track()const{return track_;}ConstTrackView result()const{return result_;}
 unsigned folded()const{return folded_;}unsigned unmapped()const{return unmapped_;}unsigned dropped()const{return dropped_;}unsigned spill_mask()const{return spill_mask_;}bool kit_import()const{return kit_after_!=nullptr;}ConstTrackView spill(unsigned t)const{return kit_after_->tracks[t];}
 bool target_unchanged(const ProjectState& p)const{
  if(!prepared_ || p.audio[track_].kind!=kind_ || ((division_&0x8000) && p.tempo!=tempo_))return false;
  if(kit_before_){if(!same_track_snapshot(track_at(p,pattern_,track_),kit_before_->tracks[track_]))return false;for(unsigned t=0;t<16;++t)if((spare_mask_&(1u<<t)) && (p.audio[t].kind!=TrackKind::Instrument || !same_track_snapshot(track_at(p,pattern_,t),kit_before_->tracks[t])))return false;}
  const auto t=track_at(p,pattern_,track_);
  if(__builtin_strcmp(t.track_name,baseline_.track_name) || t.length!=baseline_.length || t.root_note!=baseline_.root_note || t.midi_channel!=baseline_.midi_channel || t.instrument!=baseline_.instrument)return false;
  for(unsigned i=0;i<kSteps;++i)if(!same_step(t.steps[i],baseline_.steps[i]))return false;
  if(cc_lane_mask_)for(unsigned i=0;i<2;++i){if(!same_automation(t.automation[i],baseline_.automation[i]))return false;if((cc_lane_mask_&(1u<<i)) && result_.automation[i].parameter>=128 && result_.automation[i].parameter<256){unsigned cc=result_.automation[i].parameter-128;if(t.midi_cc[cc]!=baseline_.midi_cc[cc] || ((t.midi_cc_used[cc/8]^baseline_.midi_cc_used[cc/8])&(1u<<(cc%8))))return false;}}
  if(cc_lane_mask_ && (t.pitch_bend!=baseline_.pitch_bend || t.pressure!=baseline_.pressure || t.expression_used!=baseline_.expression_used))return false;
  if(nrpn_seen_mask_)for(unsigned i=0;i<4;++i){const auto& a=t.nrpn[i];const auto& b=baseline_.nrpn[i];if(a.enabled!=b.enabled || a.number!=b.number || a.value!=b.value || a.registered!=b.registered)return false;}
  return true;
 }
 void service(){
  if(!busy_)return;
  if(read_<size_){unsigned wanted=size_-read_>4096?4096:size_-read_,got=0;if(!fs_.read(path_,read_,data_+read_,wanted,got)||got!=wanted){fail("MIDI read failed");return;}read_+=got;return;}
  if(!verified_){
   if(fs_.size(path_)!=size_){fail("MIDI source changed during read");return;}
   if(revision_known_){FileRevision current;if(!fs_.revision(path_,current) || !same_file_revision(revision_,current)){fail("MIDI source changed during read");return;}verified_=true;return;}
   unsigned wanted=size_-verify_>4096?4096:size_-verify_,got=0;auto* buffer=reinterpret_cast<uint8_t*>(scratch_);
   if(!fs_.read(path_,verify_,buffer,wanted,got) || got!=wanted || __builtin_memcmp(buffer,data_+verify_,wanted)){fail("MIDI source changed during read");return;}
   verify_+=wanted;if(verify_==size_)verified_=true;return;
  }
  if(!parsing_){if(!header()){fail("Unsupported MIDI header");return;}parsing_=true;}
  for(unsigned budget=0;budget<128 && tracks_left_;++budget){if(!track_open_ && !open_track()){fail("Invalid MIDI track");return;}if(!parse_event())return;}
  if(!tracks_left_){if(position_!=size_){fail("Trailing MIDI data");return;}if(!finish())return;busy_=false;done_=true;if(kit_after_){unsigned at=0;const char* prefix="GM imported / folded ";while(*prefix)summary_[at++]=*prefix++;auto number=[&](unsigned n){char b[12];unsigned len=0;do{b[len++]=char('0'+n%10);n/=10;}while(n);while(len)summary_[at++]=b[--len];};number(folded_);auto text=[&](const char* p){while(*p && at<80)summary_[at++]=*p++;};text(" / unmapped ");number(unmapped_);text(" / dropped ");number(dropped_);summary_[at]=0;error_=summary_;}else error_=skipped_?"Imported / some details simplified":"MIDI events imported";}
 }
private:
 static bool same_automation(const AutomationLane& a,const AutomationLane& b){if(a.enabled!=b.enabled || a.parameter!=b.parameter || a.length!=b.length || a.mode!=b.mode)return false;for(unsigned i=0;i<kAutomationSteps;++i)if(a.values[i]!=b.values[i])return false;return true;}
 static uint16_t u16(const uint8_t* p){return uint16_t((unsigned(p[0])<<8)|p[1]);}static uint32_t u32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
 bool header(){if(size_<14 || __builtin_memcmp(data_,"MThd",4))return false;unsigned length=u32(data_+4);if(length<6 || uint64_t(length)+8>size_)return false;unsigned format=u16(data_+8),tracks=u16(data_+10);division_=u16(data_+12);if(format>1 || !tracks || tracks>32 || (format==0 && tracks!=1) || !division_)return false;if(division_&0x8000){int fps=256-(division_>>8);if((fps!=24 && fps!=25 && fps!=29 && fps!=30) || !(division_&255))return false;}position_=8+length;tracks_left_=tracks;return true;}
 bool open_track(){if(position_+8>size_ || __builtin_memcmp(data_+position_,"MTrk",4))return false;uint32_t length=u32(data_+position_+4);if(uint64_t(position_)+8+length>size_)return false;position_+=8;end_=position_+length;source_track_name_[0]=0;source_track_has_channel_=false;tick_=0;running_=0;track_open_=true;return true;}
 bool vlq(uint32_t& out){out=0;for(unsigned i=0;i<4;++i){if(position_>=end_)return false;uint8_t v=data_[position_++];out=(out<<7)|(v&127);if(!(v&128))return true;}return false;}
 bool parse_event(){
  uint32_t delta=0;if(!vlq(delta) || tick_>UINT64_MAX-delta){fail("Invalid MIDI delta");return false;}tick_+=delta;if(position_>=end_){fail("Truncated MIDI event");return false;}
  uint8_t status=data_[position_];if(status&128){++position_;if(status<0xf0)running_=status;else running_=0;}else{if(!running_){fail("Invalid running status");return false;}status=running_;}
  if(status==0xff){if(position_>=end_){fail("Truncated MIDI meta");return false;}uint8_t type=data_[position_++];uint32_t length=0;if(!vlq(length) || uint64_t(position_)+length>end_){fail("Invalid MIDI meta");return false;}if(type==0x03){unsigned n=length<95?length:95;for(unsigned i=0;i<n;++i){uint8_t c=data_[position_+i];source_track_name_[i]=c>=32 && c<=126?char(c):'?';}source_track_name_[n]=0;}position_+=length;if(type==0x51)++skipped_;if(type==0x2f){if(length || position_!=end_){fail("Invalid MIDI track end");return false;}if(source_track_has_channel_ && source_track_name_[0] && !imported_track_name_[0]){copy_text(imported_track_name_,96,source_track_name_);copy_text(result_.track_name,96,source_track_name_);}track_open_=false;--tracks_left_;}return true;}
  if(status==0xf0 || status==0xf7){uint32_t length=0;if(!vlq(length)||uint64_t(position_)+length>end_){fail("Invalid MIDI SysEx");return false;}position_+=length;++skipped_;return true;}
  if(status>=0xf0){fail("Unsupported MIDI event");return false;}unsigned length=(status&0xf0)==0xc0 || (status&0xf0)==0xd0?1:2;if(position_+length>end_ || data_[position_]&128 || (length==2 && data_[position_+1]&128)){fail("Invalid MIDI message");return false;}uint8_t a=data_[position_++],b=length==2?data_[position_++]:0;
  if((status&15)==channel_){source_track_has_channel_=true;if((status&0xf0)==0x80 || (status&0xf0)==0x90 || (status&0xf0)==0xe0 || (status&0xf0)==0xd0 || ((status&0xf0)==0xb0 && (a<120 || a==121) && baseline_.instrument==Instrument::ExternalMidi)){if(event_count_==kMaxEvents){fail("MIDI event limit exceeded");return false;}events_[event_count_]={tick_,event_count_,status,a,b};++event_count_;}else ++skipped_;}return true;
 }
 static bool before(const Event& a,const Event& b){return a.tick<b.tick || (a.tick==b.tick && a.order<b.order);}
 bool units(uint64_t tick,uint64_t& result)const{if(tick>(uint64_t(1)<<36))return false;uint64_t numerator,denominator;if(!(division_&0x8000)){numerator=tick*1536;denominator=division_;}else{unsigned fps=256-(division_>>8),sub=division_&255;uint64_t factor=uint64_t(tempo_)*1536*(fps==29?1001:1);if(tick>UINT64_MAX/factor)return false;numerator=tick*factor;denominator=uint64_t(fps==29?30000:fps)*sub*60;}if(numerator>UINT64_MAX-denominator/2)return false;result=(numerator+denominator/2)/denominator;return true;}
 bool finish_kit(){
  for(auto& pad:result_.kit.steps)for(auto& step:pad)step=KitStepState{};
  uint8_t destinations[128],cells[128]{};bool held[128]{};uint64_t starts[128]{};for(auto& t:destinations)t=255;unsigned last=0;
  for(unsigned i=0;i<event_count_;++i){const auto& e=events_[i];if((e.status&0xf0)!=0x80 && (e.status&0xf0)!=0x90){++skipped_;continue;}uint64_t time;if(!units(e.tick,time)){fail("MIDI time out of range");return false;}bool on=(e.status&0xf0)==0x90 && e.velocity;int pad=kit_gm_pad(baseline_.kit,e.note);
   if(on){uint64_t index=(time+192)/384;if(index>=kSteps){fail("MIDI exceeds 256 steps");return false;}unsigned cell=unsigned(index);if(cell>last)last=cell;
    if(pad>=0){auto& step=result_.kit.steps[pad][cell];step.enabled=1;step.velocity=e.velocity;step.microtiming=int8_t((int(time)-int(cell*384))/4);if(kit_gm_exact(baseline_.kit,e.note)<0)++folded_;++note_count_;continue;}
    ++unmapped_;unsigned t=destinations[e.note];if(t==255){uint16_t free=spare_mask_&~spill_mask_;if(!free){++dropped_;continue;}t=unsigned(__builtin_ctz(free));destinations[e.note]=uint8_t(t);spill_mask_|=uint16_t(1u<<t);for(auto& step:kit_after_->tracks[t].steps)step=StepState{};}
    if(held[e.note]){fail("Overlapping duplicate note");return false;}auto& step=kit_after_->tracks[t].steps[cell];step.enabled=true;step.chromatic=true;step.native_drum=true;int note=60+int(e.note)-int(kit_after_->tracks[t].root_note);if(note<0 || note>127){fail("MIDI pitch outside track range");return false;}step.note=uint8_t(note);step.velocity=e.velocity;step.microtiming=int16_t(int(time)-int(cell*384));held[e.note]=true;starts[e.note]=time;cells[e.note]=uint8_t(cell);++note_count_;
   }else if(pad<0 && held[e.note]){uint64_t duration=time-starts[e.note];if(duration>1537){fail("MIDI note exceeds gate limit");return false;}unsigned gate=unsigned((duration+2)/4);kit_after_->tracks[destinations[e.note]].steps[cells[e.note]].gate=uint16_t(gate?gate:1);held[e.note]=false;}
  }
  for(bool h:held)if(h){fail("MIDI contains unclosed notes");return false;}
  if(!note_count_){fail("No notes on MIDI channel / kit kept");return false;}
  result_.length=uint16_t((last/16+1)*16);for(unsigned t=0;t<16;++t)if(spill_mask_&(1u<<t))kit_after_->tracks[t].length=result_.length;return true;
 }
 bool finish(){
  if(!event_count_){fail("No supported events on MIDI channel");return false;}
  for(unsigned width=1;width<event_count_;width*=2){for(unsigned begin=0;begin<event_count_;begin+=width*2){unsigned mid=begin+width<event_count_?begin+width:event_count_,end=begin+width*2<event_count_?begin+width*2:event_count_,a=begin,b=mid;for(unsigned k=begin;k<end;++k)scratch_[k]=(b==end || (a<mid && before(events_[a],events_[b])))?events_[a++]:events_[b++];}Event* tmp=events_;events_=scratch_;scratch_=tmp;}
  if(kit_after_)return finish_kit();
  for(auto& step:result_.steps)step=StepState{};
  bool held[128]{};uint64_t starts[128]{};uint8_t cells[128]{};bool gate_set[kSteps]{};unsigned last=0,cc_last=0,imported_control=0;
  uint8_t cc_lane[128],nrpn_lane[4],expression_lane[2]={255,255};for(auto& x:cc_lane)x=255;for(auto& x:nrpn_lane)x=255;
  uint16_t control_value[2][kAutomationSteps]{};bool control_cells[2][kAutomationSteps]{};uint64_t last_control_tick[2][kAutomationSteps]{},first_nrpn_tick[4]{};
  int select_msb[2]={-1,-1},select_lsb[2]={-1,-1},data_msb=-1,data_lsb=0,selected_number=-1,selected_type=-1;
  auto allocate_lane=[&](uint16_t parameter)->unsigned{
   for(const auto& existing:baseline_.automation)if(existing.enabled && existing.parameter==parameter)return 255;
   for(unsigned i=0;i<2;++i)if(!baseline_.automation[i].enabled && !(cc_lane_mask_&(1u<<i))){cc_lane_mask_|=1u<<i;auto& a=result_.automation[i];a=AutomationLane{};a.enabled=true;a.parameter=parameter;a.mode=LayerMode::Replace;return i;}
   return 255;
  };
  auto record_control=[&](unsigned lane,unsigned cell,uint16_t value,uint64_t tick,bool pair){
   if(control_cells[lane][cell] && (!pair || last_control_tick[lane][cell]!=tick))++skipped_;
   control_cells[lane][cell]=true;control_value[lane][cell]=value;last_control_tick[lane][cell]=tick;if(cell>cc_last)cc_last=cell;++imported_control;
  };
  for(unsigned i=0;i<event_count_;++i){const auto& e=events_[i];uint64_t time;if(!units(e.tick,time)){fail("MIDI time out of range");return false;}
   if((e.status&0xf0)==0xe0 || (e.status&0xf0)==0xd0){unsigned kind=(e.status&0xf0)==0xe0?0:1;uint64_t cell64=(time+192)/384;if(cell64>=kAutomationSteps){++skipped_;continue;}if(time!=cell64*384)++skipped_;unsigned lane=expression_lane[kind];if(lane==255){lane=allocate_lane(uint16_t(260+kind));if(lane==255){++skipped_;continue;}expression_lane[kind]=uint8_t(lane);}unsigned value=kind==0?unsigned(e.note)|(unsigned(e.velocity)<<7):e.note;record_control(lane,unsigned(cell64),uint16_t(value),e.tick,false);continue;}
   if((e.status&0xf0)==0xb0){unsigned cc=e.note;
    if(cc==121){selected_type=selected_number=data_msb=-1;select_msb[0]=select_msb[1]=select_lsb[0]=select_lsb[1]=-1;data_lsb=0;++skipped_;continue;}
    if(cc==99 || cc==98 || cc==101 || cc==100){int type=cc==101 || cc==100?1:0;if(type!=selected_type){selected_type=type;select_msb[type]=select_lsb[type]=-1;selected_number=-1;data_msb=-1;data_lsb=0;}if(cc==99 || cc==101)select_msb[type]=e.velocity;else select_lsb[type]=e.velocity;if(select_msb[type]>=0 && select_lsb[type]>=0){int next=select_msb[type]*128+select_lsb[type];if(next!=selected_number){selected_number=next;data_msb=-1;data_lsb=0;}}continue;}
    if(cc==6 || cc==38 || cc==96 || cc==97){
     if(selected_type<0 || selected_number<0 || selected_number==16383){++skipped_;continue;}
     if(cc==6){data_msb=e.velocity;}else if(cc==38){if(data_msb<0){++skipped_;continue;}data_lsb=e.velocity;}else{
      // Relative units depend on the parameter. Never guess manufacturer NRPNs.
      if(data_msb<0 || !selected_type || (selected_number!=0 && selected_number!=1 && selected_number!=2 && selected_number!=5)){data_msb=-1;data_lsb=0;++skipped_;continue;}
      int delta=cc==96?1:-1;
      if(selected_number==0){if(data_lsb>=100){data_msb=-1;data_lsb=0;++skipped_;continue;}int cents=data_msb*100+data_lsb+delta;if(cents<0)cents=0;if(cents>12799)cents=12799;data_msb=cents/100;data_lsb=cents%100;}
      else if(selected_number==2){data_msb+=delta;if(data_msb<0)data_msb=0;if(data_msb>127)data_msb=127;}
      else{int value=data_msb*128+data_lsb+delta;if(value<0)value=0;if(value>16383)value=16383;data_msb=value/128;data_lsb=value%128;}
     }
     uint64_t cell64=(time+192)/384;if(cell64>=kAutomationSteps){++skipped_;continue;}unsigned cell=unsigned(cell64);if(time!=cell64*384)++skipped_;
     unsigned slot=255,matches=0;for(unsigned j=0;j<4;++j)if(result_.nrpn[j].enabled && result_.nrpn[j].number==unsigned(selected_number) && result_.nrpn[j].registered==bool(selected_type)){slot=j;++matches;}
     if(matches>1){++skipped_;continue;}bool fresh=false;if(!matches){for(unsigned j=0;j<4;++j)if(!result_.nrpn[j].enabled){slot=j;fresh=true;break;}}
     if(slot==255){++skipped_;continue;}unsigned lane=nrpn_lane[slot];if(lane==255){lane=allocate_lane(uint16_t(256+slot));if(lane==255){++skipped_;continue;}nrpn_lane[slot]=uint8_t(lane);if(fresh){result_.nrpn[slot].number=uint16_t(selected_number);result_.nrpn[slot].enabled=true;result_.nrpn[slot].registered=selected_type!=0;result_.nrpn[slot].value=uint16_t(data_msb*128+data_lsb);first_nrpn_tick[slot]=e.tick;nrpn_slot_mask_|=1u<<slot;}nrpn_seen_mask_|=1u<<slot;}
     uint16_t value=uint16_t(data_msb*128+data_lsb);if((nrpn_slot_mask_&(1u<<slot)) && first_nrpn_tick[slot]==e.tick)result_.nrpn[slot].value=value;record_control(lane,cell,value,e.tick,cc==38);continue;
    }
    uint64_t cell64=(time+192)/384;if(cell64>=kAutomationSteps){++skipped_;continue;}unsigned cell=unsigned(cell64);if(time!=cell64*384)++skipped_;unsigned lane=cc_lane[cc];if(lane==255){lane=allocate_lane(uint16_t(128+cc));if(lane==255){++skipped_;continue;}cc_lane[cc]=uint8_t(lane);}record_control(lane,cell,e.velocity,e.tick,false);continue;
   }
   bool on=(e.status&0xf0)==0x90 && e.velocity;
   if(on){uint64_t index=(time+192)/384;if(index>=kSteps){fail("MIDI exceeds 256 steps");return false;}unsigned cell=unsigned(index);int note=60+int(e.note)-int(result_.root_note);if(note<0 || note>127){fail("MIDI pitch outside track range");return false;}if(held[e.note]){fail("Overlapping duplicate note");return false;}auto& step=result_.steps[cell];int micro=int(time)-int(cell*384);if(!step.enabled){step.enabled=true;step.native_drum=true;step.note=uint8_t(note);step.velocity=e.velocity;step.microtiming=int16_t(micro);}else{if(step.microtiming!=micro || step.velocity!=e.velocity)++skipped_;unsigned slot=0;while(slot<kChordTones && step.chord[slot]!=255)++slot;if(slot==kChordTones){fail("More than eight notes per step");return false;}step.chord[slot]=uint8_t(note);}held[e.note]=true;starts[e.note]=time;cells[e.note]=uint8_t(cell);if(cell>last)last=cell;++note_count_;
   }else{if(!held[e.note]){++skipped_;continue;}if(time<starts[e.note]){fail("Invalid MIDI note order");return false;}uint64_t units_length=time-starts[e.note];if(units_length>1537){fail("MIDI note exceeds gate limit");return false;}unsigned gate=unsigned((units_length+2)/4);if(!gate)gate=1;if(gate>384){fail("MIDI note exceeds gate limit");return false;}auto& step=result_.steps[cells[e.note]];if(!gate_set[cells[e.note]]){step.gate=uint16_t(gate);gate_set[cells[e.note]]=true;}else if(step.gate!=gate)++skipped_;held[e.note]=false;}
  }
  for(bool note:held)if(note){fail("MIDI contains unclosed notes");return false;}
  if(!note_count_ && !imported_control){fail("No supported events on MIDI channel");return false;}
  if(imported_control && cc_last>last){last=cc_last;}result_.length=uint16_t(((last/16)+1)*16);
  for(unsigned i=0;i<2;++i)if(cc_lane_mask_&(1u<<i)){auto& a=result_.automation[i];a.length=result_.length;float value=a.parameter>=260?track_parameter(baseline_,a.parameter):a.parameter<256?float(baseline_.midi_cc[a.parameter-128]):float(result_.nrpn[a.parameter-256].value);if(!control_cells[i][0])++skipped_;for(unsigned cell=0;cell<kAutomationSteps;++cell){if(control_cells[i][cell])value=control_value[i][cell];a.values[cell]=value;}}
  return true;
 }
 void fail(const char* reason){busy_=false;done_=false;error_=reason;}
 PatternState* kit_before_=nullptr;PatternState* kit_after_=nullptr;uint16_t spare_mask_=0,spill_mask_=0;unsigned folded_=0,unmapped_=0,dropped_=0;char summary_[96]{};
 IFileAccess& fs_;uint8_t* data_=nullptr;Event* events_=nullptr;Event* scratch_=nullptr;TrackState baseline_{},result_{};char path_[160]{};const char* error_="";unsigned size_=0,read_=0,verify_=0,position_=0,end_=0,tracks_left_=0,event_count_=0,skipped_=0,note_count_=0,pattern_=0,track_=0,tempo_=120,channel_=0,cc_lane_mask_=0,nrpn_slot_mask_=0,nrpn_seen_mask_=0;uint16_t division_=0;uint8_t running_=0;uint64_t tick_=0;FileRevision revision_{};TrackKind kind_=TrackKind::Instrument;bool revision_known_=false,verified_=false,prepared_=false,busy_=false,done_=false,parsing_=false,track_open_=false;
};
}
