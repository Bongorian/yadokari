#pragma once
#include <circle/sound/usbsoundbasedevice.h>
#include "../src/engine.h"
#include "../src/backends.h"
#include "../src/callback_gap.h"
#include "../src/audio_phase_timing.h"
#include "../src/audio_slow_trace.h"
#include "../src/audio_render_slot.h"
#include <circle/timer.h>
class CMk3Audio : public CUSBSoundBaseDevice, public groove::IAudioBackend {
public:
 CMk3Audio(groove::Engine& engine,unsigned device,bool duplex=false,unsigned input_index=0,unsigned output_index=0,groove::AudioRenderSlot* render_slot=nullptr):CUSBSoundBaseDevice(48000,duplex?DeviceModeTXRX:DeviceModeTXOnly,device,input_index,output_index),m_Engine(engine),m_RenderSlot(render_slot),m_Duplex(duplex) {}
 ~CMk3Audio(){if(m_WorkerStarted){m_RenderSlot->request_stop();while(!m_RenderSlot->stopped()){} }}
 bool start() override {if(m_RenderSlot){m_WorkerStarted=m_RenderSlot->start(this,[](void* ctx,float* main,float* cue,unsigned n){static_cast<CMk3Audio*>(ctx)->RenderOnWorker(main,cue,n);});if(!m_WorkerStarted)return false;unsigned t=CTimer::GetClockTicks();while(m_RenderSlot->available()<groove::AudioRenderSlot::target && uint32_t(CTimer::GetClockTicks()-t)<500000u){} if(m_RenderSlot->available()<groove::AudioRenderSlot::block)return false;}m_Started=Start();return m_Started;}
 void EfficiencyMeter(groove::EfficiencyAudioMeter* meter){__atomic_store_n(&m_Efficiency,meter,__ATOMIC_RELEASE);}
 unsigned worker_missing()const{return m_WorkerStarted?m_RenderSlot->missing():0;}
 struct WorkerWindow {u64 total_us;u32 blocks,max_us,over500,debt_max_us;};
 void BeginWorkerWindow(){__atomic_store_n(&m_WorkerWindowEnabled,0u,__ATOMIC_RELEASE);__atomic_store_n(&m_WorkerWindowTotal,u64(0),__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerWindowBlocks,0u,__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerWindowMax,0u,__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerWindowOver500,0u,__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerDebt,0u,__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerDebtMax,0u,__ATOMIC_RELAXED);__atomic_store_n(&m_WorkerWindowEnabled,1u,__ATOMIC_RELEASE);}
 WorkerWindow EndWorkerWindow(){__atomic_store_n(&m_WorkerWindowEnabled,0u,__ATOMIC_RELEASE);return {__atomic_load_n(&m_WorkerWindowTotal,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerWindowBlocks,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerWindowMax,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerWindowOver500,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerDebtMax,__ATOMIC_RELAXED)};}
 void SetTimingPhase(unsigned p){m_PhaseTiming.phase(p);}
 void TracePhases()const;
 bool BeginAttribution(){m_AttributionReported=false;return m_SlowTrace.begin();}
 bool DiagnosticIdle()const{return m_SlowTrace.quiescent() && m_Engine.dsp_profile().quiescent() && !__atomic_load_n(&m_Efficiency,__ATOMIC_ACQUIRE);}
 void SealAttribution(){m_SlowTrace.seal();}
 void DrainAttribution();
 struct WorkerStats {u64 total_us;u32 blocks,max_us;};
 WorkerStats GetWorkerStats()const{return {__atomic_load_n(&m_WorkerTotal,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerBlocks,__ATOMIC_RELAXED),__atomic_load_n(&m_WorkerMax,__ATOMIC_RELAXED)};}
 bool operational(){return m_Started && IsRunning();}
 void stop() override { Cancel(); }
 unsigned sample_rate() const override { return 48000; }
 struct Stats { u64 frames; u32 callbacks, bad_chunks, max_us, over_125us, over_250us; };
 Stats GetStats() const;
 struct TimingWindow {u64 total_us;u32 callbacks,max_us,over_125us,over_250us,bad_chunks;};
 void BeginTimingWindow();TimingWindow EndTimingWindow();
 bool TimingTotal(u64& total)const{if(!__atomic_load_n(&m_WindowEnabled,__ATOMIC_ACQUIRE))return false;total=__atomic_load_n(&m_WindowTotal,__ATOMIC_RELAXED);return true;}
 struct ProbeStats {u32 main_peak_u,phones_peak_u,input_right_peak_l_u,input_right_peak_r_u;};
 void ResetProbeStats();
 void EndProbeStats(){__atomic_store_n(&m_ProbeEnabled,0,__ATOMIC_RELAXED);}
 ProbeStats GetProbeStats()const;
 groove::CallbackGap::Stats GetGapStats()const{return m_Gaps.stats();}
unsigned tx_channels()const{return GetHWTXChannels();}
 unsigned rx_channels()const{return m_Duplex?GetHWRXChannels():0;}
protected:
 void PutChunk(const s16*,unsigned)override;void PutChunk(const u32*,unsigned)override;
 unsigned GetChunk(s16* buffer,unsigned words) override;
 unsigned GetChunk(u32* buffer,unsigned words) override;
private:
 void Record(unsigned start,unsigned frames,bool bad);
 void RenderOnWorker(float* main,float* cue,unsigned n){auto* efficiency=__atomic_load_n(&m_Efficiency,__ATOMIC_ACQUIRE);unsigned efficiency_row=efficiency?efficiency->requested():groove::EfficiencyAudioMeter::off;bool efficiency_playing=m_Engine.diagnostic_playing();auto assist_before=efficiency_row<groove::EfficiencyAudioMeter::rows?m_Engine.va_helper_counts():groove::DspAssistCounts{};auto steals_before=efficiency_row<groove::EfficiencyAudioMeter::rows?m_Engine.poly_steal_counts():groove::PolyStealCounts{};unsigned t=CTimer::GetClockTicks(),phase=m_PhaseTiming.phase();bool trace=m_SlowTrace.worker_enter();m_Engine.render_audit().begin((trace || efficiency_row<groove::EfficiencyAudioMeter::rows)?AuditClock:nullptr,t,phase!=groove::AudioPhaseTiming::DspProbe);m_Engine.render_ports(main,cue,nullptr,n);unsigned elapsed=CTimer::GetClockTicks()-t,end_phase=m_PhaseTiming.phase();if(efficiency){auto pop=efficiency_row<groove::EfficiencyAudioMeter::rows?m_Engine.efficiency_population():groove::EfficiencyAudioMeter::Population{};if(efficiency_row<groove::EfficiencyAudioMeter::rows){pop.assist=m_Engine.va_helper_counts()-assist_before;pop.steals=m_Engine.poly_steal_counts()-steals_before;}efficiency->record(efficiency_row,elapsed,pop,m_Engine.native_active_mask(),m_Engine.native_refused_mask(),worker_missing(),(efficiency_row%8!=2 && efficiency_row%8!=6) || efficiency_playing || m_Engine.diagnostic_playing(),&m_Engine.render_audit().sample());}m_PhaseTiming.worker_in(end_phase,elapsed,t);if(trace){const auto& dsp=m_Engine.dsp_profile().last_block();m_SlowTrace.worker_exit(true,{t,elapsed,phase,end_phase,m_Engine.render_audit().sample(),dsp.poly_peak,dsp.ticks,dsp.visual,dsp.trigger,dsp.parameters});}else m_SlowTrace.acknowledge();if(__atomic_load_n(&m_WorkerWindowEnabled,__ATOMIC_ACQUIRE)){unsigned debt=__atomic_load_n(&m_WorkerDebt,__ATOMIC_RELAXED)+elapsed;debt=debt>500?debt-500:0;__atomic_store_n(&m_WorkerDebt,debt,__ATOMIC_RELAXED);if(debt>__atomic_load_n(&m_WorkerDebtMax,__ATOMIC_RELAXED))__atomic_store_n(&m_WorkerDebtMax,debt,__ATOMIC_RELAXED);__atomic_add_fetch(&m_WorkerWindowTotal,u64(elapsed),__ATOMIC_RELAXED);__atomic_add_fetch(&m_WorkerWindowBlocks,1u,__ATOMIC_RELAXED);if(elapsed>500)__atomic_add_fetch(&m_WorkerWindowOver500,1u,__ATOMIC_RELAXED);if(elapsed>__atomic_load_n(&m_WorkerWindowMax,__ATOMIC_RELAXED))__atomic_store_n(&m_WorkerWindowMax,elapsed,__ATOMIC_RELAXED);}__atomic_add_fetch(&m_WorkerTotal,u64(elapsed),__ATOMIC_RELAXED);__atomic_add_fetch(&m_WorkerBlocks,1u,__ATOMIC_RELAXED);unsigned old=__atomic_load_n(&m_WorkerMax,__ATOMIC_RELAXED);while(elapsed>old && !__atomic_compare_exchange_n(&m_WorkerMax,&old,elapsed,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){} }
 static uint32_t AuditClock(){return CTimer::GetClockTicks();}
 groove::EfficiencyAudioMeter* m_Efficiency=nullptr;
 groove::AudioSlowTrace m_SlowTrace;bool m_AttributionReported=false;unsigned m_AttributionDrained=0;
 groove::AudioPhaseTiming m_PhaseTiming;groove::CallbackGap m_Gaps;groove::Engine& m_Engine;groove::AudioRenderSlot* m_RenderSlot=nullptr;bool m_Duplex=false,m_Started=false,m_WorkerStarted=false;
 u64 m_WorkerWindowTotal=0;u32 m_WorkerWindowEnabled=0,m_WorkerWindowBlocks=0,m_WorkerWindowMax=0,m_WorkerWindowOver500=0,m_WorkerDebt=0,m_WorkerDebtMax=0;
 u64 m_WorkerTotal=0;u32 m_WorkerBlocks=0,m_WorkerMax=0;
 u64 m_Frames=0; u32 m_Callbacks=0,m_BadChunks=0,m_MaxUs=0,m_Over125us=0,m_Over250us=0;
 u64 m_WindowTotal=0;u32 m_WindowEnabled=0,m_WindowCallbacks=0,m_WindowMax=0,m_Window125=0,m_Window250=0,m_WindowBad=0;
 u32 m_MainPeak=0,m_PhonesPeak=0,m_InputRightPeakL=0,m_InputRightPeakR=0,m_ProbeEnabled=0;
};
