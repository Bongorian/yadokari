# Source-labelled synthesis percussion

Six standalone native presets and one six-piece baked Sampler kit. The native
presets use VA DUAL or FM 2 OP; no new engine is registered. Kit pads1–6 are
VA Deep Kick, FM Metal Snare, FM Tick Hat, VA Air Hat, FM Glass Rim, VA Round Tom.
Pads7–16 are empty. Closed/open hats share choke group1.

The WAVs are actual Engine output at48kHz, mono PCM16, velocity110, recipe root
notes, seed0x53445231+i. NoteOff is10ms before the end, followed by a2ms final
fade and peak normalization to24575. They are original project-authored content
under the repository GPL-3.0 license; no external recordings were imported.
`manifest.json` records hashes, CRCs and recipe/generator provenance.

Regenerate from the repository root with `make build/synth-drums`, then
`./build/synth-drums` and
`python3 tools/content/factory_synth_drums_pack.py --write`.
The tool uses [recipes](../../src/synth_drum_recipes.h) and the actual shared
Engine. Boot only unpacks and verifies the authored files; it never runs the
synth renderer to create this kit. Presets go to `FACTORY/PRESETS/DRUMS`, the kit
to `FACTORY/KITS`, and protected shared WAVs to `FACTORY/DRUMS/VA-FM`.
Nothing is automatically registered in the256-slot user pool.

Physical audition is unverified. The render verifies nonzero output and the
normal factory checks verify storage/codec integrity, not musical quality.
