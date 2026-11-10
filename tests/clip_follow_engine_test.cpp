#include "performance.h"
#include "project_codec.h"
#include "ui_renderer.h"
#include <cassert>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstring>
using namespace groove;
struct Buffer {std::vector<uint8_t> bytes;size_t at=0;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(n>bytes.size()-at)return false;__builtin_memcpy(p,bytes.data()+at,n);at+=n;return true;}bool at_end()const{return at==bytes.size();}};
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->muted_tracks=0xffff;auto t=track_at(*p,0,0);t.length=4;t.clip_follow={ClipFollowAction::Next,1,0,100};
 // Optional CLFA rev1 survives serialization without changing default fixtures.
 Buffer one;assert(encode_project(one,*p));auto restored=std::make_unique<ProjectState>();assert(decode_project(one,*restored));assert(track_at(*restored,0,0).clip_follow.action==ClipFollowAction::Next);assert(track_at(*restored,1,0).clip_follow.action==ClipFollowAction::Off);
 auto engine=std::make_unique<Engine>(8000);float audio[128]{};Performance ui(*engine,*p);uint64_t followed=0;
 auto run=[&](unsigned n){while(n){unsigned k=n>64?64:n;engine->render(audio,k);EngineEvent e;while(engine->pop_event(e)){if(e.type==EngineEventType::ClipLaunched && e.track==0)followed=e.sample;ui.accept_event(e);}n-=k;}};
 auto reset=[&](){for(auto& clip:p->clips)clip={0,true};p->active_scene=p->active_pattern=0;assert(engine->stage_project(*p));ui.project_loaded();run(1);assert(ui.play());run(1);followed=0;};
 reset();run(3999);assert(p->clips[0].pattern==0);run(1);assert(p->clips[0].pattern==1 && followed==4001);
 // Repeat count, reverse, ping-pong and rational speed use completed cycles.
 t.clip_follow.cycles=2;reset();run(7999);assert(p->clips[0].pattern==0);run(1);assert(p->clips[0].pattern==1);
 t.clip_follow={ClipFollowAction::Target,1,3,100};t.direction=PlaybackDirection::Reverse;reset();run(4000);assert(p->clips[0].pattern==3);
 t.direction=PlaybackDirection::PingPong;reset();run(5999);assert(p->clips[0].pattern==0);run(1);assert(p->clips[0].pattern==3);
 t.direction=PlaybackDirection::Forward;t.speed_num=2;reset();run(2000);assert(p->clips[0].pattern==3);t.speed_num=1;
 t.clip_follow={ClipFollowAction::Stop,1,0,100};t.steps[0].microtiming=-96;reset();run(3999);assert(p->clips[0].playing);run(1);assert(!p->clips[0].playing);t.steps[0].microtiming=0;
 // A queued manual launch owns the track until its bar; Song owns its clips.
 t.clip_follow={ClipFollowAction::Next,1,0,100};reset();assert(ui.launch_clip(0,2,true));run(4000);assert(p->clips[0].pattern==0);run(12000);assert(p->clips[0].pattern==2);
 p->song.length=1;p->song.enabled=true;p->song.sections[0]={0,8,0,0};reset();run(8000);assert(p->clips[0].pattern==0);p->song.enabled=false;
 // Dedicated Follow controls edit the selected clip and undo atomically.
 assert(ui.stop());run(1);ui.set_mode(UiMode::Clip);ui.select_pattern(0);assert(ui.soft_key(0) && ui.clip_follow_page());auto previous=t.clip_follow.action;assert(ui.knob(0,1000));assert(t.clip_follow.action!=previous);assert(ui.undo());run(1);assert(t.clip_follow.action==previous);
 auto renderer=std::make_unique<UiRenderer>();ui.animate(1000);renderer->draw(*p,ui);ui.animate(1200);renderer->draw(*p,ui);renderer->pixels[1][180*480+30]=0xffdebeef;assert(ui.knob(0,1000));ui.animate(1400);renderer->draw(*p,ui);assert(renderer->pixels[1][180*480+30]!=0xffdebeef);
 assert(!std::strcmp(ui.soft_label(0),"DONE FOLLOW"));
 assert(!ui.soft_enabled(5) && !ui.soft_key(5));assert(ui.soft_key(4) && ui.soft_enabled(5));
 assert(!std::strcmp(ui.knob_label(1),"LOOPS") && !std::strcmp(ui.knob_label(2),"TARGET") && !std::strcmp(ui.knob_label(3),"CHANCE"));
 assert(ui.knob(0,32000) && t.clip_follow.action==ClipFollowAction::Random && ui.knob_normalized(0)==1);
 assert(ui.knob(0,-32000) && t.clip_follow.action==ClipFollowAction::Off && ui.knob_normalized(0)==0);
 // Prepared bases remain editable while OFF and clamp at their actual range.
 assert(ui.knob(1,32000) && ui.knob(1,32000) && t.clip_follow.cycles==64 && ui.knob_normalized(1)==1);
 assert(ui.knob(2,32000) && t.clip_follow.target==15 && ui.knob_value(2)==16 && ui.knob_normalized(2)==1);
 for(unsigned i=0;i<4;++i)assert(ui.knob(3,-32000));
 assert(t.clip_follow.probability==0 && ui.knob_normalized(3)==0);
 assert(ui.control(4,0x80) && t.clip_follow.probability==4);ui.control(4,0);ui.set_shift(true);assert(ui.control(4,0x80) && t.clip_follow.probability==0);ui.control(4,0);ui.set_shift(false);
 assert(ui.knob(4,1000) && t.sound_override==0 && !std::strcmp(ui.knob_label(4),"PRESET"));
 assert(ui.knob(4,-1000) && t.sound_override==kNoPreset && ui.knob_normalized(4)==0);
 assert(!ui.knob(5,1000) && !std::strcmp(ui.knob_label(8),"") && ui.knob_normalized(8)==0);
 assert(ui.encoder_push() && !ui.clip_follow_page() && !std::strcmp(ui.soft_label(0),"FOLLOW EDIT"));
 assert(ui.soft_key(1) && p->active_pattern==1);
 std::puts("Clip Follow integration: exact end timing, repeat/reverse/ping-pong/speed, Stop, manual/Song priority, CLFA round-trip, shared controls, ranges and Undo passed");
}
