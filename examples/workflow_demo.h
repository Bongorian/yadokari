#pragma once
#include "../src/session_actions.h"
#include <cstdio>
namespace groove {
// Development-only setup using the same controller dispatcher as MK3. Call only
// while the audio backend is stopped; service() drains accepted audio commands.
template<class Service> bool build_workflow_demo(Engine& engine,ProjectState& project,Performance& perf,Service service){
 using T=ControllerInput::Type;bool ok=true;
 auto input=[&](ControllerInput event){if(!perf.dispatch(event)){ok=false;std::fprintf(stderr,"workflow input rejected: mode=%u type=%u index=%u value=%d\n",unsigned(perf.mode()),unsigned(event.type),event.index,event.value);}};
 auto mode=[&](UiMode value){input({T::Mode,0,int(value)});};
 auto turn=[&](unsigned knob,int steps){while(steps){int n=steps>16?16:steps< -16?-16:steps;input({T::Knob,uint8_t(knob),n*1000});steps-=n;}};
 auto pad=[&](unsigned logical){uint8_t hardware=uint8_t(mk3::pad_logical(logical));input({T::Pad,hardware,0x10,3200});input({T::Pad,hardware,0x20,0});};
 auto key=[&](unsigned index){input({T::SoftKey,uint8_t(index)});};
 auto track=[&](unsigned index){input({T::Track,uint8_t(index)});};
 auto shift=[&](bool on){input({T::Shift,0,int(on)});};
 mode(UiMode::Project);key(2);service();if(perf.browser().new_project_list){key(0);service();}if(perf.pending_project()!=Performance::ProjectAction::New)return false;key(0);if(perf.take_project_action()!=Performance::ProjectAction::New || !create_empty_session(engine,project,perf))return false;service();
 // NEW now means unassigned. Select the five sound sources explicitly.
 const Instrument initial[]={Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,Instrument::Acid,Instrument::FM};
 auto source=[&](Instrument target){for(unsigned i=0;i<instrument_count() && track_at(project,project.active_pattern,perf.selected_track()).instrument!=target;++i){input({T::Instrument});service();}return track_at(project,project.active_pattern,perf.selected_track()).instrument==target;};
 for(unsigned t=0;t<5;++t){track(t);if(!source(initial[t]))return false;service();if(t<3){mode(UiMode::Synth);key(0);turn(0,-64);turn(0,int(t)*25);service();}}
 mode(UiMode::Step);track(0);for(unsigned n=0;n<16;n+=4)pad(n);track(1);pad(4);pad(12);track(2);for(unsigned n=0;n<16;n+=2)pad(n);service();
 track(3);for(unsigned n=0;n<16;n+=4){pad(n);turn(1,int(n/4)*3);}shift(true);pad(0);shift(false);mode(UiMode::Synth);key(1);shift(true);turn(2,-16);turn(2,-4);turn(3,8);shift(false);if(!track_at(project,0,3).steps[0].locks[0].enabled || !track_at(project,0,3).steps[0].locks[1].enabled)return false;service();
 shift(true);key(0);shift(false);service();
 // Enter a chord and its track arpeggiator through the same physical dispatcher.
 mode(UiMode::Step);shift(true);pad(0);mode(UiMode::Keyboard);pad(4);pad(7);shift(false);mode(UiMode::Generate);shift(true);key(3);shift(false);turn(0,1);turn(1,1);service();if(track_at(project,0,3).arp_mode!=1 || track_at(project,0,3).arp_rate!=2)return false;key(0);
 mode(UiMode::Modulation);key(1);turn(4,-8);turn(2,1);turn(3,-16);turn(3,-4);turn(6,1);key(0);turn(1,5);turn(2,10);service();
 track(4);mode(UiMode::Step);pad(2);pad(6);pad(10);pad(14);turn(7,-9);service();
 track(15);mode(UiMode::Step);shift(true);pad(0);shift(false);mode(UiMode::Synth);if(!source(Instrument::ExternalMidi))return false;mode(UiMode::Modulation);key(3);turn(0,-10);turn(1,74);turn(2,80);shift(true);turn(2,10);shift(false);mode(UiMode::Step);for(unsigned n=0;n<16;n+=4)pad(n);service();
 mode(UiMode::Scene);key(4);service();
 mode(UiMode::Clip);key(4);input({T::Pattern,1});key(5);service();if(!perf.undo())return false;service();if(track_at(project,1,0).steps[0].enabled)return false;if(!perf.redo())return false;service();if(!track_at(project,1,0).steps[0].enabled)return false;
 track(2);input({T::Pattern,1});mode(UiMode::Generate);bool original[16];for(unsigned i=0;i<16;++i)original[i]=track_at(project,1,2).steps[i].enabled;pad(4);service();if(!perf.undo())return false;service();for(unsigned i=0;i<16;++i)if(track_at(project,1,2).steps[i].enabled!=original[i])return false;if(!perf.redo())return false;service();
 mode(UiMode::Clip);key(6);service();track(3);input({T::Pattern,1});mode(UiMode::Clip);key(6);service();
 mode(UiMode::Scene);turn(5,1);key(4);service();if(project.scenes[1].pattern_for_track[2]!=1 || project.scenes[1].pattern_for_track[3]!=1)return false;
 mode(UiMode::Effects);turn(0,15);key(1);turn(0,-10);turn(1,4);turn(5,25);key(2);turn(0,-20);turn(1,4);turn(5,25);key(0);mode(UiMode::Mixer);turn(4,10);mode(UiMode::Perform);key(4);mode(UiMode::Mixer);turn(2,-10);turn(3,6);mode(UiMode::Perform);key(5);key(1);turn(0,10);input({T::Strip,0,8192});service();
 track(3);mode(UiMode::Pad);return ok && valid_project(project) && track_at(project,0,15).instrument==Instrument::ExternalMidi;
}
}
