#include "performance.h"
#include "project_codec.h"
#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstring>
using namespace groove;
struct Buffer{std::vector<uint8_t> bytes;size_t at=0;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(n>bytes.size()-at)return false;__builtin_memcpy(p,bytes.data()+at,n);at+=n;return true;}bool at_end()const{return at==bytes.size();}};
struct Note{uint64_t sample;uint8_t channel,pitch;};
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->song.length=1;p->song.sections[0]={15,8,0,0};p->song.enabled=true;
 for(unsigned pattern=0;pattern<16;++pattern)for(unsigned t=0;t<2;++t){auto track=track_at(*p,pattern,t);track.instrument=Instrument::ExternalMidi;track.midi_channel=uint8_t(t);for(unsigned step=0;step<16;++step){track.steps[step].enabled=true;track.steps[step].note=uint8_t(60+12*t+pattern);track.steps[step].gate=48;}}
 p->scenes[2].pattern_for_track[0]=5;p->scenes[2].pattern_for_track[1]=6;
 auto engine=std::make_unique<Engine>(8000);assert(engine->stage_project(*p));float audio[128]{};engine->render(audio,1);Performance ui(*engine,*p);ui.set_mode(UiMode::Song);std::vector<Note> notes;uint64_t started=0;
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;engine->render(audio,n);ui.poll();MidiEvent event;while(engine->pop_midi(event)){if(event.bytes[0]==0xfa)started=event.sample;if((event.bytes[0]&0xf0)==0x90 && event.bytes[2])notes.push_back({event.sample,uint8_t(event.bytes[0]&15),event.bytes[1]});}frames-=n;}};
 assert(ui.scene_take_toggle());run(1);uint64_t recorded_start=started;
 run(1500);assert(ui.launch_clip(0,3,false));run(750);assert(ui.stop_clip(1,false));run(500);assert(ui.mute_or_solo(0,false));run(500);assert(ui.mute_or_solo(0,false));run(500);assert(ui.launch_clip(1,2,false));run(300);assert(ui.launch_clip(0,3,false));run(457);assert(ui.launch_scene(2,false));run(933);assert(ui.mute_or_solo(0,false));run(111);
 assert(ui.scene_take_requested());assert(ui.scene_take_toggle());for(unsigned i=0;i<300 && ui.scene_take_requested();++i)run(64);assert(!ui.scene_take_requested() && !ui.playing());run(1);
 auto captured=p->song;assert(captured.length>=8 && captured.length<=32 && song_has_snapshots(captured) && valid_song(captured));uint32_t ticks=0;bool partial=false,repeated=false;for(unsigned i=0;i<captured.length;++i){const auto& sec=captured.sections[i];ticks+=sec.ticks;partial|=sec.ticks%kSongTicksPerBar!=0;if(i && sec.patterns[0]==captured.sections[i-1].patterns[0] && sec.restart_mask==1)repeated=true;}
 assert(ticks==kSongTicksPerBar && partial && repeated);assert(captured.sections[0].restart_mask==0xffff);assert(p->muted_tracks&1);
 std::vector<Note> original;for(auto note:notes)if(note.sample>=recorded_start && note.sample<recorded_start+16000){note.sample-=recorded_start;original.push_back(note);}
 // A launch on track 0 must not move track 1's existing step at sample 2000.
 bool continuous=false;for(auto n:original)continuous|=n.channel==1 && n.sample==2000;assert(continuous);
 assert(ui.undo());run(1);assert(p->song.length==1 && p->song.sections[0].scene==15 && p->song.sections[0].bars==8);assert(ui.redo());run(1);assert(p->song.length==captured.length && p->song.sections[1].ticks==captured.sections[1].ticks);
 Buffer encoded;assert(encode_project(encoded,*p));auto decoded=std::make_unique<ProjectState>();assert(decode_project(encoded,*decoded));Buffer encoded_again;assert(encode_project(encoded_again,*decoded) && encoded.bytes==encoded_again.bytes);
 // Scene definitions are labels for snapshots; changing them cannot rewrite the take.
 Command change{CommandType::SetScene,0,0,2};change.scene.pattern_for_track[0]=15;change.scene.mute_mask=0xffff;assert(engine->submit(change));p->scenes[2]=change.scene;run(1);notes.clear();assert(ui.play());run(1);uint64_t replay_start=started;assert(!ui.audible_muted(0));run(16100);assert(!ui.playing());
 std::vector<Note> replay;for(auto n:notes)if(n.sample>=replay_start && n.sample<replay_start+16000){n.sample-=replay_start;replay.push_back(n);}
 assert(original.size()==replay.size());for(unsigned i=0;i<original.size();++i){assert(original[i].channel==replay[i].channel && original[i].pitch==replay[i].pitch);assert(std::llabs(int64_t(original[i].sample)-int64_t(replay[i].sample))<=4);}
 // Exact tick editing, captured pattern selection and stop/mute controls keep Undo.
 ui.set_mode(UiMode::Song);assert(!std::strcmp(ui.knob_label(5),"TICKS"));auto before=p->song.sections[0].ticks;assert(ui.knob(5,1000) && p->song.sections[0].ticks==before+1);assert(ui.undo());run(1);assert(p->song.sections[0].ticks==before);assert(!ui.encoder_push());assert(!ui.seek_song(4));assert(ui.knob(4,1000));assert(ui.knob(6,1000));assert(ui.undo());run(1);
 ui.set_shift(true);assert(ui.pad(12,0x10,3000));ui.pad(12,0x20,0);ui.set_shift(false);assert(ui.undo());run(1);
 auto renderer=std::make_unique<UiRenderer>();ui.animate(1000);renderer->draw(*p,ui);ui.animate(1200);renderer->draw(*p,ui);renderer->pixels[1][100*480+20]=0xffdebeef;assert(ui.knob(5,1000));ui.animate(1400);renderer->draw(*p,ui);assert(renderer->pixels[1][100*480+20]!=0xffdebeef);
 // Capture starts from the actual mixed clips, including stopped tracks.
 assert(ui.stop());run(1);p->muted_tracks=p->solo_tracks=0;p->clips[0]={7,true};p->clips[1]={9,false};track_at(*p,7,0).length=2;track_at(*p,7,0).clip_follow={ClipFollowAction::Target,1,8,100};
 assert(engine->stage_project(*p));ui.project_loaded();run(1);assert(ui.scene_take_toggle());run(1);run(501);assert(ui.mute_or_solo(1,true));run(301);assert(ui.mute_or_solo(1,true));run(2300);assert(ui.scene_take_toggle());for(unsigned i=0;i<300 && ui.scene_take_requested();++i)run(64);run(1);
 assert(p->song.sections[0].patterns[0]==7 && !(p->song.sections[0].playing_mask&2));bool followed=false,soloed=false;for(unsigned i=0;i<p->song.length;++i){followed|=p->song.sections[i].patterns[0]==8;soloed|=bool(p->song.sections[i].mute_mask&1);}assert(followed && soloed && p->solo_tracks==0);
 // Notification loss must never publish a take over the previous Song.
 SongState previous=p->song;assert(ui.scene_take_toggle());run(1);for(unsigned batch=0;batch<24;++batch){for(unsigned i=0;i<48;++i)assert(engine->submit({CommandType::LaunchClip,0,0,7}));engine->render(audio,1);}assert(engine->project_event_loss());assert(engine->submit({CommandType::SceneTakeControl,0,1,0}));
 for(unsigned i=0;i<300 && engine->scene_take_state()!=SceneTake::Ready;++i){engine->render(audio,64);}
 assert(engine->scene_take_state()==SceneTake::Ready);ui.poll();assert(!ui.scene_take_requested() && p->song.length==previous.length && p->song.sections[0].ticks==previous.sections[0].ticks);
 std::puts("Arrangement: sub-bar clip/stop/mute/scene capture, retriggers, preserved track phase, exact-tick replay, independent Scene definitions, SONG2, Undo and UI passed");
}
