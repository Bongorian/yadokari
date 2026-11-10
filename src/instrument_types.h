#pragma once
#include <stdint.h>
namespace groove {
// Runtime dispatch tags only. Persistent engine identity is the descriptor GUID.
enum class Instrument : uint8_t { PolySaw, Sampler, DaisySynth, PlaitsMacro, FM, ExternalMidi, Acid, WaveSequence, ChipClock, TapeMicrosound, WestCoast, SpectralFreeze, DaisyAnalogDrums, DaisySyntheticDrums, DrumKit, Resonator, Texture, Empty, Wavetable, VirtualAnalog, Fm6, SoundFont, Additive, PhaseDist, Layer, Formant, Granular, Unknown };
}
