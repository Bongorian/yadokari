#pragma once
#include "project.h"
#include "file_access.h"
namespace groove {
// Diagnostic-only immutable score. Never replaces a user's file or factory.
inline constexpr unsigned kUiProbeRevision=2,kUiProbeStages=22;
inline constexpr uint32_t kUiProbeSeed=0x4f505432,kUiProbeSampleCrc=0x7c7e7b61;
inline constexpr const char* kUiProbeSample="FACTORY/DEBUG/demo-slices.wav";
inline const char* ui_probe_class(unsigned stage){static const char* names[]={"FM-GENERATOR","FM-TONE","SOURCE-SELECTOR","SETTINGS","ROUTING","SAVE-MODAL","STEP","TRACKER","MIXER","SAMPLER-WAVEFORM","PAD","CLIP","SCENE","EFFECTS","PERFORM","GENERATE","KEYBOARD","MODULATION","BROWSER","DIAGNOSTICS","CHORD","SONG"};return stage && stage<=kUiProbeStages?names[stage-1]:"NONE";}
inline void make_ui_probe_project(ProjectState& p){
 make_empty_project(p);p.tempo=120;p.random_seed=kUiProbeSeed;p.routing.main=.25f;p.routing.cue=0;
 for(unsigned t=0;t<kTracks;++t){p.tracks[t].instrument=Instrument::Empty;p.clips[t].playing=false;}
 auto fm=track_at(p,0,0);fm.instrument=Instrument::FM;fm.length=8;fm.level=.2f;fm.root_note=60;p.clips[0].playing=true;
 for(unsigned i=0;i<8;++i){auto& s=fm.steps[i];s.enabled=true;s.note=uint8_t(48+3*(i%4));s.velocity=90;s.gate=48;}
 auto sample=track_at(p,0,1);sample.instrument=Instrument::Sampler;sample.sample_slot=0;
 copy_text(p.assets[0].path,96,kUiProbeSample);p.assets[0].metadata.frames=48000;p.assets[0].metadata.rate=48000;p.assets[0].metadata.channels=1;p.assets[0].metadata.bits=16;p.assets[0].metadata.content_crc=kUiProbeSampleCrc;
}
}
