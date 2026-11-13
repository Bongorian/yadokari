# Third-party notices

YADOKARI is licensed under the GNU General Public License v3.0
(see [LICENSE](LICENSE)). It builds on the following open-source
components. Full license texts live with each component under
`third_party/` or in its upstream repository.

## Firmware foundation

- **Circle** — Rene Stange — GPL-3.0
  https://github.com/rsta2/circle
  Bare-metal C++ framework for Raspberry Pi. Fetched at a pinned commit
  by `scripts/build-firmware.sh` and patched by the series in
  `scripts/patch-circle-*.py`. Circle is the foundation the whole
  firmware stands on — and the reason this work is GPL-3.0.

- **encdr** — RufusIbiza — GPL-3.0
  https://github.com/RufusIbiza/Encdr
  Open research into the MASCHINE MK3's USB protocol. Not vendored:
  YADOKARI's display framing, LED encoding and control reports were
  implemented from our own hardware study, guided by encdr's published
  findings. This project would have taken far longer without it.

## Sound engines and DSP

- **Mutable Instruments eurorack modules** — Émilie Gillet — MIT
  Plaits, Clouds and Rings sources, pinned in `third_party/mutable/`
  (see its PROVENANCE file for exact commits).
- **DaisySP** — Electro-Smith — MIT
  DSP library; the modules in use are audited per-module in
  `third_party/daisysp/` (see its PROVENANCE file). Where they
  surface in the instrument: the two Daisy drum families are built
  from its drum modules (Analog Bass Drum / Analog Snare Drum /
  Hi Hat, and Synthetic Bass Drum / Synthetic Snare Drum, plus its
  SVF) — original DaisySP designs, not recreations of any Roland
  circuit; the West Coast voice's FOLD is its Wavefolder (also
  available as a track insert); the 2-operator FM engine is its
  Fm2 (distinct from the 6-operator DX7 core above); and track
  DRIVE is its Overdrive.
- **msfa** (from the Dexed tree) — Apache-2.0
  DX7-compatible FM core. Only the msfa engine files are included;
  the Dexed application itself is not part of this project.
- **TinySoundFont** — Bernhard Schelling — MIT
  Single-header SoundFont 2 player. No sound banks are bundled
  with YADOKARI.

## Support libraries

- **Monocypher** 4.0.3 — BSD-2-Clause
  Ed25519 verification of update packages.
- **FatFs** R0.16 — ChaN — FatFs license (permissive)
  Firmware uses Circle's copy; a host-side copy lives in
  `third_party/fatfs-host/`.
- **AKWF-FREE** waveform set — CC0-1.0
  128 single-cycle waveforms used by the wavetable engine. Waveforms
  only; no code.

## Fonts

- **Barlow Semi Condensed** — Jeremy Tribby — SIL OFL-1.1
  The user-interface font.

## Factory content sample sources

The built-in kits were recorded and assembled for this project from
CC0 / public-domain sources — see `assets/factory/NOTICE.txt` and
`assets/factory/factory_sources.lock.json` for the full list:
tidalcycles TR-808 recordings (CC0), octalmusic octal-samples (CC0),
oramics CR-78 / LM-2 / TR-505 (public domain), sgossner VCSL (CC0),
VSCO-2-CE (CC0).

## Host-side tools only (not in the firmware)

- **SDL2** — the PC simulator ("virtual MK3").
- **OpenSSL** command-line tool — signs update packages on the
  maintainer's machine. No private key material is ever committed;
  see the "Update signing" section of the README.
