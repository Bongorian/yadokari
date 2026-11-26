#include "performance.h"
#include <cassert>
#include <memory>
#include <cstdio>
using namespace groove;
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);project->song.length=1;project->song.sections[0]={15,8,0,0};project->song.enabled=true;
 auto engine=std::make_unique<Engine>(8000);assert(engine->stage_project(*project));float audio[128]{};engine->render(audio,1);Performance ui(*engine,*project);ui.set_mode(UiMode::Song);
 auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;engine->render(audio,n);ui.poll();frames-=n;}};
 assert(ui.soft_key(4));run(1);assert(engine->scene_take_state()==SceneTake::Recording);
 assert(ui.launch_scene(4,true));run(16000);assert(project->active_scene==4);
 assert(ui.launch_scene(7,true));run(16000);assert(project->active_scene==7);
 assert(ui.scene_take_toggle());assert(ui.launch_scene(9,true));run(15999);engine->render(audio,1);assert(engine->scene_take_state()==SceneTake::Ready && engine->project_events_pending());ui.service_scene_take();assert(ui.scene_take_requested());ui.poll();run(1);
 assert(!ui.scene_take_requested() && !ui.playing() && project->song.length==3 && project->song.enabled && !project->song.loop);
 for(unsigned i=0;i<3;++i){assert(project->song.sections[i].scene==(i==0?0:i==1?4:7));assert(project->song.sections[i].ticks==kSongTicksPerBar);}
 assert(ui.undo());run(1);assert(project->song.length==1 && project->song.sections[0].scene==15 && project->song.sections[0].bars==8);
 assert(ui.scene_take_toggle());run(1);assert(ui.launch_clip(0,2));run(1);assert(ui.scene_take_requested() && project->song.sections[0].scene==15);assert(ui.stop());run(1);
 assert(ui.scene_take_toggle());run(1);assert(ui.stop());run(1);assert(!ui.scene_take_requested() && project->song.sections[0].scene==15);
 assert(ui.scene_take_toggle());run(1);assert(engine->stage_project(*project));run(1);ui.project_loaded();assert(!ui.scene_take_requested() && engine->scene_take_state()==SceneTake::Idle);
 // KEEP commits at the boundary and leaves live playback in charge.
 ui.set_mode(UiMode::Song);
 assert(ui.scene_take_toggle());run(1);
 ui.set_shift(true);assert(ui.soft_key(4));ui.set_shift(false);
 run(16002);
 assert(!ui.scene_take_requested() && ui.playing() && engine->diagnostic_playing());
 assert(project->song.length && !project->song.enabled && engine->scene_take_state()==SceneTake::Idle);
 assert(ui.undo());run(1);assert(project->song.enabled && project->song.sections[0].scene==15);
 assert(ui.stop());run(1);
 // Two sections sharing a Scene stay independent after editing one occurrence.
 project->song.length=2;project->song.sections[1]=project->song.sections[0];
 project->scenes[15].pattern_for_track[0]=2;
 project->patterns[2].tracks[0].steps[3].enabled=true;
 assert(engine->stage_project(*project));run(1);ui.project_loaded();ui.set_mode(UiMode::Song);
 ui.set_shift(true);assert(ui.soft_key(4));ui.set_shift(false);run(1);
 const unsigned scene=project->song.sections[0].scene,target=project->scenes[scene].pattern_for_track[0];
 assert(ui.mode()==UiMode::Step && scene!=15 && target!=2 && project->patterns[target].tracks[0].steps[3].enabled);
 assert(project->song.sections[1].scene==15 && project->scenes[15].pattern_for_track[0]==2);
 assert(ui.undo());run(1);assert(project->song.sections[0].scene==15 && !project->patterns[target].tracks[0].steps[3].enabled);
 // A withdrawn pad launch leaves the chain on its original bar and position.
 make_empty_project(*project);
 project->chain.length=3;project->chain.enabled=true;project->chain.bars=1;
 project->chain.scenes[0]=3;project->chain.scenes[1]=4;project->chain.scenes[2]=5;
 assert(engine->stage_project(*project));run(1);ui.project_loaded();ui.set_mode(UiMode::Scene);
 assert(ui.play());run(100);assert(project->active_scene==3 && project->chain.enabled);
 auto tap=[&]{assert(ui.pad(12,0x10,3000));assert(ui.pad(12,0x20,0));};
 tap();run(50);assert(ui.pending_scene()==0 && project->chain.enabled);
 tap();run(50);assert(ui.pending_scene()<0 && project->chain.enabled);
 run(16000);assert(project->active_scene==4 && project->chain.enabled);
 tap();run(100);assert(ui.pending_scene()==0 && project->chain.enabled);
 unsigned launches=0;
 for(unsigned i=0;i<250;++i){
  engine->render(audio,64);EngineEvent event;
  while(engine->pop_event(event)){if(event.type==EngineEventType::SceneLaunched){assert(event.value==0);++launches;}ui.accept_event(event);}
 }
 assert(launches==1 && project->active_scene==0 && !project->chain.enabled && ui.pending_scene()<0);
 std::puts("Scene take integration: DSP-applied scene/bar capture, old Song bypass, single Undo, individual clip capture/Stop refusal, generation reset passed");
}
