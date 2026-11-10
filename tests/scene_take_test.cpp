#include "scene_take.h"
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 SceneTake take;SongState song;ClipState clips[16]{};
 assert(take.arm());take.begin(0,2,clips,0);take.observe(100,2,clips,0,0);clips[1].pattern=4;take.observe(347,2,clips,0,2);take.observe(960,2,clips,0,0);clips[3].playing=false;take.observe(1440,2,clips,8,8);take.finish();take.end(3840);
 assert(take.copy(song) && valid_song(song) && song.length==3 && song.sections[0].ticks==347 && song.sections[1].ticks==1093 && song.sections[2].ticks==2400);
 assert(song.sections[0].restart_mask==0xffff && song.sections[1].restart_mask==2 && song.sections[1].patterns[1]==4 && !(song.sections[2].playing_mask&8) && song.sections[2].mute_mask==8);
 assert(!take.arm());take.observe(4000,0,clips,0,0xffff);SongState again;assert(take.copy(again) && again.sections[2].ticks==2400);take.acknowledge();
 // Same-tick updates coalesce but preserve all explicit restart requests.
 assert(take.arm());take.begin(0,0,clips,0);clips[0].pattern=3;take.observe(50,0,clips,0,1);clips[1].pattern=7;take.observe(50,0,clips,0,2);take.advance(49);take.end(100);assert(take.copy(song) && song.length==2 && song.sections[1].restart_mask==3 && song.sections[1].patterns[0]==3 && song.sections[1].patterns[1]==7);take.acknowledge();
 assert(take.arm());take.begin(0,0,clips,0);for(unsigned i=1;i<33;++i)take.observe(i,0,clips,0,1);assert(take.state()==SceneTake::Failed && take.error()==SceneTake::Capacity && !take.copy(song));take.acknowledge();
 assert(take.arm());take.begin(0,3,clips,0);take.advance(kSongMaxTicks);take.end(kSongMaxTicks+1);assert(take.copy(song) && valid_song(song) && song.length==2 && song.sections[0].ticks==kSongMaxTicks && song.sections[1].ticks==1 && !song.sections[1].restart_mask);take.acknowledge();
 assert(take.arm());take.finish();assert(take.error()==SceneTake::NoBars);take.acknowledge();assert(take.arm());take.begin(0,0,clips,0);take.fail(SceneTake::Interrupted);assert(!take.copy(song));take.reset();assert(take.state()==SceneTake::Idle);
 std::puts("Arrangement take: precise ticks, clip/mute/playing snapshots, restart masks, same-tick coalescing, immutable handoff and capacity passed");
}
