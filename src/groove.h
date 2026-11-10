#pragma once
#include <stdint.h>
#include <stddef.h>

namespace groove {
struct PadEvent { uint8_t pad, tag; uint16_t pressure; };
// Reports consist of one or two 64-byte sets. A callback receives events in wire order.
template<class F> bool decode_pads(const uint8_t* p, size_t n, F emit) {
    if (!p || (n != 64 && n != 128)) return false;
    // Validate the complete report before emitting any event: malformed later
    // sets must not leave partially applied input state.
    for (size_t base=0; base<n; base+=64) {
        if (p[base] != 2) return false;
        for (size_t i=1; i+2<64; i+=3) {
            uint8_t pad=p[base+i], a=p[base+i+1], b=p[base+i+2];
            if (!(pad|a|b)) continue;
            if (pad >= 16) return false;
        }
    }
    for (size_t base=0; base<n; base+=64) {
        for (size_t i=1; i+2<64; i+=3) {
            uint8_t pad=p[base+i], a=p[base+i+1], b=p[base+i+2];
            if (!(pad|a|b)) continue;
            emit(PadEvent{pad, uint8_t(a&0xf0), uint16_t(((a&15)<<8)|b)});
        }
    }
    return true;
}
struct Controls { uint8_t buttons[8]; uint16_t touch; uint8_t main_encoder; int16_t knobs[8]; uint16_t strip; };
inline bool decode_controls(const uint8_t* p, size_t n, Controls& c) {
    if (!p || (n != 32 && n != 42) || p[0] != 1) return false;
    for (unsigned i=0;i<8;++i) c.buttons[i]=p[i+1];
    c.touch=uint16_t(p[9]|(uint16_t(p[10])<<8)); c.main_encoder=p[11]&15;
    for (unsigned i=0;i<8;++i) c.knobs[i]=int16_t(uint16_t(p[12+2*i]) | (uint16_t(p[13+2*i])<<8));
    c.strip=uint16_t(p[30]|(uint16_t(p[31])<<8)); return true;
}
// Capture a baseline without generating button or encoder events. Used for a
// read-only GET_REPORT before interrupt polling and on a first-report fallback.
inline bool control_baseline(const uint8_t* p,size_t n,uint8_t raw[10],uint16_t knobs[8],uint8_t& encoder,uint16_t& strip,uint8_t buttons[2]){
 Controls controls{};if(!raw || !knobs || !buttons || !decode_controls(p,n,controls))return false;
 for(unsigned i=0;i<10;++i)raw[i]=p[i+1];
 for(unsigned i=0;i<8;++i)knobs[i]=uint16_t(controls.knobs[i]);
 encoder=controls.main_encoder;strip=controls.strip;buttons[0]=p[6];buttons[1]=p[4];return true;
}
// Fixed point clock. Rendering advances one sample at a time, preserving phase over blocks.
class Sequencer {
public:
    explicit Sequencer(uint32_t rate=48000): rate_(rate), phase_(0), step_(15), bpm_(120), running_(false) { for (auto &p: pattern_) p=0; }
    void set_bpm(unsigned bpm) { if (bpm>=20 && bpm<=300) bpm_=bpm; }
    void set_step(unsigned track,unsigned step,bool on) { if(track<16 && step<16) { uint16_t bit=uint16_t(1u<<step); if(on) pattern_[track]|=bit; else pattern_[track]&=uint16_t(~bit); } }
    void start() { phase_=0; step_=15; running_=true; }
    void stop() { running_=false; }
    uint16_t tick() { if(!running_) return 0; phase_+=uint64_t(bpm_)*4; if(phase_<uint64_t(rate_)*60) return 0; phase_-=uint64_t(rate_)*60; step_=(step_+1)&15; uint16_t hits=0; for(unsigned t=0;t<16;++t) if(pattern_[t]&(1u<<step_)) hits|=uint16_t(1u<<t); return hits; }
private: uint32_t rate_; uint64_t phase_; unsigned step_, bpm_; bool running_; uint16_t pattern_[16];
};
// Small fixed-memory drum voice: decaying sine via a stable resonator, no allocation or I/O.
class Drum {
public:
    Drum(): y1_(0),y2_(0),gain_(0) {}
    void trigger(float velocity) { if(velocity<0) velocity=0; if(velocity>1) velocity=1; y1_=velocity; y2_=0; gain_=1; }
    float tick() { float y=1.985f*y1_-0.994f*y2_; y2_=y1_; y1_=y; gain_*=0.9994f; if(gain_<0.00001f) { y1_=y2_=0; return 0; } return y*gain_*0.08f; }
private: float y1_,y2_,gain_;
};
}
