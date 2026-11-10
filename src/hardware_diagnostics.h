#pragma once
#include "usb_descriptors.h"
#include "spsc_queue.h"
namespace groove {
enum class DiagnosticKind:uint8_t {Hid,MidiIn,MidiOut,TransferError};
struct DiagnosticEvent {DiagnosticKind kind=DiagnosticKind::Hid;uint16_t size=0;uint64_t timestamp=0;uint8_t data[32]{};};
enum class MidiDiagnosticStatus:uint8_t {Unknown,ControllerMissing,InterfaceMissing,OtherController,Ready,OutputBlocked,BindingLimit};
inline MidiDiagnosticStatus midi_diagnostic_status(bool controller,bool attached,bool matches,bool blocked,bool exhausted){
 if(exhausted)return MidiDiagnosticStatus::BindingLimit;
 if(!controller)return MidiDiagnosticStatus::ControllerMissing;
 if(!attached)return MidiDiagnosticStatus::InterfaceMissing;
 if(!matches)return MidiDiagnosticStatus::OtherController;
 if(blocked)return MidiDiagnosticStatus::OutputBlocked;
 return MidiDiagnosticStatus::Ready;
}
struct HardwareDiagnostics {
 DiagnosticEvent log[16]{};unsigned log_cursor=0,log_count=0;
 MidiDiagnosticStatus midi_status=MidiDiagnosticStatus::Unknown;unsigned midi_dropped=0;
 bool connected=false,enumerated=false,audio_active=false,audio_drain_stalled=false,input_resync=false;uint16_t vendor=0,product=0;Endpoint endpoints[32]{};unsigned endpoint_count=0,reports=0,midi_in=0,midi_out=0,transfer_errors=0,audio_tx=0,audio_rx=0,tx_channels=0,rx_channels=0;uint8_t last_hid[32]{};unsigned last_hid_size=0;uint8_t midi[3]{};float input_level=0;uint32_t invalid_reports=0;
 unsigned async_out=0,explicit_feedback=0,async_without_feedback=0,capture_endpoints=0,implicit_capture_endpoints=0;bool endpoints_truncated=false;
 void disconnected(){connected=enumerated=false;endpoint_count=async_out=explicit_feedback=async_without_feedback=capture_endpoints=implicit_capture_endpoints=0;endpoints_truncated=false;vendor=product=0;}
 bool descriptors(uint16_t vid,uint16_t pid,const uint8_t* config,unsigned size){
  vendor=vid;product=pid;endpoint_count=async_out=explicit_feedback=async_without_feedback=capture_endpoints=implicit_capture_endpoints=0;endpoints_truncated=false;
  // Validate the complete byte stream before publishing any endpoint from it.
  enumerated=visit_endpoints(config,size,[](Endpoint){});connected=enumerated;if(!enumerated)return false;
  visit_endpoints(config,size,[&](Endpoint e){if(endpoint_count<32)endpoints[endpoint_count++]=e;else endpoints_truncated=true;});
  for(unsigned i=0;i<endpoint_count;++i){const auto& e=endpoints[i];
   if(e.interface_class!=1 || e.interface_subclass!=2 || (e.attributes&3)!=1)continue;
   if((e.address&0x80) && ((e.attributes&0x30)==0 || (e.attributes&0x30)==0x20)){++capture_endpoints;if((e.attributes&0x30)==0x20)++implicit_capture_endpoints;}
   if((e.address&0x80) || (e.attributes&0x30)!=0 || (e.attributes&0x0c)!=0x04)continue;
   ++async_out;bool feedback=false;
   for(unsigned j=0;j<endpoint_count;++j){const auto& f=endpoints[j];if(f.interface_number==e.interface_number && f.alternate==e.alternate && (f.address&0x80) && (f.attributes&0x3f)==0x11)feedback=true;}
   if(feedback)++explicit_feedback;else ++async_without_feedback;
  }
  return true;
 }
 void accept(const DiagnosticEvent& e){log[log_cursor]=e;log_cursor=(log_cursor+1)%16;if(log_count<16)++log_count;if(e.kind==DiagnosticKind::Hid){++reports;last_hid_size=e.size<32?e.size:32;for(unsigned i=0;i<last_hid_size;++i)last_hid[i]=e.data[i];if(!e.size || (e.data[0]!=1 && e.data[0]!=2))++invalid_reports;}else if(e.kind==DiagnosticKind::MidiIn){++midi_in;for(unsigned i=0;i<3;++i)midi[i]=e.data[i];}else if(e.kind==DiagnosticKind::MidiOut)++midi_out;else ++transfer_errors;}
};
}
