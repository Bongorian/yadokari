# Factory content identity (MGXC v12)

The firmware defines content identity in `src/preset_identity.h`:
an opaque **128-bit GUID** (text form: 32 lowercase hex chars, byte
order = hex order, as in `preset_guid_text`), a non-zero u32
**revision**, an **origin** (User=0 / Factory=1), and an optional
parent GUID + parent revision for forks. Factory content has no
parent. Display names and file paths are *not* identity.

## Manifests

- `preset-guids.json` (this pack, 91 presets) and
  `../kits/kit-guids.json` (24 kits) are the content side's
  identity record. Kits use the same `PresetIdentity` type as presets.
- In each entry, **`guid128` is the primary key**. The older `guid`
  field (`bongorian.preset.<slug>` / `bongorian.kit.<slug>`) is kept
  only as a human-readable label; `revision` starts at 1 and
  `origin` is `factory` for all entries here.
- `guid128_source` records where each value came from.

## Value sources

1. **Repo-pinned (103 of 115).** The firmware repo's
   `assets/factory-library/identities.json` (initially master 760f8a7f) is a
   fixed allocation for the current factory masters — all 79 synth
   presets and all 24 kits. Those values are adopted verbatim and
   must never be regenerated. A snapshot of that file is kept at
   `tools/factory-library-identities.json`; round3 refreshes the renamed GM Grand Piano path without changing any GUID or revision.
2. **Derived (12 of 115).** The sampler preset set (added after the
   repo allocation) uses deterministic UUID v5 (RFC 4122/9562,
   Python `uuid.uuid5`) under a fixed project namespace:

   ```
   ns_bytes = SHA-256(b"bongorian-mk3-factory-content-namespace")[0:16]
   ns       = UUID(bytes=ns_bytes, version=5)
            = 1acc4c7f-af8f-5aa4-8e34-b52d72624248
   guid128  = uuid5(ns, "preset:" + slug).hex     # presets
   guid128  = uuid5(ns, "kit:"    + slug).hex     # kits (future use)
   ```

   `slug` is the label minus its `bongorian.preset.` /
   `bongorian.kit.` prefix (e.g. `grand-piano`).

## Tooling

`tools/derive-guid128.py` refreshes the manifests from the two
sources above; `tools/derive-guid128.py --check` recomputes every
derived value from scratch and verifies counts (91 + 24), form
(32 lowercase hex / 16 bytes) and cross-manifest uniqueness without
writing anything. Run `--check` after editing either manifest or
the pinned snapshot.

Regeneration with these identities is done: all 91 presets and
24 kits were re-authored at MGXC v12 from the canonical codec at
repo master `85e64143` (see `REGENERATION-V12.md` and the
per-workspace `REGENERATION-V12.json` manifests). The 103 allocated
items' file identities match this manifest (and the repo allocation)
byte-for-byte; the 12 derived sampler identities are carried as
allocated here and are now allocated and shipped in the packed factory library. Round3 deliberately retains revision1 for all115 masters by user decision; old sounds are not preserved or migrated.
