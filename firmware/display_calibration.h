#pragma once
#include "circle_display.h"
#include "../src/display_probe_packet.h"
// Read-only visual calibration: every sent pixel is already on its LCD.
// Separate from fixed UITEST2; never changes songs, assets, or frame counters.
class CDisplayCalibration {
 unsigned state_=0,variant_=0,repetition_=0,width_=128,began_=0,case_at_=0,mask_=0;
 unsigned x_[2]{},y_[2]{};uint16_t color_[2]{};u8* packet_[2]{};Mk3DisplayTransfer* transfer_[2]{};CMk3Display* device_=nullptr;bool ok_=false;
 void release_pages(){if(!transfer_[0] && !transfer_[1])for(auto& p:packet_)if(p){pfree(p);p=nullptr;}}
 void finish(bool ok){release_pages();ok_=ok;state_=3;CLogger::Get()->Write("lcd",ok?LogNotice:LogWarning,"LCD calibration completed ok %u cases %u",unsigned(ok),variant_*3+repetition_);}
public:
 bool busy()const{return state_==1 || state_==2;}
 bool owns_output()const{return state_==2;}
 bool ready()const{return state_==3;}
 bool result()const{return ok_;}
 bool start(){if(busy() || transfer_[0] || transfer_[1])return false;state_=1;variant_=repetition_=0;mask_=0;ok_=false;began_=CTimer::GetClockTicks();return true;}
 void service(CircleDisplay& display,CMk3Display* current){
  if(!busy()){if(state_==3){for(auto& t:transfer_)if(t && __atomic_load_n(&t->state,__ATOMIC_ACQUIRE)){Mk3DisplayRelease(t);t=nullptr;}release_pages();}return;}
  if(state_==1){if(!current || !display.FrameComplete()){if(uint32_t(CTimer::GetClockTicks()-began_)>1000000)finish(false);return;}
   bool found=false;for(width_=128;width_>=4;width_/=2)if(display.UniformPatch(0,width_,x_[0],y_[0],color_[0]) && display.UniformPatch(1,width_,x_[1],y_[1],color_[1])){found=true;break;}
   if(!found){finish(false);return;}for(auto& p:packet_)if(!p)p=(u8*)palloc();if(!packet_[0] || !packet_[1]){finish(false);return;}
   device_=current;state_=2;CLogger::Get()->Write("lcd",LogNotice,"LCD calibration begin width %u rows 16 variants 8 repeats 3 / unchanged pixels",width_);
  }
  if(current!=device_ || !device_->OutputReady()){finish(false);return;}
  if(!mask_){
   unsigned mode=variant_<6?variant_%3:1;mask_=variant_<3?1:variant_<6?2:3;case_at_=CTimer::GetClockTicks();
   for(unsigned n=0;n<2;++n){unsigned side=variant_==7?1-n:n;if(!(mask_&(1u<<side)))continue;
    unsigned bytes=groove::display_probe_packet(side,x_[side],y_[side],width_,color_[side],mode,packet_[side]);transfer_[side]=new Mk3DisplayTransfer{};
    if(!bytes || !transfer_[side] || !device_->Submit(packet_[side],bytes,transfer_[side])){delete transfer_[side];transfer_[side]=nullptr;finish(false);return;}
   }
  }
  for(unsigned side=0;side<2;++side)if(transfer_[side] && !__atomic_load_n(&transfer_[side]->state,__ATOMIC_ACQUIRE)){if(uint32_t(CTimer::GetClockTicks()-case_at_)>250000)finish(false);return;}
  bool success=true;for(unsigned side=0;side<2;++side)if(transfer_[side]){const auto& t=*transfer_[side];success=success && t.state==1;CLogger::Get()->Write("lcd",LogNotice,"LCD calibration case %u repeat %u side %u bytes %u commands %u usb %u queue %u at %u done %u accepted %u audio %u gap %u",variant_,repetition_,side,t.expected,t.commands,uint32_t(t.completed_at-t.submitted_at),t.queue_us,t.submitted_at,t.completed_at,t.accepted_at,t.audio_callbacks,t.audio_gap);Mk3DisplayRelease(transfer_[side]);transfer_[side]=nullptr;}
  mask_=0;if(!success){finish(false);return;}if(++repetition_==3){repetition_=0;++variant_;}if(variant_==8)finish(true);
 }
};
