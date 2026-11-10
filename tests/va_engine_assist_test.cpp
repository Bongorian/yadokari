#include "../src/engine.h"
#include <atomic>
#include <thread>
#include <memory>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace groove;
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 for(unsigned t=0;t<16;++t){project->tracks[t].instrument=Instrument::VirtualAnalog;project->tracks[t].virtual_analog.values[14]=.15f;}
 auto serial=std::make_unique<Engine>(),assisted=std::make_unique<Engine>();auto slot=std::make_unique<VaSampleSlot>();assisted->set_va_helper(slot.get());
 assert(serial->stage_project(*project) && assisted->stage_project(*project));
 std::atomic<bool> stop{false},delay{false};std::thread worker([&]{slot->worker_ready();while(!stop.load(std::memory_order_acquire)){if(delay.load(std::memory_order_relaxed) || !slot->process_one())std::this_thread::yield();}});
 auto both=[&](Command c){assert(serial->submit(c) && assisted->submit(c));};
 float a[48],b[48],ac[48],bc[48],input[48]{};
 for(unsigned block=0;block<720;++block){
  if(block%120==1)for(unsigned t=0;t<16;++t){Command c{CommandType::Trigger,uint8_t(t),0,100};c.parameter=1;c.scalar=60+int(block/120)%7;both(c);c.parameter=0;c.scalar+=12;both(c);}
  // Rebuild the cached order after an empty/non-VA interval and ownership edits.
  if(block==280)for(unsigned t=0;t<16;++t)both({CommandType::SetInstrument,uint8_t(t),0,unsigned(Instrument::Empty)});
  if(block==320)for(unsigned t=0;t<16;++t)both({CommandType::SetInstrument,uint8_t(t),0,unsigned(Instrument::VirtualAnalog)});
  if(block==100 || block==260)both({CommandType::NoteOff,0,0,0});
  if(block==200)delay.store(true,std::memory_order_release);
  if(block==220)delay.store(false,std::memory_order_release);
  if(block==400){project->random_seed=0x71109;for(auto& track:project->tracks)track.virtual_analog.values[2]=11;assert(serial->stage_project(*project) && assisted->stage_project(*project));}
  serial->render_ports(a,ac,input,24);assisted->render_ports(b,bc,input,24);
  assert(!std::memcmp(a,b,sizeof a) && !std::memcmp(ac,bc,sizeof ac));
  for(auto* e:{serial.get(),assisted.get()}){EngineEvent event;while(e->pop_event(event)){}MidiEvent midi;while(e->pop_midi(midi)){}ClockEvent clock;while(e->pop_clock(clock)){}AudioVisualFrame frame;while(e->pop_visual(frame)){}}
 }
 stop.store(true,std::memory_order_release);worker.join();auto n=slot->counts();assert(n.used && n.busy && !n.mismatch);
 printf("VA Engine assist: 16 tracks + 32 shared voices, retrigger/release, late-worker fallback and project replacement PCM exact; used %llu late %llu busy %llu checks %llu mismatches %llu\n",(unsigned long long)n.used,(unsigned long long)n.late,(unsigned long long)n.busy,(unsigned long long)n.checked,(unsigned long long)n.mismatch);
}
