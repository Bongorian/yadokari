# Historical MK3 codec provenance

The source snapshot described below was used to generate the submitted masters.
Its duplicate code/specification copies were removed during the four-engine
retirement; the recorded commits remain the reproduction source in Git. The
current pack/inspection Makefile uses the repository codec and shared support
archive. This historical provenance and GPL notice remain intact.

- Source repository: `Bongorian/maschine-mk3-bare-metal-groovebox`
  (read from a scratch clone checked out at the pinned commit;
  no existing clone was modified).
- Source commit: `85e6414355ce3037846f6e2bef4ae3fc02d2c3a8`
  (origin/master, MGXC12 preset-system wave: pinned identities, CRC
  shelves, shared performance selection).
  Previous rounds: `af34d9d44b229a1eb1f585dbe40591ee1b66a191` (v11),
  `21abd87cb816c8a0197458bcd4fb9bf717829b81` (v10).
- Format: MGXC v12 (`kFormatVersion=12`). Spec copy:
  `PROJECT_FORMAT.md` (upstream `docs/project-format.md`, same commit).
- License: GPL-3.0 (same as the source project).
- Scope: the transitive include closure of `project_codec.h` +
  `future_factory_presets.h` + `engine_registry.h` + `fx_registry.h`,
  obtained with `g++ -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -M`
  (37 headers), plus `src/state_defaults.cpp`: at v12 the
  `empty_sound()` / `empty_layer()` defaults moved out of the headers
  into that one small translation unit, which is therefore vendored
  and compiled into packgen (see the pack Makefile). No file was
  modified.
- v11 → v12 differences inside this closure (verified by diff):
  - `project_format_version.h`: 11 → 12.
  - `preset_identity.h` (new): `PresetGuid` / `PresetIdentity`
    (guid + revision + origin + parent lineage), the issuer and the
    reserved built-in identity family.
  - `project.h` / `track_model.inc`: tracks gain `track_name`,
    `preset_identity`, `preset_fx`, `preset_slot`; `SoundState` gains
    `identity` + `fx`; `PatternState` gains per-pattern track names
    and a preset binding; `PresetFxSnapshot` is templated; a standalone
    PFXS is now legal for Kit as well as Preset.
  - `project_codec.h`: standalone Kit/Preset files require a root
    `IDEN` v1 frame (41 bytes) taken from track 0's identity, followed
    by the optional root `PFXS`; decode applies the file identity and
    FX snapshot onto track 0; `SND ` envelopes are revision 2 with
    identity + FX; duplicate assigned pool identities are rejected;
    old format versions are rejected.
  - `engine_state_types.h`: `empty_layer()` is now declared here and
    defined in `state_defaults.cpp` (see Scope).
  - `preserved_payload.h`: preserved entries gain auxiliary/opaque
    payloads (unknown SND children / future SND revisions).
  - `future_factory_presets.h`: built-in presets are stamped with
    `builtin_factory_identity(0x100 + index)`.
  - `ui/theme.h`, `ui/generated/themes9.inc`: kind-colour wave
    (v526) theme data; not used by the codec paths packgen exercises.
  - Everything else in the closure — engine registry and state codec,
    layer codec, sample state, FX registry, parameters — is
    byte-identical to the v11 round: no preset-carried engine chunk
    changed layout. For presets the v12 delta is the header version,
    the new IDEN frame, and the SND-envelope rules (standalone files
    carry an empty SNDS). `ContentKind` values are unchanged
    (Song 1 / Kit 2 / Preset 3).
- Update procedure: re-copy the closure from a newer pinned commit,
  re-run the pack readback verification, and update this file's commit.

Files: see `src/` (38 files, paths preserved from upstream `src/`).
