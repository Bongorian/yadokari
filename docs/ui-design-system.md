# UI design system

The product UI is English only. The shared [theme / widgets / motion / semantic LED](../src/ui) code is used on Desktop and Circle. Labels, headings, body text and numbers are all unified on Barlow Semi Condensed, with tnum outlines and advances baked in for every role. Smooch Sans / Archivo are not included in the current atlas. See the [font materials and OFL notices](../assets/README.md), the [pins and SHA256](../third_party/fonts/sources.json), the [atlas metadata](../assets/fonts/atlas.json), and `make fonts`.

Text uses the native-size A4 coverage and integer pixel origins. Small 12/14, Body/Bold 14/18, Heading/Knob 20/24, Number 24/28 (size/line height). Only Small labels use 1px tracking; everything else uses 0px plus the pinned kerning within the same typeface. Strips, badges, Top and legends place the line box at the vertical center of the strip, and the cap height at the center of that. Both LCDs use the same formula for the same role. Numeric fields are right-aligned in a fixed width, so a digit change such as 9→10 does not move the adjacent unit or label. Mixed text uses the same typeface, and kerning between digits is suppressed to preserve the monospace feel. All line endpoints also snap to integer pixels.

The two 480×272 LCDs, the on-screen Top1–8 and the K1–8 below are treated as one common operating surface. The Top legend is always a single 18px line; the body occupies 64–241px; 242–271px is always a two-line knob legend (upper = name, lower = current value). Only for a touched / turned knob does a 224–269px, 46px-high in-cell display open, anchored at the bottom edge of the legend, with the name at the top and the ARC + value + small unit in the center. The bottom-edge bar of an open cell disappears. The resting ARC is accent; only while turning is it motion. Values keep their native size and the same precision; only the unit is made smaller or omitted. Multiple touches open each cell independently, and each fades individually 1.2 seconds after release. There is no full-width strip, no top divider line and no duplicated name, and the body neither moves nor stretches. Text is not scaled; it uses the atlas at native size. Automation changes alone do not open the list. The [page definitions](../src/page_descriptors.h) and the [renderer](../src/ui_renderer.h) are the canonical source for layout. Top is for screens, actions and confirmation; knobs edit the values of the current target. During name entry the surface switches to CURSOR/CHARACTER and does not show the parameters behind it. An unassigned knob is never made to look operable. No background highlight is used behind values while a knob is being operated; values, knob ticks and the target are kept easy to read.

## Meaning of the diagrams

- Routing draws the fixed paths of Track/Group, Send/Return and Main/Cue. It is not a freely connectable patcher.
- FX chooses a diagram that fits the effect: Filter response, Compressor input/output, Delay repeats, Reverb decay, and so on. The computational model and the measured meter are distinguished.
- Synth displays ENGINE → TONE → OUTPUT plus FM / ADSR / LPG / Fold and similar, corresponding to the sound engine. A symbolic wavy line is not called a measured waveform.
- The Sampler waveform is an analysis cache of the real material. Analysis proceeds from Preview/Load, and a non-resident sample is not read from disk by the DSP.

## Redrawing and USB

The body viewport is always fixed; overlays, toasts and hints are layered on top as a temporary layer. On open/close, the covered region is redrawn, but the body coordinates and the TRACKER's 9 rows do not change. The static bodies of SETTINGS / PROJECT / ENGINE are reused during playback and invalidated by changes to the target, values, material names or dialogs. Screens that need meters or a playhead are updated. LCD differences are batched in 16-line units, up to 64 raw lines, or up to 256 lines under reversible compression, honoring 4px alignment, the 64 KiB packet limit and shadow retention on failure. Updates run on 30 Hz slots of a monotonic clock; late slots are not accumulated, and static pixels are not resent. FPS is judged by the count of completed full differences and the maximum interval, per screen. Ownership of the measurement window, the input pump and SD waits is specified in the F-fix design; measured values and the limits of full-screen transitions are in the Final test.

The RGB/BGR565 profile and Strip calibration are stated explicitly. Colors and LED behavior that are uncalibrated on the real device are not guaranteed from the diagrams. See the scope of on-device verification.

Screen generation follows the documentation rules and uses the actual shared renderer.

LEDs take the [lighting rules](led-behavior.md) as canonical. State priority does not depend on enum numbers; in Reduced mode, automatic note flashes, afterglow and the PLAY beat blink are omitted, and record standby / warnings vary their brightness slowly. Operability and recording start are never left to be inferred from color alone.

Pad base colors use the fixed set of 7 in [PadPalette](../src/ui/pad_feedback.h) (Synth=10, Sampler=7, Kit=4, Layer=14, Audio=8, MIDI=15, Empty=17). Over the base color, the state colors take precedence: selected=17, playing back=7, standby / warning=4, recording=1; Off / Idle are unlit. The numbers are logical indices of the MK3 indexed colors and do not guarantee the uncalibrated physical color. A short press in Full ensures 120ms of visual feedback, but Note Off is not delayed. SETTINGS → LIGHTS shows the legend and the current output, and the Pad / Step / Clip / Scene displays on the LCD use the same state colors. Only the Pad display regions whose LEDs changed are added to the redraw set, and knob context and easing are not reset.

A frame is counted complete at the successful ACK of the last differential rectangle (or at a scan where all pixels match). The worker inspects differences remaining outside rectangles and does not update the shadow before an ACK. A success from an old snapshot that straddles a new slot is counted, but does not close the new slot's unsent gate. Transfer failures and epoch invalidation are not counted as completions.

Each screen holds exactly one immutable snapshot in flight. Even when a new 30Hz slot arrives, the rectangle sequence in progress is not switched; after a successful ACK it advances to the latest renderer. Untouched intermediate slots are discarded, not accumulated. This prevents mixing strips from different frames and prevents completion from starving through mid-cycle discards every period.

USB packets reversibly compress runs of identical RGB565 pairs into command 01 and send everything else as command 00. If compression would not be shorter than raw, the conventional format is used. Characters, line counts, scrolling and pixel values are unchanged; compression happens only in the display worker. After a successful ACK, the shadow is updated from the retained snapshot. The protocol's sources and verification are in the F-fix design.

Current images are collected into a [single index of pages × themes](manual/screens.html). The 20 main page types plus the registered sound-engine editors, sub-pages, confirmations and temporary-layer fixtures are drawn with the same shared renderer, and cap centering, integer origins, left/right headers, and placement under numeric digit changes are recorded with image comparisons. Old images are kept distinct from current ones under archive/screens/pre-wave6, pre-wave7, pre-wave8 and pre-wave8-followup. A hint appears only while the physical button that opens the page is held for 400ms; results are a temporary toast. Note editing by long-pressing STEP is retained, and Modal ownership takes precedence. Transitions take at most 3 drawn frames, do not stop input, and are replaced by movement in progress.

## Icons

The [original 14×14 line art](../src/ui/icons.h) uses a 1px stroke, integer pixels and theme tokens, and does not scale native shapes. Top is icon + text; the header has 6 states: transport / Follow / Lock editing / metronome / external Clock setting / Strip. The record arm is an outline; transport is filled only while active. Marks are also attached to the right header and to ENGINE families. They are obtained from the [descriptors](../src/ui/control_descriptor.h) for action / category / status, and IDs are not written directly into the renderer. Parameter names, values and file names are text. See the [overview](manual/icons.png) and the design and verification.

The comparison rectangles and time conditions for verification, their separation from notifications, and the minimal physical-operation checks are collected in the canonical UI verification. Initial guidance is shown as guidance on that page, not as a result toast.

## Content components

[ContentValue / ContentRow](../src/ui/content.h) is drawn by the [shared widgets](../src/ui/content_widgets.h). A value row unifies the name, a fixed-width tnum value field, and the shape at its bottom edge: continuous values = bar, signed values = bar with a center mark, stepped values = segments, enumerated values = chip. Stepped values with more than 24 divisions are shown with the divisions bundled, and the exact value is read from the number. A section is a heading + divider; key/value is a pair of information; a list is ordered main information, sub information, state mark; empty carries a heading and the next action. The Mixer strip unifies meter, level, pan and state in the same hierarchy. The level body uses the shared Track-kind color; MASTER / input / Group use the existing teal token; the hold mark is motion; warnings are warn; excess / clip is danger. See the color division of Plan A. No bevels or shadows are used.

On the Synth left side, the existing knob assignments are projected onto a 2-column × 4-row grid; the declared tiers and the operable targets are not changed. Settings places each category's values into the same value rows, Browser into the same list, and Mixer into the shared strip. The Step / Tracker / Kit matrix / Song timeline — the operation targets themselves — keep their dedicated layouts.

## Visualization declarations and vocabulary

[VisualDeclaration](../src/visual_model.h) declares the element, data source, update class, fallback, model and caption. It is held by [EngineDescriptor](../src/engine_registry.h) and [FxDescriptor](../src/fx_registry.h); the [adapters](../src/ui/visual_adapters.h) resolve real snapshots / parameters into values, and only the [6 widget elements](../src/ui/visual_widgets.h) draw. The renderer is not branched by sound-engine name.

Scope = silhouette of peak / real PCM; Spectrum = surface of a real FFT; Bars = N values; Ring = real phase, intervals and takes; Curve = the declared shape and the real current point; Response = the computed response and measured points. PARAMETER MODEL / RECIPE / LIVE PCM / MEASURED FFT / not-acquired is stated explicitly, and an explanatory diagram is not called a measurement. Spectrum analyzes 3-point mono `(L+R)/2` with 512 samples / Hann. It is a mono display in which opposite phases cancel, and is separate from the stereo-wide peak meter.

Interpolation uses only the two acquired endpoints of the same source, generation and target, and never extrapolates into the future. If core2 is busy, analysis is thinned from 50→100→200ms and never makes the audio wait. The source sound, audio quality and voice count are unchanged, and analysis is limited to memory only.


When adding a sound engine, register EngineDescriptor.visual (including the default from the existing page.hero); when adding an FX, register FxDescriptor.visual. If the source's measured values are not published, choose Empty or an explicitly stated Parameters fallback, and do not add an engine-specific diagram to the renderer. Cyclic phase interpolates toward the shorter side; the Morph linear position interpolates linearly. At identical endpoints, animation is not restarted, and silent PCM / idle recipes stay still. A Loop's inner ring is the current / Undo take, not the number of independent overdub layers before flattening.


## Alignment discipline

- The fixed regions and physical assignments of Header / Top / the always-on K1–K8 legend are maintained. The body's main heading is placed on a common leading band, and both sides use the same cap center.
- Text inside a cell aligns the glyph cap center and the cell's vertical center within ±1 native pixel. y is not changed by string length. Labels use the field's left edge, numbers the field's right edge, and center captions align the field center on integer pixels.
- A value row places label/value on the same cap center, and rows of the same role share the same baseline. Different roles align their glyph cap centers. Each column uses a fixed label left edge / value right edge.
- The Hero caption is center-aligned, and the main diagram uses the surface of the declared model / measurement. A control recipe during silence is labeled as a model and is not treated as if it had generated PCM.
- The bounds/align of semantic fields are recorded in TextPlacement and audited. The final pack, covering all 20 pages, mechanically checks cap centers, left/right edges, caption centers and the heading band, and does not include the header marquee / clock in image differences.


The canonical audit sources are [alignment_errors](../src/ui/text_alignment.h) and [TextPlacement](../src/software_canvas.h). They check the left/right edges and center of fields against the native cap center, and align the main heading band's cap center to native y=84px. Editing matrices such as the Tracker keep their independent cell geometry. The audit pack covers 19 main pages per theme and 20,168 placements, examining 14,365 semantic fields and 233 hero captions per theme. See the coordinate audit and the violation list.

Surface drawing rules: Scope = real PCM filled from the center line; Curve = the declared waveform / ADSR filled; Bars = relative components / declared controls shown on a fixed rail. Wavetable shows the waveform model interpolated from the bundled table plus the scan range / real position; Additive shows partials normalized by the largest recipe component × the real envelope; an idle generator shows the declared control recipe. Model vs. measurement captions are maintained, and no fictitious idle spectrum is added to Spectrum. Numeric formatters preserve the declared 0–6 digits and do not abbreviate values such as an ADSR 0.005 seconds.


## Screen meaning and grammar (canonical)

Types are defined by the work, not by the entrance. The [machine-readable types and left/right meanings](../src/ui/screen_grammar.h) and the [correspondence with screens](../src/page_descriptors.h) are emitted to the generated controls.tsv and screens.json.

| Type | Screens | Left LCD | Right LCD |
|---|---|---|---|
| Instrument | PAD / KEYBOARD / CHORDS | Performance monitor | Sound monitor |
| Writing | STEP / TRACKER | Time axis on both LCDs | Time axis on both LCDs |
| Sound design | SYNTH / MODULATION / INSERT / GENERATE | Parameter values | The shape of the parameters |
| Material editing | All SAMPLER pages | Material waveform / work surface | The shape and values of playback |
| Signal | ROUTING / MIXER / SEND / COMP | Whole path | Diagram and values of the selected part |
| Structure | CLIP / SCENE / SONG / AUDIO | Arrangement | The shape and state of the selected element |
| Performance | PERFORM / LOOPER | Transformation values | The shape of the transformation |
| Selection | LIBRARY / BROWSER / ENGINE / PROJECT | List / hierarchy | Selection / audition |
| Settings | SETTINGS | Items and values | Preview of the selected item |
| Diagnostics | DIAGNOSTICS | USB / HID / AUDIO | MIDI / LOG |
| Temporary layer | Confirmation, name entry, hint, toast, overlay | Common components, not a page | Same |

The skeleton is fixed in this order: permanent Top legend → header → body → permanent two-line knob legend; "identity → global state → main subject → details → operation". Left Top T1–4 selects the surface, range or category of the same subject; right Top T5–8 is execution and exit. The same verb uses the same slot. No entrance to a different subject is placed here. For selection types only, left Top is open / up / audition / load, and right Top is management and exit. Confirmation is T1 confirm / T2 cancel; name entry is a common type. Sub-page movement via Top/4-D and the duplication of confirm/cancel are intended redundancy.

Knobs edit values and carry no execution verbs. The names and values of all K1–K8 are always displayed directly above. Effective values from locks / automation are tracked, but that alone does not open an overlay. Instrument / Writing types use K1 TEMPO. Sound design places the declared parameters first; SYNTH body values may be consolidated on the left (the physical correspondence is guaranteed by the legend directly above). STEP details may also consolidate onto the selected Step, including the right-side knobs. SAMPLER's left and right are fixed as the material-editing type, distinct from sound design. Assignments are never changed only while playing. Performance-oriented access goes through PERFORM, Peek and history; destructive operations pass through confirmation.

LEVEL / PAN / CUTOFF / DRIVE / DELAY SEND / CRUSH / LFO DEPTH / LFO HZ are called the "channel configuration" and use the same order and units in MIXER, Morph/Gesture and SYNTH TONE. A sub-page is another surface of the same subject; switching changes the body. Ordering is fixed per type. GENERATE is the place that generates the rhythm / note material of the edit target (physical VARIATION: HID byte5 bit1). Mutation is a surface inside PERFORM that handles the variation amount, seed and freeze/commit of existing notes while playing; it is not another name for generation.

Drawing uses the existing Scope/Spectrum/Bars/Ring/Curve/Response, hierarchized by the main subject's surface, thick lines for the sound's path, and thin lines for control / guides. The theme palette is not extended. A token of a distant hue is dedicated to filling the value bar of the value being selected / operated, and is not used on text or on the surfaces of main diagrams. Existing data-source labeling is kept, and an unmeasured parameter model is not called PCM.


Placement decisions: GENERATE is a sound-design type that edits the note-generation recipe of the selected Track, not direct writing on the time axis. The left side is recipe values; the right side is the transformation diagram of those values. K1 is that declared value — a different type from the fixed TEMPO of STEP/TRACKER. PAT PREV/NEXT and AUTO PAGE are related entrances that open the neighboring phrase / range of the same Track/automation subject, and are declared as exits on the right Top. SETTINGS is a fixed-order bank of 3 categories + MORE; only K1 is the value of the selected row; 4-D up/down is row selection, left/right is category. SYSTEM's SPACE UNIT is an in-session value that selects GiB/MiB for the DATA capacity display; it is not an execution command.

Sound snapshots are placed only on the free T7/T8 of surfaces whose subject is a sound, and RECALL POOL is not shown for an empty pool. PERFORM T8 is always AUDIO. The SAMPLER right Top prioritizes material processing / recording, and sound snapshots are reached from the PROJECT list's STORE POOL / RECALL POOL. PROJECT K4 is POOL SLOT; items are selected and executed with the 4-D's rotation and press. BROWSER's COLLECT is abolished; PROJECT T5 is the only COLLECT. Browser K7 is AUDITION LEVEL, K8 is FAV FILTER (a value); starting/stopping audition and changing FAVORITE are Top only.


DrumKit's SYNTH CORE is declared as a selection type that selects a child Pad (left: Pad list; right: the selected Pad). K1 is PAD; the following knobs are the attached material and the Pad's mix values; sound snapshots are unified into the PROJECT list. TONE returns to the sound-design type: left is the channel-configuration values, right is the shape of the filter and so on. For the instrument-type PAD, the Kit's K1 is TEMPO, and Pad selection is by SELECT + PAD / 4-D. ENGINE is right T5 LOAD ENGINE / T8 BACK; a 4-D press is the entrance to the sound list. Turning a knob alone does not attach anything. Layer's CORE and TONE are on left T1/T2; the child's USE/REMOVE are on CORE's left T3/T4 (an exception present in the shared declarations). The exit of a MIDI private profile is also placed on right T8.

## Color and lines

The [canonical 9 themes](reference/themes9.json) and the generated Theme are used; the default is CYBER 2. For the lightness corrections to three themes, see the record. The theme is stored in DEVICE.CFG and is not restored from song UI. DISPLAY → THEME is a 3×3 list of the 9 names. APPEARANCE is Motion.

Surfaces are bg / surface / raised; selection is raised + an inner accent 2px underline; playback is a motion 3px top-edge line. Both marks — selection and playback — are layered; large surfaces are neither dyed with color nor enclosed in frames. Lines are unified to auxiliary 1 / functional 2 / emphasis 3 / meter fill 6px, with right angles as the basis. Shapes that themselves represent a form, such as round knobs and signal nodes, are separate. The LCD uses theme tokens; the pad LEDs use fixed semantic indices, kept distinct from uncalibrated physical colors.

The track-identification base color shares the [fixed 7 EngineKinds](track-kind-colour.md), and seat-number colors are not used. In MONOTONE, base colors / kind chips are projected to gray, and existing event tokens are maintained. The 4×4 of Track selection is physical bottom-left = 1, and side-by-side tables such as Mixer / Tracker / MIDI channel keep reading order.

Pad LEDs use the [fixed semantic colors](led-behavior.md) and are not approximated from the LCD theme's RGB. The MONOTONE projection of the on-screen kind chips is maintained, and the responsibilities of screen and wire are separated. The rules for adding screens, families and the 3 layers are collected in the [page definitions](ui-pages.md).
