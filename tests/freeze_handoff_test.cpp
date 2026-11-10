#include "engine.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
using namespace groove;
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);project->tempo=300;
 auto track=track_at(*project,0,0);track.instrument=Instrument::PolySaw;track.length=2;track.steps[0].enabled=true;
 auto engine=std::make_unique<Engine>(8000);assert(engine->stage_project(*project));assert(engine->submit({CommandType::Start,0,0,0}));float scratch[48];engine->render(scratch,24);
 // Cancel before the audio owner even accepts the request. Release must
 // remain pending until the owner has observed the cancellation.
 assert(engine->freeze_capture_begin(0,12000));engine->freeze_capture_release();assert(engine->freeze_capture_state()==5);
 std::atomic<bool> stop{false},hold_capture{false},audio_held{false};
 std::thread audio([&]{float buffer[48];while(!stop.load(std::memory_order_acquire)){
  engine->render(buffer,24);
  if(hold_capture.load(std::memory_order_acquire) && engine->freeze_capture_state()==2){
   audio_held.store(true,std::memory_order_release);
   while(hold_capture.load(std::memory_order_acquire) && !stop.load(std::memory_order_acquire))std::this_thread::yield();
   audio_held.store(false,std::memory_order_release);
  }
 }});
 auto wait=[&](auto done){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(!done()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}};
 wait([&]{engine->service_freeze_capture();return engine->freeze_capture_state()==0;});
 for(unsigned take=0;take<32;++take){
  hold_capture.store(true,std::memory_order_release);assert(engine->freeze_capture_begin(0,12000));
  wait([&]{return audio_held.load(std::memory_order_acquire);});
  assert(engine->freeze_capture_state()==2 && engine->freeze_capture_data()==nullptr);
  float progress=engine->freeze_capture_progress();assert(progress>=0 && progress<=1);
  // Also order a project replacement before the final cancellation. Old
  // generation storage must remain valid until that callback acknowledges.
  if(take==31)assert(engine->stage_project(*project));
  engine->freeze_capture_release();engine->service_freeze_capture();
  assert(engine->freeze_capture_state()==2); // consumer is deliberately paused
  hold_capture.store(false,std::memory_order_release);
  wait([&]{engine->service_freeze_capture();return engine->freeze_capture_state()==0;});
  wait([&]{return !audio_held.load(std::memory_order_acquire);});
 }
 stop.store(true,std::memory_order_release);audio.join();
 std::puts("freeze handoff: pending start, in-flight cancel, release acknowledgement and project replacement passed");
}
