# Mutable DSP source provenance

Source: https://github.com/pichenettes/eurorack commit `f4cfa7697095a28d292bff758beb51deb49b5a86`, with its exact stmlib submodule commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`. The Plaits DSP, resources and selected stmlib dependencies retain their per-file copyright and MIT notices. The upstream repository says STM32F code is MIT; its other projects have different licenses. Upstream trademark guidance applies; source names identify provenance.

Changes: `stmlib/dsp/dsp.h` drops `<cmath>` and uses the portable saturation/square-root implementations on AArch64; `src/freestanding` supplies the small algorithm/math header subset needed without Circle's C++ standard library. `plaits/dsp/dsp.h` uses 48000 Hz instead of the original STM32 DAC's 47872.34 Hz. `plaits/dsp/fm/dx_units.h` uses an unsigned shift for negative exponents, removing undefined behavior. Under `GROOVE_PLAITS`, `plaits/user_data.h` avoids STM32 flash and `voice.*` accepts optional immutable 4096-byte FM banks. No external bank falls back to the upstream factory banks. Original flash programming is not ported.

All 24 engine registrations and factory resources are included. Host tests check selection, nonzero sound, sanitizer behavior, and a synthetic 32-voice SysEx bank; they do not establish sound equivalence or real DX7 bank compatibility. `src/plaits_voice.h` renders at 48 kHz in 12-sample quanta. The application shares one voice between tracks, uses only the main output, and does not yet connect USB audio, SD file loading or the physical UI. `src/dx7_bank.h` validates Yamaha bulk framing and checksum outside the audio render; the backing bytes must remain valid until no longer referenced. Loading a bank triggers upstream patch unpacking at a render boundary; real-time duration on Pi is unmeasured.

The project's `PlaitsVoice` wrapper now saves/restores stmlib RNG state around initialization and each 12-sample render quantum, keeping a seed per wrapper for deterministic interleaved offline renders. Upstream's global RNG implementation is unchanged. All Plaits instances must still execute on a single DSP owner thread/core; this does not make upstream static state safe for simultaneous rendering on multiple cores.


## Resonator and texture cores

The `rings/dsp` and its resources are copied byte-for-byte from the eurorack
revision above. `clouds/dsp` and its resources are copied byte-for-byte from
`2a468734bdd53bea0f25165bf34d92a4c571db13`; the reason for this separate
Clouds pin is recorded below. New stmlib FFT dependencies
come from the exact submodule revision above. `resonator-texture-sources.json`
records SHA-256 for every newly vendored file. All retain their MIT notices.
No Parasites or other alternative firmware is used. Hardware drivers, flash UI
and Easter egg selection are not included in the application. Original DSP source
(including unused bonus implementations) is retained to avoid editing the core.
Native processing is 48 kHz / 24 frames for the resonator and 32 kHz / 32 frames
for texture; texture's own low-fidelity path remains under upstream control.

The older Plaits integration paragraph describes its initial bring-up, not the
current hardware or voice allocation. Current ownership and allocation are in
`src/engine.h` and `docs/architecture.md`. New fidelity results must explicitly
identify the DSP boundary and rate conversion; the earlier Plaits smoke tests
are not equivalence tests for these new cores.

Portability exception discovered by UBSan during integration: original Clouds
`correlator.cc:60` shifts a uint32 by 32 at zero offset. The original file remains
byte-identical. `tools/mutable/portable_correlator.py` verifies its SHA and emits
one guarded statement for compilation, defining the zero result of Cortex-M LSR
instead of relying on C++ undefined behavior / AArch64's masked register count.
The independent reference build applies the same explicitly documented CPU
semantics correction to its separately downloaded source. Raw uncorrected-host
results and their differences are recorded separately. No tables, modes, quality
settings, memory sizes or musical equations are changed. The original debug-pin
header is included unchanged and its upstream TEST branch disables only STM32
GPIO instrumentation; Circle freestanding standard-library shims supply the
required algorithms/math declarations.

`portable_loop.py` additionally emits the original looping-player header with
six fixed-point left shifts explicitly unsigned. This preserves low-bit modulo
arithmetic and eliminates signed-overflow UB; native output is unchanged by
this second correction. Both overlays are used by the independent oracle and
the host/Circle adapter builds. Vendored source SHA entries remain original.

ASan also caught the original full-wet table interpolation reading index 17 of
a 17-entry table even though its interpolation weight is zero.
`portable_processor.py` returns the existing index-16 endpoint for full wet;
it does not shrink the knob range or alter tables. Raw-host A/B output is
retained for comparison, and exact-endpoint Engine tests cover all modes and
qualities. These three mechanical safety corrections are the only Clouds
DSP-source overlay categories. The endpoint category also guards the
4097-entry grain-window and 257-entry grain-size tables at their inclusive
maximum, returning their original final entries. `portable_loop.py` emits
these two header overlays alongside the fixed-point correction.


Clouds is pinned separately to upstream `2a468734bdd53bea0f25165bf34d92a4c571db13`
(2019-04-02), with the same explicitly pinned shared stmlib above. Every selected
Clouds source/resource byte matches this upstream snapshot. Compared with the
previous 2023 snapshot, only `clouds/dsp/window.h` differs: upstream merge
`d1d8839c1147d8bf70c6012cf491be64aee66aa1` combined two duplicate-assignment
cleanups and removed both `done_ = false` statements from `Window::Start`.
That leaves STRETCH permanently silent on its wet path. The 2019 original
retains both assignments unchanged. We select the intact upstream implementation
instead of writing an alternative DSP. History:
https://github.com/pichenettes/eurorack/commits/master/clouds/dsp/window.h
The final fidelity fixtures are regenerated against the independent 2019
snapshot; results from the defective 2023 STRETCH core are superseded. Engine
checks require nonzero full-wet output in every mode/quality.


## Exact derived optimizations (F03)

Original vendored Rings/Clouds files and hashes remain unchanged.
`tools/mutable/cached_string.py` memoizes only String coefficients dependent on
frequency, brightness and damping, with original arithmetic and interpolators.
The paired generated header/source must be built together. A separately named
unmodified String is compared bit-for-bit for 20,000 blocks at varied controls.
`portable_correlator.py` additionally replaces the integer SWAR bit count by
`__builtin_popcount`, retaining candidate order, tie-breaking and search size.
`windowed_stft.py` batches the spectral analysis window through
`src/native_window.h`; each NEON lane performs the original int16-to-float and
single multiplication. FFT and overlap-add are unchanged. No quality, buffer
size, internal voice count or sample rate is reduced by these optimizations.
