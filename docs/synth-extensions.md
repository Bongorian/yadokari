# Synth Extensions

Engine extensions beyond the base engine set: the FORMANT/VOSIM and GRANULAR sources, the VA DUAL additions, the PITCH SHIFT and ROTARY insert effects, and the optional preset root FX. State is stored in the [project format](project-format.md) — VATH v2 for the VA, GRAN v1 for GRANULAR, PFXS v1 for root FX. Files in older formats are rejected with a clear error; there is no migration.

## Operation scope

SOURCE provides FORMANT (MODE = FORMANT / VOSIM) and GRANULAR, edited under GENERATOR / ENVELOPE / TONE. The VA's conventional performance knob assignments stay in place; the additions live in the detail sections. INCLUDE FX in SAVE PRESET AS / OVERWRITE PRESET starts OFF; only when ON does it bundle 2 Inserts plus the Delay/Reverb Sends. Pool registrations and Layer children do not carry FX. PITCH SHIFT's MIX is the existing WET, with two operations: SHIFT / MIX. ROTARY has SPEED / DRIVE plus the common WET.

## E1: VA DUAL

The 26 values of `mk3.va-dual` are stored together in VATH v2. The 8 added values are in the detail sections; the existing 5 performance knobs and the ADSR are kept. Only during SYNC / CROSS MOD does the signal path branch to an in-house polyBLEP path at 4× the internal rate, with saw/pulse edge correction, triangle integration and a step correction on sync reset. By default, the signal goes through the existing Daisy Oscillator/Ladder computation path unchanged. The pitch envelope is an exponential decay shared by both oscillators (amount ±48 st, time constant 5 ms–10 s). SUB SQR uses the existing Daisy polyBLEP rectangle. The SVF provides LP/HP/BP, and MORPH continuously interpolates LP → Notch → HP. The SVF/MORPH drive is an input SoftClip; the Ladder keeps the conventional InputDrive. The pitch decay is also kept from being confused with DECAY on the ADSR display.

With the added values at their defaults, the output is float-exact identical to the base voice. Capacity: cost 4, up to 8 voices, with an admission ceiling of 32 units.

## E2: GRANULAR

The existing Clouds is used from an independent GranularDsp adapter; the TEXTURE source, buffer and processing are unchanged. Four dedicated resources are allocated at Engine initialization, and resource IDs are reserved from the generic voice allocator. Layer resource admission is per-resource. The source material is an immutable WavView with a path + CRC, as with the Sampler; it is loop-fed from the existing WavVoice to native 32k, and a root-relative value is passed to Clouds' note pitch. The existing FIR boundary is used up to 48k. The pitch range is within Clouds' ±48 st. Voices already sounding keep reading the material they hold when the source changes.

Each Clouds audio channel is 16384 bytes = 8192 PCM16 samples (256 ms). A large arena of 69760 bytes and a small arena of 16384 bytes are used, and a 53376-byte FX workspace is allocated. Four arena sets total 344576 bytes, plus the Clouds object, the fixed stretch reservation for WavVoice, and the FIR/block arrays. At note-on the existing arena is initialized, but no allocation, file I/O or locking takes place. FREEZE becomes effective after 256 ms of filling. Cost 8, with reservations for 4 voices in total.

Of the 12 values, GENERATOR's 5 values and ENVELOPE's 4 values are performance-section values; JITTER / FREEZE / PITCH OFFSET are detail values. No new detail editing UI is added. WAV assignment in the BROWSER keeps GRANULAR when the selected source is GRANULAR. A Preset carries PARM v2 + GRAN v1, and missing material defers mounting. The rules for root FX, pool and Layer children are the same as elsewhere.

## E3: FORMANT

The fixed DaisySP 2c72eaf FormantOscillator / VosimOscillator are enabled unmodified, with their MIT notices. The empty constructor's numeric state is zero-initialized on the adapter side before Init. The vowel trajectory continuously interpolates five F1/F2 pairs; FORMANT uses two FormantOscillators and VOSIM uses two formant inputs. A common ADSR is used, state is per-voice, and no shared RNG is used. Registered capacity: 8 voices, cost 1.

## E4: ROTARY

ID 21. SPEED is SLOW / FAST / BRAKE, plus DRIVE and the common MIX. A mono cabinet is split by an 800 Hz 1-pole filter into horn/high and drum/low, and each channel of the existing 2048-frame stereo buffer stores the two bands. Reading each band from a different distance according to the left/right mic phase difference adds Doppler and amplitude rotation. The horn runs at slow .8 Hz / fast 6.8 Hz, the drum at .65/5.4 Hz. BRAKE decelerates to 0 Hz and holds the angle. The exponential ramp time constants are horn acceleration .8 s / deceleration 1.2 s and drum acceleration 1.8 s / deceleration 2.5 s (reaching about 90% takes 2.303× each). The delay base is horn 4 ms ±1 ms, drum 7 ms ±2 ms. No allocation and no full buffer clear happen during render.

## E5: Optional root FX

A preset may carry a preset-only PFXS v1 (the two inserts' type/A/B/MIX, disable bits, Delay/Reverb sends). Track volume, pan, compressor and master FX are not included. The structures of SoundLeaf / pool / Layer children are unaffected; the FX are applied only to the TrackHistory of the mount transaction when a root file is OPENed. When absent, the current FX are kept; unknown revisions are preserved as bytes/flags by the existing PreservedStore. New saves do not fill the pool. PREVIEW auditions through the conventional existing strip; bundled FX are applied only on OPEN. Kits use the conventional kit common section.

K3 INCLUDE FX in SAVE PRESET AS / OVERWRITE PRESET starts OFF, and the state is also shown inside the modal.

## E6: PITCH SHIFT

ID 20. SHIFT has 25 steps from -12 to +12. Only the common slot MIX controls dry/wet; B is blank. The kind limits for all tracks/groups/pads and the profile aggregation width are consolidated into the same kFxCount. Knob operation and display for the options are also obtained from the descriptors.

The Daisy PitchShifter holds 16384×2 floats per channel. It is fixed-allocated for every slot, avoiding allocation and Init during render (about 256 KiB per slot, about 24 MiB for 96 slots). The window is 2048 samples, Fun 0. On slot reset, the past delay is not cleared: output stays dry until 2048 new samples have arrived, then connects to the shift over 256 samples, preventing leakage from the previous sound. The delay window is about 42.7 ms; transparent frequency conversion and zero latency are not guaranteed. The slots' embedded state (about 24 MiB in total) is reserved from the fallible heap at slot construction time; there is no allocation in render or reset, and if the reservation falls short, project load is rejected.

## Displays

The screens show the Pitch dry/wet path and the Rotary horn/drum split as structure diagrams. The rotation phase is not fabricated; the display states explicitly that the speed ramps. While a VA addition is in use, the base LP/fixed-sub recipe is not drawn; the display falls back to actual PCM or declared controls.

## Detail values and preview

The VA's 8 added values and GRANULAR's JITTER / FREEZE / OFFSET are detail-section values; no new detail editor is provided on the device. PREVIEW uses the current strip's FX; INCLUDE FX is applied on OPEN. Support in the external mgxs-converter is outside the scope of this document.
