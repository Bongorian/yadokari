#pragma once
#include "project_storage.h"
#include "../src/session_files.h"
#include "../src/dsp_probe_workload.h"
#include "../src/alloc_policy.h"
#include <circle/logger.h>
// Measurement only, following CDspProbe's staging pattern. The caller has
// already quiesced the audio worker, so this renders one fixed built-in
// probe project off-realtime on the task core and logs an FNV-1a hash of
// the interleaved PCM. The sequence is fully fixed (staging, settle blocks,
// trigger blocks, 24-frame render quantum) so two boards running the same
// firmware can be compared sample-for-sample for the identity gate.
class CRenderHashProbe {
 groove::ProjectState* original_=nullptr;groove::ProjectState* candidate_=nullptr;
 groove::WavView original_views_[64]{},views_[64]{};
 uint8_t pcm_[512*2]{};
 static uint64_t Fnv(uint64_t hash,const void* data,size_t bytes){const uint8_t* p=static_cast<const uint8_t*>(data);for(size_t i=0;i<bytes;++i){hash^=p[i];hash*=1099511628211ull;}return hash;}
public:
 bool run(groove::Engine& engine,groove::ProjectState& project,groove::Performance& ui,groove::SessionFiles& files){
  original_=groove::alloc_new<groove::ProjectState>(project);candidate_=groove::alloc_new<groove::ProjectState>();
  if(!original_ || !candidate_){delete original_;delete candidate_;original_=candidate_=nullptr;return false;}
  files.copy_bindings(original_views_);for(auto& view:views_)view={};
  for(unsigned i=0;i<512;++i){int16_t value=(i%128)<64?8192:-8192;pcm_[2*i]=uint8_t(value);pcm_[2*i+1]=uint8_t(uint16_t(value)>>8);}
  views_[0]={pcm_,512,48000,1,16,1};
  const groove::DspCondition condition{groove::DspCondition::Saturate,groove::Instrument::PlaitsMacro,2,8,8};
  groove::make_dsp_probe_project(*candidate_,condition);
  groove::copy_text(candidate_->assets[0].path,96,"GROOVE/DSP-RESIDENT.WAV");candidate_->assets[0].metadata.frames=512;candidate_->assets[0].metadata.rate=48000;candidate_->assets[0].metadata.channels=1;candidate_->assets[0].metadata.bits=16;
  bool ok=engine.stage_project(*candidate_,views_);
  float block[48]{};uint64_t hash=1469598103934665603ull;unsigned round=0;
  if(ok){
   engine.submit({groove::CommandType::Panic,0,0,0});
   for(unsigned b=0;b<250;++b)engine.render(block,24);
   for(unsigned b=0;b<4000;++b){
    if(b%1000==0){for(unsigned t=0;t<condition.tracks;++t)for(unsigned n=0;n<condition.notes;++n){groove::Command cmd{groove::CommandType::Trigger,uint8_t(t),uint8_t(n),100};cmd.parameter=1;cmd.scalar=float(48+int((n*3+t*5+round*11)%40));engine.submit(cmd);}++round;}
    engine.render(block,24);hash=Fnv(hash,block,sizeof block);
   }
  }
  CLogger::Get()->Write("dsp",LogNotice,"RENDERHASH fnv1a64 %016llx frames 96000 blocks 4000 condition saturate-plaits-2 ok %u",(unsigned long long)hash,unsigned(ok));
  engine.submit({groove::CommandType::Panic,0,0,0});
  bool restored=engine.stage_project(*original_,original_views_);
  engine.render(block,0);ui.project_loaded();
  delete original_;delete candidate_;original_=candidate_=nullptr;
  return ok && restored;
 }
};
