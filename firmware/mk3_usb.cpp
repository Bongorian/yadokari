#include "mk3_usb.h"
#include "../src/usb_progress.h"
#include "../src/mk3_controls.h"
#include "../src/groove.h"
#include "../src/mk3_output.h"
#include "../src/display_wire_facts.h"
#include <circle/usb/xhcidevice.h>
#include <circle/usb/xhcimmiospace.h>
#include <circle/usb/xhci.h>
#include <circle/devicenameservice.h>
#include <circle/usb/usbdevice.h>
#include <circle/usb/usbhostcontroller.h>
#include <circle/usb/usbhid.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <circle/alloc.h>
#include <circle/timer.h>
static const char* kHid="mk3hid";
static const char* kDisplay="mk3display";
struct Mk3LedOwner {
 CUSBEndpoint* endpoint=nullptr;CUSBRequest* request=nullptr;u8* packet=nullptr;
 unsigned completed=0,expected=0;bool active=false;Mk3LedOwner* next=nullptr;
};
static Mk3LedOwner* s_RetiredLeds=nullptr;
static void FreeLedOwner(Mk3LedOwner* o){delete o->request;delete o->endpoint;pfree(o->packet);delete o;}
void GrooveReapUsbOutput(){auto** link=&s_RetiredLeds;while(*link){auto* o=*link;if(o->active && !__atomic_load_n(&o->completed,__ATOMIC_ACQUIRE)){link=&o->next;continue;}*link=o->next;FreeLedOwner(o);}}
CMk3Hid::~CMk3Hid() {
 if(m_Registered) CDeviceNameService::Get()->RemoveDevice(kHid,FALSE);
 if(m_Output){m_Output->next=s_RetiredLeds;s_RetiredLeds=m_Output;GrooveReapUsbOutput();}
 else if(m_LedBuffer && m_OutputGuard.ready())pfree(m_LedBuffer);
}
bool CMk3Hid::RestartInput(u32& resync){
 unsigned expected=1;if(!__atomic_compare_exchange_n(&m_PollState,&expected,2,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return false;
 ++m_InputRecoveryFailures;m_InputFailureProgress=InputProgress();
 bool recovered=RecoverInputEndpoint();if(recovered)resync=m_Events.request_resync();
 if(!recovered || !StartRequest()){
  __atomic_store_n(&m_PollState,1,__ATOMIC_RELEASE);return false;
 }
 expected=2;
 return __atomic_compare_exchange_n(&m_PollState,&expected,0,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
boolean CMk3Hid::Configure() {
 CUSBFunction probe(this); const TUSBEndpointDescriptor* endpoint;
 while((endpoint=(const TUSBEndpointDescriptor*)probe.GetDescriptor(DESCRIPTOR_ENDPOINT))!=nullptr) CLogger::Get()->Write("mk3",LogNotice,"HID ep 0x%02x attr 0x%02x packet %u interval %u",unsigned(endpoint->bEndpointAddress),unsigned(endpoint->bmAttributes),unsigned(endpoint->wMaxPacketSize),unsigned(endpoint->bInterval));
 CUSBFunction hid_probe(this);const auto* hid=(const TUSBHIDDescriptor*)hid_probe.GetDescriptor(DESCRIPTOR_HID);unsigned input_size=0;
 if(hid && hid->bLength>=9 && hid->bReportDescriptorType==DESCRIPTOR_REPORT && hid->wReportDescriptorLength && hid->wReportDescriptorLength<=4096){unsigned length=hid->wReportDescriptorLength;u8* descriptor=new u8[length];if(descriptor){if(GetHost()->GetDescriptor(GetEndpoint0(),DESCRIPTOR_REPORT,DESCRIPTOR_INDEX_DEFAULT,descriptor,length,REQUEST_IN|REQUEST_TO_INTERFACE,GetInterfaceNumber())==int(length)){unsigned pads=groove::hid_output_report_bytes(descriptor,length,0x81),buttons=groove::hid_output_report_bytes(descriptor,length,0x80);if(pads==42 || pads==81)m_PadReportBytes=pads;if(buttons==63)m_ButtonReportBytes=buttons;unsigned controls=groove::hid_input_report_bytes(descriptor,length,1),pad_input=groove::hid_input_report_bytes(descriptor,length,2);if((controls==32 || controls==42) && (pad_input==64 || pad_input==128)){m_ControlReportBytes=controls;m_PadInputReportBytes=pad_input;input_size=controls>pad_input?controls:pad_input;const unsigned extra_ids[]={0x20,0x40};for(unsigned id:extra_ids){unsigned other=groove::hid_input_report_bytes(descriptor,length,u8(id));if(other>128){input_size=0;break;}if(other>input_size)input_size=other;}}}delete[] descriptor;}}
 CLogger::Get()->Write("mk3",LogNotice,"HID report lengths: input %u pads %u buttons %u (0 disables output)",input_size,m_PadReportBytes,m_ButtonReportBytes);
 if(!input_size || !ConfigureHID(input_size)) return FALSE;
 m_LedBuffer=(u8*)palloc(); if(!m_LedBuffer) return FALSE;
 // The actual unit may not send an idle interrupt report. Seed state using a
 // read-only HID GET_REPORT so its first button press is not lost. No events
 // are emitted for controls already held during initialization.
 unsigned baseline_started=CTimer::GetClockTicks();int initial=GetHost()->ControlMessage(GetEndpoint0(),REQUEST_IN|REQUEST_CLASS|REQUEST_TO_INTERFACE,GET_REPORT,(REPORT_TYPE_INPUT<<8)|1,GetInterfaceNumber(),m_LedBuffer,m_ControlReportBytes);
 m_OutputGuard.completed(initial==int(m_ControlReportBytes),CTimer::GetClockTicks()-baseline_started);if(!m_OutputGuard.ready())CLogger::Get()->Write("mk3",LogWarning,"Input baseline timed out; reconnect MK3 before LED output");
 if(initial==int(m_ControlReportBytes) && groove::control_baseline(m_LedBuffer,unsigned(initial),m_LastRaw,m_Knobs,m_Encoder,m_Strip,m_LastButtons)){
  m_InputInitialized=true;
  for(unsigned i=0;i<10;++i)m_Events.push(Mk3Pad{Mk3EventType::ControlBaseline,u8(i+1),m_LastRaw[i],0});
 }
 else {if(initial<0)m_InputRecoveryFailures=3;CLogger::Get()->Write("mk3",LogWarning,"Input baseline unavailable; first interrupt report establishes state");}
 if(m_OutputGuard.ready()){
  m_Output=new Mk3LedOwner{};if(!m_Output)return FALSE;
  m_Output->endpoint=TakeOutputEndpoint();m_Output->packet=m_LedBuffer;
  if(!m_Output->endpoint)return FALSE;
  m_Output->request=new CUSBRequest(m_Output->endpoint,nullptr,0);if(!m_Output->request)return FALSE;
 }
 CDeviceNameService::Get()->AddDevice(kHid,this,FALSE); m_Registered=true;
 CLogger::Get()->Write("mk3",LogNotice,"HID interface %u configured",unsigned(GetInterfaceNumber()));
 return StartRequest();
}
void CMk3Hid::ReportHandler(const u8* data,unsigned size) {
 if(!data) return;
 if(!size || !groove::mk3_input_report_length_valid(data[0],size,m_ControlReportBytes,m_PadInputReportBytes)){m_Events.mark_loss();return;}
 __atomic_add_fetch(&m_PollProgress,1,__ATOMIC_RELEASE);
 const bool control=data[0]==1;bool resynced=false;
 // If the first report after a loss is a control snapshot, use that exact
 // snapshot as baseline rather than turning its held buttons into presses.
 if(!m_Events.prepare(control?data+1:m_LastRaw,resynced,control)){
  if(control && groove::control_baseline(data,size,m_LastRaw,m_Knobs,m_Encoder,m_Strip,m_LastButtons))m_InputInitialized=true;
  return;
 }
 if(resynced && control)m_InputInitialized=false;
 groove::DiagnosticEvent report;report.size=u16(size);report.timestamp=CTimer::GetClockTicks64();for(unsigned i=0;i<32 && i<size;++i)report.data[i]=data[i];m_Diagnostics.push(report);
 if((size==64 || size==128) && data[0]==2) {
  if(!groove::decode_pads(data,size,[this](groove::PadEvent e) {
   m_Events.push(Mk3Pad{Mk3EventType::Pad,e.pad,e.tag,e.pressure});
  }))m_Events.mark_loss();
 } else if(size>=32 && data[0]==1) {
  // encdr descriptor mapping; must be verified against physical reports.
  // Button edges, including transport, are handled once by Performance::control.
  // Forward all raw button/touch bytes, encoder counters and strip changes.
  for(unsigned i=0;i<10;++i) { u8 value=data[i+1]; if(!m_InputInitialized || value!=m_LastRaw[i]) m_Events.push(Mk3Pad{m_InputInitialized?Mk3EventType::ControlByte:Mk3EventType::ControlBaseline,u8(i+1),value,0}); m_LastRaw[i]=value; }
  for(unsigned i=0;i<8;++i) { u16 value=u16(data[12+2*i]) | u16(u16(data[13+2*i])<<8); if(m_InputInitialized && value!=m_Knobs[i]) { s16 delta; if(groove::mk3_rotary_delta(m_Knobs[i],value,delta)) { m_Events.push(Mk3Pad{Mk3EventType::Knob,u8(i),0,u16(s16(delta*32))}); } else __atomic_add_fetch(&m_InvalidKnobs,1,__ATOMIC_RELAXED); } m_Knobs[i]=value; }
  u8 enc=data[11]&15; if(m_InputInitialized && enc!=m_Encoder) m_Events.push(Mk3Pad{Mk3EventType::MainEncoder,0,0,u16(groove::mk3_main_delta(m_Encoder,enc))}); m_Encoder=enc;
  u16 strip=u16(data[30]) | u16(u16(data[31])<<8); if(m_InputInitialized && strip!=m_Strip) m_Events.push(Mk3Pad{Mk3EventType::Strip,0,0,strip}); m_Strip=strip; m_InputInitialized=true;
 }
}
bool CMk3Hid::SetPadLeds(const u8 pads[16]) {
 u8 strip[25]={};return set_pad_strip_leds(pads,strip);
}
bool CMk3Hid::set_pad_strip_leds(const uint8_t pads[16],const uint8_t strip[25]){
 if(!m_OutputGuard.ready() || !m_Output || !m_PadReportBytes)return false;
 u8 report[81]{};groove::pad_led_report(strip,pads,report);return m_Latest.update(0,report,m_PadReportBytes);
}
bool CMk3Hid::set_button_leds(const uint8_t values[62]) {
 if(!m_OutputGuard.ready() || !m_Output || !m_ButtonReportBytes)return false;
 u8 report[63];report[0]=0x80;for(unsigned i=0;i<62;++i)report[i+1]=values[i];return m_Latest.update(1,report,63);
}
void CMk3Hid::LedCompleted(CUSBRequest* request,void* parameter,void*){
 auto* o=static_cast<Mk3LedOwner*>(parameter);
 bool ok=request->GetStatus() && request->GetResultLength()==o->expected;
 __atomic_store_n(&o->completed,ok?1u:2u,__ATOMIC_RELEASE);
}
void CMk3Hid::ServiceOutput(){
 auto* o=m_Output;if(!o)return;
 unsigned now=CTimer::GetClockTicks();uint64_t ms=CTimer::GetClockTicks64()/1000;
 if(o->active){
  unsigned done=__atomic_load_n(&o->completed,__ATOMIC_ACQUIRE);
  if(!done){if(m_OutputGuard.ready() && uint32_t(now-m_LedStarted)>=3000000u){m_OutputGuard.completed(false,3000000u);CLogger::Get()->Write("mk3",LogWarning,"LED completion timed out; buffer retained until reconnect");}return;}
  o->active=false;m_Latest.complete(done==1);
  if(done==1)m_LedRetry.reset();else{m_LedRetry.failed(ms);CLogger::Get()->Write("mk3",LogWarning,"LED write failed; latest state retained");}
 }
 if(!m_OutputGuard.ready() || !m_LedRetry.ready(ms))return;
 unsigned kind=0,size=0;if(!m_Latest.prepare(o->packet,kind,size))return;
 o->expected=size;__atomic_store_n(&o->completed,0u,__ATOMIC_RELAXED);o->active=true;m_LedStarted=now;
 o->request->Reuse(o->packet,size);o->request->SetCompletionRoutine(LedCompleted,o,nullptr);
 if(!GetHost()->SubmitAsyncRequest(o->request)){o->active=false;m_Latest.complete(false);m_LedRetry.failed(ms);}
}
static void FreeDisplayOwner(Mk3DisplayOwner* owner){for(auto& slot:owner->requests)delete slot.request;delete owner->endpoint;delete owner;}
void Mk3DisplayRelease(Mk3DisplayTransfer* transfer){auto* owner=transfer->owner;if(owner){for(auto& slot:owner->requests)if(slot.active==transfer)slot.active=nullptr;if(owner->detached && !owner->busy())FreeDisplayOwner(owner);}delete transfer;}
CMk3Display::~CMk3Display() {
 if(m_Registered) CDeviceNameService::Get()->RemoveDevice(kDisplay,FALSE);
 // A pending request still references this endpoint. Keep it until its
 // completion context is reclaimed, even after the function is removed.
 if(m_Lifetime && m_Lifetime->busy())m_Lifetime->detached=true;
 else if(m_Lifetime)FreeDisplayOwner(m_Lifetime);else delete m_Out;
 m_Out=nullptr;m_Lifetime=nullptr;
}
boolean CMk3Display::Configure() {
 const TUSBEndpointDescriptor* desc;
 while((desc=(const TUSBEndpointDescriptor*)GetDescriptor(DESCRIPTOR_ENDPOINT))!=nullptr) {
  CLogger::Get()->Write("mk3",LogNotice,"Display ep 0x%02x attr 0x%02x packet %u",unsigned(desc->bEndpointAddress),unsigned(desc->bmAttributes),unsigned(desc->wMaxPacketSize));
  if((desc->bmAttributes&3)==2 && !(desc->bEndpointAddress&0x80)) {
   if(m_Out) return FALSE;
   m_Out=new CUSBEndpoint(GetDevice(),desc);
  }
 }
 if(!m_Out || !CUSBFunction::Configure()) return FALSE;
 auto* controller=static_cast<CXHCIDevice*>(GetHost());
 CLogger::Get()->Write("mk3",LogNotice,"Display completion IMOD register %u / shared endpoint FIFO",unsigned(controller->GetMMIOSpace()->rt_read32(0,XHCI_REG_RT_IR_IMOD)&XHCI_REG_RT_IR_IMOD_IMODI__MASK));
 m_Lifetime=new Mk3DisplayOwner{};if(!m_Lifetime)return FALSE;m_Lifetime->endpoint=m_Out;
 for(auto& slot:m_Lifetime->requests){slot.request=new CUSBRequest(m_Out,nullptr,0);if(!slot.request)return FALSE;}
 CDeviceNameService::Get()->AddDevice(kDisplay,this,FALSE); m_Registered=true;
 CLogger::Get()->Write("mk3",LogNotice,"Display bulk OUT %u configured",unsigned(m_Out->GetNumber()));
 return TRUE;
}
void CMk3Display::Completed(CUSBRequest* request,void* parameter,void*) {
 auto* transfer=static_cast<Mk3DisplayTransfer*>(parameter);
 const bool ok=request->GetStatus() && request->GetResultLength()==transfer->expected;
 transfer->completed_at=CTimer::GetClockTicks();{auto p=groove::display_usb_progress.close(transfer->progress_token,transfer->completed_at);transfer->audio_callbacks=p.callbacks;transfer->audio_gap=p.max_gap;}
 auto* owner=transfer->owner;
 if(owner){if(transfer->predecessor && owner->last_sequence==transfer->predecessor && int32_t(owner->last_completed_at-transfer->submitted_at)>0)transfer->queue_us=uint32_t(owner->last_completed_at-transfer->submitted_at);owner->last_sequence=transfer->sequence;owner->last_completed_at=transfer->completed_at;}
 __atomic_store_n(&transfer->state,ok?1u:2u,__ATOMIC_RELEASE);
}
bool CMk3Display::Submit(const void* data,unsigned size,Mk3DisplayTransfer* transfer) {
 if(!OutputReady() || !m_Lifetime || !data || !size || !transfer)return false;
 for(auto& slot:m_Lifetime->requests){
  if(slot.active)continue;
  transfer->owner=m_Lifetime;transfer->expected=size;transfer->sequence=++m_Lifetime->submitted;
  const auto facts=groove::display_wire_facts(static_cast<const uint8_t*>(data),size);transfer->commands=facts.valid?facts.commands:0;
  for(const auto& previous:m_Lifetime->requests)if(previous.active && !__atomic_load_n(&previous.active->state,__ATOMIC_ACQUIRE))transfer->predecessor=previous.active->sequence;
  transfer->submitted_at=CTimer::GetClockTicks();transfer->progress_token=groove::display_usb_progress.open(transfer->submitted_at);__atomic_store_n(&transfer->state,0u,__ATOMIC_RELAXED);
  slot.request->Reuse(const_cast<void*>(data),size);slot.request->SetCompletionRoutine(Completed,transfer,nullptr);
  slot.active=transfer;
  if(GetHost()->SubmitAsyncRequest(slot.request)){transfer->accepted_at=CTimer::GetClockTicks();return true;}
  groove::display_usb_progress.close(transfer->progress_token,CTimer::GetClockTicks());slot.active=nullptr;transfer->owner=nullptr;return false;
 }
 return false;
}
extern "C" CUSBFunction* GrooveCreateUSBFunction(CUSBFunction* parent) {
 const TUSBDeviceDescriptor* d=parent->GetDevice()->GetDeviceDescriptor();
 if(!d || d->idVendor!=0x17cc || d->idProduct!=0x1600) return nullptr;
 const auto* config=parent->GetDevice()->GetConfigurationDescriptor();
 const auto* interface=parent->GetInterfaceDescriptor();
 if(!config || !interface)return nullptr;
 const auto role=groove::mk3_interface_role(reinterpret_cast<const u8*>(config),config->wTotalLength,interface->bInterfaceNumber,interface->bAlternateSetting);
 if(role==groove::Mk3InterfaceRole::Hid)return new CMk3Hid(parent);
 if(role==groove::Mk3InterfaceRole::Display)return new CMk3Display(parent);
 return nullptr;
}

void CMk3Hid::Describe(groove::HardwareDiagnostics& state){const auto* d=GetDevice()->GetDeviceDescriptor();const auto* c=GetDevice()->GetConfigurationDescriptor();if(d && c)state.descriptors(d->idVendor,d->idProduct,reinterpret_cast<const u8*>(c),c->wTotalLength);}
