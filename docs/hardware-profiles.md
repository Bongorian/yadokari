# Hardware profiles and per-board performance scaling

The firmware decodes the board revision once at boot
(`groove::hardware::set_active_profile`, src/hardware_profile.h) and every
budget decision afterwards reads the active profile. The baseline is the
Raspberry Pi 4 with 4 GB: every guarantee is defined there, its scale
factor is 1000 permille (identity), and its behaviour is unchanged from
the pre-profile build. Larger boards scale budget-side values only; cost
unit prices stay common on all boards.

## What scales (Pi 5, factor 2984 permille, measured on device)

- Shared voice budgets, shared slots and per-instrument voice limits in
  `Engine::plan_shared` (shared storage capacity is 16; the Pi 4
  effective slot count stays at the declared 4).
- Native (Texture / Resonator) admission: the AVERAGE limit scales; the
  BURST limit does not. The burst reservation protects the 192-frame
  render-ahead against synchronised work -- a real-time wall, not a
  throughput budget. Full scaling of both limits was measured to trade
  refusals for deadline overruns (over500 12 -> 1,828), so only the
  average limit scales.
- Layer admission cost gates (layer_track / layer_total) and the shared
  slot/budget gates inside layer admission.

## Known constraint: layer admission asymmetry

Layer admission scales with the board factor (decision A, 2026-11-15).
A layer project admitted on a Pi 5 may therefore be refused on a Pi 4.
This asymmetry is accepted and documented here -- the same class as the
shared-budget scaling -- rather than pinning admission to the baseline
floor. Projects that stay within the Pi 4 baseline budgets behave
identically on every board.

## Memory

- CPU arena: Pi 4 keeps the fixed 960 MiB window. On Pi 5 the arena is
  carved from the top of DRAM by scripts/patch-circle-cpu-heap.py,
  decided at runtime from installed DRAM: 8 GB boards carve 2 GiB,
  4 GB boards keep 960 MiB. The profile value reported by
  hardware_profile.h follows the same rule (verified on device:
  cpu_ram_mb=2048, heap headroom 1979 MB on Pi 5 8 GB).
- SoundFont resident budget: 512 MiB baseline; 1536 MiB on Pi 5 / 8 GB
  (hardware::sf2_budget_bytes, applied at both allocation sites).

## Not yet verified

- A real SoundFont larger than 512 MiB has not been load-tested; the
  on-device diagnostic generator produces only a 32 MiB file. The
  1536 MiB budget is code-verified at both allocation sites and the
  SoundFont DSPTEST case runs, but the >512 MiB load itself is unrun.
