# 🐚 The story so far

*A side story. This page is the whole of it — it is not a changelog
and it will not grow into one. What happens next is told by the
releases themselves.*

---

A MASCHINE MK3 is a wonderful piece of hardware with one quiet
dependency: it does nothing without a computer. The pads, the knobs,
the two colour screens — all of it waits for a machine on the other
end of the USB cable to tell it what it is.

So the question that started everything was simple: *what if the
computer was a Raspberry Pi, and the Pi didn't run an operating
system at all?* Bare metal. The Pi moves into the USB port, plays the
role the computer used to play, and the controller becomes an
instrument that stands on its own.

That experiment lived in a private repository for one autumn —
**1,133 commits**, the firmware's own version counter ticking up to
**v570**. It was vibe-coded in the open-hearted sense of the word: a
human designing, arguing and listening to every version on the real
hardware, and AI pair programmers — Meta's **Muse** and OpenAI's
**Codex** — writing the code in long sessions, then testing it
against the actual machine until it behaved.

Some scenes from along the way:

**The first sound** was the whole proof of concept — one pad, one
note, no computer in the room. Everything after that was just
refusing to stop.

**The screens** were drawn and redrawn more than any other part.
The rule that finally stuck: *a value should be visible as a shape* —
you should see what a knob is set to, not read about it. Nine colour
themes, one drawing grammar, and a two-line legend that always shows
what the eight knobs are and what they're set to.

**The factory library** grew to 187 presets and kits — famous
machines called by their names, TB-303 and friends — and then every
one of its 384 drum pads was measured and level-balanced, one by one,
because a kit that whispers on one pad and shouts on another isn't an
instrument yet.

**The borders learned to translate.** Inside, the machine keeps its
own performance order; at its edges — MIDI in and out, song files,
SoundFonts — it speaks the world's conventions, GM included, so
nothing you already own is stranded outside.

**TWIN** arrived when tracks got heavy: press one button and the
device plays a synth track's notes one by one, captures them as a
sampled twin of itself, and swaps the twin in at the next bar —
READY, ACTIVE, and STALE if you've touched the original since.
A heavy track, folded into the instrument's pocket.

**Songs arrived from AI pipelines** and were judged the old way: by
ear, on the device, most of them rejected. The ones that survived
taught the pipeline more than the passes did.

**The power was cut mid-write**, on purpose, more than once. The
card survived. An instrument you can't trust with its own songs is
a toy, and nobody here wanted a toy.

And the rule underneath all of it, the one that never bent: **the
shell stays untouched.** No hardware mod, no rewritten controller
firmware. Unplug the Pi and the MASCHINE is a MASCHINE again, exactly
as it left the factory.

The name came last, the way names do. An animal that borrows an
empty shell, lives in it without changing it, and walks away when it
outgrows it: *yadokari*, the hermit crab. A hand-drawn swirl shell
became the mark; the splash screen — the mark on black, the firmware
stamping its own version underneath — was the last thing v570 ever
added.

Then the private tree was frozen, audited line by line for anything
that wasn't the public's business, and snapshotted into the
repository you're reading now: one clean history, the version
counter rebased, v570 reborn as **0.9.0**.

The story continues in the open now. 🐚
