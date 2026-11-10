#pragma once
#include <stdint.h>
#include <math.h>
#include "Synthesis/oscillator.h"
#include "Control/adsr.h"
namespace groove {
// Fixed-memory monophonic subtractive-oscillator voice: two DaisySP oscillators
// with a DaisySP ADSR. No filter or dynamic allocation is involved.
class DaisySynth {
public:
 void init(uint32_t rate) {
  rate_=rate?rate:48000;
  main_.Init(float(rate_)); sub_.Init(float(rate_));
  main_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
  sub_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SQUARE);
  main_.SetAmp(0.7f); sub_.SetAmp(0.3f);
  envelope_.Init(float(rate_)); envelope_.SetAttackTime(0.005f); envelope_.SetDecayTime(0.12f);
  envelope_.SetSustainLevel(0.55f); envelope_.SetReleaseTime(0.12f); set_note(60);
 }
 void set_note(uint8_t note) {
  set_note_float(float(note));
 }
 void set_note_float(float note) {
  if(!(note>=0 && note<=127)) return;
  const float freq=440.0f*powf(2.0f,(float(note)-69.0f)/12.0f);
  // Keep the upstream oscillator within its valid Nyquist range.
  const float limited=freq<float(rate_)*0.49f?freq:float(rate_)*0.49f;
  main_.SetFreq(limited); sub_.SetFreq(limited*0.5f);
 }
 void set_adsr(float attack,float decay,float sustain,float release) { envelope_.SetAttackTime(attack); envelope_.SetDecayTime(decay); envelope_.SetSustainLevel(sustain); envelope_.SetReleaseTime(release); }
 void trigger(uint8_t velocity) { gain_=float(velocity>127?127:velocity)/127.0f; remaining_=rate_/5; gate_=true; envelope_.Retrigger(true); }
 void hold() { remaining_=0; }
 void note_off() { gate_=false; remaining_=0; }
 bool active() const { return gate_ || envelope_.IsRunning(); }
 float process() {
  if(!gate_ && !envelope_.IsRunning()) return 0;
  const float env=envelope_.Process(gate_);
  if(gate_ && remaining_ && --remaining_==0) gate_=false;
  return (main_.Process()+sub_.Process())*env*gain_;
 }
private:
 uint32_t rate_=48000,remaining_=0; bool gate_=false; float gain_=0;
 daisysp::Oscillator main_,sub_; daisysp::Adsr envelope_;
};
}
