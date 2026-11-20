// Construct exact pool occupancy to exercise admission and slot retirement.
#include "engine.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace groove;
int main(){
 auto e=std::make_unique<Engine>();
 auto proj=std::make_unique<ProjectState>();make_empty_project(*proj);
 auto t=track_at(*proj,0,0);t.instrument=Instrument::Layer;t.layer.count=2;
 for(auto& part:t.layer.parts)part.sound.instrument=Instrument::VirtualAnalog;
 assert(e->stage_project(*proj));float pcm[2];e->render(pcm,0);
 auto occupy=[&](unsigned slot,unsigned track,unsigned cost,bool released){
  auto& p=e->poly_voices_[slot];e->set_poly_owner(p,track);p.age=slot;
  p.voice.composite=true;p.voice.instrument=unsigned(Instrument::VirtualAnalog);
  p.voice.released=released;p.voice.layer_cost=uint8_t(cost);
  return &p.voice;
 };
 auto* head=occupy(0,0,1,true);
 auto* middle=occupy(1,0,1,true);
 auto* last=occupy(2,0,1,false);
 head->layer_peer=middle;middle->layer_peer=last;
 assert(!e->oldest_layer_tail(0,true,0)); // a held child protects the whole group
 last->released=true;
 assert(e->oldest_layer_tail(0,true,0)==7);
 e->set_poly_owner(e->poly_voices_[1],255);
 occupy(1,1,3,false); // another held note now owns the retired slot
 assert(head->layer_peer==last && e->oldest_layer_tail(0,true,0)==5);
 last->last_l=.25f;head->last_l=.125f;
 e->steal_layer_tails(5,1);
 assert(e->layer_note_voices(0)==0 && e->layer_note_voices(1)==1);
 assert(e->declick_[0].remaining==64 && e->declick_[0].l==.375f);
 // A tail is available but freeing it cannot admit another 8 units.
 for(unsigned i=2;i<7;++i)occupy(i,0,4,false);
 occupy(7,0,2,true);
 auto steals=e->poly_steal_counts();
 Command on{CommandType::MidiInput,0,62,0x90};on.scalar=110;
 assert(e->submit(on));e->render(pcm,0);
 assert(e->layer_last_reason(0)==LayerReason::Cost && e->layer_note_voices(0)==6);
 assert(e->poly_steal_counts().released==steals.released && e->poly_owner_masks_valid());
 std::puts("PASS mixed Layer releases, reused leaf slot, declick sum and all-or-nothing admission");
}
