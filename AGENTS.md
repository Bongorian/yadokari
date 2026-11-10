# Working in this repository

Notes for contributors — human or AI — making changes to YADOKARI.

## Ground rules

- **No Native Instruments material.** No NI code, binaries, firmware,
  sounds or artwork, in commits, tests or fixtures. The firmware is an
  original implementation; keep it that way.
- Contributions are accepted under the **DCO** (`Signed-off-by` on
  every commit), no CLA. See [CONTRIBUTING.md](CONTRIBUTING.md).
- CI must pass. The environment is locked (Ubuntu 24.04, GCC 13.3,
  pinned Arm GNU toolchain) — see [BUILDING.md](BUILDING.md).

## Build and test

```sh
make test                 # host test suite (no hardware needed)
make docs-check           # documentation classification + link check
make simulator            # PC virtual MK3 (needs SDL2)
scripts/build-firmware.sh # firmware image for the Pi
```

## Documentation rules

- Docs describe the **current state only**. No changelogs, test or
  validation records, design history or work notes in `docs/` —
  release facts live in [CHANGELOG.md](CHANGELOG.md), and known
  limitations live in exactly one place:
  [docs/known-limitations.md](docs/known-limitations.md).
- Every Markdown file under `docs/` is classified in
  `docs/catalog.json`. Add new docs to the catalog and run
  `make docs-check` before pushing.
- The current file format family is **MGXC13** (`.mgxs` / `.mgxk` /
  `.mgxp`). A format change updates
  [docs/project-format.md](docs/project-format.md) in the same commit.
