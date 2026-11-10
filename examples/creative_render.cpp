#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include "ui_renderer.h"
#include "wav_writer.h"
#include "session_actions.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
#include <fstream>
struct Sink {std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
struct Source {const std::vector<uint8_t>& bytes;size_t at=0;bool read(uint8_t* p,size_t n){if(n>bytes.size()-at)return false;__builtin_memcpy(p,bytes.data()+at,n);at+=n;return true;}bool at_end(){return at==bytes.size();}};
static void screen(const char* name,const groove::UiRenderer& ui){char path[128];std::snprintf(path,sizeof path,"build/creative-%s.ppm",name);FILE* f=std::fopen(path,"wb");assert(f);std::fprintf(f,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){auto c=ui.pixels[x/480][y*480+x%480];uint8_t p[]={uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)};assert(std::fwrite(p,1,3,f)==3);}assert(std::fclose(f)==0);}
int main(){using namespace groove;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto e=std::make_unique<Engine>();assert(e->stage_project(*p));auto perf=std::make_unique<Performance>(*e,*p);float block[256];auto service=[&](){e->render(block,0);perf->poll();};service();
 auto draw_screen=[&](const char* name){auto frame=std::make_unique<UiRenderer>();frame->draw(*p,*perf);screen(name,*frame);};
 const Instrument instruments[]={Instrument::WaveSequence,Instrument::ChipClock,Instrument::TapeMicrosound,Instrument::WestCoast,Instrument::SpectralFreeze};
 const char* names[]={"wave-sequence","chip-clock","tape-micro","west-coast","spectral-freeze"};
 for(unsigned t=0;t<5;++t){assert(perf->select_track(t));assert(perf->set_instrument(instruments[t]));assert(perf->set_parameter(32,t==2?0.78f:t==4?0.2f:0.5f));assert(perf->set_parameter(33,t==4?0.5f:0.72f));assert(perf->set_parameter(34,t==2?0.3f:0.5f));assert(perf->set_parameter(35,t==4?0.95f:0.65f));assert(perf->set_parameter(36,t==2?0:0.5f));perf->set_mode(UiMode::Step);assert(perf->dispatch({ControllerInput::Type::Pad,12,0x10,3500}));assert(perf->dispatch({ControllerInput::Type::Pad,12,0x20,0}));assert(perf->set_step_note(0,60));assert(e->submit({CommandType::SetGate,uint8_t(t),0,384}));track_at(*p,0,t).steps[0].gate=384;assert(perf->set_lock(0,32,0.7f));assert(perf->store_sound(t));service();}
 // Engine-lab edits, tuning, preset recall and model-independent modulation.
 perf->select_track(0);perf->set_mode(UiMode::Synth);assert(perf->soft_key(3));assert(perf->knob(3,1000));perf->set_shift(true);assert(perf->knob(7,17000));perf->set_shift(false);service();assert(track_at(*p,0,0).creative.length[0]==5 && p->tuning[0]==17);assert(perf->store_sound(0));assert(perf->creative(CreativeState{}));assert(perf->recall_sound(0));service();assert(track_at(*p,0,0).creative.length[0]==5);
 AutomationLane lane;lane.enabled=true;lane.parameter=33;lane.values[0]=0.6f;lane.values[1]=0.85f;lane.length=2;assert(perf->automation(0,lane));service();
 // Record physical-style controls on the same sample clock; replay is an overlay.
 perf->set_mode(UiMode::Perform);assert(perf->soft_key(2));assert(perf->set_strip_mode(StripMode::Perform));assert(perf->play());service();assert(perf->gesture_record(true));service();for(unsigned n=0;n<24;++n){if(n%4==0){assert(perf->dispatch({ControllerInput::Type::Knob,uint8_t((n/4)%8),1000}));assert(perf->dispatch({ControllerInput::Type::Strip,0,int(n)*400}));}e->render(block,128);perf->poll();MidiEvent m;while(e->pop_midi(m)){}ClockEvent c;while(e->pop_clock(c)){}}
 assert(perf->gesture_record(false));service();assert(p->gesture.count>=10);float level=track_at(*p,0,0).level;assert(perf->gesture_play(true));e->render(block,128);perf->poll();assert(track_at(*p,0,0).level==level);assert(perf->gesture_play(false));service();assert(perf->stop());service();assert(perf->reset_live());service();
 Sink saved;assert(encode_project(saved,*p));auto reload=std::make_unique<ProjectState>();Source input{saved.bytes};assert(decode_project(input,*reload));assert(reload->gesture.count==p->gesture.count && reload->tuning[0]==17 && reload->sounds[0].creative.length[0]==5);std::ofstream("build/creative.mgxs",std::ios::binary).write(reinterpret_cast<const char*>(saved.bytes.data()),saved.bytes.size());
 assert(e->stage_project(*reload));*p=*reload;perf->project_loaded();service();std::vector<float> audio;audio.reserve(48000*10*2);float peaks[5]{};
 for(unsigned t=0;t<5;++t){perf->select_track(t);perf->set_mode(UiMode::Synth);perf->soft_key(0);for(unsigned i=0;i<5;++i){Command c{CommandType::SetSolo,uint8_t(i),0,uint16_t(i==t)};assert(e->submit(c));set_track_mixer_flag(p->solo_tracks,i,i==t);}assert(perf->play());for(unsigned n=0;n<750;++n){if(t==4 && n==160)assert(perf->set_parameter(32,1));e->render(block,128);for(float v:block){assert(std::isfinite(v) && std::fabs(v)<=1);if(std::fabs(v)>peaks[t])peaks[t]=std::fabs(v);audio.push_back(v);}perf->animate(e->samples_rendered()/48);perf->poll();MidiEvent m;while(e->pop_midi(m)){}ClockEvent c;while(e->pop_clock(c)){}if(n==120){assert(perf->dispatch({ControllerInput::Type::KnobTouch,2,1}));draw_screen(names[t]);perf->dispatch({ControllerInput::Type::KnobTouch,2,0});}}assert(peaks[t]>0.0001f);assert(perf->stop());service();}
 perf->set_mode(UiMode::Step);draw_screen("step16");perf->set_shift(true);assert(perf->soft_key(3));perf->set_shift(false);draw_screen("step64");perf->set_mode(UiMode::Perform);assert(perf->soft_key(2));perf->set_shift(false);draw_screen("gesture");assert(perf->dice());service();assert(perf->undo());service();
 perf->set_mode(UiMode::Synth);assert(perf->soft_key(3));draw_screen("lab");
 Sink wav;assert(write_wav(wav,audio.data(),audio.size()/2,48000));std::ofstream("build/creative-engines.wav",std::ios::binary).write(reinterpret_cast<const char*>(wav.bytes.data()),wav.bytes.size());std::printf("creative: five sequenced engines, locks, automation, engine lab, tuning, preset, gesture overlay, Dice/Undo and MGXC reload PASS; peaks %.4f %.4f %.4f %.4f %.4f; gesture=%u\n",peaks[0],peaks[1],peaks[2],peaks[3],peaks[4],p->gesture.count);
}
