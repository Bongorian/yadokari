# AKWF waveform content

Source: [AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE), pinned commit `8de90bf94376670947369e69de0af6b9fbd19286`. Retrieved 2026-11-04. The upstream CC0-1.0 dedication is retained in `LICENSE`. Only WAV content and its license are included; no upstream code is imported.

`manifest.json` lists 128 source paths and SHA-256 hashes. Selection takes eight evenly spaced file indices from each of 16 timbral families, ordered from simple/smooth to increasingly complex spectra. This is deterministic curation, not a claim of listening evaluation. Original mono PCM16 cycles are retained in `waves/`.

From the repository root, run `make wavetable-bank` (Python 3 with NumPy). The original generator removes DC, aligns fundamental phase, computes seven Fourier low-pass levels with 64/32/16/8/4/2/1 harmonics, resamples each cycle to 256 points, normalizes all levels of a wave together to 0.94 peak, and quantizes to signed 16-bit. Generation verifies the source hashes and checks out-of-band quantization residue. The firmware uses the generated read-only tables; it does not run FFTs or read these source WAVs in the audio callback.

The generated table occupies 458,752 PCM bytes. Wave and mip interpolation, safe mip selection and oscillator/envelope composition are original application code, in `src/wavetable_voice.h`. License/provenance and generator files are not user DATA content.
