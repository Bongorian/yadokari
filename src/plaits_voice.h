#pragma once
#include <stdint.h>
#include "plaits/dsp/voice.h"
#include "dx7_bank.h"
#include "stmlib/utils/buffer_allocator.h"
#include "stmlib/utils/random.h"
namespace groove {
// One monophonic 48 kHz Plaits voice; fixed 12-sample render quantum.
// Storage and DSP state are owned by this object; no allocation in render.
class PlaitsVoice {
public:
 void init() {
  uint32_t saved=stmlib::Random::state(); stmlib::Random::Seed(random_);
  stmlib::BufferAllocator allocator(arena_,sizeof arena_); voice_.Init(&allocator);
  patch_={}; mod_={}; patch_.note=48; patch_.harmonics=0.5f; patch_.timbre=0.5f; patch_.morph=0.5f; patch_.decay=0.5f; patch_.lpg_colour=0.5f;
  mod_.trigger_patched=true; held_=gate_open_=retrigger_low_=gated_=false;gate_blocks_=silent_quanta_=0;position_=12; random_=stmlib::Random::state(); stmlib::Random::Seed(saved);
 }
 void seed(uint32_t value) { random_=value; }
 bool set_model(unsigned model) { if(model>=24) return false; if(patch_.engine!=int(model)) wake(); patch_.engine=int(model); return true; }
 bool set_parameter(unsigned id, float value) {
  if(id>=5 || !(value>=0 && value<=1)) return false;
  float* field=nullptr;
  switch(id) { case 0: field=&patch_.harmonics; break; case 1: field=&patch_.timbre; break; case 2: field=&patch_.morph; break; case 3: field=&patch_.decay; break; case 4: field=&patch_.lpg_colour; break; }
  if(*field!=value) { wake(); *field=value; } return true;
 }
 int active_model() const { return voice_.active_engine(); }
 void set_note_float(float note) { if(note>=0 && note<=127 && patch_.note!=note){ wake(); patch_.note=note; } }
 void set_note(unsigned note) { if(note<128) set_note_float(float(note)); }
 bool set_dx7_bank(unsigned slot,Dx7BankView bank) { if(slot>=3 || !bank.data) return false; wake(); voice_.SetExternalBank(slot,bank.data); return true; }
 void trigger(uint8_t velocity=100,bool held=false) { wake(true); mod_.level=float(velocity)/127.0f; mod_.level_patched=true; gate_blocks_=4; held_=held;retrigger_low_=gate_open_; }
 void release(){held_=false;gate_blocks_=0;}
 void stop(){release();gated_=true;silent_quanta_=0;position_=12;}
 // O-01 silence gate: after kGateQuanta consecutive quanta whose
 // peak stays at or below kSilencePeak, Render is skipped entirely
 // and next() returns 0 until a trigger or an actual parameter /
 // model / note / bank change wakes the voice. Plaits has no
 // self-exciting element without a trigger and its LPG output
 // converges to 0, so a frozen silent voice stays silent.
 static constexpr int kSilencePeak=1;
 static constexpr unsigned kGateQuanta=16;
 bool gated() const { return gated_; }
 void wake(bool discard=false) { gated_=false; silent_quanta_=0;if(discard)position_=12; }
 float next() {
  if(gated_) return 0.0f;
  if(position_==12) { mod_.trigger=retrigger_low_?0.0f:(held_ || gate_blocks_?1.0f:0.0f);if(!retrigger_low_ && gate_blocks_) --gate_blocks_;retrigger_low_=false;gate_open_=mod_.trigger!=0; uint32_t saved=stmlib::Random::state(); stmlib::Random::Seed(random_); voice_.Render(patch_,mod_,frames_,12); random_=stmlib::Random::state(); stmlib::Random::Seed(saved); position_=0;
   int peak=0; for(unsigned i=0;i<12;++i){int v=frames_[i].out; if(v<0)v=-v; if(v>peak)peak=v;}
   if(peak<=kSilencePeak) { if(++silent_quanta_>=kGateQuanta) gated_=true; } else silent_quanta_=0; }
  return float(frames_[position_++].out)/32768.0f;
 }
private:
 alignas(16) uint8_t arena_[32768]{};
 plaits::Voice voice_{}; plaits::Patch patch_{}; plaits::Modulations mod_{}; plaits::Voice::Frame frames_[12]{};
 uint32_t random_=0x13579bdf; unsigned position_=12,gate_blocks_=0,silent_quanta_=0; bool gated_=false,held_=false,gate_open_=false,retrigger_low_=false;
};
}
