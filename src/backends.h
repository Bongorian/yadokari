#pragma once
#include <stddef.h>
#include <stdint.h>
namespace groove {
struct MidiPacket { uint64_t sample=0; uint8_t cable=0,length=0,bytes[3]{}; };
inline bool valid_midi_packet(const MidiPacket& packet) {
 uint8_t status=packet.bytes[0]; if(status>=0xf8)return packet.length==1 && (status==0xf8 || status==0xfa || status==0xfb || status==0xfc);
 if(status==0xf2)return packet.length==3 && packet.bytes[1]<128 && packet.bytes[2]<128;
 if(status<0x80 || status>=0xf0)return false;
 unsigned length=(status&0xf0)==0xc0 || (status&0xf0)==0xd0?2:3;
 return packet.length==length && packet.bytes[1]<128 && (length==2 || packet.bytes[2]<128);
}
struct ControllerInput { enum class Type:uint8_t { Pad, Knob, Mode, Strip, Shift, Play, Stop, Track, Pattern, Undo, Redo, Euclidean, StoreMorph, Fill, Stutter, Instrument, StepPage, SoftKey, MuteModifier, SoloModifier, ControlAll, WaveProfile, Panic, Control, Encoder, KnobTouch }; Type type; uint8_t index=0; int32_t value=0; uint16_t pressure=0; };
class IAudioBackend { public: virtual ~IAudioBackend(){} virtual bool start()=0; virtual void stop()=0; virtual unsigned sample_rate() const=0; };
class IMidiBackend { public: virtual ~IMidiBackend(){} virtual bool receive(MidiPacket& packet)=0; virtual bool send(const MidiPacket& packet)=0; };
class IControllerInput { public: virtual ~IControllerInput(){} virtual bool poll(ControllerInput& input)=0; };
// A true result accepts the latest state. Async implementations retain and retry
// it internally until a successful full USB acknowledgement.
class IControllerOutput { public: virtual ~IControllerOutput(){} virtual bool set_pad_leds(const uint8_t values[16])=0; virtual bool set_button_leds(const uint8_t values[62])=0; virtual bool set_pad_strip_leds(const uint8_t pads[16],const uint8_t strip[25])=0; };
class IDisplayBackend { public: virtual ~IDisplayBackend(){} virtual bool present(unsigned side,const uint32_t* argb,unsigned width,unsigned height)=0; };
class IStorageBackend { public: virtual ~IStorageBackend(){} virtual bool read(const char* name,size_t offset,void* out,size_t size)=0; virtual bool write_atomic(const char* name,const void* data,size_t size)=0; };
class IClockSource { public: virtual ~IClockSource(){} virtual uint64_t microseconds() const=0; };
}
