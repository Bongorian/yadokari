#pragma once
#include "theme.h"
namespace groove::ui {
// Plan A: live level belongs to its source; motion is reserved for held peaks.
// Preserve the existing 12-segment warning/overload boundaries.
inline uint32_t meter_segment_color(const Theme& theme,uint32_t source,unsigned segment){return segment>=11?theme.danger:segment>=9?theme.warn:source;}
inline uint32_t meter_level_color(const Theme& theme,uint32_t source,float amount){return amount>11.f/12?theme.danger:amount>9.f/12?theme.warn:source;}
inline uint32_t meter_peak_color(const Theme& theme,float held){return held>=1?theme.danger:theme.motion;}
}
