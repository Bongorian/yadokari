#pragma once
#include "../../src/backends.h"
#include "engine.h"
#include <vector>
#include <string>
#include <cstring>
namespace groove {
class MockClock : public IClockSource { public: uint64_t now=0; uint64_t microseconds() const override {return now;} };
class MockAudio : public IAudioBackend {
public: explicit MockAudio(Engine& engine):engine_(engine){} bool start() override {active_=true;return true;} void stop() override {active_=false;} unsigned sample_rate() const override{return 48000;}
 bool render(float* out,size_t frames) {if(!active_)return false;engine_.render(out,frames);return true;}
private: Engine& engine_;bool active_=false;
};
class VirtualMidi : public IMidiBackend {
public: std::vector<MidiPacket> input,output; size_t cursor=0;
 bool receive(MidiPacket& packet) override {if(cursor>=input.size())return false;packet=input[cursor++];return true;}
 bool send(const MidiPacket& packet) override {if(!packet.length || packet.length>3)return false;output.push_back(packet);return true;}
};
class MockController : public IControllerInput,public IControllerOutput {
public: std::vector<ControllerInput> events;size_t cursor=0;uint8_t leds[16]{},buttons[62]{},strip[25]{};bool fail_output=false;
 bool poll(ControllerInput& event) override {if(cursor>=events.size())return false;event=events[cursor++];return true;}
 bool set_pad_leds(const uint8_t values[16]) override {if(fail_output)return false;std::memcpy(leds,values,16);return true;}
 bool set_pad_strip_leds(const uint8_t pads[16],const uint8_t values[25])override{if(fail_output)return false;std::memcpy(leds,pads,16);std::memcpy(strip,values,25);return true;}
 bool set_button_leds(const uint8_t values[62]) override {if(fail_output)return false;std::memcpy(buttons,values,62);return true;}
};
class MockDisplay : public IDisplayBackend {
public: std::vector<uint32_t> pixels[2];
 bool present(unsigned side,const uint32_t* data,unsigned width,unsigned height) override {if(side>1 || width!=480 || height!=272 || !data)return false;pixels[side].assign(data,data+width*height);return true;}
};
class MockStorage : public IStorageBackend {
public: std::vector<uint8_t> bytes;std::string filename;bool fail_reads=false,fail_writes=false;
 bool read(const char* name,size_t offset,void* out,size_t size) override {if(fail_reads || filename!=name || offset>bytes.size() || size>bytes.size()-offset)return false;std::memcpy(out,bytes.data()+offset,size);return true;}
 bool write_atomic(const char* name,const void* data,size_t size) override {if(fail_writes)return false;const auto* p=static_cast<const uint8_t*>(data);std::vector<uint8_t> staged(p,p+size);bytes.swap(staged);filename=name;return true;}
};
}
