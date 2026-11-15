#include "engine.h"
#include "midi_clock_estimator.h"
#include "midi_clock_sync.h"
#include "midi_input_router.h"
#include "project.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
using namespace groove;
namespace {
constexpr unsigned kRate=8000;
// Settled lock, in samples at this low test rate (125 us each). The step grid
// leads the engine's own tick phase by a sample or two, with or without a lock.
constexpr long kSettled=4;
// One external-MIDI track with a note on every sixteenth: each note-on is a
// timestamp of the slave's step grid. The rig plays the part of the master.
struct Rig {
 std::unique_ptr<ProjectState> project=std::make_unique<ProjectState>();
 std::unique_ptr<Engine> engine;
 std::vector<uint64_t> notes,ticks;std::vector<uint8_t> realtime_out;std::vector<float> audio=std::vector<float>(128);
 double next_tick=0,interval=0;bool clock=false,counting=true;unsigned tempo=0,jitter=0,burst=1;uint32_t seed=1;
 explicit Rig(bool slave,unsigned bpm=120){
  make_empty_project(*project);project->tempo=uint16_t(bpm);project->midi_clock_slave=slave;tempo=bpm;
  auto track=track_at(*project,0,0);track.instrument=Instrument::ExternalMidi;track.length=16;
  for(unsigned s=0;s<16;++s){track.steps[s].enabled=true;track.steps[s].note=60;}
  engine=std::make_unique<Engine>(kRate);assert(engine->stage_project(*project));engine->render(audio.data(),0);drain();notes.clear();realtime_out.clear();
 }
 uint64_t now()const{return engine->samples_rendered();}
 unsigned state()const{return engine->external_clock_state();}
 void drain(){
  MidiEvent m;while(engine->pop_midi(m)){if((m.bytes[0]&0xf0)==0x90 && m.bytes[2])notes.push_back(m.sample);else if(m.bytes[0]>=0xf0)realtime_out.push_back(m.bytes[0]);}
  EngineEvent event;while(engine->pop_event(event))if(event.type==EngineEventType::TempoChanged)tempo=unsigned(event.scalar+0.5f);
 }
 void send(uint64_t at,uint8_t status,uint8_t a=0,uint8_t b=0){MidiPacket packet{at?at:1,0,uint8_t(status==0xf2?3:1),{status,a,b}};assert(engine->receive_midi(packet,0));}
 void master(double start,double bpm){next_tick=start;interval=double(kRate)*60/(bpm*24);clock=true;}
 void retempo(double bpm){interval=double(kRate)*60/(bpm*24);}
 // Ticks are stamped with their arrival sample. `counting` is false while the
 // master is stopped but still sending clock; those ticks are not positions.
 void run(uint64_t end){
  while(now()<end){
   uint64_t begin=now(),stop=begin+32<end?begin+32:end;
   while(clock && next_tick<double(stop)){
    uint64_t ideal=uint64_t(next_tick+0.5),stamp=ideal;
    if(jitter){seed=seed*1664525u+1013904223u;stamp+=(seed>>16)%(jitter+1);}
    if(burst>1){uint64_t index=ticks.size();double last=next_tick+interval*double(burst-1-index%burst);stamp=uint64_t(last+0.5);}
    if(stamp<begin)stamp=begin;
    send(stamp,0xf8);if(counting)ticks.push_back(ideal);next_tick+=interval;
   }
   engine->render(audio.data(),stop-begin);drain();
  }
 }
 // Largest distance, in samples, between step k and master tick 6k.
 long worst(unsigned first,unsigned last,long offset=0)const{
  long result=0;assert(last<notes.size() && last*6<ticks.size());
  for(unsigned k=first;k<=last;++k){long d=std::labs(long(notes[k])-long(ticks[k*6])-offset);if(d>result)result=d;}
  return result;
 }
};

void loop_logic(){
 // The loop alone, against a model of the engine's 24 PPQN accumulator.
 const unsigned rate=48000,bpm=120;const uint64_t quarter=uint64_t(rate)*60;const double tick=double(quarter)/(bpm*24);
 MidiClockSync sync;assert(sync.state()==MidiClockSync::Free && !sync.present(0));
 sync.seek();sync.arm(100000);assert(sync.state()==MidiClockSync::Waiting);
 uint64_t phase=0;long local=0;bool first=true;
 auto step=[&](uint64_t now){unsigned pace=sync.advance(now);if(!pace)return pace;if(first){first=false;return pace;}phase+=uint64_t(bpm)*24*pace;if(phase>=quarter){phase-=quarter;++local;sync.wrapped();}return pace;};
 for(uint64_t n=0;n<500;++n)assert(step(n)==0); // held: no tick yet
 // A master 0.5 % fast. Without a lock the model would lose a whole tick in 200.
 const double master=tick/1.005;double next=500;long sent=0;double worst_late=0;
 for(uint64_t n=500;n<uint64_t(rate)*30;++n){
  if(double(n)>=next){sync.tick(n,true,double(phase)/double(quarter),tick,rate/8);++sent;next+=master;}
  step(n);
  if(n==500)assert(sync.state()==MidiClockSync::Locked && !first);
  if(n>uint64_t(rate)*5){double position=double(local)+double(phase)/double(quarter),truth=double(sent-1)+(double(n+1)-(next-master))/master;double error=std::fabs(position-truth);if(error>worst_late)worst_late=error;}
 }
 assert(worst_late<0.02); // settled within 2 % of a tick, 0.4 ms at 120 BPM
 // Silence: the flywheel keeps the learned pace and reports the loss.
 uint64_t silent=uint64_t(rate)*30;unsigned doubled=0,total=0;
 for(uint64_t n=silent;n<silent+rate;++n){unsigned pace=step(n);assert(pace==1 || pace==2);doubled+=pace==2;++total;}
 assert(sync.state()==MidiClockSync::Free && !sync.present(silent+rate));
 assert(std::fabs(double(doubled)/total-0.005)<0.001);
 // Ticks handed over together are one reading taken at the last of them.
 MidiClockSync burst;burst.seek();burst.arm(0);assert(burst.state()==MidiClockSync::Free);
 burst.tick(10,true,0,tick,rate/8);assert(burst.advance(10)==1 && burst.state()==MidiClockSync::Locked);
 burst.wrapped();burst.wrapped();burst.wrapped(); // three ticks of local time, their clocks arriving late and at once
 for(unsigned n=0;n<3;++n)burst.tick(10+uint64_t(tick*3),true,0,tick,rate/8);
 for(unsigned n=0;n<200;++n)assert(burst.advance(10+uint64_t(tick*3)+n)==1);
 // A rounding flip of the nominal tempo carries the learned pace over; a
 // real change starts from the estimate.
 MidiClockSync tuned;tuned.retune(120,121,0);assert(std::fabs(tuned.trim()-(120.0/121-1))<1e-9);
 tuned.retune(121,120,0);assert(std::fabs(tuned.trim())<1e-9);tuned.retune(120,90,unsigned(90.5*256));assert(std::fabs(tuned.trim()-(90.5/90-1))<1e-6);
 MidiClockEstimator estimate;unsigned tempo=0;assert(!estimate.observe(0,8000,tempo,3));assert(estimate.observe(500,8000,tempo,3) && tempo==120);assert(estimate.tempo_q8(8000)>=120*256-128 && estimate.tempo_q8(8000)<=120*256+128);
}

void starts_on_the_first_tick(){
 Rig r(true);r.send(300,0xfa);r.master(400,120);
 r.run(400);assert(r.notes.empty() && r.state()==2); // armed, silent
 r.run(401);assert(r.notes.size()==1 && r.notes[0]==400 && r.state()==3);
 r.run(8000);assert(r.notes.size()==8 && r.worst(0,7)<=1);
 assert(r.realtime_out.empty()); // a slave never echoes clock or transport
}

void holds_phase_at_a_fractional_tempo(){
 // 120.4 BPM: the nominal tempo stays 120 and the lock carries the rest.
 // Free-running at 120 would be 850 samples late after 64 beats.
 Rig r(true);r.send(100,0xfa);r.master(100,120.4);
 r.run(kRate*33);assert(r.notes.size()>=256);
 assert(r.notes[0]==100 && r.worst(0,255)<=8 && r.worst(32,255)<=kSettled);
 assert(r.tempo==120 && r.state()==3 && r.realtime_out.empty());
}

void follows_a_tempo_change_without_slipping(){
 Rig r(true);r.send(100,0xfa);r.master(100,120.4);
 r.run(kRate*8);r.retempo(87.3);r.run(kRate*20);r.retempo(171.6);r.run(kRate*32);
 // Step k is still master tick 6k: the count survived both changes.
 unsigned last=unsigned(r.notes.size()-1);assert(last*6<r.ticks.size());
 unsigned at_change=0;while(r.notes[at_change]<kRate*20)++at_change;
 assert(r.worst(at_change-8,at_change-1)<=kSettled && r.worst(last-16,last)<=kSettled);
 // An abrupt change is followed within a few steps, and even on the step
 // that meets it the slave stays inside one master tick.
 assert(r.worst(0,last)<long(double(kRate)*60/(87.3*24)));
 assert(r.tempo==172 && r.state()==3);
}

void resumes_in_place_after_stop_and_continue(){
 Rig r(true);r.send(100,0xfa);r.master(100,126);
 r.run(4000);size_t before=r.notes.size();assert(before>=5);
 // The master stops between ticks and keeps sending clock while stopped.
 r.send(4020,0xfc);r.counting=false;r.run(9000);assert(r.notes.size()==before && r.state()==1);
 r.send(9010,0xfb);r.counting=true;r.run(9011);assert(r.state()==2);
 r.run(20000);assert(r.notes.size()>=before+8 && r.state()==3);
 assert(r.worst(unsigned(before),unsigned(r.notes.size()-1))<=kSettled);
 // Song Position while stopped, then Continue: the next tick is that sixteenth.
 r.send(20010,0xfc);r.counting=false;r.run(21000);size_t stopped=r.notes.size();
 uint64_t resume=uint64_t(r.next_tick+0.5);if(resume<r.now()+3){r.run(resume+1);resume=uint64_t(r.next_tick+0.5);}
 r.send(resume-2,0xf2,8,0);r.send(resume-1,0xfb);r.run(resume);assert(r.notes.size()==stopped && r.state()==2);
 r.run(resume+1);assert(r.notes.size()==stopped+1 && r.notes.back()==resume && r.state()==3);
}

void flywheels_through_a_lost_clock(){
 Rig r(true);r.send(100,0xfa);r.master(100,120.4);
 r.run(kRate*12);assert(r.state()==3);size_t locked=r.notes.size();
 // The cable goes: the transport keeps the pace it had learned.
 r.clock=false;double silent=r.next_tick;r.run(kRate*16);assert(r.state()==1 && r.notes.size()>=locked+7);
 const double sixteenth=double(kRate)*60/(120.4*4);
 for(size_t k=locked+1;k<r.notes.size();++k)assert(std::fabs(double(r.notes[k]-r.notes[k-1])-sixteenth)<=2);
 // It comes back on the same grid; the slave is still on it.
 while(silent<double(kRate*16)){r.ticks.push_back(uint64_t(silent+0.5));silent+=r.interval;}
 r.next_tick=silent;r.clock=true;r.run(kRate*24);
 assert(r.state()==3 && r.worst(unsigned(locked),unsigned(r.notes.size()-1))<=12 && r.worst(unsigned(r.notes.size()-9),unsigned(r.notes.size()-1))<=kSettled);
}

void runs_on_its_own_without_a_clock(){
 // Local PLAY with no clock anywhere: exactly the internal sequencer.
 Rig slave(true),internal(false);
 assert(slave.engine->submit({CommandType::Start,0,0,0}) && internal.engine->submit({CommandType::Start,0,0,0}));
 slave.run(8000);internal.run(8000);
 assert(slave.notes==internal.notes && slave.notes.size()==8 && slave.state()==1 && internal.state()==0);
 assert(slave.realtime_out.empty() && !internal.realtime_out.empty());
 // A remote Start that no clock follows waits a tick and a half, then runs.
 Rig remote(true);remote.send(500,0xfa);remote.run(9000);
 assert(remote.notes.size()>=8 && remote.notes[0]>500 && remote.notes[0]<=500+252);
 for(size_t k=1;k<remote.notes.size();++k)assert(remote.notes[k]-remote.notes[k-1]==1000);
 // With the mode off an incoming clock changes nothing.
 Rig deaf(false);deaf.master(1,133);assert(deaf.engine->submit({CommandType::Start,0,0,0}));deaf.run(8000);
 assert(deaf.notes==internal.notes && deaf.tempo==120);
}

void locks_a_running_transport_to_the_nearest_tick(){
 // Started locally, then a clock appears: tempo and tick phase are taken up
 // without restarting. The position is the player's, so only spacing is known.
 Rig r(true);assert(r.engine->submit({CommandType::Start,0,0,0}));r.run(3000);assert(r.state()==1);
 r.master(3050,97.3);r.run(kRate*20);assert(r.state()==3 && r.tempo==97);
 const double sixteenth=double(kRate)*60/(97.3*4);size_t last=r.notes.size()-1;
 assert(std::fabs(double(r.notes[last]-r.notes[last-32])-sixteenth*32)<=2);
 // Each step sits on a master tick.
 double phase=std::fmod(double(r.notes[last])-3050.0,r.interval);if(phase>r.interval/2)phase-=r.interval;assert(std::fabs(phase)<=2);
 // Turning the mode off mid-flight returns to the internal clock at once.
 assert(r.engine->submit({CommandType::SetExternalClock,0,0,0}));r.run(kRate*24);assert(r.state()==0);
 last=r.notes.size()-1;assert(r.notes[last]-r.notes[last-1]==uint64_t(kRate)*60/(97*4) || r.notes[last]-r.notes[last-1]==uint64_t(kRate)*60/(97*4)+1);
}

void rides_out_jitter_and_bursts(){
 // Up to 2 ms of delivery delay on every tick (mean 8 samples).
 Rig jittery(true);jittery.jitter=16;jittery.send(100,0xfa);jittery.master(100,120.4);jittery.run(kRate*33);
 assert(jittery.state()==3 && jittery.tempo==120 && jittery.worst(32,255,8)<=6);
 // Ticks handed over three at a time, two of them late. Only the last of each
 // group is on time, and the count is still right: the slave gives up waiting
 // for the first tick, starts late, catches up, and sits on the master's true
 // grid rather than two ticks behind it.
 Rig bursty(true);bursty.burst=3;bursty.send(100,0xfa);bursty.master(100,120.4);bursty.run(kRate*33);
 assert(bursty.notes.size()>=256 && bursty.notes[0]>100 && bursty.notes[0]<=100+252);
 assert(bursty.state()==3 && bursty.tempo==120 && bursty.worst(4,255)<=8 && bursty.worst(32,255)<=kSettled);
}

void ignores_what_a_slave_does_not_need(){
 // Active Sensing, MTC, SysEx and the like are legal traffic, not errors.
 Rig r(true);MidiInputRouter router;
 for(uint8_t status:{uint8_t(0xfe),uint8_t(0xff),uint8_t(0xf6)})assert(router.accept(*r.engine,*r.project,{0,0,1,{status,0,0}}));
 assert(router.accept(*r.engine,*r.project,{0,0,2,{0xf1,0x12,0}}) && router.accept(*r.engine,*r.project,{0,0,2,{0xf3,5,0}}));
 assert(router.accept(*r.engine,*r.project,{0,0,3,{0xf0,0x7e,0x7f}}) && router.accept(*r.engine,*r.project,{0,0,3,{0x06,0x01,0xf7}}) && router.accept(*r.engine,*r.project,{0,0,1,{0xf7,0,0}}));
 // Malformed packets are still refused, and real traffic still routes.
 assert(!router.accept(*r.engine,*r.project,{0,0,0,{0xfe,0,0}}) && !router.accept(*r.engine,*r.project,{0,16,1,{0xfe,0,0}}) && !router.accept(*r.engine,*r.project,{0,0,2,{0x90,60,100}}));
 assert(router.accept(*r.engine,*r.project,{0,0,1,{0xf8,0,0}}) && router.accept(*r.engine,*r.project,{0,0,1,{0xfa,0,0}}));
 r.run(64);assert(r.state()==2);
}
}
int main(){
 loop_logic();
 starts_on_the_first_tick();
 holds_phase_at_a_fractional_tempo();
 follows_a_tempo_change_without_slipping();
 resumes_in_place_after_stop_and_continue();
 flywheels_through_a_lost_clock();
 runs_on_its_own_without_a_clock();
 locks_a_running_transport_to_the_nearest_tick();
 rides_out_jitter_and_bursts();
 ignores_what_a_slave_does_not_need();
 std::puts("MIDI slave clock: starts on the first tick, holds phase, follows tempo, resumes in place, flywheels through loss");
}
