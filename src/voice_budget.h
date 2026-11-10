#pragma once
namespace groove {
// Wave 10: 24 frames/500 us, 40% reserved for routing/FX/control. These are
// Layer admission units, not a CPU guarantee for ordinary voices or all patches.
struct VoiceBudget {
 static constexpr unsigned layer_note=12,layer_track=24,layer_total=48,pool_slots=32;
 static constexpr unsigned deadline_us=500,voice_us=300,reserved_us=200;
 static constexpr float unit_us=float(voice_us)/layer_total;
};
}
