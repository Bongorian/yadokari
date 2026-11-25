# Changelog

YADOKARI's public releases start here. Versions use the packed
semver numbering described in the release notes
(0.9.0 → 900, 1.0.0 → 10000, monotonic).

## 1.0.27

- The factory synth library is swapped: 23 Wavetable sounds join -
  the ten audition presets and thirteen built on seven new morph
  tables (Vowel, Harmbuild, Pwm, Stack, Digistep, Submorph and
  Metallic, staged as card sample assets), every one carrying
  sound-local modulation routes - plus the first four CHIMERA
  factory sounds. Eight overlapping presets retire (JP-8000
  Hypersaw, K5000+CZ Glass Duo, Psytrance Kick, FM Rimshot,
  FM Clave, Frozen Cathedral, Alien Choir and Funk Chops 104).
  Factory sounds now total 182.
- The composed Startup demo song ships in FACTORY/DEMOS.
- Display work is cheaper: LCD scan and packet preparation are
  reduced, pad edges survive repeated pressure, and MK3 LED
  brightness is corrected. The MK3 transport and the measured
  Pi 5 display limits are documented.

## 1.0.23

- WAVETABLE covers the full extended voice: warp modes (Bend, Sync,
  PWM, Asym), an internal two-operator phase modulator, stereo unison
  up to 7 voices with band-limited rendering, modulation routing for
  POSITION, WARP AMT, PM AMT and DETUNE, and morph-table assets (YWT1)
  that the converter builds from single-cycle waveforms. Admission
  accounts oscillators per rendering path at measured costs, including
  notes held while modulation moves them into the extended path.
- Song arranging grows up: sections move and duplicate from the
  arranger action bank, a rectangle of steps moves across tracks and
  patterns as one undoable edit, and groove templates travel with the
  Song. Files lists orphaned staged Song saves under their canonical
  names so an interrupted save can be opened and recovered.
- Sounds can carry up to four sound-local modulation routes from the
  LFO, envelope, pressure and velocity. Project routes override them,
  and they stay dormant while the sound is a Layer leaf.
- Instrument screens are refreshed across both LCDs: layered
  instrument surfaces, unified controls, calibrated mixer meters and
  a screen atlas checked against all nine themes. Full-page display
  latency on Pi 5 is measured and documented.
- Factory kits are revision 3: pad levels are restored to the source
  material and lifted uniformly, keeping the pad balance of each kit.
- The sampler idle screen labels its input REC SOURCE.
- The Pi 4 build is included with extended Wavetable admission
  limits derived from the Pi 5 measurements; the Pi 4 mixed-load
  qualification has not been run yet, so those limits are derived,
  not measured, as the known limitations describe.

## 1.0.5

- Added Ethernet Network MIDI (RTP-MIDI / AppleMIDI). Enable NETWORK MIDI
  in SETTINGS → MIDI and set NET LATENCY there. Network MIDI is off by
  default. Network sessions use router cables 12–15.
- On internal instruments, press the 4-D encoder in SYNTH to cycle between
  the engine view, the post-fader waveform and the spectrum. Touch a
  supported continuous control and turn 4-D for one-tenth increments.
- Added operator level and detune views for FM6, base formant frequencies
  for FORMANT and an oscillator cycle for PHASE DIST. FM 2 OP and VA DUAL
  retain their oscillator models during playback. The CHIMERA cutaway
  labels the vocal tract and marks where its walls meet.

## 1.0.4

- CHIMERA gains a TALK insert effect: any sound can be played through
  the vocal tract, with optional note following (TALK TO TRACK). A new
  PERFORM surface (LIVE TOOLS -> VOWELS) plays and locks vowel steps,
  CHIMERA sounds gain UNISON (1-4 voices) with SPREAD detune, and an
  optional SPEAK scene reading ducks the other tracks while a scene is
  read aloud.
- Layer sounds now steal the oldest released Layer chain when the voice
  pool is full, instead of refusing new notes, and VA voices take part
  in the render-ahead bundles.
- Fixed a freeze on the SETTINGS -> SYSTEM page with large cards: the
  free-space count published while the DATA volume grows is now valid,
  so opening the page no longer triggers a full card scan.
- Saving a Song no longer stalls input while moved assets are searched
  for: the search is spread across file-service rounds, recordings
  carry their content CRC from every capture flow, and temporary files
  are never mistaken for assets.
- Undo now shows UNDO LIMIT / OLDEST EDITS REMOVED once when the
  64-entry history limit first drops the oldest edits, instead of
  discarding them silently.
- Hardened the DaisySP drum bank: out-of-range or non-finite control
  values can no longer wedge a generator, and overlapping hits step a
  shared generator once per sample instead of two or three times.

## 1.0.2

- Added Song stem export: on the PROJECT page, hold SHIFT and press
  SONG STEMS to record one full Song pass into RECORDINGS/Stems-NNN as
  eleven stereo WAV files (Group 1-8, Direct, Returns and Mix), all the
  same length, ready to mix in a DAW.
- Fixed Timeline audio clips jumping to a wrong position when a clip was
  edited after a tempo change: playback position is now anchored to
  performed time, and editing a clip that is not playing no longer
  restarts the one that is.
- MIDI import now counts drum-kit collisions honestly: notes landing on
  the same pad and step are reported as merged instead of being silently
  dropped, and an import that would lose notes asks for confirmation
  before anything is discarded.
- USB file transfer can now be started and stopped from the host over
  the control channel (FILESSTART / FILESSTOP), so files can be moved
  without the device screen or a connection code. USB file transfer now
  requires REMOTE.ON, like the other remote facilities: with remote
  control off, USB transfer stays off. The Ethernet transfer started
  from the screen still uses its display code, unchanged.

## 1.0.0

- CHIMERA is now a regular sound: it runs in the shared voice pool (up to
  two voices on Pi 4, three on Pi 5, by measured cost), offers POLY, MONO
  and LEGATO voice modes, works as a layer leaf, and carries its IPA
  phrase inside the sound so phrases travel with presets. Speak and Sing
  sounds can carry IPA text in presets, with separate Song track
  overrides.
- Added a dedicated VOCODER page in PERFORM: opening it assigns the
  vocoder insert and its modulator and carrier automatically.
- The looper is now its own PERFORM page and prepares a free track and
  audio lane automatically; re-entering restores the previous looper
  track, and linear editing stays on the AUDIO TIMELINE page.
- Added PERFORM pages for RECORDING, FX MAP and MIDI MODULE (the
  multitimbral module moved out of SETTINGS), mode family tags, and a
  persistent file transfer pause notice. Consolidated the recording and
  capture actions on the RECORDING page (CAPTURE AUDIO), and made
  PAT NEXT available on MIXER without SHIFT.
- Arrangement recording can be armed during playback and starts at the
  next bar.
- TWIN PREPARE no longer needs the track stopped or silent; preparation
  wraps at a bar boundary while playing.
- Preserved STEP edit focus while another pattern plays. MIXER pads now
  show the audible mute state during a recorded Song.
- Made physical STOP reliable during command queue saturation. STOP also
  clears pending launches and repeat indicators, including on MIDI STOP.
- Released latched pad effects on exit from LIVE FX. Its Song FX controls
  now show that their values are saved with the Song.
- Kept a queued FILES OPEN bound to the selected row when a pad is pressed.
- The SD image now bundles the shared samples the factory library
  references (122 materials), and a release gate loads every one of the
  187 factory originals from a blank card image before shipping.
- Recordings register their content CRC at completion in the LOOPER,
  TIMELINE and FILES flows, so recorded takes save with the Song again.
  FREEZE bounces also save and reload audibly: a frozen voice could
  start silent, and its engine type could be overwritten during render.
- The score converter round-trips CHIMERA sounds and kit output trim
  (PTRM); a sound it cannot preserve now aborts the conversion with the
  sound named instead of degrading silently.
- File transfer now works over Ethernet as well as USB-C: FILE TRANSFER
  starts a wired DHCP/HTTP session protected by the same connection
  code shown on screen.
- SD distribution is unified on the provision-built A/B image
  (BOOT / A / B / DATA); the single-partition ZIP is retired. DATA can
  be expanded to the end of the card on the device, and REMOTE.ON is
  bundled by default. Remote diagnostics are documented policy: they
  require a prepared A/B card and the REMOTE.ON marker.
- The default and recovery song slot is FACTORY/DEMOS/Startup.mgxs.
  Until the official demo song ships, a card with no saved song boots
  to an empty project. The FACTORY/DEBUG diagnostic material is no
  longer installed.
- A known engine whose required state chunk is missing is now rejected
  at load instead of degrading to Unknown; unknown engines are still
  preserved as unknown.

## 0.9.6

- Completed CHIMERA: all playing surfaces (PAD, KEYBOARD, CHORD and step
  playback), three bodies (male, female, child), vibrato, glide and breath
  controls, and all 17 articulation parameters (five macros plus a detail
  tier), with the tract shown on the right display while sounding.
- Added CHIMERA programs: Beatbox (articulation presets on pads,
  step-programmable), Speak (IPA phrases over USB), Sing (syllables timed
  so vowels land on the beat) and Bestiary (songbird, seal, alligator and
  elephant sources).
- Separated sound browsing from file management: the library stays
  PRESETS/KITS, the folder view is titled FILES with a processing
  indication, and engines are listed grouped by family.
- Unified load terminology: OPEN for files and folders, LOAD for putting
  a sound on a track, PREVIEW for auditioning. "LOAD ENGINE" is now
  "DEFAULT SOUND".
- Fixed file-view navigation dropping wheel and button input while a file
  operation was pending; movement inside the loaded range now applies
  immediately and further movement is queued.

## 0.9.5

- Consume unsupported MIDI input messages such as Active Sensing, System
  Reset, and SysEx without interrupting the input queue. Thanks to @tandooridog.
- Align slave playback to the first measured MIDI clock interval so it starts
  in sync even when the saved project tempo differs. Thanks to @tandooridog
  for the fix and the independent timing test.

## 0.9.4 — 2026-11-18

- Added CHIMERA, a monophonic physical vocal-tract synthesizer. The right
  MK3 display shows live articulation and pressure in GENERATOR, PAD,
  KEYBOARD and CHORD.
- Fixed render-ahead jobs that could become unavailable after a worker
  race. If the worker is late or its work is invalidated, serial
  rendering produces the same PCM.
- Fixed shared voice admission so a rejected Plaits note leaves sounding
  notes alone. Granular now uses the shared voice limit for the detected
  board.
- Added Pi 5 support to the tool for building an A/B update SD image.

## 0.9.0 — 2026-11-11

First public release.

- The complete instrument: a stock MASCHINE MK3 plus a
  Raspberry Pi 4 becomes a standalone groovebox — 16 tracks,
  synth engines (VA, FM6 with DX7 SysEx import, Plaits / Rings /
  Clouds physical modelling and granular, wavetable, formant /
  VOSIM, additive, phase distortion), sampler with slicing,
  SoundFont 2 playback, drum kits with per-pad GM declaration,
  looper and audio tracks, FREEZE and TWIN, 256 steps with
  per-step attributes, scenes and song arrangement, mixer and
  performance FX, file transfer over Ethernet and USB.
- Wi-Fi is disabled in this build: the WLAN stack available to
  the firmware carries Plan 9-derived code under the GPL v2
  only, whose combination with this GPL v3 firmware has not
  been cleared. See docs/known-limitations.md.
- Factory library built in: 187 presets and kits (MGXC13).
- Signed updates (Ed25519): official images carry the project
  release key; self-built images use the builder's own key and
  do not cross-update with official ones.
- The controller is never modified: unplug the Pi and it is a
  normal MASCHINE again.
