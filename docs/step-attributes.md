# Step Attributes: SLIDE / ACCENT

SLIDE and ACCENT are per-step attributes stored in StepState. Canonical definitions: [StepState](../src/project.h), [the common single-note representation](../src/step_expression.h), [playback](../src/engine.cpp), [the save codec](../src/project_codec.h), [device operation](../src/performance.h), [rendering](../src/ui_renderer.h). Steps that carry no attributes render identically to plain steps.

## Data and operations

SLIDE and ACCENT are 2-bit attributes of StepState (stored in one byte of existing padding) and belong to the note set as a whole. No per-note voice matching or implicit chord ordering is created; in polyphony, SLIDE degenerates to a normal retrigger. Attributes can be stored on disabled / lock-only steps but have no effect on sounding there. Kit pad lanes are separate data, and their triggering is unchanged by this feature.

In STEP and TRACKER, SHIFT+K3 ACCENT / SHIFT+K4 SLIDE are assigned to the selected step. Turning right sets ON, turning left sets OFF. The non-SHIFT VELOCITY / GATE, the existing Top, preset lock editing while a pad is held, and Kit operations are unchanged. No new page is added. The step cell / tracker row draws a triangle for accent and a bending line for slide, rather than relying on caption text alone.

## Playback

SLIDE is an attribute of the arriving note. It connects monophonic notes of the same source and the same sound lock. The common gate-continuation test and pitch ramp are used by ACID / PolySaw / FM, preserving the existing oscillator phase and envelope. Pitch is logarithmically interpolated over a fixed 80 ms, independent of tempo and gate. If there is no preceding note gate, or the note has already been released, or the context is a chord, arp, ratchet, Layer or a different sound, it degenerates to a normal note-on.

Even with a short GATE, if the next executed step is a valid monophonic SLIDE, the note is held from the original gate expiry until arrival. Direction / microtiming / groove / mutation and conditions/probability are judged from the normal next step; no separate judgment that consumes random numbers is created. If the arrival falls out by condition or is a rest, or the clip or transport stops, the connection is released. Loops connect in actual playback order. The connection is judged on a valid sequenced gate and a non-zero original velocity, so natural decay of the amp/filter envelope down to the inactive threshold alone never causes a retrigger. ACCENT on ACID has its own emphasis stage: without changing velocity, it multiplies the volume by 1.6 and strengthens the filter envelope response. Even together with slide, the envelope is not restarted. ACCENT on other sources is stored and displayed only; it is not converted into velocity.

MIDI OUT remains the conventional Note On/Off and velocity. No automatic pitch bend, CC65 or accent-velocity conversion is sent. No proprietary attribute events are added to SMF. External MIDI and polyphonic playback are unaffected, and stored attributes are preserved.

## Save format

In the leading flags byte of both full and extended step records, bit4 = SLIDE and bit5 = ACCENT (bit0 enabled, bit1 lock-only, bit2 chromatic, bit3 native drum). Bits 6–7 are reserved and rejected when non-zero. The record byte counts, the subsequent field order and the extended bitmap follow the PSEQ4 step record layout. The mutation frozen source uses the same record. Files in older formats are rejected with a clear error; there is no migration, and rejected files are never deleted.

## Display pacing

The display producer does not issue a new frame generation until the immutable frames of both LCDs have completed, and does not consume clock slots while blocked. Navigation and track changes are drawn once, immediately, when the output is ready. The steady clock is 30 Hz.
