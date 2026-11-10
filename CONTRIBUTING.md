# Contributing to YADOKARI

Thanks for your interest. YADOKARI is a one-human-plus-robots project,
and contributions are genuinely welcome — code, presets, kits, docs,
bug reports, all of it.

## You don't need the hardware

The same C++ core runs on a PC as a **virtual MK3** (the simulator).
UI, sequencing and most sound work can be developed and reviewed
without a MASCHINE MK3 or a Pi:

```sh
make test        # host test suite
make simulator   # PC virtual MK3 (needs SDL2)
```

Real-hardware behaviour is verified by the maintainer on a physical
unit; say in your PR if a change needs a hardware check.

## Ground rules

- **CI must pass.** Both jobs: the host test suite and the firmware
  build.
- **DCO, no CLA.** Contributions are accepted under the
  [Developer Certificate of Origin](https://developercertificate.org/).
  Add a `Signed-off-by: Your Name <you@example.com>` line to each
  commit (`git commit -s`).
- **License of contributions** is GPL-3.0, the project's license.
- **No Native Instruments material.** No NI code, binaries, firmware,
  sounds, samples or artwork — in code, in test fixtures, anywhere.
  This is a hard rule; it protects the project.
- **Formats stay honest.** Changes to the project/preset/kit file
  formats must update `docs/project-format.md` and
  `docs/preset-system.md` in the same PR, and old files must either
  keep loading or fail with a clear error — never silently mis-load.

## What fits the project

YADOKARI is a live-performance machine. Features are judged by one
question: *would its absence force the player to carry something
else on stage?* Polish, stability and operation flow beat feature
count. UI changes must follow the screen grammar in
`docs/user-manual` and the architecture notes — when in doubt, open
an issue first and let's talk it through.

## Presets and kits

Sound content is a contribution too. Factory-style presets and kits
go through the same review; include the recipe/provenance of any
samples you use (CC0 / public-domain sources only for anything
destined for the factory library).

## Bugs

Use the bug-report template. The most useful reports include the
firmware version (shown on the boot splash), what you were doing,
and — for sound issues — the preset/kit names involved.
