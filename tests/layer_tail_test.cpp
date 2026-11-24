// Construct exact pool occupancy to exercise admission and slot retirement.
#include "engine.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace groove;
static void wavetable_admission(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 for(unsigned t=0;t<4;++t){auto track=track_at(*project,0,t);track.instrument=Instrument::Wavetable;
  track.wavetable.values[WavetablePmAmount]=t==0?0:.8f;track.wavetable.values[4]=.005f;
 }
 auto layer=track_at(*project,0,4);layer.instrument=Instrument::Layer;layer.layer.count=1;
 auto& child=layer.layer.parts[0].sound;child.instrument=Instrument::Wavetable;child.wavetable.values[WavetableWarpAmount]=.7f;
 capture_parameter_snapshot(project->morph_a[0],*project,0);
 project->morph_b[0]=project->morph_a[0];project->morph_b[0].engine_values[WavetablePmAmount]=.8f;
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*project));float pcm[256]{};engine->render(pcm,0);
 auto play=[&](unsigned t,unsigned pitch){Command cmd{CommandType::Trigger,uint8_t(t),0,100};cmd.parameter=1;cmd.scalar=float(pitch);assert(engine->submit(cmd));engine->render(pcm,128);};
 auto population=[&](unsigned t){WavetableLoad load{};for(const auto& p:engine->poly_voices_)if(p.owner==t)load.add(p.voice.wt_reserved);return load;};
 for(unsigned n=0;n<5;++n)play(0,60+n);
 assert(population(0).oscillators==4 && population(0).cost()==8);
 auto& lock=engine->runtime_[0].active_locks[0];
 lock={engine_target(Instrument::Wavetable,WavetablePmAmount),.8f,true};engine->render(pcm,128);
 assert(population(0).extended==2 && engine->poly_note_count(0)==4);
 lock.enabled=false;engine->render(pcm,128);assert(!population(0).extended);
 Command enable{CommandType::SetMorphEnabled,0,0,1},amount{CommandType::SetMorph,0,0,0};amount.scalar=1;
 assert(engine->submit(enable) && engine->submit(amount));engine->render(pcm,128);
 assert(population(0).extended==2 && engine->poly_note_count(0)==4);
 enable.value=0;assert(engine->submit(enable));engine->render(pcm,128);assert(!population(0).extended);
 Command edit{CommandType::SetEngineParameter,0,0,0};edit.parameter=WavetableWarpAmount;edit.scalar=.8f;
 assert(engine->submit(edit));engine->render(pcm,128);
 assert(engine->poly_note_count(0)==4 && population(0).extended==2 && population(0).cost()==26);
 play(4,72);assert(population(4).cost()==11);
 play(1,74);assert(population(1).cost()==11);
 play(2,76);assert(population(2).oscillators==0); // 26 + 11 + 11 exhausts the shared budget
 play(4,77);assert(engine->layer_last_reason(4)==LayerReason::Cost && engine->layer_note_voices(4)==1);
 edit.scalar=0;assert(engine->submit(edit));engine->render(pcm,32);
 assert(population(0).extended==2); // the two wet filters are still fading
 engine->render(pcm,128);assert(population(0).cost()==8 && engine->poly_note_count(0)==4);
 play(2,76);assert(population(2).extended==1);
 auto& state=engine->track(3).wavetable;state.values[WavetableUnisonCount]=3;
 play(3,69);assert(!population(3).oscillators); // a single request exceeds the Pi 4 wet cap
 // Reject a growth request without stealing held siblings. Once voices
 // release, the surviving owner picks up the still-requested unison size.
 Command resize{CommandType::SetEngineParameter,0,0,0};resize.parameter=WavetableUnisonCount;resize.scalar=2;
 assert(engine->submit(resize));engine->render(pcm,128);assert(population(0).oscillators==4);
 unsigned left=0;
 for(auto& p:engine->poly_voices_)if(p.owner==0 && left++)engine->release_poly(p,false);
 for(unsigned i=0;i<48;++i)engine->render(pcm,128);
 assert(engine->poly_note_count(0)==1 && population(0).oscillators==2);
 // A held wet note can turn dry while its requested seven oscillators
 // exceed the entire track. Other tracks can immediately use its refund.
 resize.track=1;resize.scalar=7;edit.track=1;edit.parameter=WavetablePmAmount;edit.scalar=0;
 assert(engine->submit(resize) && engine->submit(edit));engine->render(pcm,32);
 assert(population(1).cost()==11);
 engine->render(pcm,128);
 assert(population(1).cost()==2 && engine->poly_note_count(1)==1);
}
int main(){
 wavetable_admission();
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
