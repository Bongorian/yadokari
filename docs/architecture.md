# Architecture and ownership

UITEST is UI-only; only an explicit OPTTEST runs UI/DSP/Fluid continuously. The main task's diagnostic_busy is determined from the executing owner and excludes update preparation, USB transfer, and other diagnostics. DEPLOYCHECK drives the normal UI/SessionFiles without calling render, inspects text output with a private renderer, and restores the original state. Update snapshots are kept separate under GROOVE.

New edits use the existing staging and the single DSP owner and do not change saved definitions. Only the Groove bulk operation uses a temporary PatternState scope to swap Groove atomically; it writes to no other engine, Track setting, or phrase. Template enumeration, validation, and loading run on the SessionFiles file owner, and after loading a template is sent to the first SAVE AS as Untitled.

## Structure

| Layer | Main implementation | Responsibility |
|---|---|---|
| Data / codec | [project.h](../src/project.h), [project_codec.h](../src/project_codec.h), [project-format](project-format.md) | Bounded ProjectState, explicit MGXC fields, CRC / validation, preserved payload |
| Performance | [engine.h](../src/engine.h), [engine.cpp](../src/engine.cpp) | Sequencing, voices, DSP, timestamped MIDI, capture |
| Control | [performance.h](../src/performance.h), [control descriptors](../src/ui/control_descriptor.h) | UI mirror, input → command, Undo, reservation targets, display state |
| Files | [session_files.h](../src/session_files.h), [project_files.h](../src/project_files.h), [file_access.h](../src/file_access.h) | Browser, Project/Kit/Preset, Collect, recording, sample lifetime |
| Rendering | [ui_renderer.h](../src/ui_renderer.h), [ui/](../src/ui) | Shared pixels, figures/widgets/glyphs, redraw of only the necessary parts |
| Hardware | [kernel.cpp](../firmware/kernel.cpp), [Circle adapters](../firmware) | USB / SD / workers / update, ownership handoff |
| Host | [desktop](../platform/desktop), [examples](../examples), [tests](../tests) | Execution, regression, and image generation with the same engine and control model |

Linux/SDL is a development backend. The target is Pi 4 / AArch64 Circle.

The Top of every UI page consumes the shared descriptor provider, and confirmations block input under a single Modal owner. The 8 history entries retain sub-pages, focus, and positions.

Level rendering colors are shared through a pure color function that separates the current value's kind color, the hold marker's motion color, and warn/danger.

Unknown MGXC container children are preserved with their hierarchy in the [file-owned sidecar](../src/preserved_payload.h), and PREF likewise duplicates the corresponding Pattern/Phrase extensions. ENGN checks the declaration's required mask, flags, and duplicates, and does not mix missing state with defaults.

The shared step grid and the Kit's 16 pad lanes align rotation, repetition, and emptiness checks in [common sequence operations](../src/sequence_operations.h). Layer WaveRing material is assigned with a uniform shift that preserves each child's relative offset, and arbitrary material is judged from each leaf's declaration.

Long rhythm edits under VARIATION keep a bounded snapshot of the one target Track in Undo and apply only the phrase via [replace_track_phrase](../src/engine.cpp) and [install_track_phrase](../src/project.h). This is separate from the replace_track used for normal preset mounting, and the external MIDI / Mixer bases, engine/FX, and Morph keep their current values. On allocation failure, the original data and the PULSES/ROTATION display are left unchanged.

## The single canonical DSP owner and the private VA helper

`Engine::render_ports` is the sole owner of the canonical audio state. It performs no allocation, file I/O, or blocking locks inside render. UI/MIDI commands are received through a bounded queue, and stale notifications are quarantined by project generation. Sample views and metadata are published together. Plaits' upstream shared RNG is not safe for simultaneous rendering on multiple cores.

On the Pi, core 0 handles USB/UI/storage control, core 1 the display diff, core 2 the private VA helper plus project encode/decode, SoundFont decode, sample analysis, and live visual analysis, and core 3 the audio worker. The USB side consumes the worker's PCM ring; underruns become silence and a counter. On stop, disconnect, or project exchange, ownership is returned only after waiting for the worker-stop acknowledgement. Even on fallback paths, two DSP owners never run at the same time.

The VA helper publishes value-only voice snapshots into [fixed 32 job slots](../src/va_sample_slot.h). The private shadow is kept per fixed key — 16 main + 32 shared — and voices are not re-copied when the selection order changes. When the target is fewer than 12 voices, at most 8 jobs are used; at 12 voices or more, the earliest 3 voices remain on the canonical side and at most 32 jobs are selected. It computes only samples that use no shared math cache or coefficient updates. An empty candidate order is invalidated on command/trigger/shared-voice-ownership changes and rebuilt at the next quantum. While VA candidates exist, the order is rebuilt every quantum as before. On unchanged blocks without VA, not all voices are scanned every time; each sample still verifies the live instrument kind, env, and gate. FPCR, generation, allocation age, sample/voice/gate/frequency, and completion are checked and the result is adopted at the original render position; a late job is not waited for and is computed canonically. Mix/FX order and sounding count follow the canonical render; the VA16 guarantee and the shared 32 best effort hold. Plaits, Clouds/Rings, the shared RNG, the Sampler reader, and serial FX remain on core 3. The shadow is reused only across consecutive adoptions of the same voice and is resynchronized at gaps. The Pi 4's CPU-only heap is the 960MiB at C0000000..FBFFFFFF, separated from the DMA LOW/HIGH areas, and I/O goes through LOW scratch.

The Sampler is called directly from the render loop into a [dedicated render function](../src/engine_synth.cpp) and does not pass through the large frame and branching of the common all-engine function. Pitch and the conventional 5 controls are updated through a common helper, preserving the order of sampler processing, gain, kit watch, and profiling. The Sampler's bin/activity metadata is assembled only for blocks that are actually measured.

Sound notifications pass [prepared immutable records](../src/prepared_sound_queue.h) by SPSC token. The DSP producer owns 65 records and does not release a pin until the UI consumer finishes copying. The queue capacity of 64 notifications is maintained and generations are matched on reuse. Payloads needed by the next Song/Clip/Scene are prepared in chunks of at most 16KiB per block, and preparation in progress is invalidated on pool/project changes. For unpredictable launches, a bounded fallback copy is made at the same sounding time. Mounting retains all SoundState/kit/layer and does not treat identity by GUID alone as sameness.

The Sampler's page service, within the existing budget of 8 requests, bundles only the actual reads of the same service call in a [FileReadSession](../src/file_access.h). It does not open files on a resident hit and closes the cursor at the end. The 8MiB resident threshold, the DSP resident-page-only rule, and request order are maintained.

Detailed heartbeat/frame aggregation logs in normal operation are limited to the startup health record and explicit probes / `mk3trace=1`. Accumulated faults warn on change and do not repeat the same value. End-of-probe meter/trace is released, and unused trace events are not generated every block. LOG hex-encodes a 240-byte payload into the existing 512-byte reply and static_asserts the capacity including line ending and terminator.

The existing LOW/HIGH DMA areas are distinguished from the explicit CPU-only heap. When Circle's kernel reservation is changed, the app, recovery, and Circle libraries are all rebuilt with the same configuration. See [the build procedure](pi4-development-setup.md) and [allocation policy](allocation-policy.md).

The DIN [loopback probe](../src/midi_loopback_probe.h) is owned inside a core-0 task. After performance stops and input is released, it settles, stops normal MIDI output, and matches one real reception at a time. Test receptions are not passed to the Engine/transport. It ends in a bounded way on consecutive timeouts, disconnection, or send failure, and neither the DSP nor Project saved values own diagnostic state. For operation and scope, see [hardware](hardware.md#din-midi).

## Sequences and parameters

16 tracks, 16 patterns, 256 steps, 16 scenes. Each Track's Clip can play a different Pattern simultaneously. Scene launch and Song/Arrangement are control above that. Time is in rendered-sample units; the Arrangement is 960 PPQN. Clock input uses a tempo follower and does not guarantee physical phase lock.

The order in [parameter_resolver.h](../src/parameter_resolver.h):

`base → Morph → step automation → Gesture → macro → step/pad lock → Control All → smoothing / modulation`

Morph holds 4 sets of A/B, split between the common 409 values in [parameters.h](../src/parameters.h) and banks declared by engine GUID / local declaration. MODEL and ADSR are also resolved from engine declarations. Lock/automation/Macro/Gesture/Control All reference common declarations or engine + local declarations. Saving uses GUIDs; at runtime they are resolved to 16-bit handles only. Edit data is retained when the engine changes and is not applied to a different engine. Target selection is the common declarations plus the current engine's declarations. Unknown references are kept in a fixed 64-owner table inside the Project with their GUID and local number and are not applied to playback. Engine state and DSP connect directly to declarations, and the CORE/SHAPE/Native screens use the same references. Changing an engine aligns that Track's 4 sets to the current values, restored by Undo. The Group/Master banks are outside the restore scope of Track editing. Unassigned CCs are not made Morph targets on their own. UI edits, Undo, persistent bases, and temporary overlays are kept separate. Strip pitch/MOD are runtime overlays and do not rewrite the saved base.

An engine's GUID, display name, family, pages, local declarations, display conversion, limits/cost are centralized in [engine_registry.h](../src/engine_registry.h). State chunk encode/decode/defaults are called from the registrations in [engine_state_codec.h](../src/engine_state_codec.h). The binding that applies resolved values in declaration order to voice targets is used in the layered resolution of the Sampler, macro engines, Daisy ADSR, and Native engines. The existing Sampler/Pad operations and call APIs keep numeric-ID entry points, where they are converted to engine declarations. DSP engine processing does not go through this bridge. The code is split into 16 persistent TrackSettings and phrase-only PhrasePattern, with 256 sound slots. TrackState is limited to editing snapshots, and DSP/UI use prebuilt reference views. A Pattern copy carries only the phrase and launch-time override references. A mounted instance is copied at launch and is independent of pool editing. Actual allocation of the shared pool remains in [engine.h](../src/engine.h) / [engine.cpp](../src/engine.cpp). Plaits uses independent Track state plus shared additional DSP, capped at 2 voices for models 2–4 and 4 voices otherwise. No promise is made about the maximum simultaneous voice count across all Tracks ignoring shared resources.

## Routing and material

The fixed configuration is: Track processing, 2-slot inserts/compressor, Preset root trim, level/pan, direct or 8 group buses, Main/Cue, and shared sends/returns. Groups have serial/parallel processing. This is not an arbitrary FX graph. WAV preview uses the Cue path; Preset audition uses a temporary instance on the selected Track through the existing strip/output. Monitor and capture sources are separate settings.

Old immutable sample buffers live on while a previous voice still references them. A normal Sampler voice holds the source generation and resolved speed from its note-on and is detached from speed edits made after an engine/assignment/Pattern exchange. Edits to the same source are reflected in both mono and poly, and a step sound's loop/reverse/slice selection is fixed at note-on. [sample_stream.h](../src/sample_stream.h) passes only resident pages to the audio side; the DSP is the only producer of the request queue. Cache misses are not filled by file reads; they become silence and a counter. At save time, material paths are container-relative; at runtime they are DATA-root canonical.

At recording acceptance, the Track/Pattern/Kit pad/source/quantize are retained. Capture and Overdub also check target changes, generation, and revision. The 4-second Capture and the 1-second Main performance history are separate buffers, and changing the capture source clears the capture history.

## Saving and reloading

The [DATA structure](data-content.md) separates the restorable FACTORY originals from the user-edited copies in PROJECTS. In MGXC13, TYPE distinguishes Song/Kit/Preset; persistent Tracks go to TRKS, phrases to PTRN/PHRS, and identical phrase groups are compressed into PREF references and expanded at load. A Preset does not save phrases or general strip settings; only an explicitly ON PFXS carries an Insert/send snapshot. Files in older formats are not read; the MGX family and temporaries are retained at startup regardless of version or corruption. Validation, restoration, and organization of standard content happen only before the audio worker starts.

An MGXC candidate is validated and its material prepared before the project/sample is handed over. Loading replaces voice/transport state and does not restore FX tails or Undo history. Preservation of unknown engines/chunks lives in a PreservedStore outside ProjectState. Presets/Kits carry fixed GUID/revision/provenance and optional FX; songs save fixed references, bundled copies, and an independent TNAM. The [library/CRC index](../src/content_shelf.h) is owned by the file service, and material paths are hints for CRC matching. For details, see [the preset design](preset-system.md).

A normal save is snapshot → encode → temporary write → validate → promote. The boundary between the audio owner and the snapshot copy is respected. [project_save_slot.h](../src/project_save_slot.h) handles the handover with the worker; SessionFiles handles file control. File buffering, validation, and promotion are separated into `project_files.h` and do not depend on UI/Engine. Recovery candidates are only the current main and `.tmp`; there is no search over past backups/staged/archive and no save API for them. Promotion of a tmp when main is missing is validated with the current decoder; when main is corrupt and a tmp remains, the save is refused and both are kept. No Project space is reserved for a candidate whose file does not exist. This procedure alone does not guarantee FAT power-cut safety.

The current song's path is runtime UI state; the startup song is `GROOVE/CURRENT.TXT` via [project_location.h](../src/project_location.h). These are not MGXC schema changes. SAVE AS and Collect are separate operations.

## UI and boundaries

The hardware and Desktop use the same Performance/UiRenderer. Audio snapshots are asynchronous; audio is never made to wait even if the display falls behind. The scope does not display mismatches of instrument/model/pattern. Static body text is retained and updated on changes to values, context, dialogs, and the like.

[display_rectangle.h](../src/display_rectangle.h) builds 4px-aligned packets of at most 32 lines and compares the transfer sizes of 16/32 lines. The shadow is not updated until success. The lifetime of transfer packets and worker slots is maintained until asynchronous completion.

The UI has three axes — Focus/PadMode/StripMode — and three layers: pages with history, dialogs under a single owner, and displays with no input. There is no OverlayStack; Note Repeat and the like belong to their actual owning pages/pad grammar. See [UI pages](ui-pages.md).

Pads are encoded with the state colors and time rules of `ui/pad_feedback.h`, buttons with the semantic definitions of `ui/led_state.h`, and `Performance::led_frame` passes the state of the screen/recording owners. The recording target is not inferred from the selected Track; the reserved/active target is referenced. Page transitions record the explicitly designated screen in history, and STEP/SYNTH and the like are not implicitly diverted to PERFORM even within an audio frame. See [the lighting rules](led-behavior.md) and [the operation definitions](ui-pages.md).

The canonical sources for update raw-write boundaries, keys, and trials are [updates](firmware-updates.md) and [the threat model](update-threat-model.md).

### Native resonator adapter

`ResonatorDsp` isolates original resource macros and owns a control-time allocated Part and 32,768-word reverb memory per track. Rendering and reset allocate nothing. The single DSP owner restores per-instance upstream RNG state; this is not a multicore safety guarantee. A native 24-frame quantum runs at 48 kHz. MIDI strums serialize simultaneous notes (four-note chord spread at most 1.5 ms); analog onset detection/debounce is outside this MIDI boundary. Original internal 1/2/4 voice allocation and tails remain. Frequency-knob transpose (0–60 semitones) and FM apply to the original tonic/note fields; the bonus-only chord control is not exposed.

### Native texture adapter

`TextureDsp` owns its processor and original 118,784 + 65,408 byte buffers through `alloc_new`, allocated once per track at Engine construction. Original 32-frame/32kHz processing, 4 modes, 4 quality combinations and μ-law low-fidelity storage remain. One `Prepare` call precedes each Process on the single owner; unlike the original asynchronous hardware foreground loop, this cadence is deterministic. Each instance preserves upstream RNG state. DSP state, freeze memory and tails are reset on project load and Panic and are not serialized.

`TextureVoice` uses a 193-tap Kaiser FIR on the 96kHz common grid, rational 2:3 / 3:2 conversion and native int16 codec quantization. The fixed buffering is 48 host frames plus 1 ms FIR delay at each boundary (about 3 ms before native-mode latency). There is no extra amplitude envelope or native-buffer shortening. Input selection uses capture source IDs 0–60. Line In is the current sanitized frame; all internal sources use the previous completed frame. Separate next/previous arrays make track order irrelevant, including feedback routes. Source changes retain native recorded memory; mode/quality changes follow upstream resets. Texture runs continuously when selected, including stopped transport; Panic suspends it until Start, a trigger or instrument selection.

Freeze/Trigger controls use edges. Upstream Prepare can release Freeze after mode/quality reset without changing persistent parameter bases; the display reports actual BUFFER FROZEN/RECORDING telemetry. Toggle Freeze off/on to latch again. All 15 IDs use the standard resolver. The Cortex-M shift, unsigned fixed-point and table-endpoint safety overlays are generated outside immutable vendored sources; see the host validation JSON and provenance.


Native parameter banks check active resolver layers once per block. If Morph,
automation, gesture, macro, lock or Control All targets the bank, the standard
per-parameter resolver still runs in its original order. Otherwise clamped base
values (including a locked Sound snapshot) are copied without a persistent cache.
The texture boundary FIR uses the original 193 coefficients and tap order,
phase-packed coefficient vectors, mirrored history and AArch64 stereo-pair NEON
FMAs. The fixed 48/32-frame interface and latency apply throughout.

## Looper / Song audio lanes

[AudioLane](../src/audio_lane.h) is project-wide; four of the 16 tracks can carry loop or arrangement audio. Steps and native voices are bypassed on those tracks. Existing WavVoice/sample bindings feed the common track routing; shared strip controls belong to the persistent track. A versioned AUDO chunk carries placement and settings. SessionFiles reuses the single live-record queue and incremental overdub preparation/mix/verification, with generation and lane revision guards. Audio-lane edits have one independent take-undo state per track. The DSP owns the stream request producer; service performs all disk reads/writes. [Detailed workflow and limits](audio-tracks.md).

`Instrument::Empty` (GUID `mk3.empty`) is the unassigned state, distinguished from a real engine. It is the default for new Tracks/Projects, and a trigger allocates no voice and stays silent. Changing an engine to EMPTY stops existing voices. Name display on the shared SoftwareCanvas performs a clipped marquee over a fixed-length state array, and UiRenderer redraws only the band whose character positions move. PCM generation for the standard 3 kits happens during Factory preparation and does not enter audio rendering.

### Preset audition ownership

Preset preview stages immutable PCM views and a SoundState on the task side.
The audio owner temporarily mounts it on the selected track and keeps a private
restore image; the task-side project, preset pool and Undo history do not change.
STOP PREVIEW, edits, transport, clip changes and project loads restore the original
instance before their operation. Browser/track selection changes cancel pending
or active audition. Async decoding and asset verification finish before the single
mount command. Temporary asset slots exclude current track/pool/phrase/audio refs.
The existing track FX/output path is used; WAV preview continues to use Cue.

Saved names are Song `.mgxs` / Kit `.mgxk` / Preset `.mgxp`. The common MGXC reader does not guess the kind from the extension but judges by ContentKind, and the Browser enumerates 4 extensions including `.mgx`. See [the format contract](project-format.md).

Material references are enumerated by the descriptor's `EngineAssetMode` and [preset_assets.h](../src/preset_assets.h). COLLECT, Preset saving, and the free-slot judgement at audition/mount destinations use the same reference set, and pool-only material, audio/freeze, and sleeping sample locks are also protected. WAVESEQ relocation rotates the 64-slot ring and does not change offset values that double as fallback waveforms.

Engine declarations also carry performance/detail, type, and group. For the read-only schema and JSON generation, see [the engine schema](reference/engine-schema.json). Adding a category does not change existing device pages or saved values.

WAVETABLE has a read-only AKWF table, its own oscillator, and a per-engine WVTB state codec. State/voices are independent, and no allocation, FFT, or file I/O happens in the audio callback.

VA DUAL uses the MIT Oscillator/ADSR/LadderFilter of the same fixed DaisySP version. It has per-voice state and noise RNG and connects to the shared FX path. A new engine publishes its descriptor's performance/detail attributes, types, ranges, units, and groups, and can generate a schema for Web editing. Detail values are saved into the same engine state codec, and UI attributes do not affect the value format.

BAKE is temporary operational guidance in Performance and is not saved to the Project. It tracks its target by Track/generation/assignment revision. It advances to TRIM only after SessionFiles' existing resample completes, and leads to a 1-zone setting, a named Preset, and COLLECT of a single saved Preset. Asset CRC verification, relative path conversion, and immutable sample handoff use the existing rules. `PRESETCHECK` is a short functional test that hands saving, EMPTY switching, and remounting from the main loop to the owner queue, waits for the audio owner's mounted-engine telemetry, restores the originally mounted sound from memory, and deletes temporary files. It is not a test of audio load or listening quality.

Circle's [heap profile](../firmware/circle-pi4-multicore-4m.mk) holds reusable buckets up to 512MiB, so a 46000612-byte Project can be repeatedly allocated and freed in a normal save. The default 512KiB cap is not used. Save readback is verified only with the parser's private candidate and does not create an unnecessary Project copy. See [allocation policy](allocation-policy.md).

FM 6 calls the unmodified msfa block DSP from a fixed-size note adapter and embeds the 128-byte DX7 patch in the GUID state of `mk3.fm6`. External syx files are expanded into 32 Presets by the file service.

SoundFont holds the SF2 path + CRC in the existing asset table and passes an explicit resource pointer to the audio owner. 48 fixed voice contexts that share TinySoundFont's sample data are prepared by the file service, each pre-allocating 32 regions. The cache holds the lifetime of the data and voice contexts, and no resources are freed inside render.

The first example of an FX descriptor is the Vocoder in [fx_registry.h](../src/fx_registry.h). It retains the existing type/A/B/wet saving and declares the A/B choice meanings and external input requirements. The DSP receives the same frame's USB input as a sidechain and uses the slot input or an external input as the carrier.

The Tracker is display state of STEP only. It adds no persistent data and shares the existing phrase/selected step/edit commands/Undo. Each column wraps at its own length; during FOLLOW it shows each Track's current step on the center row, but while a knob is being operated it returns the edit row to the center. Normally 11 rows / 7 rows while operating, and POS during playback also remains in the column headings.

ADDITIVE and PHASE DIST have fixed-size independent voices, their own GUID chunks, and two-layer parameter declarations.

Layer embeds a fixed 4-leaf snapshot in GUID state, with per-leaf key/velocity ranges and leaf PFXS. Sounding first judges poly/shared resources and the declared cost, then reserves fixed voices for all layers from the audio owner. Layer voices are not subject to normal stealing. Asset collection and mounting pass through a one-level child walk.

MULTITIMBRAL is a transient audio-owner mode. The task side updates router ownership at the accepted switching epoch, and the audio side also constrains received channels to fixed routes. The PC uses the existing SoundAttached transaction.

Device settings are device settings with a 16-byte checksum, MDEV revision 1, independent of MGXC. SessionFiles' file owner performs saving, readback, and DATA free-space queries. Input gain goes from the command queue to the DSP; L/R peaks and the 1024-point tuner history go to the UI in a visual snapshot. The tuner's period analysis is performed only by the UI owner. Local Off MIDI notes have a bounded transmission-ownership table of 16 Pads × 4 notes and retry failed offs. Transmission ownership is released on switching, STOP, and Project load, and is not confused with sequencer or external-input notes.

The recording Circle FAT backend pre-allocates `.part` up to the maximum length and keeps a single cursor. It appends in units of 8,192 frames / 32KiB, syncing roughly every 1 second and at finalization. The recording queue is 65,536 frames, derived from measured write/sync stalls plus margin. At completion it truncates to the actual length and finalizes the WAV. SD waits in the recording, stream service, and asynchronous save service return to core 0's reentrancy-guarded UI pump, which processes HID, clock, LCD, and LED. FatFs/SessionFiles/hotplug are not reentered, and the DSP keeps sole ownership on core 3.

### Memory and runtime budgets for upstream engines

Fallible allocation for mounting and file processing looks at the reusable amount of the Circle heap buckets and a diagnostic headroom and refuses in advance. Preset mount Undo holds the before/after of an independent Track rather than a whole Pattern. Large SF2 payloads go to high RAM with a resident cache of at most 512MiB; voices sounding pin them. Only unpinned old cache entries are evicted, and a non-resident sound is refused with a reason until reloaded. A resource mount failure does not partially replace the original Track.

The RESONATOR/TEXTURE descriptors carry average/burst costs per model/quality. The single DSP owner admits the 16-Track total within a budget of 300µs average / 1,200µs burst and refuses excess requests as `DSP budget`. When a high-cost change to an already accepted engine is refused, the previously accepted value is kept. The Texture mode0/q1 declaration is 21/81µs and Resonator model2 is 21/80µs. Quality and upstream internal voice counts are not changed automatically. Rings String's coefficient cache and Clouds' integer popcount and windowed SIMD match the original computation and retain the provenance of generated derivative files.

The dynamic UI computes 30Hz release slots from a monotonic clock and allows drawing/packet preparation from 8ms before. USB submission and clean-frame completion wait until release, and late slots do not accumulate. The immediate frame on context change and the immutable ownership while blocked are preserved. Core 1 handles diff rectangles; core 0 handles frame drawing and USB submission. Reflection into the USB shadow and completion of the full-screen diff scan are counted per screen; rectangle counts are not converted into fps. Unchanged pixels are not transferred.

The audio worker's pre-generation target is 192 frames (4 ms at 48 kHz), the block is 24, and the fixed SPSC capacity is 256 frames, adding 2 ms of logical output latency over a 96-frame target. Single DSP ownership and the native admission budget apply.

Asynchronous Project saving splits its write and full-byte readback into 8 KiB pieces, advancing alternately with the normal loop's stream service. SampleStream's allocation of 8×2048 frames stands, and 3 additional page requests are issued at the first read rather than at the page end. There is no reentry into the filesystem from an SD-waiting UI callback, and the DSP remains the only request producer.

MIDI file import is owned by the file task as a ticketed [MidiImportJob](../src/midi_import_job.h): Queued → Reading → WaitingCommit → Applied / Cancelled / Failed. Acceptance snapshots the addressed Pattern/Track before any file I/O; editor selection is not an operation identity. The UI retains progress ownership and posts a matching cancellation ticket, drained before each file-task service, including after reentrant storage UI callbacks. Source revision or bounded readback rejects files changed during reading. Sequence publication addresses the captured target and distinguishes temporary queue pressure from invalid data, allocation failure and lost synchronization. One Engine replacement and one Undo record publish the whole sequence; cancellation and failures preserve the original.

The shared UI fixes its document viewport in [Layout](../src/ui/layout.h); independent knob cells, advice and result layers only cover it. [Font rendering](../src/ui/font_renderer.h) uses Barlow Semi Condensed in all roles with frozen tnum figures, centralizes cap/line metrics and uses integer pixel origins. [Header](../src/ui/header.h) and band widgets use the same metrics on both LCDs. File-result toast lifetime belongs to Performance, so rebuilding the renderer cannot replay stale results. [PageTransition](../src/ui/page_transition.h) is an interruptible sequence of at most three rendered frames; it never owns input or changes the 30 Hz slot / immutable display snapshot contract. Only changed LCD bodies slide. Pending launch sweeps consume published bar phase without scheduling audio.

Top icons and labels share a display projection of action descriptors; category/status/family providers own icon identity. [Icons](../src/ui/icons.h) use native 14px integer strokes independently of body anchor mapping. Knob touch/linger ownership is per control; a separate clean underlay restores active cells without accumulating alpha or sharing transition snapshots.


## Content parts and the live visual contract

[ContentValue/Row/Node](../src/ui/content.h) are passed to the common Widgets, which draw names, fixed-value fields, bars/segments/chips, list hierarchies, Mixer strips, and signal chains centrally. Physical assignment follows the shared screen grammar.

[VisualDeclaration](../src/visual_model.h) carries Scope/Spectrum/Bars/Ring/Curve/Response, source, cadence, fallback, parameter model, and caption. Both EngineDescriptor and FxDescriptor provide declarations, resolved to the same 6 drawing primitives as the adapters. The procedure for adding a new engine requires, in addition to GUID, schema, codec, binding, cost, and unknown preservation, the selection of this declaration. For unpublished internal state, Scope is chosen and no virtual particles or the like are created. FX additions likewise register a type/control definition and a visual declaration, distinguishing models from measured points. The common renderer is extended only when the vocabulary grows.

The DSP dispersively copies the latest 512 mono points of Input, the selected Track post-fader (or the existing Capture source), and Main post-limiter into memory. A [single analysis slot](../src/live_visual_analysis.h) passes them under SPSC ownership, and core 2 computes Hann/FFT512 and 64 log bands after saving, loading, and material analysis. There is no FFT or waiting during SD reads or audio callbacks. The UI checks generation, Track, source, and the 250ms deadline, and when busy thins out 50→100→200ms. The same analysis on Desktop runs on the UI thread. Interpolation uses only measured endpoints, and phase wrap is applied only to periodic displays.


## Fixed USB output requests and ACKs

The latest LED state and the successfully sent state are kept separate, and the task side sends HID through an asynchronous queue. Left and right screen packets are sent with at most 2 ordered bulk requests, holding the buffer/snapshot until each completion ACK. Circle's minimal reset/reuse patch pre-allocates the requests for audio TX/RX, HID IN, screens, and LED, removing per-transfer allocation/freeing from callbacks. Audio capture cadence, implicit feedback, the DSP owner, format, and UI pacing/gate are maintained. See [ownership and limits](hardware.md).

## DSP measurement and calibration

Per-engine/variant active voice time, track-shared native DSP, Insert/System FX, and control are measured with generation-scoped counters. Core 3 owns the counters, and the primary reads only the immutable results of Ready. Fine-grained measurement runs once every 127 blocks; control boundaries run every block. Unless diagnostics are started, the counter clock is disabled. Core 0 splits and drains the results after a fixed window ends.

Layer's 12/24/48 and the physical pool 32 are aggregated into [VoiceBudget](../src/voice_budget.h). Cost is the measured maximum ×1.25, rounded up to 6.25µs units, and never lower than the existing descriptor reservation. The per-track weighted budget for normal independent engines is scaled proportionally, preserving the conventional sounding counts. Plaits' shared weighted budget stays 4, giving 1/2/4 voices by model cost. The overall 48 admission does not apply to normal non-Layer sounding, and pool 32 is not a CPU guarantee.

Native reset on Panic/song load invalidates immediately and initializes with the same seed before the next use. Old buffers are not output, and initialization/RNG are handled only on core 3. Recording write = core 0 and codec/analysis = core 2 are maintained. No offline DSP or audio owner on another core is added.

## Asynchronous loading and the host build

Asynchronous loading checks the current header/version/size before reserving, and decodes using only core 2's default template and reuse candidates. Preservation of the public decoder's current destination is maintained; a failed private candidate is reclaimed and returned to the default before the next use. Cancellation frees immediately during a read; while a worker owns the slot it is processed as cancel request → result reclamation → recycle. Slot destruction is limited to after the consumer stops.

The host audio implementation and DSP helper cpp files are shared objects with sanitizers, and the serial/parallel/full-host tools use the same test-run inventory. When adding a target, confirm that no C++ fixture leaks out of the inventory. Processing under different flags, such as profiling/TSan, is not mixed.


## Screen types, physical wiring, and current values

Against the types in [screen_grammar.h](../src/ui/screen_grammar.h), [page_descriptors.h](../src/page_descriptors.h) classifies the actual contexts. The role derived from a page provider's actions is passed to `ControlPresentation.work` and wired to physical slots in `Performance::top_slot`. Labels, enabled/selected, input, and LED use the same wiring. Apart from the management screens' sound-snapshot duplication and the Diagnostics invariant sub-page state, confirmations are consolidated into the common Modal owner. Roles, widths, and types are audited across all themes' generated controls and screens.

The legend is two rows of names/values at y242–271, the body is y64–241, and the touch overlay is y186–237. TRACKER keeps a 16px pitch and 8 columns over 9 rows. Current values are passed from the UI to the audio owner for only the 8 visible targets as 32-bit atomic keys (Track + target), and the resolved values and keys are stored together in the existing 50ms AudioVisualFrame. The UI adopts them only when generation, Pattern, and key match, and while editing a lock the selected Step value takes priority. No Audio-time allocation, I/O, waiting, or additional DSP owner is added. The common [channel configuration](../src/channel_controls.h) is used for Mixer/Morph/Gesture/Tone. Figures from declaration recipes are made from the current edit values; the residual values of the last voice are not drawn as the current setting.

## Input boundaries

Input contract: keyboard pads enumerate unique scale degrees;
explicit chord shapes retain chromatic intervals. Eight-note phrase capacity is separate
from each engine's voice budget. Step chord edits publish one undoable full cell.
Kit GM declarations live in engine-owned GMAP, while pad addresses remain 36+pad.
SMF Kit import publishes the selected Kit and Empty spill tracks in one ApplyPattern
transaction; target/source/generation checks prevent partial or stale publication.
Wire Kit gates own GM Note Off independently of PCM availability and voice stealing.
SF2 bank 128 maps pad pitches once, retaining native external-MIDI pitches on record.
Unified Kit browsing reuses the transactional preview/load path without filling the pool.

## User file transfer

[UserFiles](../src/user_files.h) owns shelves, kind validation, hashes, temporary storage and promotion, and reference protection, and [FileWeb](../src/file_web.h) maps them onto HTTP API v1. The [hardware adapter](../firmware/file_transfer.cpp) owns Circle TCP / AP DHCP / mDNS and the mode lifetime. Before a transfer, USB audio is stopped and the DSP worker joined; FatFs is handled by the file service's single owner, and afterwards the normal audio reconnection path is taken. HTTP and the AP exist only after a physical START, and on exit/timeout the listener, clients, and owned tmp files are destroyed. A large PUT processes one HTTP request in 32KiB pieces, and GETs are also read in small pieces. The host loopback adapter is for verifying the same core and does not replace the product backend with Linux. No GBX1 file command or mass storage is added. USB connects from the dedicated MGF1 bulk entry to the same UserFiles.

## Transfer and device appearance

[UserFiles](../src/user_files.h) owns the permitted shelves, ContentKind, hashes, and reference protection shared by Wi-Fi HTTP and [MGF1 USB](reference/usb-files.md). The kernel joins the audio worker before a transfer starts and makes SessionFiles jobs and updates mutually exclusive. Reception is processed in 32KiB pieces, and same-name files are replaced via the verified plan of [FileReplace](../src/file_replace.h). At startup, recovery runs before factory/song loading; unknown candidates are not deleted but halt processing. This is separate from a physical power-cut guarantee inside FAT/SD.

The theme owner is DEVICE.CFG of MDEV v2. Reserved bytes in song UI are read-and-discarded / saved as 0, and loading a song does not change the device theme. Tokens generated from the [fixed 9 themes](reference/themes9.json) are the sole coloring for LCDs and logical LED RGB. MK3 index quantization is separated into the transport profile, and physical SingleColor lights and uncalibrated RGB are not supplemented with invented values.

## Installation and display at startup

The Factory boot gate checks the build version, catalog revision/hash, and target stat, and in a normal boot does not invoke the factory's full synthesis, re-encode, or full-byte comparison. The marker is invalidated before full processing and a synchronized marker is promoted only on success. Immediately after USB enumeration and before the SD, `CKernel::Initialize` sends the all-black left / fixed splash right, and hands the ACKed shadow and CircleDisplay over to Run. [The YADOKARI version number](splash-yadokari.md) is added by Run at the lower center of the right side from the same STATUS value and reflected into the shadow through the same USB ACK path. The first static send is not delayed. The normal diff worker and drawing grammar are unchanged.

A pad's ground color decides the target Track from its role and passes the current EngineKind to the `engine_kind_color` shared with the LCD band/chip. MONOTONE projects only the ground color and chip to gray, and the wire preview decodes the final code. Mixer's held peaks are centralized in the UI-owned `MeterBallistics` and passed to strips and numeric values. Sample overviews are 440 bins for both resident and stream, and `SampleWaveCache` holds only the final completed values whose source, format, and Project generation match. Analysis and I/O belong to SessionFiles/workers; the no-worker resident fallback reads at most 4096 frames per UI tick and adds nothing to DSP rendering.

[Synth extensions](synth-extensions.md) adds descriptor-backed FORMANT/GRANULAR and Pitch Shift/Rotary inserts. Granular has four separate Clouds resource slots on the one DSP owner. Root preset PFXS applies within the sound/assets mount transaction; child/pool SoundLeaf remains FX-free.

Step SLIDE/ACCENT are saved into phrases as common 2-bit attributes, and STEP/TRACKER use the same commands/history. See [step attributes](step-attributes.md). A valid next step before the gate deadline is read ahead exactly once to hold a tie; when a monophonic engine/sound arrives, the pitch ramp is updated without rebuilding the voice. No new allocation, I/O, or locks are added to render.

## Engine translation units and reservation boundaries

The [common source inventory](../src/engine_sources.mk) is shared between the host and Circle. Engine generation/resources are in engine.cpp, parameter resolution in engine_parameters.cpp, sequencer timing in engine_sequence.cpp, voice allocation in engine_voice.cpp, recording/freeze in engine_recording.cpp, launch/song in engine_launch.cpp, commands in engine_command.cpp, engine DSP in engine_synth.cpp, and sample render/mix in engine_render.cpp. The translation-unit split does not change the sole DSP owner or the parameter resolution order.

The 96 Pitch states are reserved in bulk in an Engine-internal fallible Pool before audio starts and bound exclusively to each slot. The Pool is constructed before the slots and destroyed after them. No reservation happens on automation-driven FX selection, reset, or render. A pool failure connects to the existing stage_project refusal and does not report success with partial sounding. Initialization of a standalone slot fixture uses an independent fallible reservation. See [allocation policy](allocation-policy.md).

Asynchronous SoundFont loading hands decoding to core 2 via [SoundFontLoadSlot](../src/soundfont_load_slot.h). The worker does not touch the real filesystem and requests reads of at most 64KiB from the task. Even on cancel, scratch is not freed before the read response. The Browser's SF2/Preset OPEN proceeds while retaining the original sound, and does not mount if the original Track/generation/asset dependencies have changed. The synchronous API is kept for boot and host/probe. UI/input and the DSP stay in the normal loop, and no render owner is added for decoding.

Circle translation units use a common -O3 with small inline budgets and hint multiplier constraints; a kernel-only -Os is not used. Only one common-header drawing function uses a size-priority code-generation attribute on both host and Circle; DSP settings are unaffected by this exception. Large Performance/renderer functions are shared across 11 UI translation units, and explicitly designated leaves such as pixel blend keep inlining. The empty constants of SoundState/Layer refer to the single entity in state_defaults.cpp. Even under link section GC, the signature identity is kept as an explicit root, and the boundary including 4MiB of BSS, a 64KiB margin, and the known boot stack are checked at the build gate. These are not substitutes for DSP deadlines.

The immutable AKWF bank is restored exactly once at control initialization from its own reversible Rice storage. The immutable pointer is published after the full-bank CRC, and no restoration or allocation happens inside audio. The task-owned read session has independent FILs for SF2 jobs and general checksum/Project readers and reuses the cursor across sequential reads. Worker cancellation waits for the read response and the join before closing.

The LCD worker scans an immutable frame's dirty bands/extents exactly once and reuses them for the split packets within the same frame. The ACK revision and rectangles passed by control erase the diff of the private scan plan. Currently, a predicted ACK for only one pending rectangle per LCD is passed, overlapping preparation of the next packet with the wait for transfer completion. The real shadow/valid/cursor are updated only on real ACK success; on failure or epoch change, the advance preparation is discarded and a rescan is made from the real shadow. Changes to epoch/frame/order/buffer trigger a rescan. The plans for the two LCDs are independent, the cache is changed only by the worker, and ACK information is passed in an SPSC job. RLE consolidates short runs into literals and preserves the pixels after decoding. A FIFO of at most 2 requests for left and right and the frame clock/30Hz are maintained. Extra packet pages are reserved as only one per LCD, falling back to sequential preparation when a reservation fails. Completion reclamation is placed before the cadence judgement, and worker results/prepared packets are processed without waiting for the next normal poll.

RESONATOR's damping endpoints, in a generated TU after pin/hash verification, replace only the lookup call with the adapter. Non-endpoints use the same interpolator, endpoints use a valid final element, and the vendor original is unchanged. The VA reset template / single-argument math memo are exclusive to the Engine's DSP owner and copy value-state into voices. Sound times and independent state are not shared. The 20Hz visual clamps directly only when it can confirm all layers are empty, and otherwise returns to the existing resolver.

The LCD maintains the existing frame clock/ACK shadow/SPSC and allows the full 272-line height and dirty-band coalesce candidates within an extra 2048 bytes. Speculative encoding of wide candidates is limited by the existing raw cap guard. USB request completion and LCD drawing completion are not the same thing.

## Diagnostic accounting for the same frame/block

[DisplayAttribution](../src/display_attribution.h) ties frame/side/job/request together and records the times of draw/copy, the single encoder worker, the shared bulk FIFO, callback/reap/shadow completion, and cadence. Exclusive intervals are computed at analysis time from the same maximum frame; component maxima from other frames are not added. A coalesce candidate is re-encoded only when the union raw cap on 4-pixel boundaries fits within the packet capacity and the existing +2048-byte budget, and immutable pixel/shadow ownership is maintained until the existing ACK succeeds.

[RenderAudit](../src/render_audit.h) and [AudioSlowTrace](../src/audio_slow_trace.h) hold the command/reset/control/total and phase boundaries of the single DSP owner, and after sealing the task logs them. Normal firmware uses GROOVE_DETAILED_RENDER_ATTRIBUTION=0 (no additional sample-loop clocks). The frequency of the existing DspProfile and the all-block clocks are maintained, and a VA16 excess saves the existing tick of the same block. Measured values of the detailed diagnostic build and the normal build are distinguished, and the attribution record is canonical.

The MGXC originals of the factory LIBRARY are managed in assets/factory-library, and the derived bank for ROM is reversibly compressed with GBL1. At boot/file-service the bank is reserved once and SHA256-checked; reusing the same file scratch, files are expanded one by one and SHA256-checked, then passed to the normal factory repair and marker/stat gate. PCM/SF2 are shared DATA material and do not change preset values, FX, ContentKind, the pool, or the audio render path. See [the library contract](factory-library.md).

### Definition and material ownership

Canonical pool registration and working-Track mounting are separate operations. `PresetRegistration`, `TrackInstall`, and `AssetClonePlan` separate each role by type. ROOT/BPM branch at copy time as the acoustic interpretation of the material, and shared PCM stays immutable while references are added. Favourite changes only a UI attribute by a separate command. Reference enumeration and placement planning are centralized in `preset_assets.h`, using the same rules for Sampler/Granular/SF2/Kit/Layer/WaveRing.

Unknown definition bytes are the immutable records of `DefinitionExtensions`; current container metadata is the `PreservedStore`. Track/SoundState/Undo hold runtime cookies, and the file service composes the sidecar at save time. The original scopes of root / Preset TRCK / Song SND are carried in PUNK. The owner is not guessed from a slot or display name. See [the preset system](preset-system.md).

Sound selection is the independent Library page; the file shelves are the Browser page. They share the index's 64-line display buffer but separate transitions, history, and input providers, and the Library is not a hidden sub-screen inside the Browser. For an inspect of the list inside ENGINE, the file service merely narrows the existing index; no I/O during drawing, implicit mounting, or pool registration takes place. See [the UI addition rules](ui-pages.md).

ENGINE exposes all 27 registrations to manual selection; selection uses no visibility flags or sparse index conversions. The saved GUIDs are invariant, and the enum is a runtime-only consecutive numbering. Adoption of a normal file validates the registration of all engine references, and if unsupported, keeps the current content and returns an explicit error. The codec retains unknown bytes for round-trip saving.

Library entry, movement, and KIND/ENGINE/ORIGIN changes are operations on index metadata only and do not start audition decoding or material preparation. Audition happens only when requested by Top5 PREVIEW, and the next selection cancels an unfinished audition as well. The previous/next Preset shortcuts in performance are retained as explicit mounting. The 6 DAISY standalone sounds are code-generated factory Presets. The 6 WAVs baked from actual VA/FM DSP output are retained. The current Library holds 187 items and verifies fixed GUIDs and material CRCs. Sounds are not re-baked during startup, and no live synth is added on top of Kit pads.

## TWIN preparation and live exchange

[TWIN](twin.md) reuses the bounded freeze capture handshake and dry track tap.
The DSP owner triggers one key, isolates the target from output/feedback buses,
and acknowledges a float capture slab. The file owner trims and appends notes
to one WAV, constructs normal sampler zones, then submits one atomic pair/asset
installation. Cancellation never replaces the live track. Prepared material uses
an explicit resident load in the existing immutable sample cache; ordinary
streaming thresholds and cache budgets are unchanged.

Two embedded pool definitions and an optional Song association persist the pair.
The existing attachment path exchanges it at a bar boundary, retires source
voices, retriggers held keys and applies a 64-sample output transition. No source
engine continues rendering while the Sampler is active; heavy per-step sound
locks and clip overrides are bypassed until return. Track strip FX remain live.
