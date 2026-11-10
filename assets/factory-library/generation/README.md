# MGXC12 factory content — generation code and provenance

> Historical recipes and provenance. The old MGXC12 masters have been retired. For current generation and submission follow the MGXC13 handoff; use the current codec, preserve allocated GUIDs and validate new output. Historical preservation instructions below are not the current submission contract.

This directory holds the content-side generation code that produced
the 103 MGXC **v12** factory masters in `../PRESETS/` and `../KITS/`
(branch `content/v12-factory-103`). Nothing here is built by the
firmware build; it is the reproducible record of how the masters were
made, per `docs/reference/preset-v12-content-handoff.md`.

## Codec provenance

Both generators use the repository's canonical MGXC codec at commit
**85e6414355ce3037846f6e2bef4ae3fc02d2c3a8** ("Connect pinned preset
lifecycles, CRC shelves and shared performance selection") — the
commit that fixed the MGXC12 format (identity / pinned references /
CRC shelves). The branch itself is based on master `d5ceb681`, which
adds only the handover document on top of that codec.

## Presets (79) — `presets/`

- `presets/Makefile` + `presets/tools/packgen.cpp`: the preset-pack
  generator. It now uses the current repository codec and shared support
  archive. The former duplicate codec closure has been removed;
  `presets/vendor/mk3/PROVENANCE.md` retains the original pinned commit
  for historical reproduction. Existing submitted masters are unchanged.
- Definitions: `presets/tools/presets.json`. Identities are inputs,
  never derived from file names at build time:
  `presets/preset-guids.json` carries the fixed allocation
  (`assets/factory-library/identities.json`, snapshotted as
  `presets/tools/factory-library-identities.json`); see
  `presets/IDENTITY.md`.
- Reproduce the submitted masters:
  `cd presets && make gen-factory && make verify-factory`
  (outputs in `presets/out-v12-factory/`). `presets/tools/inspect.cpp`
  (`make inspect`) is the read-only checker used for the submission's
  spec battery (format / kind / IDEN / chunk inventory / asset
  records).

### What changed for this submission (vs the first v12 round)

1. **Factory path base.** packgen gained a `factory` mode
   (`gen|verify <defs> <outdir> <guids> factory`). The first v12 round
   stored ASST paths relative to the user shelf (`PRESETS/<file>`,
   `"../…"`). The submitted masters are placed at
   `FACTORY/PRESETS/LIBRARY/<file>`, so in factory mode the stored
   form is `"../../../" + canonical` — the exact form the canonical
   `project_paths()` produces for that logical path, resolving
   directly to the DATA-root canonical path.
2. **Legacy sample routing baked in.** The two samples whose
   references pointed at the retired `assets/factory` tree
   (VA-Hit / FM-Metal) now carry the canonical DATA placement the
   firmware's own importer routes them to:
   `ASSETS/SAMPLES/GBX-22f22446-VA-Hit.wav` /
   `ASSETS/SAMPLES/GBX-9810cb6c-FM-Metal.wav` (CRC-named copies of the
   same audio; content CRCs unchanged). No sound, FX, or value in any
   preset was changed — the only byte differences against the first
   v12 round are the ASST stored-path strings (path hints; identity
   is the CRC).

## Kits (24) — `kits/`

- `kits/user_kits_v12.cpp`: scratch builder used with the repo's own
  engine objects in a scratch clone of the repo at the codec commit
  above (identical pad construction to the repo pipeline's licensed
  kits; samples from `tools/content/build_factory_library.py`'s
  pinned factory sources, `build/factory-kits.tsv`).
- This submission's round writes each kit for its factory placement
  `FACTORY/KITS/LIBRARY/<name>.mgxk` (the first v12 round wrote for
  the user `KITS/` shelf); asset records are stored relative to that
  directory by the canonical `project_paths()`. Sound content is
  byte-for-byte the same decoded state as the shelf round — verified
  field-by-field (only ASST path strings differ).
- Identities: `kits/kit-identities.tsv` is the exact table the build
  consumed, generated from the fixed allocation (see
  `kits/kit-guids.json`).
- Reproduce: in a repo clone at the codec commit, place
  `user_kits_v12.cpp` under `tools/content/`, generate
  `build/factory-kits.tsv` via `tools/content/build_factory_library.py`
  and `build/kit-identities.tsv` from `identities.json`, then follow
  `kits/rebuild-kits.sh` (compile/link commands mirroring the
  Makefile's `factory-content` target) and run the builder's `verify`
  mode from a staging DATA root (`FACTORY/KITS/LIBRARY/` +
  `ASSETS/SAMPLES/`). Evidence of the submitted run:
  `kits/build-log-v12-factory.txt` (24 built, 24 VERIFY OK).

## Verification standard

Every master: full decode with the canonical codec, `valid_project`,
canonical encode→decode→encode byte identity, IDEN equal to the
fixed allocation, per-file size ≤ 131,072 bytes. Every referenced
asset's content_crc was checked against an actual file held by the
content side (see `../SUBMISSION.md` for the scope).
