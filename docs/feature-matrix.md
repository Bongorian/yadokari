# Feature overview

What the instrument does, area by area. File formats are specified in
[project-format.md](project-format.md); the preset model is described in
[preset-system.md](preset-system.md).

## Performance & sequencing

- 16 persistent Tracks and 16 Patterns of up to 256 Steps. Steps carry
  probability, conditions, Swing, Ratchet, Gate, microtiming, and up to
  8 notes.
- [Step attributes](step-attributes.md): SLIDE and ACCENT on STEP and
  TRACKER steps, with marks and Undo. Tie/portamento applies to
  monophonic ACID, PolySaw and FM sounds, and ACID has an independent
  accent; polyphonic sounds use normal triggers for these steps.
- Sound design per step and track: 4 parameter locks per Step,
  2 automation lanes per Track, 8 macros × 16 routes, per-engine Morph,
  and Gesture.
- Clips and Scenes: independent Clips, 16 Scenes, quantized launch and
  Follow Action. The A–H keys are Track shortcuts.
- Song: up to 32 sections of placed Scenes and a performed Arrangement
  at 960 PPQN. A snapshot holds the Clip state; it is not a recording of
  all parameters or a tempo map.
- Composition workflow: hierarchical CLIP / SCENE / SONG display,
  TRACK ONLY copying, DUP + VAR pattern variation, HARMONY generation,
  batch Groove application, Scene duplication, a 5-section Song skeleton
  and NEW project templates.
- Tracker view: the same phrase and Undo as STEP, in 8 columns with
  center-fixed FOLLOW, per-column length wrap, and a `*` mark for hidden
  attributes.
- Editing aids: STEP RESET, long VARIATION editing, folder windowing in
  file lists, and on-screen reasons when a Layer child is rejected.
- Pitch input: 21 scales with unique successive degrees, a saved
  keyboard octave, and coupled root/key editing. Chords: 13 shapes ×
  4 inversions, with pitch/chord names and an explicit STEP editor.
- Performance strip: pitch addition, internal filter LFO depth / MIDI
  CC1, a 2-octave note scan, and Perform Morph/FX. Strip values are
  transient; there is no dedicated automation recording of MOD
  operations.
- [TWIN](twin.md): auto-capture of up to 16 notes in 1 velocity layer
  into Sampler zones, one-button switching between a synth and its
  sampled twin at the next bar, READY / STALE / ACTIVE states, and pairs
  saved in the Song.

## Sound engines

- 27 registered engines, all selectable from the ENGINE page and
  addressed by stable GUIDs: VA DUAL, FM 6, Plaits, RESONATOR, TEXTURE,
  WAVETABLE, GRANULAR, FORMANT/VOSIM, ADDITIVE, PHASE DIST, SoundFont,
  Sampler, Drum Kit and Layer, among others. The five Creative engines
  are original bounded prototypes, not vendor- or chip-compatible
  recreations.
- VA DUAL: 2 oscillators, PWM, sub, noise, Ladder filter and ADSR, with
  up to 32 voices. FM 6 is a 6-operator engine with DX7 bank loading;
  Plaits carries 24 models.
- [Synth extensions](synth-extensions.md): FORMANT/VOSIM and GRANULAR
  engines, VA sync/PM/pitch/sub/filter additions, ADDITIVE with a
  16-partial macro, PHASE DIST with 4 phase mappings and an editing
  ADSR, Pitch Shift and Rotary inserts, and arbitrary Preset FX
  assignment.
- RESONATOR: 3 original models, an internal exciter, 1/2/4 voices and
  8 parameters.
- TEXTURE: 4 original modes, 4 qualities, Freeze, per-engine recording
  source selection and 15 parameters. Freeze memory and the DSP tail
  are not saved.
- WAVETABLE: 128-waveform position scanning with a band-limited mip
  table.
- Polyphony: basic voices plus shared additional voices. Plaits sounds
  take 1/2/4 voices by declared cost against a shared weighted budget
  of 4.
- Layer: up to 4 independent children, no nesting, with gain/pan/detune,
  key/velocity ranges, per-leaf FX snapshots (PFXS) and embedded child
  Presets that travel with COLLECT. A Layer with unknown or missing
  material makes no partial sound.
- Tempo-synced LFO: beat lock with 12 divisions, registered as
  SYNC/DIVISION.
- Preset output trim (PTRM): root gain 0.125–4, applied after the shared
  Insert and saved/loaded independently of FX.
- SoundFont playback runs in RAM within TinySoundFont's supported
  range.

## Sampling & audio

- Sampler: WAV in PCM 8/16/24/32 and float32, mono/stereo, sample-rate
  conversion, trim/slice/reverse/loop/envelope/choke, key/velocity
  zones, Stretch and Freeze. File I/O runs outside the DSP, streams read
  resident pages only, and Sampler playback state stays separate when a
  Track's sound is switched.
- Drum Kits: 16 pads per Track, 2 layers per pad, per-pad sequence,
  mix, insert and send, and dual-grid Kit operation. Kits are saved by
  name; kit pad definitions live in the pool and in files, while phrases
  stay with the load target.
- Audio tracks and Looper: up to 4 of the shared 16 slots, loops of
  64 bars or free recordings up to 5 minutes, overdub with feedback,
  take Undo, and Song placement with trim/duplicate/delete. There is
  one recording path.
- Recording: Live Input or a selected bus, 4-second Capture, wet
  Resample and conditional Overdub. Capture and Main performance
  history are separate.
- Routing and FX: Track → direct/group → Main/Cue, 8 groups, 2 insert
  slots per Track, sends, a compressor, and preview/monitor. The
  routing is not an arbitrary connection graph. The Vocoder insert has
  16 bands, input L/R/stereo modulator, and a slot or external carrier.

## Content & files

- One content model: Songs `.mgxs`, Kits `.mgxk`, Presets `.mgxp`. The
  current format is MGXC13; the `.mgx` extension is also read, and
  ContentKind decides when an extension and its content disagree. There
  is no migration from older MGXC generations: older or corrupt members
  of the MGX family are rejected at load and are never deleted
  automatically. Undo history and DSP tails are not saved.
- Preset model: stable Preset/Kit GUIDs with revisions, fixed
  references into a bundled pool, a CRC index, User lineage preserved
  across saves with explicit Duplicate, and Track names independent of
  the attached sound. The pool holds 256 slots of named Presets with
  temporary audition.
- Library and Browser: the sound list is an independent Library page,
  BROWSER is dedicated to file shelves, and sounds are also shown
  inside ENGINE. Kit and Preset access uses one unified User/Factory
  list with preview/load and previous/next stepping. BROWSER loads
  asynchronously and offers SF2 / Preset OPEN, WAV Cue audition and
  assignment, Preset PREVIEW audition on the selected Track,
  Rename/Delete/New Folder, and protection of referenced and standard
  materials.
- Factory library: 187 items (163 Presets + 24 Kits), level-normalised.
  See [factory-library.md](factory-library.md).
- Projects: SAVE AS and current-song SAVE, Browser OPEN, name/type case
  handling, per-song COLLECT that gathers referenced materials, and
  boot-song memory.
- File transfer: Ethernet, or Wi-Fi in AP or STATION mode (selected
  explicitly; Wi-Fi is disabled in the 0.9.0 build),
  with shelves, capacity display, add/download/delete, code
  authentication, length / SHA-256 / type verification, reference
  protection, and timeout/interruption cleanup; plus the wired
  [MGF1 USB bulk / CLI](reference/usb-files.md) path. There is no GBX1
  file command and no USB mass-storage mode.
- Firmware updates: Ed25519 signatures, updates applied to the inactive
  KERNEL slot with trial/confirm/recovery, over Ethernet or USB-C
  control. Recovery and DATA are protected.

## Display & surface

- Two shared 480×272 displays, 20 main pages, 6 declared visualizations,
  a permanent two-line legend (name + current value), resolved-value
  snapshots of 8 visible targets, a fixed body and the 9-row TRACKER,
  with differential transfer at 30 Hz.
- One typeface (Barlow Semi Condensed), descriptor-derived Top / state /
  family / Strip icons, and an in-cell ARC shown only for the touched
  knob. Screen flows are described in [ui-pages.md](ui-pages.md).
- 9 colour themes. Track identification colours share the seven
  EngineKind categories across the LCD and the pad LEDs — see
  [track-kind-colour.md](track-kind-colour.md) — and the SELECT
  Track-selection map is bottom-left = 1. Level meters use the
  Track-kind colour for the body, motion for the hold mark, warn/danger
  for warnings and overload, and a neutral colour for non-Track meters.
  Pad LEDs use five colours plus off with time-based rules — see
  [led-behavior.md](led-behavior.md).
- Operation grammar: control types with left/right roles, Top
  descriptors on all pages, EVENTS Undo/Redo, STEP cycling and
  long-press, sticky A–H, touch + ERASE, quantized restart, and a shared
  Modal with history restoration; MIDI Import confirmation,
  in-progress cancellation and atomic application to a fixed target run
  through the shared Modal / ticketed Backend.
- Boot: the left display stays black and the right display shows the
  YADOKARI splash with the runtime version number immediately after USB
  enumeration — see [splash-yadokari.md](splash-yadokari.md).
- Device settings: DATA free space, pixel intensity, 3 LED brightness
  levels, Pad curve, digital input gain with L/R display, a monophonic
  tuner, and Local Control. There is no LCD backlight control.
- The 4-D encoder provides direction, rotation and press input paths,
  with time-limited HID input diagnostics.

## Connectivity

- MIDI: Notes, CC, Program Change, NRPN/RPN and Clock, with recording
  and Standard MIDI File exchange. An external Clock is followed as a
  tempo source. There is no SysEx route and no tempo-map import.
- Multitimbral mode: fixed Ch 1–16 → Track 1–16, pool selection by
  Program Change with Bank Select 0/1 × 128, and transport stop on
  entry and exit; normal routes are retained. AUDIO / Looper slots are
  not sound targets, and there is no SMF playback in this mode.
- Kit MIDI translation: each pad declares its GM counterpart (GMAP).
  Translation uses exact matches and explicit folds; in SMF import,
  unmatched percussion spills to a free track at its GM pitch and
  anything still unplaced is dropped and counted, all as one
  transactional Undo. SF2 drum kits (bank 128) translate pad notes to
  GM keys at play time.
