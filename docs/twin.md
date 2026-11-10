# TWIN: a synth/sample pair for live use

## Storage design

A Pool sound alone cannot identify the link to a Track or the digest at preparation time. To avoid disguising the purpose inside the fields for GUID / name / FX or FREEZE, the Song container carries the pair in a dedicated optional `TWIN` chunk; a Song without the chunk has no TWIN data. SoundState / Sampler zone / WAV formats are unaffected.

Only in a Song, and only when used, is a single chunk saved. The payload is a u8 count (1–16), followed by count records of `u8 track / u16 source_pool / u16 sampled_pool / u8 active / u8 digest[32]`. Values are little-endian. Duplicate Tracks, out-of-range values, identical pairs, slot sharing with another Track, missing definitions, a sampled version with no Sampler zones, and any mismatch between active and the attached destination are rejected. flags is 0, and the data is included in the existing CRC. The link is not exposed in Presets / Kits.

Preparation deliberately uses two Pool slots: an independent User copy of the original working sound, and the Sampler version. No slot is reserved until the first preparation, and no Track, Pool, or material assignment changes until preparation succeeds. Re-preparation updates the same two slots. Returning to Source attaches this User copy. When the Sampler version is saved and registered in a new revision slot, the pair's reference follows, and the old Sampler slot is freed if nothing else references it. On failure or cancellation, the original preset identity and assignments are also kept. A named Preset of the Sampler version can be carried out through normal save / COLLECT, without attaching the song-specific pair link.

## Operation

- MIXER Top6 **TWIN**: if not prepared, PREPARE with the default settings. If READY, arm a switch at the next bar. If ACTIVE, the same single operation returns to Source. If STALE, re-prepare; it never silently switches to a stale version that no longer matches the current sound source.
- FREEZE keeps the current physical Top5, and PAT NEXT stays on SHIFT+Top5. TRACK NAME is available from SHIFT+Top6 on SYNTH. No exception is added that turns the left Top row into process buttons.
- **SHIFT+Top6 TWIN**: LOW NOTE / HIGH NOTE / INTERVAL / VELOCITY / HOLD MS / LIMIT MS. Initial values are 36–84, 12 semitones, velocity 100, hold 500 ms, maximum 6000 ms. At most 16 notes, 1 layer. A specification that exceeds the range or interval is shown as such before starting, and cannot be started.
- During PREPARE, progress is shown, with Top1 CANCEL. Only a target whose Transport is stopped, whose Clip is stopped, or whose current phrase has no sounding step is captured. Continuous-input Texture, Kit, External MIDI, Empty, Unknown, Audio/Looper, frozen tracks, and tracks already ACTIVE are excluded. No automatic chords are added; the capture records one key at a time, including the constituent notes of a Layer.
- Dry is used, and the Track's TONE / Insert / Send / level/pan remain live at playback as well. Leaf Inserts inside a Layer are included in the sound source's output. Per-leaf Delay/Reverb Sends are not baked into the Dry material; the Sampler version uses the root Track's Sends. The root PTRM is not double-baked into the recording; it is carried over to the Sampler version.
- Silence detection for a note is about −72 dBFS for about 83 ms, with the longest capture at LIMIT. The note is released at hold, and the capture moves to the next note once silence or the limit is reached. A short margin is left before and after for automatic trim. No sustain loops are created.
- The notes are laid out in a single stereo float32 WAV; the actual captured pitch becomes the root, and the midpoints between adjacent roots become the key-range boundaries. The lowest and highest key ranges extend to 0 / 127. A single note uses the same scheme.
- The WAV is saved with the amplitude factor of the captured velocity removed, and the playback velocity scale is applied exactly once, as usual. A Dry level that exceeds the existing Sampler's float input range does not silently clip; preparation fails and the original sound is kept. Track transpose / tuning / bend are not double-baked either.
- The initial settings are held within the session, and the prepared pair, pitch zones, and ACTIVE state are saved into the song. The velocity/hold/limit of the preparation settings themselves are not additionally saved into the song. Re-preparation uses the current settings.
- For instant switching, the TWIN WAV is loaded explicitly resident into the existing immutable cache. The usual 8 MiB streaming judgement and the overall cache budget are unchanged. If memory is short at preparation or reload, the operation is refused; quality and note count are not reduced.
- The Top6 LED is off when unprepared, dim when READY, bright when ACTIVE, slow-blinking when STALE, and fast-blinking while a switch is armed. The Top row is single-colour; no unconfirmed RGB colour is assigned. The on-screen state vocabulary is canonical.

## Switching and saving boundaries

Switching uses the existing Pool attachment: old voices are stopped, and held keys are re-sounded on the new sound source. A 64-sample output transition is provided. Resumption of the old engine's phase / envelope position, and perfect continuity for sounds with long attacks, are not guaranteed. No additional resident DSP or double-sounding hold is used. Return also follows the budget of the existing voice / Layer admission.

While ACTIVE, sound locks on Steps and Preset overrides on Clips do not restart the original sound source. The normal Track LEVEL / PAN / TONE / Insert are maintained. On stop or Project switch, the transition output values are cleared and the previous sound is not carried into the next start.

TWIN preparation itself does not change a Track's attachment. Only on success does the audio owner publish the two definitions, the material, and the link together. On cancellation, write failure, or a Project generation change, the original Track and the previous pair are kept, and only the temporary files created by this job itself are cleaned up. Even if names are exhausted, an existing `.part` is not deleted. A WAV that succeeded in the past may be referenced by another Preset, so it is never deleted automatically.

SAVE PRESET AS on the sampled version can use the normal INCLUDE FX. To carry the shared Insert/Send out as well, turn it ON and COLLECT that Preset. A Song's COLLECT includes the materials of both the synth side and the sample side.
