#include "mk3_midi.h"
#include "mk3_usb.h"
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/timer.h>
void CMk3Midi::Scan(CMk3Hid* control) {
 if(m_Device || !control) return;
 const auto* usb_device=control->GetDevice();
 if(!usb_device)return;
 for(unsigned n=1;n<=16;++n) {
  CString name; name.Format("umidi%u",n);
  auto* candidate=static_cast<CUSBMIDIDevice*>(CDeviceNameService::Get()->GetDevice(name,FALSE));
  if(!candidate) continue;
  if(candidate->GetUSBDevice()!=usb_device)continue;
  // Callback contexts are never recycled during a boot: a delayed callback
  // cannot acquire a later binding's identity, even if Circle reuses a number.
  if(m_BindingsUsed>=kMaxBindings){if(!m_BindingsExhausted)CLogger::Get()->Write("mk3",LogWarning,"MIDI binding limit reached; restart required");m_BindingsExhausted=true;return;}
  Binding* binding=&m_Bindings[m_BindingsUsed++];binding->owner=this;binding->generation=m_BindingsUsed;
  m_Fence=m_Events.consumer_boundary();m_FenceActive=true;m_DeviceNumber=n;
  __atomic_store_n(&m_ActiveGeneration,binding->generation,__ATOMIC_RELEASE);
  m_OutputGuard.reset();m_Device=candidate; m_Device->RegisterRemovedHandler(Removed,binding);
  m_Device->RegisterPacketHandler(Packet,binding);
  CLogger::Get()->Write("mk3",LogNotice,"MIDI attached %s; output USB cable 0",(const char*)name); break;
 }
}
bool CMk3Midi::Pop(Mk3MidiEvent& event) {
 while(m_FenceActive && !m_Events.consumer_at_boundary(m_Fence)) {if(!m_Events.pop(event))return false;}
 m_FenceActive=false;
 while(m_Events.pop(event)){if(m_Device && event.device==m_DeviceNumber && event.generation==__atomic_load_n(&m_ActiveGeneration,__ATOMIC_ACQUIRE))return true;}
 return false;
}
bool CMk3Midi::Matches(CMk3Hid* control)const {return control && m_Device && m_Device->GetUSBDevice()==control->GetDevice();}
void CMk3Midi::Packet(unsigned cable,u8* bytes,unsigned length,unsigned device,void* context) {
 if(!context || !bytes || length<1 || length>3 || cable>15) return;
 auto* binding=static_cast<Binding*>(context);auto* self=binding->owner;if(!self || binding->generation!=__atomic_load_n(&self->m_ActiveGeneration,__ATOMIC_ACQUIRE))return; Mk3MidiEvent e{}; e.cable=u8(cable); e.length=u8(length); e.device=device;e.generation=binding->generation;
 for(unsigned i=0;i<length;++i) e.bytes[i]=bytes[i];
 if(!self->m_Events.push(e)) __atomic_add_fetch(&self->m_Dropped,1,__ATOMIC_RELAXED);
}
void CMk3Midi::Removed(CDevice* device,void* context) {
 auto* binding=static_cast<Binding*>(context);auto* self=binding?binding->owner:nullptr; if(self && binding->generation==__atomic_load_n(&self->m_ActiveGeneration,__ATOMIC_ACQUIRE) && self->m_Device==device) self->m_Device=nullptr;
}
bool CMk3Midi::Send(unsigned cable,const u8* bytes,unsigned length) {
 if(!m_OutputGuard.ready() || !m_Device || cable>=16 || !bytes || !length)return false;
 unsigned start=CTimer::GetClockTicks();bool ok;
 if(length==1 && bytes[0]>=0xf8){
  // USB MIDI 1.0 Table 4-2: realtime uses CIN F, not CIN 5 (system common).
  // The pinned Circle SendPlainMIDI groups realtime with Tune Request as CIN 5.
  // Keep its parser for other messages; transmit realtime with the proper header.
  const u8 event[4]={u8((cable<<4)|0x0f),bytes[0],0,0};
  ok=m_Device->SendEventPackets(event,sizeof event);
 }else ok=m_Device->SendPlainMIDI(cable,bytes,length);
 m_OutputGuard.completed(ok,CTimer::GetClockTicks()-start);
 if(!m_OutputGuard.ready())CLogger::Get()->Write("mk3",LogWarning,"MIDI output timed out; reconnect MK3 before retry");
 return ok;
}
