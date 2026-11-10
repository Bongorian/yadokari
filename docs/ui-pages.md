# Page and operation definitions

[page_descriptors.h](../src/page_descriptors.h) defines the names, geometry and Pad grammar of the 20 pages. [Performance](../src/performance.h) handles input and transitions, and [UiRenderer](../src/ui_renderer.h) handles drawing. Operating procedures are in the [manual](user-manual.ja.md).

| Page | Main edit targets / entry points |
|---|---|
| PAD / KEYBOARD / CHORDS | Playing Tracks, Kits, scales and chords |
| STEP | Step selection, notes, the 256-Step pages, Locks |
| CLIP | Starting/stopping Patterns per Track, Follow |
| SCENE | Saving/recalling Clip configurations, Chains |
| SONG | Scene configuration / Arrangement takes |
| SYNTH | Generator / Tone / Shape, Engine selection, the Creative ENGINE LAB |
| SAMPLER | Playback / Trim / Slices / Wave / Zones, Kit layers |
| MODULATION | LFO / Automation / Macro Map / MIDI / RPN・NRPN |
| FX | Sends, Compressor, Track/Pad Inserts, Chains |
| ROUTING | Main/Cue, Groups, recording sources, Capture/Resample |
| MIXER | Level, Mute, Freeze |
| PERFORM | Morph, Macro, Gesture, Mutation, Live FX, the AUDIO entry |
| GENERATE | Rhythm, Accumulator, Groove, Lanes, Arp |
| PROJECT | SAVE / OPEN / NEW / SAVE AS / COLLECT / FILES / STORE POOL / RECALL POOL |
| PRESETS / KITS | The unified sound list of KIND × ENGINE × ORIGIN. Entered from ENGINE by a 4-D press, and from the performance-face Top5 |
| BROWSER | File shelves, preview, loading, file operations. The physical BROWSER key always goes here |
| SETTINGS | APPEARANCE / DISPLAY / LIGHTS / PAD / AUDIO / MIDI / SYSTEM. Three categories on the left Top plus MORE; the 4-D selects an item, K1 changes the value. Related exits are on the right Top |
| DIAGNOSTICS | USB / HID / Audio / MIDI / Log |

In PROJECT, ASSET is K1 and POOL SLOT is K4. The list is selected by 4-D rotation, and a press executes PROJECT FILES / STORE POOL / RECALL POOL. Nothing is listed twice with Top7/8. In SYNTH, the left Top holds GENERATOR / TONE / ENVELOPE and the like, and the right Top holds ENGINE / MODULATION / sound saving and the like. Layer USE CHILD / REMOVE are an exception placed on the left Top3/4, which have no face-specific page. The right Top5–8 keep the common column ENGINE / MODULATION / STORE POOL / RECALL POOL. In a Kit, CORE selects a child Pad and TONE is the common channel editing. During BAKE guidance, Top8 is ROOT / ZONE → SAVE PRESET; after saving, the flow is PROJECT Top5 COLLECT → Top8 DONE. For the exact per-screen assignments, see the [generated table](manual/controls.tsv).

## State and input ownership

[UiAxes](../src/ui_axes.h) handles the three independent axes Focus / PadMode / StripMode, the page layer (a history of 8 entries), the dialog layer (a single Modal owner) and the input-less display layer. The unused OverlayStack has been removed. NOTE REPEAT / Arp are modifiers of the pad grammar, Control All / Perform FX belong to PERFORM, and Step Edit is a state inside STEP. The history takes the page, sub-page and focus as identity, and also saves the cursor, Track, Browser directory and the like. A visit inside PERFORM is also independent. HOME/RETURN and Mixer Peek use the same snapshot. This is not an Undo of audio state.

On every page, the Top labels / enabled / selected / action are wired to physical positions by the per-page `*_controls.h` providers through `top_slot()`. The same wiring is used by input, drawing and the LEDs, and the types and roles of the [screen grammar](../src/ui/screen_grammar.h) are audited in the generated table. The [common Top](../src/common_top_controls.h) and the [confirmation Top](../src/modal_controls.h) take priority over normal pages. `ControlDescriptor.icon` is resolved by the providers of Top / category / status / family. The single owner/value of [ModalState](../src/ui/modal_state.h) is the canonical source for confirmations; earlier confirmation names are typed accessors, not independent booleans. Browser naming payloads also take priority at the common input gate and suppress input to what is behind them. For MIDI file Import, the UI Modal also holds the target path, Pattern, Track, channel and Project generation, and passes them to SessionFiles exactly once on 4-D confirmation. The Modal stays up while processing runs and can be cancelled to the left. After cancellation, confirmation is asked again. Processing after confirmation is independent of the current screen selection; editing the real target or replacing the Project cancels it while preserving the original data.

The header always shows the Track/Pattern/sound source and the difference between the edited Pattern and the playing Clip. Unassigned knobs return no parameter ID, and never editing the values behind the ENGINE or a naming dialog is a regression target.

RESONATOR: Top 1 RESONATOR maps K1–K7 to Model / Structure / Brightness / Damping / Position / Level / Pan; K8 is unused. Top 2 TONE retains common processing. Top 3 EXCITER maps K1–K3 to Polyphony (1/2/4), FM, Transpose; K4–K8 are unused. Top 6 MODULATION / Top 5 ENGINE retain the shared navigation. The generator hero is a measured post-fader PCM spectrum; no hidden resonator state is invented.

TEXTURE: Top 1 GRAINS maps K1–K8 to Mode / Position / Size / Pitch / Density / Texture / Freeze / Level. Top 2 TONE is common. Top 3 BLEND maps K1–K6 to Dry/Wet / Spread / Feedback / Reverb / Level / Pan; K7–K8 unused. Top 4 INPUT / QUALITY maps K1–K5 to Input / Quality / Freeze / Trigger / Gate; K6–K8 unused. Top 5 ENGINE, Top 6 MODULATION, Top 7 STORE POOL, Top 8 RECALL POOL. Quality labels show 32k/16b or 16k/8b and stereo/mono. The diagram explains parameter shape or dry/wet/feedback routing; it does not invent internal grain positions or FFT observations.

POOL SLOT has 256 slots. In PROJECT, K4 POOL SLOT selects the target slot, and STORE POOL / RECALL POOL in the list are executed by a 4-D press. On sound-target faces, registration/mounting are also placed on the free Top7/8. Registration into the pool and named file saving in the Browser are separate operations.

The canonical entry to LOOPER / AUDIO is **PERFORM → Top8 AUDIO**. DIAGNOSTICS is **SETTINGS → SYSTEM by 4-D left/right → Top7 DIAGNOSTICS**. Shortcuts from SONG and implicit diversion by audio role have been removed; STEP / SYNTH / SAMPLER / GENERATE open the designated screen. [Audio operation list](audio-tracks.md).

## Common operation rules

- **Top1–8**: press the display label in the upper row. Targets are page selection, executing a process, and confirmation. A dark Top cannot be executed. **K1–8**: edit the value corresponding to the name / current value in the two lower rows. Only the ARC overlay of a touched/rotated cell closes about 1.2 seconds after release; the permanent values and the body area do not move.
- **EVENTS / SHIFT+EVENTS**: Undo/Redo common to all pages. During confirmation or naming, the owner takes priority. **FIXED VEL**: toggles fixed Velocity. **Pressing STEP again**: 16→64→TRACKER; holding 400ms: note editing.
- **A–H**: Track selection on the sticky layer. With SHIFT, layer switching plus selection; with MUTE / SOLO / SELECT, Track operations; holding 400ms opens Mixer Peek. In ROUTING, the Group family selects a bus.
- **Knob touch + ERASE**: returns to the declared initial value, and is Undoable. Selection and movement are excluded. **SHIFT+RESTART**: restarts at the next bar head.
- **Confirmation screens**: confirm with a 4-D press, cancel to the left. Top1/2 show the same confirm/cancel. While a process is running, only cancellation is available.
- **4-D rotation / up-down**: selects the target of the current list, Step, Track and the like. **Press**: executes, or switches to value editing. Some screens have no assignment for the press.
- **4-D left/right**: sub-pages of the same screen. In the Browser, parent / Open; in Clip, Scene, Song and the like, selection movement. SHIFT+left/right selects the edited Pattern; SHIFT+rotation selects the Track. During name entry, left cancels and right is the character cursor. K1 can select the left/right position.
- **The standalone page left/right buttons**: go back/forward in the history of pages, sub-pages and focus. Holding both for about 400ms goes HOME to PAD, and again for RETURN. This is a different operation from 4-D left/right. Selecting a main screen separately discards RETURN. HOME is suppressed during confirmation.
- The physical entry to GENERATE is VARIATION (HID byte5 bit1). GENERATE generates note material by recipe; PERFORM's MUTATION mutates, freezes and commits existing notes.

| Screen | 4-D rotation / up-down | Role of 4-D left/right and the Tops |
|---|---|---|
| PAD / KEYBOARD / CHORDS | Track; in a PAD Kit, the Kit Pad | The KEYBOARD left Top is SCALE / OCTAVE; CHORDS is SHAPE / INVERT (SCALE / OCTAVE with SHIFT). The right Top is PRESETS / KITS depending on the track kind, CHORD EDIT / UNDO / REDO. The basic 4-D grammar is retained |
| STEP | Step | 16 Steps at a time to the left/right. A press toggles Note ON/OFF |
| CLIP / SCENE / SONG | The edited Pattern / Scene / Song section | Left/right also select the target. Starting and configuration editing are by press / Top. Playback of a recorded Song take is PLAY |
| SYNTH | Track; during ENGINE, the Engine | Left/right cover the range GENERATOR → TONE → sound-source-specific pages. Screen entries such as MODULATION / TRIM / ENGINE are selected explicitly from the Tops |
| SAMPLER | Slice / Kit Pad | Left/right move through PLAYBACK / TRIM / SLICES・LAYERS / WAVE / ZONES. The Tops handle the page and asset processing |
| FX | Track; in INSERTS the Slot, in PAD INSERT the Pad, in CHAIN the Node | Left/right move through SEND / MASTER COMP / TRACK COMP / INSERTS / PAD INSERT / CHAIN. PAD INSERT is skipped for non-Kits |
| ROUTING | Track; in GROUP FX the Group | For a Group Insert, left/right is the Slot. The Tops select Main/Cue, Groups and recording processing |
| MIXER | Track | Left/right are also Track. K edits the Level and the like of the selected Track; MUTE / SOLO are used with the Pads |
| PERFORM | Track; in CONTROL ALL, the target parameter | Left/right move through Morph / Macro / Gesture / Mutation / Live FX / Audio. The Audio Tops handle role selection, recording, playback and asset operations |
| GENERATE | Track | Left/right move through RHYTHM / ACCUM / GROOVE / LANES / ARP |
| MODULATION | Track; in MACRO MAP, the Route | Left/right move through LFO / AUTOMATION / MACRO MAP / MIDI / RPN・NRPN |
| PROJECT | A list item | A press executes the selected item. K1 Asset, K4 POOL SLOT. Confirmation is Top1 / press, cancellation is Top2 / left |
| BROWSER | File | Left goes to the parent, right / press OPENs. During name entry, K1 is the character position and 4-D left cancels |
| SETTINGS | A Setting; after a press, the value | Left/right move through the 7 categories. In AUDIO, Top5 is ROUTING; in MIDI, Top6 is MIDI ROUTES; in SYSTEM, Top7 is DIAGNOSTICS and Top8 is UPDATE |
| DIAGNOSTICS | Scrolling the LOG | USB / HID / AUDIO / MIDI / LOG are shown simultaneously. No left/right sub-pages. Top5–7 are explicit tests |

The WAVE PROFILE of external MIDI is switched at the Top MACROS / SN CLOCK, and TRIGGER or SET REF are not executed from 4-D left/right. The behavior of jumping to another screen from a SYNTH sub-page move is also removed.

## Short workflows by purpose

| Purpose | Shortest flow | Finish / next action |
|---|---|---|
| Create the core of a song | FILE → NEW → CONFIRM → select a Track with A–H → PLUG-IN → ENGINE → enter with the Pads in STEP → PLAY | Switch Clips in PATTERN, save the configuration in SCENE |
| Prepare sounds and routing | Select a Track → sound source / TONE on the PLUG-IN Tops → ROUTING / FX in CHANNEL → MIXER | Return to the previous main screen with page-left |
| Use a sample | BROWSER → select a File → PREVIEW → LOAD → SAMPLING | TRIM / WAVE / ZONES. Keep preview distinct from assignment to the song |
| Keep a performance as a configuration | Develop in CLIP / SCENE → edit the SONG or take a take in ARRANGER | Check the whole with PLAY → save with FILE |
| Record a loop | PERFORM → Top8 AUDIO → Track → Top1 LOOPER → INPUT / LENGTH → PLAY → RECORD | Wait for the finish processing → OVERDUB or TAKE UNDO → save |
| Place audio onto the song | The same Audio page → Top2 AUDIO → prepare the SONG → Track / CLIP / INPUT → RECORD | Adjust placement with START BAR / TRIM → save |
| Save / carry out | SHIFT+FILE; if unnamed, enter a name → APPLY | Gather the assets with FILE → COLLECT. After OPEN, check the song name and assets |

The detailed procedures and discard conditions are canonically defined in the [instruction manual](user-manual.ja.md) and the [audio frame](audio-tracks.md). Correspondence with lighting is in the [LED rules](led-behavior.md). History and RETURN restore the selection position and are separate from the Undo that cancels changes to audio parameters.

## Correspondence between pads and display

[pad_role / pad_feedback](../src/performance.h) classifies the current destination of Pad input, [PadPalette](../src/ui/pad_feedback.h) determines the role's target Track / neutral colors and the strength of states, and event wire hues are excluded from the ground. The LED preview is after the final wire quantization; the chip before a PAD's name is the fixed 7-category engine kind. A physical Pad press is not uniformly converted into a sounding display. A sounding notification of another Track is not reused for a Kit / Slice / File / Cell, and unassigned Pads in PERFORM and normal Pads in DIAGNOSTICS stay dark. MUTE / SOLO on the AUDIO page are per-Track even when the selected frame is an INSTRUMENT. Details of color, hold, release, reservation and Motion are in the [LED rules](led-behavior.md).

## Common display rules

The bottom 30px is the two-row legend of name + current value, and the ARC overlay is layered over the body. TRACKER always has 9 rows and does not change its central row during an overlay. Holding a button that opens a page for 400ms shows the guidance valid under the current conditions in a hint layer of "operation > effect", and releasing returns. The STEP long-press editing and the per-button behaviors are retained. DIAGNOSTICS is entered by holding the entry Top8. A discovery toast is shown on first entry, and operation results are also aggregated into toasts. Typography follows the [common rules](ui-design-system.md), and the current images are in the [screen index](manual/screens.html).


On a normal Synth page, the left shows the common value row of the existing 8 knobs and the right shows a hero derived from the descriptors. Additive = actual harmonics / idle recipe, Wavetable = a scan model and the actual position, Texture/Resonator = an actual FFT, and the rest are in principle actual PCM. Tone/ADSR distinguishes the model from the measured points. The Browser/Sampler peaks and live input, the Looper's actual position and held takes, and the models / actual followers of all FX types are also the common 6 elements. [How to read the visualizations](user-manual.ja.md).


The input, saving and GM boundaries follow the input contract. The Kit list is a dedicated Browser display: USER / FACTORY are chosen in the same list, and PREVIEW / LOAD are separate. Step chord editing places the focus on KEYBOARD, keeps the selected Step and note names, and returns to the original screen. K5 in normal input is a velocity multiplier; only K5/6 in Step chord editing are STEP VEL / STEP GATE.

In SETTINGS → SYSTEM, the right Top5 is FILE TRANSFER. On the transfer face, the left Top1 AP / Top2 STATION are the connection method, and the right Top5 START / Top6 BROWSER / Top8 CLOSE are execution and exit. After starting, the method and START are disabled, and the knobs and Pads are not operated. The SSID / code / URL / state, progress and the remaining seconds to automatic end are displayed. [Operation and restrictions](user-manual.ja.md).

Themes are consolidated into the THEME item of DISPLAY, and the 9 names are shown in a 3×3 list on the right LCD. APPEARANCE is MOTION. In both, an item is selected with the 4-D and its value is changed with K1. Only the theme is owned by DEVICE.CFG, independent of the song's UI state, and is maintained after a song load. During a transfer, performance input is stopped over both USB and Wi-Fi, and the transfer ends with CLOSE or BROWSER.


## Rules for adding screens

The UI organization plan is canonical: the 20 pages are classified into 5 families (Performance / Writing / Composition / Sound / Management). The classification is also declared in `PageDescriptor.family`. Normal family movement uses the physical entry points / history / HOME. Descending from the performance Top5 and the ENGINE press to the shared sound selection is a regular exception, not a new management hub.

1. Information with no input goes to the display layer (ARC / toast / help), and does not add modes.
2. Confirmation, progress and naming go to the dialog layer. A single owner takes all input and does not operate what is behind it.
3. Editing of an existing target goes to a sub-page of the existing page. As a principle, the left Top is selection, the right Top is execution/exit, and 4-D left/right is movement within the face. An operation confined to a face may be placed on a left-Top slot that has no face-specific page; when placed, it is stated here as an exception.
4. Only resident work that changes the pad grammar is a candidate for a new mode. The regular indirect entry points use the two forms SETTINGS→SYSTEM and PERFORM→AUDIO.
5. A new page comes with all four: page_descriptors, a controls provider, a manual-render frame, and the generated controls.tsv.

The selection targets are ENGINE (the method), SOUND (the completed form of values, Layers and optional FX) and KIT (a bundle that carries a track kind). Layer children are exchanged on the LAYER face after mounting and are not made an independent selection category. The POOL is the storage place for embedding into a song and for Step references, and is not a KIND in the list. The selection axes are KIND × ENGINE × ORIGIN, and the verbs are PREVIEW (temporary audition, no pool registration), LOAD (mount) and STORE (save the current sound). The save destination is made explicit with STORE FILE / STORE POOL, and RECALL POOL recalls a registered slot. Only the SOURCE of REC SOURCE points to a recording input.

### Inventory of exceptions by family

There is no wholesale rearrangement of the Tops. The following existing exceptions keep each provider supplying the current input and display from the same definition.

| Family | Existing operations outside the principle, and the reason |
|---|---|
| Performance | The KEYBOARD / CHORD left Tops are not pages but selections such as SCALE / OCTAVE, so that performance is not interrupted. The performance Top5 is an exception that descends to the shared sound selection. PAD 4-D left/right is the Step display position; combined with SELECT it is an explicit shortcut for mounting |
| Writing | STEP 4-D left/right is in units of 16 Steps; in TRACKER it is the Track column; GENERATE is recipe pages. Priority is given to the position of the work target. Undo/Redo keep EVENTS as the common entry |
| Composition | CLIP / SCENE / SONG 4-D left/right select the Pattern / Scene / section. There is no implicit movement to a new resident screen |
| Sound | **CHANNEL alone keeps the ROUTING⇄FX toggle**. The start/stop of recording keeps the paired execution in the same position. Sound-source-specific pages and MIDI profiles follow the descriptors. Layer left Top3 USE CHILD / Top4 REMOVE are an exception placing an in-face operation in a slot with no face-specific page. The common column of the right Top5–8 is not moved. ENGINE leaves the left Top empty, has LOAD ENGINE on the right Top5 and BACK on Top8, and a press descends to the sound list |
| Management | Browser left/right are parent / Open (movement within the same shelf). PROJECT still has document operations such as SAVE on the left Top as well. During naming, Top1 APPLY / Top2 CANCEL form the single dialog layer. PRESETS / KITS align KIND / ORIGIN / PREV / NEXT on the left Top and PREVIEW / LOAD / FILES / BACK on the right Top |

The PRESETS / KITS history restores KIND, ENGINE, ORIGIN, the cursor within the 64-row window, and the return destination. Even after going out to FILES, the history can return to the sound list. Merely displaying the lower list in ENGINE performs no preview, mounting or pool registration. When empty, NO SOUNDS FOR THIS ENGINE and the LOAD ENGINE of the default sound are offered as guidance.

The kind and save operations of AUDIO / LOOPER are performed at the conventional PERFORM→AUDIO. The sound list and stepping target the held Instrument, and the existing contract of switching to the Instrument on LOAD is maintained. A Preset of a Lane itself is not added. The SOURCE in MODULATION's Macro Map is an input of a control signal and remains a separate value name from the ENGINE of sound selection. These are also treated as exceptions that do not change the current meanings.
