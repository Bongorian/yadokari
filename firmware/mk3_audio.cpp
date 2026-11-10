#include "mk3_audio.h"
#include "../src/usb_progress.h"
#include "../src/pcm_output.h"
#include <circle/util.h>
#include <circle/timer.h>
#include <circle/logger.h>
static void ProbeMaximum(u32& target,u32 value){u32 old=__atomic_load_n(&target,__ATOMIC_RELAXED);while(value>old && !__atomic_compare_exchange_n(&target,&old,value,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}}
static u32 ProbeMagnitude(float value){value=groove::sanitize_audio(value);if(value<0)value=-value;return u32(value*1000000.0f+0.5f);}
void CMk3Audio::ResetProbeStats(){__atomic_store_n(&m_ProbeEnabled,0,__ATOMIC_RELAXED);__atomic_store_n(&m_MainPeak,0,__ATOMIC_RELAXED);__atomic_store_n(&m_PhonesPeak,0,__ATOMIC_RELAXED);__atomic_store_n(&m_InputRightPeakL,0,__ATOMIC_RELAXED);__atomic_store_n(&m_InputRightPeakR,0,__ATOMIC_RELAXED);__atomic_store_n(&m_ProbeEnabled,1,__ATOMIC_RELAXED);}
CMk3Audio::ProbeStats CMk3Audio::GetProbeStats()const{return {__atomic_load_n(&m_MainPeak,__ATOMIC_RELAXED),__atomic_load_n(&m_PhonesPeak,__ATOMIC_RELAXED),__atomic_load_n(&m_InputRightPeakL,__ATOMIC_RELAXED),__atomic_load_n(&m_InputRightPeakR,__ATOMIC_RELAXED)};}
CMk3Audio::Stats CMk3Audio::GetStats() const { return {__atomic_load_n(&m_Frames,__ATOMIC_RELAXED),__atomic_load_n(&m_Callbacks,__ATOMIC_RELAXED),__atomic_load_n(&m_BadChunks,__ATOMIC_RELAXED),__atomic_load_n(&m_MaxUs,__ATOMIC_RELAXED),__atomic_load_n(&m_Over125us,__ATOMIC_RELAXED),__atomic_load_n(&m_Over250us,__ATOMIC_RELAXED)}; }
void CMk3Audio::BeginTimingWindow(){__atomic_store_n(&m_WindowEnabled,0,__ATOMIC_RELEASE);__atomic_store_n(&m_WindowTotal,0,__ATOMIC_RELAXED);__atomic_store_n(&m_WindowCallbacks,0,__ATOMIC_RELAXED);__atomic_store_n(&m_WindowMax,0,__ATOMIC_RELAXED);__atomic_store_n(&m_Window125,0,__ATOMIC_RELAXED);__atomic_store_n(&m_Window250,0,__ATOMIC_RELAXED);__atomic_store_n(&m_WindowBad,0,__ATOMIC_RELAXED);__atomic_store_n(&m_WindowEnabled,1,__ATOMIC_RELEASE);}
CMk3Audio::TimingWindow CMk3Audio::EndTimingWindow(){__atomic_store_n(&m_WindowEnabled,0,__ATOMIC_RELEASE);return {__atomic_load_n(&m_WindowTotal,__ATOMIC_RELAXED),__atomic_load_n(&m_WindowCallbacks,__ATOMIC_RELAXED),__atomic_load_n(&m_WindowMax,__ATOMIC_RELAXED),__atomic_load_n(&m_Window125,__ATOMIC_RELAXED),__atomic_load_n(&m_Window250,__ATOMIC_RELAXED),__atomic_load_n(&m_WindowBad,__ATOMIC_RELAXED)};}
void CMk3Audio::Record(unsigned start,unsigned frames,bool bad) { unsigned elapsed=CTimer::GetClockTicks()-start; if(__atomic_load_n(&m_WindowEnabled,__ATOMIC_ACQUIRE)){__atomic_add_fetch(&m_WindowTotal,u64(elapsed),__ATOMIC_RELAXED);__atomic_add_fetch(&m_WindowCallbacks,1,__ATOMIC_RELAXED);if(elapsed>125)__atomic_add_fetch(&m_Window125,1,__ATOMIC_RELAXED);if(elapsed>250)__atomic_add_fetch(&m_Window250,1,__ATOMIC_RELAXED);if(bad)__atomic_add_fetch(&m_WindowBad,1,__ATOMIC_RELAXED);ProbeMaximum(m_WindowMax,elapsed);} __atomic_add_fetch(&m_Frames,u64(frames),__ATOMIC_RELAXED); __atomic_add_fetch(&m_Callbacks,1,__ATOMIC_RELAXED); if(bad) __atomic_add_fetch(&m_BadChunks,1,__ATOMIC_RELAXED); if(elapsed>125) __atomic_add_fetch(&m_Over125us,1,__ATOMIC_RELAXED); if(elapsed>250) __atomic_add_fetch(&m_Over250us,1,__ATOMIC_RELAXED); unsigned old=__atomic_load_n(&m_MaxUs,__ATOMIC_RELAXED); while(elapsed>old && !__atomic_compare_exchange_n(&m_MaxUs,&old,elapsed,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {} }
unsigned CMk3Audio::GetChunk(s16* buffer,unsigned words) {
 const unsigned start=CTimer::GetClockTicks();m_Gaps.record(start);groove::display_usb_progress.audio(start);m_PhaseTiming.callback(start); const unsigned channels=GetHWTXChannels();
 if(!channels || words%channels) { memset(buffer,0,words*sizeof(s16)); Record(start,0,true); return words; }
 unsigned remaining=words/channels; unsigned cursor=0; float data[128],cue[128];
 while(remaining) {
  unsigned n=remaining<64?remaining:64; if(m_WorkerStarted)m_RenderSlot->read(data,cue,n);else m_Engine.render_ports(data,cue,nullptr,n);
  if(__atomic_load_n(&m_ProbeEnabled,__ATOMIC_RELAXED)){u32 main_peak=0,phones_peak=0;for(unsigned i=0;i<2*n;++i){u32 a=ProbeMagnitude(data[i]),b=ProbeMagnitude(cue[i]);if(a>main_peak)main_peak=a;if(b>phones_peak)phones_peak=b;}ProbeMaximum(m_MainPeak,main_peak);ProbeMaximum(m_PhonesPeak,phones_peak);}
  for(unsigned i=0;i<n;++i) for(unsigned c=0;c<channels;++c) {
   float sample=c<2?data[2*i+c]:c<4?cue[2*i+c-2]:0.0f;
   buffer[cursor++]=groove::pcm16(sample);
  }
  remaining-=n;
 }
 Record(start,words/channels,false); return words;
}
unsigned CMk3Audio::GetChunk(u32* raw,unsigned words) {
 const unsigned start=CTimer::GetClockTicks();m_Gaps.record(start);groove::display_usb_progress.audio(start);m_PhaseTiming.callback(start); u8* buffer=(u8*)raw; const unsigned channels=GetHWTXChannels(); const unsigned slot=GetUSBSubslotBytes();
 if(!channels || words%channels || (slot!=3 && slot!=4)) { memset(buffer,0,words*slot); Record(start,0,true); return words; }
 unsigned remaining=words/channels; unsigned cursor=0; float data[128],cue[128];
 while(remaining) {
  unsigned n=remaining<64?remaining:64; if(m_WorkerStarted)m_RenderSlot->read(data,cue,n);else m_Engine.render_ports(data,cue,nullptr,n);
  if(__atomic_load_n(&m_ProbeEnabled,__ATOMIC_RELAXED)){u32 main_peak=0,phones_peak=0;for(unsigned i=0;i<2*n;++i){u32 a=ProbeMagnitude(data[i]),b=ProbeMagnitude(cue[i]);if(a>main_peak)main_peak=a;if(b>phones_peak)phones_peak=b;}ProbeMaximum(m_MainPeak,main_peak);ProbeMaximum(m_PhonesPeak,phones_peak);}
  for(unsigned i=0;i<n;++i) for(unsigned c=0;c<channels;++c) {
   float sample=c<2?data[2*i+c]:c<4?cue[2*i+c-2]:0.0f;
   groove::pcm24le(sample,buffer+cursor); if(slot==4) buffer[cursor+3]=0; cursor+=slot;
  }
  remaining-=n;
 }
 Record(start,words/channels,false); return words;
}

void CMk3Audio::PutChunk(const s16* buffer,unsigned words){unsigned ch=GetHWRXChannels();if(!ch || words%ch){__atomic_add_fetch(&m_BadChunks,1,__ATOMIC_RELAXED);return;}for(unsigned i=0;i<words;i+=ch)m_Engine.receive_audio(float(buffer[i])/32768,float(buffer[i+(ch>1?1:0)])/32768);}
void CMk3Audio::PutChunk(const u32* raw,unsigned words){unsigned ch=GetHWRXChannels(),slot=GetUSBSubslotBytes();if(!ch || words%ch || (slot!=3 && slot!=4)){__atomic_add_fetch(&m_BadChunks,1,__ATOMIC_RELAXED);return;}const auto* bytes=reinterpret_cast<const u8*>(raw);u32 right_l=0,right_r=0;bool probe=slot==4 && __atomic_load_n(&m_ProbeEnabled,__ATOMIC_RELAXED);for(unsigned i=0;i<words;i+=ch){const auto* frame=bytes+size_t(i)*slot;const auto* second=frame+(ch>1?slot:0);float l=slot==4?groove::mk3_pcm24_capture(frame):groove::pcm24le_read(frame),r=slot==4?groove::mk3_pcm24_capture(second):groove::pcm24le_read(second);m_Engine.receive_audio(l,r);if(probe){u32 a=ProbeMagnitude(l),b=ProbeMagnitude(r);if(a>right_l)right_l=a;if(b>right_r)right_r=b;}}if(probe){ProbeMaximum(m_InputRightPeakL,right_l);ProbeMaximum(m_InputRightPeakR,right_r);}}

void CMk3Audio::TracePhases()const{
 for(unsigned p=0;p<groove::AudioPhaseTiming::Count;++p){auto w=m_PhaseTiming.worker_stats(p);auto c=m_PhaseTiming.callback_stats(p);CLogger::Get()->Write("groove",LogNotice,"Audio phase %s blocks %u worker-max-us %u over500 %u first-us %u callbacks %u gap-max-us %u over500 %u first-us %u",groove::AudioPhaseTiming::name(p),w.calls,w.maximum,w.over500,w.first_at,c.calls,c.maximum,c.over500,c.first_at);}
}

void CMk3Audio::DrainAttribution(){
 if(m_AttributionReported || !m_SlowTrace.ready()){return;}
 unsigned now=CTimer::GetClockTicks();if(uint32_t(now-m_AttributionDrained)<5000)return;m_AttributionDrained=now;
 if(auto* e=m_SlowTrace.next()){const auto& a=e->work;CLogger::Get()->Write("audit",LogNotice,"Audio block at %u us %u phase %u/%u gen %u/%u render %u control %u commands %u count %u top %u/%u/%u visual %u",e->at,e->elapsed,e->phase_start,e->phase_end,a.generation_before,a.generation_after,a.total,a.control,a.commands,a.command_count,a.top_kind,a.top_track,a.top_us,a.visual);CLogger::Get()->Write("audit",LogNotice,"Audio reset at %u performance %u group %u delay %u reverb %u track %u snapshot %u",e->at,a.reset[0],a.reset[1],a.reset[2],a.reset[3],a.reset[4],a.reset[5]);CLogger::Get()->Write("audit",LogNotice,"Audio stages at %u events %u tracks %u output %u tail %u",e->at,a.events,a.tracks,a.output,a.total-a.control-a.events-a.tracks-a.output);if(e->dsp_ticks)CLogger::Get()->Write("audit",LogNotice,"Audio DSP at %u poly %u ticks %u visual %u trigger %u parameters %u detailed %u",e->at,e->poly_peak,e->dsp_ticks,e->dsp_visual,e->dsp_trigger,e->dsp_parameters,unsigned(GROOVE_DETAILED_RENDER_ATTRIBUTION));}
 else{CLogger::Get()->Write("audit",LogNotice,"Audio attribution completed events %u lost %u",m_SlowTrace.count(),m_SlowTrace.lost());m_AttributionReported=true;}
}
