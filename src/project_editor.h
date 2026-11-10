#pragma once
#include "project.h"
#include "engine.h"
namespace groove {
// Single UI producer only. Every accepted edit names its pattern explicitly.
// Pattern/clip switching is owned by Performance; this editor handles direct edits.
class ProjectEditor {
public:
 ProjectEditor(ProjectState& project, Engine& engine):project_(project),engine_(engine) {}
 bool set_step(unsigned track,unsigned step,bool enabled) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || step>=kSteps) return false;
  if(!send({CommandType::SetStep,uint8_t(track),uint8_t(step),uint16_t(enabled)})) return false;
  track_at(project_,project_.active_pattern,track).steps[step].enabled=enabled; return true;
 }
 bool set_probability(unsigned track,unsigned step,unsigned value) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || step>=kSteps || value>100) return false;
  if(!send({CommandType::SetProbability,uint8_t(track),uint8_t(step),uint16_t(value)})) return false;
  track_at(project_,project_.active_pattern,track).steps[step].probability=uint8_t(value); return true;
 }
 bool set_mute(unsigned track,bool on) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || !send({CommandType::SetMute,uint8_t(track),0,uint16_t(on)})) return false;
  set_track_mixer_flag(project_.muted_tracks,track,on); return true;
 }
 bool set_solo(unsigned track,bool on) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || !send({CommandType::SetSolo,uint8_t(track),0,uint16_t(on)})) return false;
  set_track_mixer_flag(project_.solo_tracks,track,on); return true;
 }
 bool set_length(unsigned track,unsigned length) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || length<1 || length>kSteps || !send({CommandType::SetLength,uint8_t(track),0,uint16_t(length)})) return false;
  track_at(project_,project_.active_pattern,track).length=uint16_t(length); return true;
 }
 bool set_note(unsigned track,unsigned note) {
  if(project_.active_pattern>=kPatterns || track>=kTracks || note>127 || !send({CommandType::SetNote,uint8_t(track),0,uint16_t(note)})) return false;
  track_at(project_,project_.active_pattern,track).root_note=uint8_t(note); return true;
 }
 bool set_engine_parameter(unsigned track,unsigned local,float value){if(track>=kTracks || project_.active_pattern>=kPatterns)return false;const auto t=track_at(project_,project_.active_pattern,track);const auto* d=engine_parameter(t.instrument,local);if(!d || !(value>=d->value.minimum && value<=d->value.maximum) || (d->value.interpolation==Interpolation::Discrete && floorf(value)!=value))return false;Command c{CommandType::SetEngineParameter,uint8_t(track),0,0};c.parameter=uint16_t(local);c.scalar=value;if(!send(c))return false;return groove::set_engine_parameter(t,local,value);}
 bool set_instrument(unsigned track,Instrument instrument) {
  unsigned type=unsigned(instrument); if(project_.active_pattern>=kPatterns || track>=kTracks || !engine_.supports_engine(instrument) || !send({CommandType::SetInstrument,uint8_t(track),0,uint16_t(type)})) return false;
  track_at(project_,project_.active_pattern,track).instrument=instrument; return true;
 }
private:
 bool send(const Command& c){return engine_.submit(c,project_.active_pattern);}
 ProjectState& project_; Engine& engine_;
};
}
