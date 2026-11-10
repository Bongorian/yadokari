#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
struct Bytes {std::vector<uint8_t> data;size_t offset=0;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(n>data.size()-offset)return false;__builtin_memcpy(p,data.data()+offset,n);offset+=n;return true;}bool at_end()const{return offset==data.size();}};
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->random_seed=412;
 auto src=track_at(*p,0,0);src.instrument=Instrument::VirtualAnalog;src.level=.43f;src.steps[0].enabled=true;src.steps[0].note=60;src.steps[0].chord[0]=64;src.steps[0].chord[1]=67;src.steps[8]=src.steps[0];src.steps[8].note=65;src.steps[8].chord[0]=69;src.steps[8].chord[1]=72;src.steps[0].locks[0]={1,.3f,true};src.automation[0].enabled=true;src.automation[0].parameter=1;src.automation[0].values[0]=.4f;
 p->tracks[2].instrument=Instrument::DrumKit;p->patterns[0].tracks[2].kit.steps[3][2].enabled=true;
 auto bass=track_at(*p,0,1);bass.instrument=Instrument::VirtualAnalog;bass.root_note=36;bass.level=.28f;
 // Harmony follows heard source pitches even when the destination has transpose.
 bass.transpose=5;auto transposed=std::make_unique<TrackState>(bass);assert(make_harmony(*transposed,ConstTrackView(src),ConstTrackView(bass),HarmonyMode::Bass));assert(int(bass.root_note)+transposed->steps[0].note-60+bass.transpose==36);bass.transpose=0;
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));auto ui=std::make_unique<Performance>(*e,*p);float buffer[2]{};auto flush=[&](){e->render(buffer,0);ui->poll();};flush();
 auto original=std::make_unique<TrackState>(track_at(*p,0,0));auto untouched=std::make_unique<TrackState>(track_at(*p,0,1));
 ui->set_mode(UiMode::Clip);ui->set_shift(true);assert(ui->knob(7,1000));assert(ui->copy_track_only());ui->set_shift(false);assert(ui->copy_pattern());assert(ui->select_pattern(1));flush();assert(ui->paste_pattern());flush();assert(same_track_snapshot(track_at(*p,1,0),ConstTrackView(*original)));assert(p->tracks[0].level==.43f && p->tracks[1].level==.28f);assert(ui->undo());flush();assert(sequence_empty(p->patterns[1].tracks[0]));assert(ui->redo());flush();assert(p->patterns[1].tracks[0].automation[0].enabled);
 assert(ui->select_pattern(0));flush();
 for(auto kind:{PatternVariation::Thin,PatternVariation::Transpose,PatternVariation::DropTrack}){
  ui->variation_options().kind=kind;ui->variation_options().density=0;ui->variation_options().transpose=2;ui->variation_options().drop_track=0;
  assert(ui->duplicate_variation());flush();unsigned dest=p->active_pattern;assert(dest==2);assert(same_track_snapshot(track_at(*p,0,0),ConstTrackView(*original)));
  if(kind==PatternVariation::Thin)assert(!p->patterns[dest].tracks[0].steps[0].enabled && p->patterns[dest].tracks[0].steps[0].lock_only && !p->patterns[dest].tracks[2].kit.steps[3][2].enabled);
  if(kind==PatternVariation::Transpose)assert(p->patterns[dest].tracks[0].steps[0].note==62 && p->patterns[dest].tracks[2].kit.steps[3][2].pitch_lock==2);
  if(kind==PatternVariation::DropTrack)assert(sequence_empty(p->patterns[dest].tracks[0]));
  auto first=std::make_unique<PatternState>(*p,dest);assert(ui->undo());flush();assert(sequence_empty(p->patterns[dest].tracks[0]));assert(ui->select_pattern(0));flush();assert(ui->duplicate_variation());flush();for(unsigned t=0;t<16;++t)assert(same_track_snapshot(track_at(*p,dest,t),ConstTrackView(first->tracks[t])));assert(ui->undo());flush();assert(ui->select_pattern(0));flush();
 }
 {bool was[kPatterns];for(unsigned i=0;i<kPatterns;++i){was[i]=p->patterns[i].tracks[0].steps[0].enabled;p->patterns[i].tracks[0].steps[0].enabled=true;}assert(!ui->duplicate_variation() && ui->notice_error());for(unsigned i=0;i<kPatterns;++i)p->patterns[i].tracks[0].steps[0].enabled=was[i];}
 ui->set_mode(UiMode::Generate);for(unsigned n=0;n<5;++n)ui->encoder_direction(0x08);assert(ui->generate_page()==5);assert(ui->knob(1,1000));assert(ui->selected_track()==1);assert(ui->harmony_generate());flush();assert(p->patterns[0].tracks[1].steps[0].enabled && p->patterns[0].tracks[1].steps[0].note==60);assert(!p->patterns[0].tracks[1].steps[1].enabled);assert(ui->undo());flush();assert(same_track_snapshot(track_at(*p,0,1),ConstTrackView(*untouched)));assert(ui->knob(2,1000));assert(ui->harmony_generate());flush();for(unsigned n=0;n<16;++n)assert(p->patterns[0].tracks[1].steps[n].enabled);assert(ui->undo());flush();assert(ui->knob(0,3000));assert(!ui->harmony_generate() && ui->notice_error());
 assert(ui->select_track(0));ui->set_mode(UiMode::Generate);
 auto previous=std::make_unique<PatternState>(*p,0);p->tracks[0].groove={};p->tracks[0].groove.preset=2;p->tracks[0].groove.amount=67;assert(e->stage_project(*p));flush();ui->project_loaded();assert(ui->apply_groove_all());flush();for(auto& t:p->tracks)assert(t.groove.preset==2 && t.groove.amount==67);p->tracks[3].level=.21f;assert(ui->undo());flush();assert(p->tracks[1].groove.amount==previous->tracks[1].groove.amount && p->tracks[3].level==.21f);
 // Song skeleton is a single edit, never overwrites an existing arrangement.
 ui->set_mode(UiMode::Song);assert(ui->soft_key(5));flush();assert(p->song.length==5 && p->song.loop && p->song.enabled);const unsigned bars[]={4,8,8,8,4};for(unsigned i=0;i<5;++i)assert(p->song.sections[i].scene==i && p->song.sections[i].bars==bars[i]);assert(!ui->song_skeleton() && ui->notice_error());assert(ui->undo());flush();assert(!p->song.length);
 // Duplicate the selected scene without launching it, refusing occupied targets.
 p->scenes[0].pattern_for_track[0]=3;unsigned live=p->active_scene;auto clips=p->clips[0];assert(ui->duplicate_scene(0,7));flush();assert(p->scenes[7].pattern_for_track[0]==3 && p->active_scene==live && p->clips[0].pattern==clips.pattern && p->clips[0].playing==clips.playing);assert(!ui->duplicate_scene(0,7));assert(ui->undo());flush();assert(p->scenes[7].pattern_for_track[0]==0);ui->set_mode(UiMode::Scene);ui->set_shift(true);assert(ui->soft_key(4) && ui->composition_dialog()==2);assert(ui->knob(0,1000));assert(ui->modal_accept());flush();assert(p->active_scene==live);assert(ui->undo());flush();ui->set_shift(false);
 // A live recording held across APPLY ALL must still receive its matching gate.
 auto record_poll=[&](){EngineEvent event;while(e->pop_event(event)){if(event.type==EngineEventType::RecordedNote || event.type==EngineEventType::RecordedGate)std::printf("RECORD type=%u track=%u pattern=%u step=%u gate/note=%u id=%u revision=%u\n",unsigned(event.type),event.track,event.flags,event.value,event.parameter,event.recording_id,event.pattern_revision);ui->accept_event(event);}ui->poll();};
 ui->set_mode(UiMode::Keyboard);assert(ui->select_track(0));assert(ui->select_pattern(0));assert(ui->arm_note_record(true));assert(ui->play());flush();float recording_audio[256]{};e->render(recording_audio,128);record_poll();assert(ui->pad(12,0x10,4095));e->render(recording_audio,128);record_poll();uint32_t revision=e->pattern_revision(0,0);assert(ui->apply_groove_all());flush();assert(e->pattern_revision(0,0)==revision);for(unsigned i=0;i<72;++i){e->render(recording_audio,128);record_poll();}assert(ui->pad(12,0x20,0));e->render(recording_audio,128);record_poll();std::printf("GATE state=%u notice=%s clip=%u tempo=%u\n",p->patterns[0].tracks[0].steps[0].gate,ui->notice_text(),p->clips[0].pattern,p->tempo);assert(p->patterns[0].tracks[0].steps[0].gate>96);assert(ui->notice()!=UiNotice::RecordLost);assert(ui->arm_note_record(false));assert(ui->stop());flush();
 Bytes encoded,round;assert(encode_project(encoded,*p));auto decoded=std::make_unique<ProjectState>();assert(decode_project(encoded,*decoded));assert(encode_project(round,*decoded) && round.data==encoded.data && round.data[4]==13);
 puts("composition B3/B4 PASS: track-only locks/automation; deterministic variations + Undo; chord Bass/Arp/rejection; all-track Groove + Undo; MGXC13 byte roundtrip");
}
