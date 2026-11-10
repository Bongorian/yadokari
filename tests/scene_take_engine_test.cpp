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
 std::puts("Scene take integration: DSP-applied scene/bar capture, old Song bypass, single Undo, individual clip capture/Stop refusal, generation reset passed");
}
