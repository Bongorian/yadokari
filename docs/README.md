# Documentation

Start with the [README](../README.md) and the
[user manual](user-manual.md) (also available in
[Japanese](user-manual.ja.md)). The technical
documents below are the minimal set that describes how YADOKARI
works today; development history and test records are not kept
in this repository.

## Using it

- [Getting started on the device](device-start.md) — first boot,
  the card layout, and where things live.
- [Firmware updates](firmware-updates.md) — how signed updates
  are packaged and applied.
- [Known limitations](known-limitations.md) — honest release-time
  facts.
- [Hardware](hardware.md) — the Pi 4 / MK3 connection, power and
  the development link.
- [Building](../BUILDING.md) — build the firmware and the SD
  image, on Linux or in the dev container.
- [Pi 4 development setup](pi4-development-setup.md) — developing
  against the real hardware.
- [The dream build](internal-build.md) — the optional
  Pi-inside-the-MK3 build (🚧 under construction).

## Sound and content

- [Factory library](factory-library.md) — what ships built in
  and how it is generated.
- [Data and content layout](data-content.md) — factory vs user
  shelves, and the drop-in door.
- [Preset system](preset-system.md) — how presets, kits and
  layers are identified and stored (.mgxp / .mgxk).
- [Project format](project-format.md) — the .mgxs song format.
- [Synth extensions](synth-extensions.md) — engine parameters
  beyond the common core.
- [TWIN](twin.md) — sampling a heavy track into a playable twin
  for live switching.

## System design

- [Architecture](architecture.md) — the big picture: cores,
  tasks, audio path, storage.
- [Allocation policy](allocation-policy.md) — voices, CPU and
  memory budgets.
- [Feature overview](feature-matrix.md) — the full capability
  list, per area.
- [Update threat model](update-threat-model.md) — what the
  update signing does and does not protect against.

## Interface

- [UI design system](ui-design-system.md) — the screen grammar:
  layout, type, colour and motion.
- [UI pages](ui-pages.md) — the page inventory.
- [LED behaviour](led-behavior.md) — what the lights mean.
- [Step attributes](step-attributes.md) — per-step data.
- [Track-kind colour](track-kind-colour.md) — the single
  colour-derivation rule for tracks.
- [Audio tracks](audio-tracks.md) — recording and playback
  tracks.
- [Boot splash](splash-yadokari.md) — the YADOKARI splash and
  the runtime version overlay.

## Reference data

- [Reference directory](reference/) — the canonical theme
  definitions (themes9.json), the engine schema, and the USB
  file-transfer protocol.

## And finally

- [The story so far](story.md) — how a private experiment became
  this repository, told as one side story. 🐚
