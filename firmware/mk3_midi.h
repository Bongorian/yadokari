#pragma once
#include <circle/usb/usbmidi.h>
#include <circle/device.h>
#include "../src/spsc_queue.h"
#include "../src/backends.h"
#include "../src/sync_output_guard.h"
struct Mk3MidiEvent { u8 cable, length, bytes[3]; unsigned device, generation; };
class CMk3Hid;
class CMk3Midi {
public:
 void Scan(CMk3Hid* mk3_control);
 bool Pop(Mk3MidiEvent& event);
 unsigned Dropped() const { return __atomic_load_n(&m_Dropped,__ATOMIC_RELAXED); }
 bool Connected()const{return m_Device!=nullptr;}
 bool Matches(CMk3Hid* control)const;
 bool output_blocked()const{return !m_OutputGuard.ready();}
 bool binding_exhausted()const{return m_BindingsExhausted;}
 // Intended for task context only; cable-to-DIN mapping requires hardware verification.
 bool Send(unsigned cable,const u8* bytes,unsigned length);
private:
 struct Binding {CMk3Midi* owner=nullptr;unsigned generation=0;};
 static constexpr unsigned kMaxBindings=256;
 static void Packet(unsigned cable,u8* bytes,unsigned length,unsigned device,void* context);
 static void Removed(CDevice* device,void* context);
 CUSBMIDIDevice* m_Device=nullptr;groove::SyncOutputGuard m_OutputGuard;
 groove::SpscQueue<Mk3MidiEvent,128> m_Events; unsigned m_Dropped=0,m_DeviceNumber=0,m_Fence=0,m_ActiveGeneration=0,m_BindingsUsed=0;bool m_FenceActive=false,m_BindingsExhausted=false;Binding m_Bindings[kMaxBindings]{};
};
