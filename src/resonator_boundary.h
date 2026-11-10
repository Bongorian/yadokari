#pragma once
#include "rings/resources.h"
#include "stmlib/dsp/dsp.h"
namespace groove {
// The original 257-entry table has no interpolation neighbour at index256.
// Only its formerly undefined upper endpoint changes; all lower values use
// the original interpolation, including its operation order.
inline float resonator_damping_lookup(float damping){
 return damping>=1.0f?rings::lut_4_decades[256]:stmlib::Interpolate(rings::lut_4_decades,damping,256.0f);
}
}
