# Project format MGXC

Implementation: [project_codec.h](../src/project_codec.h), [engine_registry.h](../src/engine_registry.h), [engine_state_codec.h](../src/engine_state_codec.h), [parameters.h](../src/parameters.h).

New files use `.mgxs` for Song, `.mgxk` for Kit and `.mgxp` for Preset.
The reader and Browser accept `.mgx`, `.mgxs`, `.mgxk` and `.mgxp`, case-insensitively.
Extensions are discovery/display hints only: the `TYPE` frame (`ContentKind`)
is authoritative even when it disagrees with the suffix. A renamed preset with
`.mgxs` still loads as a preset; an old-format file with `.mgxp` is still rejected.
New SAVE AS and named preset exports replace any recognized family suffix with
the standard suffix for their content. Ordinary SAVE preserves an existing path;
there is no user-file rename or migration. COLLECT writes `project.mgxs`.
A DrumKit instance exported through SAVE PRESET or the Kit library has
ContentKind=Kit and uses `.mgxk`. Ordinary sound presets use ContentKind=Preset
and `.mgxp`.

Future exchange kinds follow **mgx + kind suffix**. Allocate a distinct ContentKind
and suffix together, update the shared discovery helpers, guards and documentation,
and retain header-based dispatch. Adding a suffix alone does not change the wire
format version. See [data_layout.h](../src/data_layout.h).

MGXC **format 13** (MGXC13) uses engine-owned Morph banks and scoped influence targets. The format
uses a required `TYPE` frame to distinguish Song, Kit and Preset. Files in formats 1–12 and the old
positional MGX formats are rejected with an explicit version error; there is no migration tool or compatibility
reader. Factory/debug containers are generated in the current format; unsupported user containers
are retained on disk and never deleted. WAV and MIDI remain external audio/event files.

## Optional TWIN association

Song files may contain one `TWIN` revision-1, flags-0 frame. It is omitted when
no track has a prepared pair. Payload: `u8 count` (1–16), then `count` records
of `u8 track, u16 source_pool, u16 sampled_pool, u8 active, u8 digest[32]`.
The definitions remain ordinary `SNDS` entries; the WAV and the existing
16 slice/zone sampler representation do not change. No association is exported
in a Preset or Kit. See [TWIN](twin.md) for the complete runtime contract.

Track indices and both pool indices must be in range, the two slots must differ,
and records may share neither a track nor a pool slot. Source must be nonempty;
sampled must be a Sampler with zones. `active` is 0/1 and must agree with the
track's attached Sampler/pool slot. Duplicate chunks, trailing payload and
invalid associations fail decoding; there is no inference from names or GUIDs.
Digest is the existing canonical content digest plus the effective Morph/macro
and opted-in MIDI CC context. It identifies the source used for preparation,
not the generated WAV. Material CRC remains the normal asset reference.

## Stream layout

All integers are little endian; floats are IEEE float32. A CRC-32/
ISO-HDLC (the same `ProjectCrc` as MGXZ and WAV data) covers everything
after the magic, and the reader requires the CRC to match and the source
to be exactly at its end.

```
char[4]  magic "MGXC"                    (raw, not CRC-covered)
u16      format_version = 13
u32      payload_length                  (bytes of chunk stream)
chunk[]  chunk stream
u32      crc32                           (raw)
```

Chunk header (12 bytes): `u32 kind` (FourCC, written in reading order),
`u16 version`, `u16 flags`, `u32 length`, then `length` payload bytes.
A reader skips chunks it does not know by length, so unknown chunks
never break parsing.

Top-level chunks:

| kind   | contents |
|--------|----------|
| `TYPE` | first frame, one u8: 1 Song, 2 Kit, 3 Preset |
| `IDEN` revision 1 | required once for Kit/Preset: stable preset identity and provenance, 41 bytes |
| `PFXS` revision 1 | optional Kit/Preset FX content, 41 bytes |
| `PTRM` revision 1 | optional preset root output gain, one float32, 4 bytes |
| `PREF` | u8 destination pattern, u8 earlier pattern; exact state reuse, no audio-time indirection |
| `AUDO` | audio lanes and clips |
| `PROJ` revision 3 | tempo, active pattern/scene, mute/solo masks, MIDI input routes, clock slave flag, random seed, tuning, gesture recording, UI state |
| `ASST` | the 64 sample assets (path + metadata) |
| `SCEN` | scenes, scene chain, clips |
| `SONG` revision 2 | active Scene/Arrangement sections, enabled/loop flags |
| `MACR` revision 5 | macros, morph on/off + position, morph A/B snapshots |
| `MFX ` | master effects and routing (incl. group buses) |
| `TRKS` | required persistent tracks: 16 for Song, only track 0 for Kit/Preset |
| `PTRN` | one per pattern: u8 pattern index, u8 swing, then `PHRS` sub-chunks |
| `SNDS` | Song embedded copies: `SND ` revision 2 envelopes, 256 slots, u16 index |

Songs encode all 16 patterns; identical serialized patterns use `PREF` after
one complete `PTRN`. The reader rejects forward/self references, duplicate
pattern destinations and missing reference sources. Unknown engine payloads belong
to the 16 persistent tracks, independent of pattern reuse.

Kits and presets store only the engine instance of track 0, the required root
`IDEN` and optional root `PFXS`/`PTRM`, without `TCOM`, `PROJ` or phrase data. Kit requires
`mk3.drumkit` on track 0. A kit is a single DrumKit definition, **not** an old
16-track kit/song snapshot. Re-author those files; there is no conversion reader.
Neither kind stores timeline, audio lanes, scenes, macros, Morph or master routing.
The emitted standalone `SNDS` container is empty; Layer children are already
embedded in the engine state.

`TRCK`: u8 track index, `TCOM` (Song only), optional `TNAM`/`PRID`/`PFXS`/`PTRM` (Song only), and
`ENGN`. `TNAM` revision 1 is 1–95 printable ASCII bytes without trailing NUL;
omit it for an empty track name. It is independent of `ENGN/PNAM` and survives
engine/preset replacement. UI preference: track name, preset name, engine name.
`PRID` revision 1 is u16 embedded pool slot followed by the 41-byte identity below.
Track values remain independent working copies; resolving their pinned base must
not overwrite saved engine edits, strip values or current FX. Duplicate typed
track fields are errors. Unknown container children retain framing, flags and
encounter order after known fields on canonical re-encode.

### Preset identity and embedded copy

`IDEN` v1 (41 bytes, shared by standalone files and `SND ` envelopes):

```
byte[16] guid                opaque bytes in printed hexadecimal order
u32      revision            nonzero, monotonically increasing within GUID
u8       origin              0 User, 1 Factory
byte[16] parent_guid         all zero when there is no parent
u32      parent_revision     0 iff parent_guid is zero
```

GUID zero is unassigned runtime state, not a valid standalone preset/kit identity.
The original factory allocation uses first byte `f0`; generated user GUIDs use first byte `80`.
These are issuance conventions, not codec validity checks. The twelve sampler
GUIDs adopted from the content-side UUIDv5 allocation keep their existing opaque
128-bit values (see the identity map); origin=Factory does not require an `f0` prefix.
A factory-derived user save keeps the factory GUID, origin=User, records the
factory parent GUID/revision and increases revision. A distinct duplicate gets a
new user GUID and records the source as parent. Renaming/moving is not identity.
Factory allocations are fixed in [the identity map](../assets/factory-library/identities.json);
The reserved built-in key family `f04255494c54000000000000 + u32be key` is listed under `builtins` in the same map.
Keep those GUIDs when regenerating/moving/renaming content. They are not recomputed
from filenames on future saves.

`SNDS/SND ` revision 2: u16 slot index, `IDEN` v1, optional `PFXS` v1 / `PTRM` v1, `ENGN` v1.
Every named/registered entry carries identity; same GUID/revision is registered
once; duplicate assigned identities or a track PRID that does not match its embedded slot are rejected. Unassigned runtime sounds must acquire identity before named/session save.
`ENGN` payload and `PNAM` remain unchanged. FX is part of the content both here
and in standalone files; attach uses the same engine+Layer+FX semantics. Unknown
SND envelope children are preserved. A future whole SND revision is retained
opaque and muted rather than interpreted as revision 2.

`PUNK` revision 1 is optional, flags=0. Its payload is three consecutive
`u32 byte_length + raw framed TLV bytes` streams, in this order: standalone
Preset root, standalone Preset TRCK, Song SND. Empty streams have length zero.
Each stream preserves kind/revision/flags/payload and original parent scope;
malformed framing or duplicate typed PUNK is rejected. Song TRCK can carry all
three streams. SND writes its own unknown children directly and transports the
other two streams. A standalone file writes root/TRCK children at their original
scope and transports only Song SND children in a root PUNK. Canonical re-encode
therefore does not duplicate fields or confuse unknown Song track metadata with
preset content. A future PUNK revision is retained as unknown data.

The runtime `definition_cookie` is a transient reference to an immutable,
file-service-owned extension record. It is never serialized, is independent of
GUID/revision and follows copies/Undo. The store emits the current definition's
unknown data at save time, rather than treating a mutable pool slot as its owner.

The pool copy is pinned. Library search uses exact GUID/revision, User before
Factory, and otherwise the embedded copy. A newer revision is an explicitly
selected update; it is never substituted automatically. Asset slot numbers are
container-local; the asset's CRC, not its path, identifies the material. Paths
remain relative hints under ASST; resolve/verify CRC before publishing bindings.

`PHRS`: u8 track index, `PSEQ`, optional `KSEQ` and `CLFA`. Each pattern owns
16 phrases. `PREF` copies only phrases, swing and pattern mute state.

`CLFA` revision 1 is exactly four u8 values: action (0 Off, 1 Stop, 2 Next, 3 Target, 4 Random), completed cycles (1–64), target pattern (0–15), probability (0–100). It is omitted for the default Off/1/0/100 configuration. Unsupported CLFA revisions or invalid values reject the container. This state belongs to a clip (pattern/track), not to its sound engine.

`TCOM` holds persistent strip, MIDI/expression/NRPN, pitch mapping, arp,
groove, insert FX, compressor, output and TEXTURE INPUT, followed by f32 LFO SYNC (0/1) and f32 LFO DIVISION (0–11, default 6). Common Morph snapshots contain 409 values (SYNC/DIVISION appended). `PSEQ` revision 4 holds u16
length, steps/locks/chords, mutation source, trigger lanes, automation,
direction/speed, frozen asset and u16 sound override. Sound/override slots
are 0–255; 65535 is unassigned. The complete decoded project must pass
`valid_project` and the container CRC before publication.

Each step stores a root plus seven optional chord tones (255 = empty), up to
eight notes independently of the engine DSP limit. The flags byte uses bits 0–3
for enabled, lock-only, chromatic chord pitches, and native GM drum pitches.
Bits4–5 are SLIDE and ACCENT. Bits6–7 must be zero. Attributes belong to the
whole melodic step, including disabled/lock-only records and frozen mutation sources.
Both the first64 full records and sparse extended records retain their byte sizes
and field order; only the existing flags byte changes. See [step semantics](step-attributes.md).
Explicit chord shapes bypass scale snapping; SCALE retains diatonic behavior.
Native drum pitches let recorded external MIDI play SF2 bank 128 without a
second pad-to-GM mapping. PROJ adds sixteen signed keyboard octave offsets
(-4 through +4); they do not alter persistent track roots.

`KSEQ` revision 1 belongs to `PHRS` and stores the sparse 16-pad step sequence.
It survives even when a different engine is attached. `KIT ` revision 2 inside
`ENGN` stores only pad definitions and the solo mask, so mounting a kit preset
cannot replace a phrase. Pool and track engine chunks use the same sampler
payload. `ADSR` revision 1 stores four Daisy envelope floats; `MIDI` revision 1
stores program and opted-in CC bases. MIDI channel and root note remain track
properties. Formats 1–12 are rejected; no migration is provided.
`GMAP` revision 1 inside ENGN optionally stores sixteen u8 GM pitches (0–127,
255 = undeclared). It is independent of KIT and KSEQ. An absent declaration
uses [the factory role map](../src/drum_gm.h); no GM mapping editor is exposed.


## Scoped influence targets

`PROJ` revision 3 (Gesture), `PSEQ` revision 4 (locks/automation, including
mutation source steps), and `MACR` revision 5 (routes) encode every target as:

- u8 length = 0, followed by u16 common declaration ID; or
- u8 GUID length (1–64), ASCII GUID bytes, then u16 engine-local declaration index.

Runtime engine ordinals are never written. Common targets include track/FX/MIDI
controls; special phrase sample selectors 300–302 remain lock-only. Engine numeric
controls are not valid persisted targets. MODEL and ADSR
use the same engine-local references as other engine parameters. A disabled field
may have the zero common sentinel. Gesture Morph/Macro selectors use the common
branch and retain their target-specific validation.

Changing the mounted engine keeps locks, automation and routes dormant until
that engine is used again, including step presets. Common controls remain shared.
The order is Morph → automation → Gesture → Macro → lock → Control All;
MODEL is resolved at note-on before voice-cost admission. Selectors enumerate
common declarations plus the current engine's declaration table.

Unknown influence GUIDs and unavailable declarations of known GUIDs are retained
in a fixed project-local table (64 owners, 32 declarations per owner). Their values,
enabled flags and targets round-trip; they never affect audio. The UI identifies the
GUID as unavailable and allows selecting a supported replacement target. Value/mode
edits are disabled until a supported target is selected. Table overflow, invalid
GUID bytes or out-of-range local indices reject the load transactionally. The table
is included in project snapshots and cleared on NEW; it is not a preset pool.

`MACR` revision 5 stores four mandatory A/B sets. Each snapshot contains 407 common f32 values in `morph_parameter_at` order, a u8 GUID byte length (1–64), GUID bytes, a u16 declaration count (0–32), and that many f32 engine values in declaration order. Known GUIDs require the exact descriptor count/ranges; unknown GUIDs retain bounded finite values verbatim. Engine values do not occupy common numeric-ID slots. MODEL and ADSR participate through local declarations; TEXTURE INPUT stays common.

## Song / Arrangement (`SONG`, revision 2)

The payload begins with `u8 length` (0–32), `u8 enabled`, `u8 loop`. Only active sections follow, each exactly 30 bytes:

```
u8 scene; u16 bars; i8 transpose; u16 mute_mask;
u32 ticks; u16 playing_mask; u16 restart_mask; u8 patterns[16];
```

`ticks == 0` selects a Scene section: `bars` is 1–1024 and `scene` supplies the patterns. Nonzero `ticks` selects an independent clip snapshot, measured at 960 ticks per quarter note (3840/bar), bounded to 1024 bars per section. It stores all 16 pattern IDs and clip playing bits; `mute_mask` captures effective track/pattern/solo mutes. `restart_mask` identifies explicit launches/retriggers, allowing unchanged tracks to keep their sequencer/automation phase. At arrangement start and loop wrap all tracks restart. Playback uses a mute overlay and never writes recorded masks into persistent mixer bases.

The writer emits revision 2 for every Song and omits unused section slots. There is no positional revision-1 Song layout. Unknown revisions retain the normal opaque-chunk/default behavior; they do not provide an interpreted Song. A file without SONG uses the default empty Song. Pattern contents, live parameters, tempo automation and FX state are not embedded in an arrangement snapshot.

## Engine payload (`ENGN`) and the GUID registry

Optional `PNAM` revision 1 within `ENGN` holds an ASCII preset name (1–95 bytes, no trailing NUL). Empty pool slots are omitted from `SNDS` and decode as Empty. File preset attachment copies engine state; track strip and phrase state are not included in a Preset file.

`ENGN` payload: `u8 guid_length`, guid bytes, `u16 schema_version`
(= 1), then a sub-chunk stream. Engines are identified by a stable
string GUID (`src/engine_registry.h`), never by the numeric `Instrument`
enum value, so display order or enum order can change freely.

| GUID | engine |
|------|--------|
| `mk3.polysaw` | PolySaw |
| `mk3.sampler` | Sampler |
| `mk3.daisy-synth` | DaisySynth |
| `mk3.plaits` | PlaitsMacro |
| `mk3.fm` | FM |
| `mk3.ext-midi` | ExternalMidi |
| `mk3.acid` | Acid |
| `mk3.waveseq` | WaveSequence |
| `mk3.chipclock` | ChipClock |
| `mk3.tape-micro` | TapeMicrosound |
| `mk3.westcoast` | WestCoast |
| `mk3.spectral` | SpectralFreeze |
| `mk3.daisy-adrums` | DaisyAnalogDrums |
| `mk3.daisy-sdrums` | DaisySyntheticDrums |
| `mk3.drumkit` | DrumKit |
| `mk3.resonator` | native physical resonator, 3 models |
| `mk3.texture` | native input texture processor, 4 modes |
| `mk3.wavetable` | original oscillator, 128 CC0 AKWF waves; `WVTB` revision 1, 8 f32 values |
| `mk3.va-dual` | two MIT DaisySP oscillators / sub / noise / PWM / Ladder / editable ADSR; `VATH` revision 1, 18 f32 values |
| `mk3.fm6` | six-operator msfa adapter; `FMS ` revision 1: 21 f32 macro/detail values followed by a 128-byte packed DX7 voice. PATCH -1 selects embedded voice; 0–95 selects existing factory banks. |
| `mk3.soundfont` | TinySoundFont player; `SF2 ` revision 1: u8 asset slot then five f32 BANK/PROGRAM/TUNE/GAIN/VELOCITY values. The SF2 file uses the existing asset path + CRC identity; no PCM is embedded in the preset. |
| `mk3.unknown` | written for a runtime-unknown track that has no preserved payload; decodes back to unknown |

Sub-chunks inside `ENGN`:

| kind   | contents | written when |
|--------|----------|--------------|
| `PARM` revision 2 | discrete model u8, five normalized f32 engine values (initial 0.5), sample slot u8 | always |
| `SMPL` revision 2 | sample edit (+ zones), loop/reverse/slice state; no track root note | the engine uses the sampler block, or the block is non-default |
| `TXTR` revision 2 | fourteen texture floats, excluding track INPUT; DSP buffers and reverb are not saved | required or non-default |
| `CREA` | creative state | the engine is a creative engine, or the block is non-default |
| `KIT ` | drum kit definition (pads and solo mask; revision 2) | DrumKit only (as in MGXZ) |

The "or non-default" rule keeps save/load an identity for any valid
project while typical files only carry the payload their engine uses.
Which engines use which block is declared in the registry
(`EngineGuidInfo::uses_sampler / uses_creative / uses_kit`).

Runtime correspondence: the format separates the engine payload
from the common track state at exactly the `TCOM`/`ENGN` boundary
above. `TrackState` still co-locates the fields at runtime;
a future runtime-side payload split can follow this boundary without
requiring another disk-format change.

`RESO` engine subchunk revision 1 contains eight f32 values in parameter-ID order 600–607 (model, polyphony index, structure, brightness, damping, position, FM, transpose). `TXTR` revision 2 contains fourteen f32 values in declaration order (mode, position, size, pitch, density, texture, dry/wet, stereo spread, feedback, reverb amount, freeze, quality, trigger, gate). INPUT belongs to `TCOM`; input history and reverb buffers are not persisted. Unsupported engine subchunks retain the existing opaque/preserved payload path.

## Unknown engines and unknown chunks (fallback)

Decoding never rejects a whole project because of one engine:

- An `ENGN` whose GUID is not registered, whose schema version is
  newer than supported, or whose known sub-chunks fail validation,
  makes that one track `Instrument::Unknown`. The engine renders
  Unknown silent. The complete `ENGN` payload bytes are kept verbatim
  in the `PreservedStore` sidecar (never inside `ProjectState`, which
  must stay trivially copyable) and are written back unchanged on save
  for as long as the track stays Unknown.
- Unknown sub-chunks inside a known engine's `ENGN` are preserved the
  same way and re-appended after the typed sub-chunks on save, as long
  as the track's engine GUID is unchanged.
- Unknown top-level chunks are preserved in encounter order and
  re-appended after the typed chunks. A known chunk kind with a newer
  version is treated as unknown (preserved; the section stays default).
- Preservation is size-capped (2 MiB per engine payload / chunk,
  8 MiB total per store). Anything beyond the cap is skipped and
  recorded via `PreservedStore::dropped_unknown`, so the UI can warn before a lossy save instead of dropping data silently.
- Sounds (`SND `) follow the same rules, keyed by sound index.

Changing a track's engine is what discards a preserved payload: the
encoder only honours a preserved `ENGN` while the runtime track is
Unknown, and preserved sub-chunks only while the GUID matches, so a
stale entry can never resurrect an old engine over a user edit. The
confirmation modal belongs to the UI
layer; the codec behaviour and the `PreservedStore::clear_track` /
`clear_sound` hooks are provided as described here.

## Versioning

The current format version is 13. `format_version` and each chunk's `version` start at 1. A file with a newer `format_version` is rejected (the stream framing itself may have changed); a file in an older format is rejected with an explicit version error, and there is no migration or compatibility reader. Newer chunk versions within a format-13 file fall back to preservation as described above.

## AUDO revision 1

Project-wide 16 track roles (Instrument / Looper / Audio), with at most four non-instrument lanes. Each lane stores kind u8, length mode u8, bars u8, source u8, enabled bool8, gain/pan/feedback f32, then eight clips (asset u8, start tick u32, trim first u32, frame count u32, gain f32). Asset 255 denotes an empty clip. Placement is relative to Song start at 960 PPQN; audio duration uses source frames, not tempo stretch. Assets use the existing ASST table. Unknown typed revisions remain preserved chunks. Take undo is transient. See [audio tracks](audio-tracks.md).

Capture source values 53–60 select Group A–H post-insert/gain, before shared send returns, via the u8 routing capture source. Resample bar count is transient UI state, not persisted.

An unassigned Track is saved with the ENGN GUID `mk3.empty`. This is a normal silent state, distinct from the preservation fallback for unknown GUIDs (Unknown), and is the default for a new Track.

Performance/detail tier, declared value type and UI group are descriptor metadata, not serialized values. New GUIDs add their own state chunks; unknown-engine preservation works as described above. BAKE progress and the generated editor JSON schema are not project payloads.

FM 6 and SoundFont use descriptor-owned GUID chunks. The Vocoder is insert type 19 in the existing type/A/B/wet storage: A selects Input L/R/mono L+R at normalized 0/.5/1; B selects slot signal/Input L/Input R/stereo L+R at 0/⅓/⅔/1. Runtime choices use nearest index. Tracker is transient display state and serializes no duplicate phrase.

### Device theme

The single `ui.theme` byte retained in MGXC is a reserved field: it is read and discarded, and saves always write 0. Its value never causes a song to be rejected and is never applied to the device. In MDEV v2 (`GROOVE/DEVICE.CFG`, 16 bytes), byte 11 is the theme index 0–8, default 6 = CYBER 2. A v1 configuration has no theme, so 6 is supplied while the other device preferences are kept. Changing the theme does not change the MGXC version or any sound values.

## Synth extension chunks

`mk3.formant` owns FRMT v1 (9 floats); `mk3.granular` owns PARM v2 (sample slot) and GRAN v1 (12 floats). `mk3.va-dual` owns VATH v2, 26 floats; the base 18 values keep their order, followed by sync, phase PM, pitch envelope amount/decay, sub waveform, filter type, SVF mode and filter morph.

Optional top-level `PFXS` v1 is legal once in Preset or Kit content; the identical frame is optional in SND v2 and Track working copies. Payload is eight floats for two insert type/A/B/MIX values, Delay and Reverb send floats, then one u8 disable mask (bits0–1). Absent means preserve destination FX; present mounts with engine/assets in one undo transaction. Pool sounds and Layer children can retain their own FX snapshots under the LAYR revision 2 contract. Unknown revisions retain bytes and flags through PreservedStore. See [synth extensions](synth-extensions.md).


## Preset output trim (optional PTRM v1)

`PTRM` FourCC (`0x4d525450`), revision1, flags0, exactly one little-endian
IEEE754 float32 linear gain. Finite range **0.125–4 inclusive**, default1.
Standard writers omit gain1; absence reads as1. No container/PFXS/LAYR revision
changes. Legal once at Preset/Kit root, or once per Song `TRKS/TRCK` working
instance and `SNDS/SND ` v2 definition; not at Song root, standalone TRCK,
ENGN or Layer child. Known duplicate/flags/length/NaN/Inf/range errors reject
transactionally; future revisions use ordinary opaque preservation.

Trim belongs to the root sound, independently of INCLUDE FX. Mount/snapshot,
preview restore, Undo, pool, Song and user preset saves retain it; LEVEL id1
remains track-owned. The canonical content digest includes non-unity PTRM.
Audio applies gain once after common inserts, before level/pan/send splitting,
including Layer/Kit leaf send branches. Unity bypasses multiplication. Freeze
and dry/post-insert taps remain before trim; frozen playback and post-fader
capture include trim once. No new limiter or UI parameter.

## Step attributes

Step records are PSEQ revision 4. The flags byte in both full and extended StepState records uses bit4 (incoming slide) and bit5 (accent). The same record layout is used for frozen mutation sources. Missing bitmap entries decode with both attributes false. Writers reject runtime attribute bits other than 0–1; readers reject wire flag bits 6–7. Files in older formats are rejected before project publication; there is no migration and no user-file deletion.

Slide means tie plus 80ms logarithmic portamento for eligible single-note ACID/PolySaw/FM transitions. Poly/chord/arp/ratchet/Layer/engine or sound changes fall back to the existing trigger path. Accent is an independent ACID volume/filter-envelope stage and does not modify stored velocity. MIDI/SMF carry ordinary notes and velocity without custom slide/accent events, generated pitch bends or CC65.

Normal file admission requires all referenced engines to be registered, including
Layer children, pool definitions, engine Morph banks and parameter owners. An
unregistered engine produces `Unsupported engine / current content kept`; the
current project/sound is not replaced. Low-level codec preservation below the
admission boundary remains available for inspection and lossless roundtrip.
Runtime enum ordinals are not persisted; the 27 registered engine GUIDs are the
persistent identities.

## Layer children (LAYR revision 2)

LAYR revision 2 stores four u8 inclusive key/velocity bounds per child, before its
u32 payload length. Each child may contain an optional PFXS v1 using the same
41-byte payload as root FX. Root and leaf FX coexist. Preset identities are as
specified above.
