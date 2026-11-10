/* Minimal compatibility subset of DaisySP Utility/dsp.h.
 * Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet.
 * MIT: see third_party/daisysp/LICENSE. */
#pragma once
#include <math.h>

#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)
namespace daisysp {
// Bare-metal port: deterministic audio-owner RNG in place of libc rand().
static constexpr float kRandFrac=1.0f/2147483647.0f;
inline int DaisyDrumRandom(){static unsigned state=0x5a17b4e3u;state^=state<<13;state^=state>>17;state^=state<<5;return int(state&0x7fffffffu);}
static constexpr float kOneTwelfth=1.0f/12.0f;
inline void fonepole(float& out,float in,float coefficient){out+=(in-out)*coefficient;}
inline float fclamp(float x,float lo,float hi) { return x<lo?lo:(x>hi?hi:x); }
inline float fastmod1f(float x) { return x-floorf(x); }
// Upstream DaisySP SoftLimit/SoftClip, required by Effects/overdrive.cpp.
inline float SoftLimit(float x) { return x*(27.0f+x*x)/(27.0f+9.0f*x*x); }
inline float SoftClip(float x) { return x < -3.0f ? -1.0f : x > 3.0f ? 1.0f : SoftLimit(x); }
}
