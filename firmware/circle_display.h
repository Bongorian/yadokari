#pragma once
#include "mk3_usb.h"
#include "../src/backends.h"
#include "../src/mk3_output.h"
#include "../src/ui/framebuffer565.h"
#include "../src/display_frame_gate.h"
#include "../src/display_job_slot.h"
#include "../src/boot_splash.h"
#include "../src/boot_splash_version.h"
#include "../src/alloc_policy.h"
#include "../src/display_attribution.h"
#include "../src/usb_progress.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <circle/memorymap.h>
#include <circle/alloc.h>
#include <circle/timer.h>
// One DMA packet plus one prepared packet per LCD; at most two FIFO requests.
// Prepare the next rectangle while its predecessor waits for the actual ACK.
class CircleDisplay : public groove::IDisplayBackend {
 struct Pending {
  Mk3DisplayTransfer* transfer=nullptr;const u32* pixels=nullptr;
  unsigned started=0,epoch=0,frame=0,first=0,y=0,rows=0,left=0,width=0;
  bool last=false,detached=false,splash=false;unsigned job=0;bool boot_version=false;
 };
 struct Prepared {groove::PreparedDisplayRect rect{};unsigned frame=0,epoch=0,job=0;bool ready=false,next=false;};
public:
 explicit CircleDisplay(u8* packet){m_Packets[0]=packet;static_assert(PAGE_SIZE>=groove::kDisplayPacketCapacity,"display packet requires AArch64 page");for(auto& shadow:m_Shadow)shadow=new u16[480*272]();}
 ~CircleDisplay(){
  if(m_Worker && m_Worker->busy()){groove::DisplayJob done;if(!m_Worker->collect(done))return;}
  Reap();
  for(unsigned side=0;side<2;++side){auto& pending=m_Pending[side];if(pending.transfer && __atomic_load_n(&pending.transfer->state,__ATOMIC_ACQUIRE)){Mk3DisplayRelease(pending.transfer);pending={};}if(!pending.transfer && m_Packets[side])pfree(m_Packets[side]);if(m_NextPackets[side])pfree(m_NextPackets[side]);delete[] m_WorkPixels[side];delete[] m_Shadow[side];}
  delete[] m_Trace;
  // Pending DMA pages and independent completion contexts outlive this object.
 }
 bool UniformPatch(unsigned side,unsigned width,unsigned& x,unsigned& y,uint16_t& color)const{
  if(side>1 || width>480 || !FrameComplete())return false;
  for(unsigned band=0;band<17;++band)if(m_Valid[side][band])for(unsigned left=0;left+width<=480;left+=4){uint16_t value=m_Shadow[side][band*16*480+left];bool same=true;
   for(unsigned row=band*16;same && row<(band+1)*16;++row)for(unsigned column=left;column<left+width;++column)if(m_Shadow[side][row*480+column]!=value){same=false;break;}
   if(same){x=left;y=band*16;color=value;return true;}
  }
  return false;
 }
 struct TraceEntry {unsigned submitted,usb,queue,reap,bytes,commands,frame;uint8_t side,phase,stage,steady;unsigned done,job,sequence,predecessor,audio_callbacks,audio_gap,accepted,shadow_end;};
 bool TraceCapturing()const{return m_TraceCapturing;}
 bool BeginTrace(){if(m_TraceCapturing)return true;if(!m_Trace && groove::allocation_admitted(sizeof(TraceEntry)*8192))m_Trace=groove::alloc_array<TraceEntry>(8192);m_TraceCount=m_TraceRead=m_TraceLost=m_JobTraceRead=0;m_TraceCapturing=m_Trace && m_Attribution.begin(m_FrameSerial);m_TraceReady=false;groove::display_usb_progress.enabled(m_TraceCapturing);return m_TraceCapturing;}
 unsigned TaskBegin(){return m_TraceCapturing?CTimer::GetClockTicks():0;}
 void TaskEnd(unsigned kind,unsigned begin){if(m_TraceCapturing && begin)m_Attribution.task(kind,begin,CTimer::GetClockTicks());}
 void TracePhase(unsigned phase){m_TracePhase=phase;}
 void TraceWindow(unsigned stage,bool steady){m_TraceStage=stage;m_TraceSteady=steady;}
 void EndTrace(){m_TraceCapturing=false;m_TraceReady=m_Trace!=nullptr;groove::display_usb_progress.enabled(false);}
 void DrainTrace(){if(!m_TraceReady)return;unsigned now=CTimer::GetClockTicks();if(uint32_t(now-m_TraceDrained)<10000)return;m_TraceDrained=now;
  if(m_TraceRead<m_TraceCount){const auto& t=m_Trace[m_TraceRead++];CLogger::Get()->Write("lcd",LogNotice,"LCD packet %u at %u side %u phase %u stage %u steady %u bytes %u commands %u usb %u queue %u reap %u frame %u",m_TraceRead,t.submitted,unsigned(t.side),unsigned(t.phase),unsigned(t.stage),unsigned(t.steady),t.bytes,t.commands,t.usb,t.queue,t.reap,t.frame);CLogger::Get()->Write("lcd",LogNotice,"LCD detail %u call %u done %u job %u sequence %u predecessor %u audio %u gap %u accepted %u shadow %u",m_TraceRead,unsigned(t.done-t.usb),t.done,t.job,t.sequence,t.predecessor,t.audio_callbacks,t.audio_gap,t.accepted,t.shadow_end);}
  else if(auto* t=m_Attribution.next_task()){CLogger::Get()->Write("lcd",LogNotice,"Task span kind %u begin %u end %u",t->kind,t->begin,t->end);}
  else if(auto* j=m_Attribution.next_job()){++m_JobTraceRead;CLogger::Get()->Write("lcd",LogNotice,"LCD job %u frame %u side %u copy %u/%u submit %u begin %u end %u collect %u cpu %u/%u/%u bytes %u dirty %u attempt %u",m_JobTraceRead,j->serial,j->side,j->copy_begin,j->copy_end,j->submitted,j->began,j->ended,j->collected,j->scan,j->encode,j->coalesce,j->bytes,j->dirty,j->attempted);}
  else if(auto* f=m_Attribution.next_frame()){CLogger::Get()->Write("lcd",LogNotice,"LCD frame %u stage %u steady %u due %u gate %u begin %u drawn %u",f->serial,f->stage,f->steady,f->due,f->gate,f->begin,f->drawn);CLogger::Get()->Write("lcd",LogNotice,"LCD finish %u left %u/%u/%u right %u/%u/%u",f->serial,f->origin[0],f->completed[0],f->window[0],f->origin[1],f->completed[1],f->window[1]);}
  else{CLogger::Get()->Write("lcd",LogNotice,"LCD attribution completed frames %u jobs %u lost %u/%u tasks %u/%u",m_Attribution.frames(),m_Attribution.jobs(),m_Attribution.lost_frames(),m_Attribution.lost_jobs(),m_Attribution.tasks(),m_Attribution.lost_tasks());CLogger::Get()->Write("lcd",LogNotice,"LCD trace completed entries %u lost %u",m_TraceCount,m_TraceLost);m_TraceReady=false;}
 }
 struct TransportStats {unsigned usb_us=0,usb_bytes=0,reap_us=0,prepare_us=0,draw_us=0,bytes=0,packets=0;};
 const TransportStats& Transport()const{return m_Transport;}
 void BeginDraw(unsigned at,uint64_t due){m_DrawBegin=at;m_DrawDue=unsigned(due);}
 void RecordDraw(unsigned us){m_DrawEnd=CTimer::GetClockTicks();if(us>m_Transport.draw_us)m_Transport.draw_us=us;}
 struct Stats {unsigned submitted,completed,bytes,max_wait_us,max_usb_us,max_reap_us,worker_submitted,worker_completed;};
 Stats GetStats()const{return {m_Submitted,m_Completed,m_Bytes,m_MaxWaitUs,m_MaxUsbUs,m_MaxReapUs,m_WorkSubmitted,m_WorkCompleted};}
 // Boot-only full-screen writes. Keep this same backend/shadow for the app.
 bool BootSplash(CMk3Display* device){
  Bind(device);if(!device || !device->OutputReady())return false;
  for(unsigned side=0;side<2;++side){
   if(!m_Shadow[side])return false;
   if(!m_Packets[side])m_Packets[side]=(u8*)palloc();
   if(!m_Packets[side])return false;
   unsigned bytes=0;const u8* packet=groove::boot_splash_packet(side,bytes);if(bytes>groove::kDisplayPacketCapacity)return false;
   memcpy(m_Packets[side],packet,bytes);auto* transfer=new Mk3DisplayTransfer{};if(!transfer)return false;
   unsigned now=CTimer::GetClockTicks();if(!device->Submit(m_Packets[side],bytes,transfer)){delete transfer;return false;}
   ++m_Submitted;m_Bytes+=bytes;m_PendingBytes[side]=bytes;m_Pending[side]={transfer,nullptr,now,m_Epoch,0,0,0,272,0,480,true,false,true};
  }
  return true;
 }
 bool BootSplashComplete(){CollectCompletions();return !m_Pending[0].transfer && !m_Pending[1].transfer && m_Valid[0][0] && m_Valid[1][0] && !m_Failed[0] && !m_Failed[1];}
 // After static ACK, reuse the normal raw-rectangle/shadow ownership path.
 bool BootSplashVersion(uint32_t version){
  if(m_BootVersionSubmitted){CollectCompletions();return true;}
  if(m_FrameSerial || !BootSplashComplete() || !m_Device || !m_Device->OutputReady())return false;
  unsigned bytes=0;if(!groove::boot_splash_version_packet(version,m_Shadow[1],m_Packets[1],groove::kDisplayPacketCapacity,bytes))return false;
  auto* transfer=new Mk3DisplayTransfer{};if(!transfer)return false;
  unsigned now=CTimer::GetClockTicks();if(!m_Device->Submit(m_Packets[1],bytes,transfer)){delete transfer;return false;}
  const auto r=groove::kBootVersionRect;
  m_Pending[1]={transfer,nullptr,now,m_Epoch,0,unsigned(r.y/16),r.y,r.h,r.x,r.w,true,false};m_Pending[1].boot_version=true;
  ++m_Submitted;m_Bytes+=bytes;m_PendingBytes[1]=bytes;m_BootVersionSubmitted=true;
  char text[24];groove::boot_splash_version(version,text);CLogger::Get()->Write("lcd",LogNotice,"Boot splash version %s raw %u submitted",text,version);
  return true;
 }
 bool UseWorker(groove::DisplayJobSlot* worker){if(!worker || m_Worker)return false;for(auto& pixels:m_WorkPixels){pixels=new u32[480*272];if(!pixels)return false;}for(auto& page:m_NextPackets)page=(u8*)palloc();m_Worker=worker;return true;}
 void NewFrame(uint64_t release_at=0){for(unsigned side=0;side<2;++side){const auto& p=m_Pending[side];if(m_FrameSerial && m_FrameGate.needs_scan(side) && !(m_WorkActive[side] && m_WorkFrame[side]==m_FrameSerial) && !(p.transfer && p.last && p.frame==m_FrameSerial))++m_FrameStats.discarded[side];}++m_FrameSerial;m_ReleaseAt=release_at;m_FrameGate.new_frame();if(m_TraceCapturing)m_Attribution.frame({m_FrameSerial,m_TraceStage,unsigned(m_TraceSteady),m_DrawDue,m_LastReady,m_DrawBegin,m_DrawEnd,{},{},{}});}
 const groove::DisplayFrameStats& FrameStats()const{return m_FrameStats;}
 void ResetFrameStats(uint64_t now){m_FrameStats.reset(now);m_Transport={};}
 bool FrameComplete()const{return !m_Pending[0].transfer && !m_Pending[1].transfer && (!m_Worker || !m_Worker->busy()) && m_FrameGate.complete();}
 // Reap ACKs before deciding whether the next absolute30Hz slot may draw.
 void Poll(){Reap();CollectCompletions();}
 bool ReadyToPresent()const{return (m_Worker && m_Worker->ready()) || (CTimer::GetClockTicks64()>=m_ReleaseAt && ((m_Prepared[0].ready && !m_Pending[0].transfer) || (m_Prepared[1].ready && !m_Pending[1].transfer)));}
 void Invalidate(){m_FrameGate.invalidate();++m_Epoch;for(auto& prepared:m_Prepared)prepared={};for(auto& screen:m_Valid)for(auto& valid:screen)valid=false;}
 void SetPixelOrder(groove::ui::PixelOrder order){if(order!=m_Order){m_Order=order;Invalidate();}}
 void Bind(CMk3Display* device){
  Reap();if(device==m_Device)return;
  // The old function may already be destroyed. Only independent owners remain.
  m_Device=device;
  for(unsigned side=0;side<2;++side){auto& p=m_Pending[side];if(!p.transfer)continue;p.detached=true;if(__atomic_load_n(&p.transfer->state,__ATOMIC_ACQUIRE)){Mk3DisplayRelease(p.transfer);p={};}else Quarantine(side);}
  m_Failed[0]=m_Failed[1]=false;Invalidate();
 }
 bool present(unsigned side,const uint32_t* pixels,unsigned width,unsigned height)override{
  Reap();if(side>1 || width!=480 || height!=272)return false;
  CollectCompletions();
  if(m_Failed[side]){m_Failed[side]=false;return false;}
  if(!m_Device || !m_Device->OutputReady())return false;
  groove::PreparedDisplayRect rect{};unsigned prepared_frame=m_FrameSerial;bool next_packet=false;
  // Collect whichever LCD the worker prepared, independently of the caller.
  if(m_Worker && m_Worker->busy()){
   groove::DisplayJob done;if(!m_Worker->collect(done))return true;++m_WorkCompleted;unsigned prep=uint32_t(CTimer::GetClockTicks()-m_WorkStarted);if(prep>m_Transport.prepare_us)m_Transport.prepare_us=prep;
   if(done.epoch!=m_Epoch || done.frame!=m_WorkFrame[done.side])return true;
   rect=done.result;side=done.side;prepared_frame=done.frame;next_packet=done.packet==m_NextPackets[side];if(m_TraceCapturing)m_JobId[side]=m_Attribution.job({done.frame,side,m_CopyBegin[side],m_CopyEnd[side],m_WorkStarted,done.worker_begin,done.worker_end,CTimer::GetClockTicks(),done.scan_us,done.encode_us,done.coalesce_us,rect.bytes,unsigned(rect.dirty),done.coalesce_attempted});
   if((next_packet && m_Pending[side].transfer) || CTimer::GetClockTicks64()<m_ReleaseAt){m_Prepared[side]={rect,done.frame,done.epoch,m_JobId[side],true,next_packet};return true;}
  }else if(CTimer::GetClockTicks64()>=m_ReleaseAt && ((m_Prepared[0].ready && !m_Pending[0].transfer) || (m_Prepared[1].ready && !m_Pending[1].transfer))){
   side=m_Prepared[0].ready && !m_Pending[0].transfer?0:1;auto prepared=m_Prepared[side];m_Prepared[side]={};
   if(prepared.epoch!=m_Epoch)return true;
   rect=prepared.rect;prepared_frame=prepared.frame;m_JobId[side]=prepared.job;next_packet=prepared.next;
  }else{
   auto& pending=m_Pending[side];
   if(!m_FrameGate.needs_scan(side) || m_Prepared[side].ready)return true;
   if(pending.transfer){
    if(!m_Worker || pending.detached || pending.last || pending.epoch!=m_Epoch)return true;
    if(!m_NextPackets[side])return true;
    next_packet=true;
   }
   if(!m_Packets[side]){m_Packets[side]=(u8*)palloc();}if(!m_Packets[side])return false;
   if(m_Worker){
    if(!m_WorkActive[side] || m_WorkEpoch[side]!=m_Epoch){m_WorkActive[side]=true;if(m_TraceCapturing)m_CopyBegin[side]=CTimer::GetClockTicks();memcpy(m_WorkPixels[side],pixels,480*272*sizeof(u32));if(m_TraceCapturing)m_CopyEnd[side]=CTimer::GetClockTicks();m_WorkFrame[side]=m_FrameSerial;m_WorkEpoch[side]=m_Epoch;}
    groove::DisplayJob job;job.clock=m_TraceCapturing?TraceClock:nullptr;job.pixels=m_WorkPixels[side];job.max_rows=272;job.compact=true;job.coalesce=true;job.shadow=m_Shadow[side];job.packet=next_packet?m_NextPackets[side]:m_Packets[side];job.side=side;job.cursor=next_packet?(pending.first+pending.rows/16)%17:m_Cursor[side];job.epoch=m_Epoch;job.frame=m_WorkFrame[side];job.order=m_Order;
    job.acknowledged=m_Acknowledged[side];job.ack_first=m_AckFirst[side];job.ack_rows=m_AckRows[side];
    // This predicts only the private scan plan. Real shadow/valid/cursor and
    // USB ownership remain unchanged until the pending request succeeds.
    if(next_packet){++job.acknowledged;job.ack_first=pending.first;job.ack_rows=pending.rows;}
    for(unsigned i=0;i<17;++i)job.valid[i]=m_Valid[side][i];
    m_WorkStarted=CTimer::GetClockTicks();if(!m_Worker->submit(job)){return false;}++m_WorkSubmitted;return true;
   }
   rect=groove::prepare_display_rectangle(side,pixels,m_Shadow[side],m_Valid[side],m_Cursor[side],m_Order,m_Packets[side],64);
  }
  if(CTimer::GetClockTicks64()<m_ReleaseAt){m_Prepared[side]={rect,prepared_frame,m_Epoch,m_JobId[side],true,next_packet};return true;}
  if(!rect.valid){Invalidate();return false;}
  if(!rect.dirty){m_WorkActive[side]=false;MarkComplete(side,prepared_frame);if(prepared_frame==m_FrameSerial)m_FrameGate.scanned(side);NoteReady();return true;}
  if(next_packet){auto* page=m_Packets[side];m_Packets[side]=m_NextPackets[side];m_NextPackets[side]=page;}
  auto* transfer=new Mk3DisplayTransfer{};if(!transfer){Invalidate();return false;}
  unsigned started=CTimer::GetClockTicks();
  if(!m_Device->Submit(m_Packets[side],rect.bytes,transfer)){delete transfer;Invalidate();return false;}
  ++m_Submitted;m_Bytes+=rect.bytes;m_PendingBytes[side]=rect.bytes;
  m_Pending[side]={transfer,m_Worker?m_WorkPixels[side]:nullptr,started,m_Epoch,prepared_frame,rect.first,rect.y,rect.rows,rect.left,rect.width,rect.last,false};m_Pending[side].job=m_JobId[side];return true;
 }
private:
 struct Orphan {Mk3DisplayTransfer* transfer=nullptr;u8* page=nullptr;};
 void Reap(){for(auto& o:m_Orphans)if(o.transfer && __atomic_load_n(&o.transfer->state,__ATOMIC_ACQUIRE)){Mk3DisplayRelease(o.transfer);pfree(o.page);o={};}}
 bool Quarantine(unsigned side){for(auto& o:m_Orphans)if(!o.transfer){o={m_Pending[side].transfer,m_Packets[side]};m_Pending[side]={};m_Packets[side]=nullptr;return true;}return false;}
 void CollectCompletions(){
  for(unsigned side=0;side<2;++side){auto& p=m_Pending[side];if(!p.transfer)continue;
   unsigned state=__atomic_load_n(&p.transfer->state,__ATOMIC_ACQUIRE);
   if(!state){if(!p.detached && uint32_t(CTimer::GetClockTicks()-p.started)>=3000000u){if(m_Device)m_Device->FenceOutput();m_Failed[side]=true;p.detached=true;Quarantine(side);}continue;}
   unsigned now=CTimer::GetClockTicks(),wait=uint32_t(now-p.started),usb=uint32_t(p.transfer->completed_at-p.started),reap=uint32_t(now-p.transfer->completed_at);if(wait>m_MaxWaitUs)m_MaxWaitUs=wait;if(usb>m_MaxUsbUs)m_MaxUsbUs=usb;if(reap>m_MaxReapUs)m_MaxReapUs=reap;++m_Completed;
   unsigned trace_index=m_TraceCount;
   if(m_TraceCapturing){if(m_TraceCount<8192)m_Trace[m_TraceCount++]={p.transfer->submitted_at,usb,p.transfer->queue_us,reap,m_PendingBytes[side],p.transfer->commands,p.frame,uint8_t(side),uint8_t(m_TracePhase),uint8_t(m_TraceStage),uint8_t(m_TraceSteady),p.transfer->completed_at,p.job,p.transfer->sequence,p.transfer->predecessor,p.transfer->audio_callbacks,p.transfer->audio_gap,p.transfer->accepted_at,0};else ++m_TraceLost;}
   if(usb>m_Transport.usb_us){m_Transport.usb_us=usb;m_Transport.usb_bytes=m_PendingBytes[side];}if(reap>m_Transport.reap_us)m_Transport.reap_us=reap;m_Transport.bytes+=m_PendingBytes[side];++m_Transport.packets;
   if(!p.detached && state==1 && p.epoch==m_Epoch){
    if(p.splash){if(!groove::boot_splash_shadow(side,m_Shadow[side])){m_Failed[side]=true;Mk3DisplayRelease(p.transfer);p={};continue;}}
    else for(unsigned j=0;j<p.rows;++j)for(unsigned x=0;x<p.width;++x){unsigned i=20+(j*p.width+x)*2;m_Shadow[side][(p.y+j)*480+p.left+x]=p.pixels?groove::ui::pack565(p.pixels[(p.y+j)*480+p.left+x],m_Order):u16((unsigned(m_Packets[side][i])<<8)|m_Packets[side][i+1]);}
    for(unsigned row=0;row<p.rows/16;++row){m_Valid[side][p.first+row]=true;}m_Cursor[side]=(p.first+p.rows/16)%17;
    ++m_Acknowledged[side];m_AckFirst[side]=p.first;m_AckRows[side]=p.rows;
    if(p.boot_version)CLogger::Get()->Write("lcd",LogNotice,"Boot splash version ACK side %u bytes %u",side,m_PendingBytes[side]);
    if(p.last){m_WorkActive[side]=false;MarkComplete(side,p.frame);if(p.frame==m_FrameSerial)m_FrameGate.scanned(side);}
   }
   if(!p.detached && state!=1){m_Failed[side]=true;Invalidate();}
   if(m_TraceCapturing && trace_index<m_TraceCount)m_Trace[trace_index].shadow_end=CTimer::GetClockTicks();
   Mk3DisplayRelease(p.transfer);p={};
  }
  NoteReady();
 }
 static uint32_t TraceClock(){return CTimer::GetClockTicks();}
 void MarkComplete(unsigned side,unsigned frame){uint64_t now=CTimer::GetClockTicks64();if(m_TraceCapturing)m_Attribution.complete(frame,side,unsigned(m_FrameStats.last[side]),unsigned(now),(m_TraceStage<<1)|unsigned(m_TraceSteady));m_FrameStats.completed(side,now);}
 void NoteReady(){if(m_LastReadyFrame!=m_FrameSerial && FrameComplete()){m_LastReady=CTimer::GetClockTicks();m_LastReadyFrame=m_FrameSerial;}}
 groove::DisplayAttribution m_Attribution;uint64_t m_ReleaseAt=0;unsigned m_CopyBegin[2]{},m_CopyEnd[2]{},m_JobId[2]{},m_JobTraceRead=0,m_DrawBegin=0,m_DrawEnd=0,m_DrawDue=0,m_LastReady=0,m_LastReadyFrame=0;
 TraceEntry* m_Trace=nullptr;unsigned m_TraceCount=0,m_TraceRead=0,m_TraceLost=0,m_TraceDrained=0,m_TracePhase=0,m_TraceStage=0;bool m_TraceSteady=false,m_TraceCapturing=false,m_TraceReady=false;
 unsigned m_Acknowledged[2]{},m_AckFirst[2]{},m_AckRows[2]{};
 TransportStats m_Transport{};unsigned m_PendingBytes[2]{},m_WorkStarted=0;
 groove::DisplayFrameGate m_FrameGate;groove::DisplayJobSlot* m_Worker=nullptr;groove::DisplayFrameStats m_FrameStats;
 u32* m_WorkPixels[2]{};unsigned m_WorkFrame[2]{},m_WorkEpoch[2]{};bool m_WorkActive[2]{};unsigned m_FrameSerial=0,m_WorkSubmitted=0,m_WorkCompleted=0;
 unsigned m_Submitted=0,m_Completed=0,m_Bytes=0,m_MaxWaitUs=0,m_MaxUsbUs=0,m_MaxReapUs=0;
 CMk3Display* m_Device=nullptr;u8* m_Packets[2]{},*m_NextPackets[2]{};Prepared m_Prepared[2]{};u16* m_Shadow[2]{};bool m_Valid[2][17]{},m_Failed[2]{};unsigned m_Cursor[2]{};groove::ui::PixelOrder m_Order=groove::ui::PixelOrder::RGB;
 Pending m_Pending[2]{};Orphan m_Orphans[8]{};unsigned m_Epoch=0;bool m_BootVersionSubmitted=false;
};
