#include "engine.h"
#include "groove.h"
#include <cassert>
#include <memory>
#include <vector>
int main() {
 auto project=std::make_unique<groove::ProjectState>();
 groove::make_empty_project(*project);
 auto engine=std::make_unique<groove::Engine>(48000);
 assert(engine->stage_project(*project));
 std::vector<float> audio(2*2500);
 engine->render(audio.data(),8);
 auto frame=std::make_unique<groove::AudioVisualFrame>();
 assert(!engine->pop_visual(*frame)); // partial snapshots are never published
 assert(engine->stage_project(*project));
 const auto current=engine->staged_generation();
 engine->render(audio.data(),8); // project replacement invalidates partial frame
 assert(!engine->pop_visual(*frame));
 for(unsigned i=0;i<320;++i) engine->render(audio.data(),8);
 bool seen=false;
 while(engine->pop_visual(*frame)) { assert(frame->generation==current); seen=true; }
 assert(seen);
}
