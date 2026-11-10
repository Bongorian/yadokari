#pragma once
#include "../src/display_job_slot.h"
#include "../src/sample_analysis_slot.h"
#include "../src/audio_render_slot.h"
#include "../src/va_sample_slot.h"
#include "../src/project_save_slot.h"
#include "../src/project_load_slot.h"
#include "../src/soundfont_load_slot.h"
#ifdef GROOVE_MULTICORE_DISPLAY
#ifndef ARM_ALLOW_MULTI_CORE
#error GROOVE_MULTICORE_DISPLAY requires ARM_ALLOW_MULTI_CORE
#endif
#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/alloc.h>
#include <circle/timer.h>
#include <circle/logger.h>
class CDisplayWorker:public CMultiCoreSupport {
 public:
  CDisplayWorker():CMultiCoreSupport(CMemorySystem::Get()){m_Slot.enable_coalescing();m_Work.clock([](){return CTimer::GetClockTicks();});m_ProjectLoad.initialize(&m_Audio);m_ProjectSave.set_snapshot_copy(CopySnapshot,this,BeginSnapshot,EndSnapshot);m_ProjectSave.set_buffer_copy(CopySnapshot,this);}
  void set_snapshot_pump(void (*pump)(void*),void* context){m_CopyPump=pump;m_CopyContext=context;}
  groove::VaSampleSlot& VaSlot(){return m_Va;}
  groove::CoreWorkMeter& Work(){return m_Work;}
  groove::DisplayJobSlot& Slot(){return m_Slot;}
  groove::LiveVisualAnalysisSlot& VisualSlot(){return m_VisualAnalysis;}
  groove::SampleAnalysisSlot& AnalysisSlot(){return m_Analysis;}
  groove::AudioRenderSlot& AudioSlot(){return m_Audio;}
  groove::ProjectEncodeSlot& ProjectSaveSlot(){return m_ProjectSave;}
  groove::SoundFontLoadSlot& SoundFontSlot(){return m_SoundFont;}
  groove::ProjectDecodeSlot& ProjectLoadSlot(){return m_ProjectLoad;}
  bool ProbeAudio(){unsigned start=CTimer::GetClockTicks();while(!__atomic_load_n(&m_AudioReady,__ATOMIC_ACQUIRE)){if(uint32_t(CTimer::GetClockTicks()-start)>=250000u)return false;}return true;}
  bool ProbeAnalysis(){
   unsigned start=CTimer::GetClockTicks();while(!__atomic_load_n(&m_AnalysisReady,__ATOMIC_ACQUIRE)){if(uint32_t(CTimer::GetClockTicks()-start)>=250000u)return false;}
   static const uint8_t pcm[]={0,0,0,64,0,192,0,0};groove::WavView view{pcm,4,48000,1,16,1};
   if(!m_Analysis.submit(view,0,1,1))return false;
   start=CTimer::GetClockTicks();groove::SampleAnalysis result;uint32_t token=0;
   while(!m_Analysis.collect(result,token)){if(uint32_t(CTimer::GetClockTicks()-start)>=250000u)return false;}
   return token==1 && result.slices>0;
  }
  // Circle's spin-table acknowledgement precedes Run(). Verify both entry and
  // a complete shared-memory job before handing live display buffers over.
  bool Probe(){
   unsigned start=CTimer::GetClockTicks();
   while(!__atomic_load_n(&m_Ready,__ATOMIC_ACQUIRE)){
    if(uint32_t(CTimer::GetClockTicks()-start)>=250000u)return false;
   }
   auto* pixels=new uint32_t[480*272]();auto* shadow=new uint16_t[480*272]();auto* packet=(uint8_t*)palloc();
   if(!pixels || !shadow || !packet){delete[] pixels;delete[] shadow;if(packet)pfree(packet);return false;}
   groove::DisplayJob job;job.pixels=pixels;job.shadow=shadow;job.packet=packet;
   if(!m_Slot.submit(job)){delete[] pixels;delete[] shadow;pfree(packet);return false;}
   start=CTimer::GetClockTicks();groove::DisplayJob done;
   while(!m_Slot.collect(done)){
    // On timeout the worker may still hold these buffers: retain all three.
    if(uint32_t(CTimer::GetClockTicks()-start)>=250000u)return false;
   }
   bool ok=done.result.valid && done.result.dirty;
   delete[] pixels;delete[] shadow;pfree(packet);return ok;
  }
  void Run(unsigned core)override{
   if(core==1)__atomic_store_n(&m_Ready,1u,__ATOMIC_RELEASE);
   if(core==2){m_Va.worker_ready();__atomic_store_n(&m_AnalysisReady,1u,__ATOMIC_RELEASE);}
   if(core==3)__atomic_store_n(&m_AudioReady,1u,__ATOMIC_RELEASE);
   for(;;){auto stamp=m_Work.begin();bool productive=core==1?m_Slot.process_one():core==2?ProcessAnalysis():core==3?m_Audio.process_one():false;m_Work.end(core,stamp,productive);if(!productive)asm volatile("wfe" ::: "memory");}
  }

 private:
  bool ProcessAnalysis(){if(m_Va.process_one())return true;if(m_ProjectSave.busy()){unsigned before=m_Audio.missing(),begin=CTimer::GetClockTicks();if(m_ProjectSave.process_one()){CLogger::Get()->Write("groove",LogNotice,"Project encode completed us %u worker-missing %u",uint32_t(CTimer::GetClockTicks()-begin),m_Audio.missing()-before);return true;}}return m_SoundFont.process_one() || m_ProjectLoad.process_one() || m_Analysis.process_one() || m_VisualAnalysis.process_one();}
  groove::CoreWorkMeter m_Work;groove::VaSampleSlot m_Va;
  // One total budget covers all project spans and preserved payloads. Buffer copies
  // run after publication to core 2 and keep their independent local budget.
  static void BeginSnapshot(void* context,size_t bytes){auto& self=*static_cast<CDisplayWorker*>(context);self.m_CopyBegin=CTimer::GetClockTicks();self.m_CopyMissing=self.m_Audio.missing();self.m_CopyWaits=self.m_CopyMaxWait=self.m_CopyPumpUs=0;self.m_CopyBytes=bytes;self.m_CopyBatch=true;}
  static void EndSnapshot(void* context,bool ok){auto& self=*static_cast<CDisplayWorker*>(context);self.m_CopyBatch=false;CLogger::Get()->Write("groove",ok?LogNotice:LogWarning,"Snapshot copy %s us %u waits %u max-wait-us %u worker-missing %u bytes %u ui-us %u",ok?"completed":"failed",uint32_t(CTimer::GetClockTicks()-self.m_CopyBegin),self.m_CopyWaits,self.m_CopyMaxWait,self.m_Audio.missing()-self.m_CopyMissing,unsigned(self.m_CopyBytes),self.m_CopyPumpUs);}
  static bool CopySnapshot(void* context,void* destination,const void* source,size_t bytes){
   auto& self=*static_cast<CDisplayWorker*>(context);auto& audio=self.m_Audio;bool batch=self.m_CopyBatch;
   auto* dst=static_cast<uint8_t*>(destination);const auto* src=static_cast<const uint8_t*>(source);
   unsigned begin=batch?self.m_CopyBegin:CTimer::GetClockTicks();
   while(bytes){
    unsigned started=CTimer::GetClockTicks();if(uint32_t(started-begin)>=500000u)return false;
    while(audio.active() && audio.available()<groove::AudioRenderSlot::target-groove::AudioRenderSlot::block){
     if(uint32_t(CTimer::GetClockTicks()-started)>=20000u || uint32_t(CTimer::GetClockTicks()-begin)>=500000u){CLogger::Get()->Write("groove",LogWarning,"Snapshot copy refused remaining %u audio-frames %u elapsed-us %u",unsigned(bytes),audio.available(),uint32_t(CTimer::GetClockTicks()-begin));return false;}
     asm volatile("yield" ::: "memory");
    }
    unsigned waited=uint32_t(CTimer::GetClockTicks()-started);if(batch && waited){++self.m_CopyWaits;if(waited>self.m_CopyMaxWait)self.m_CopyMaxWait=waited;}
    size_t count=bytes>32768?32768:bytes;
    __builtin_memcpy(dst,src,count);dst+=count;src+=count;bytes-=count;
    // Only producer/task copies may pump presentation. No input dispatch,
    // Performance::poll, file service or project mutation during the snapshot.
    if(batch && self.m_CopyPump){unsigned started=CTimer::GetClockTicks();self.m_CopyPump(self.m_CopyContext);self.m_CopyPumpUs+=uint32_t(CTimer::GetClockTicks()-started);}
   }
   return true;
  }
  void (*m_CopyPump)(void*)=nullptr;void* m_CopyContext=nullptr;unsigned m_CopyPumpUs=0;
  bool m_CopyBatch=false;unsigned m_CopyBegin=0,m_CopyMissing=0,m_CopyWaits=0,m_CopyMaxWait=0;size_t m_CopyBytes=0;
  groove::SoundFontLoadSlot m_SoundFont;groove::LiveVisualAnalysisSlot m_VisualAnalysis;groove::DisplayJobSlot m_Slot;groove::SampleAnalysisSlot m_Analysis;groove::AudioRenderSlot m_Audio;groove::ProjectEncodeSlot m_ProjectSave;groove::ProjectDecodeSlot m_ProjectLoad;unsigned m_Ready=0,m_AnalysisReady=0,m_AudioReady=0;
};
#endif
