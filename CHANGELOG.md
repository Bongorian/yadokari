# Changelog

YADOKARI's public releases start here. Versions use the packed
semver numbering described in the release notes
(0.9.0 → 900, 1.0.0 → 10000, monotonic).

## 0.9.0 — 2026-11-11

First public release.

- The complete instrument: a stock MASCHINE MK3 plus a
  Raspberry Pi 4 becomes a standalone groovebox — 16 tracks,
  synth engines (VA, FM6 with DX7 SysEx import, Plaits / Rings /
  Clouds physical modelling and granular, wavetable, formant /
  VOSIM, additive, phase distortion), sampler with slicing,
  SoundFont 2 playback, drum kits with per-pad GM declaration,
  looper and audio tracks, FREEZE and TWIN, 256 steps with
  per-step attributes, scenes and song arrangement, mixer and
  performance FX, file transfer over Ethernet and USB.
- Wi-Fi is disabled in this build: the WLAN stack available to
  the firmware carries Plan 9-derived code under the GPL v2
  only, whose combination with this GPL v3 firmware has not
  been cleared. See docs/known-limitations.md.
- Factory library built in: 187 presets and kits (MGXC13).
- Signed updates (Ed25519): official images carry the project
  release key; self-built images use the builder's own key and
  do not cross-update with official ones.
- The controller is never modified: unplug the Pi and it is a
  normal MASCHINE again.
