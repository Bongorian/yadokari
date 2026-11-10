#pragma once
#include "engine.h"
#include "project.h"
#include "update/sha256.h"
#include <memory>
#include <cassert>
namespace groove {
// Captured from master21abd87c before step-expression implementation; float PCM.
inline void render_plain_phrase(Instrument instrument,uint8_t digest[32]){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);project->tempo=137;
 auto t=track_at(*project,0,0);t.instrument=instrument;t.root_note=48;t.length=7;
 for(unsigned i=0;i<7;++i){auto& s=t.steps[i];s.enabled=i!=3;s.note=uint8_t(60+(i*5)%12);s.velocity=uint8_t(75+i*6);s.gate=uint16_t(i%2?48:96);s.ratchet=i==5?2:1;}
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*project));assert(engine->submit({CommandType::Start,0,0,0}));
 update::Sha256 hash;float pcm[256];for(unsigned i=0;i<750;++i){engine->render(pcm,128);hash.add(reinterpret_cast<const uint8_t*>(pcm),sizeof pcm);}hash.finish(digest);
}
}
