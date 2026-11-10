# Update and control-plane threat model

This document states what the firmware update signature system does
and does not claim, who it defends against, and the decision taken for
the one known authentication gap (review item M1). It covers the
signed update path (`src/update/manager.h`, `firmware/update_runtime.cpp`,
`firmware/recovery/`), the boot journal, and the GBX1 control plane
(`src/update/serial_control.h`). Current implementation is the authority; physical verification scope is recorded in implementation status. The threat model is not a power-loss test result.

## Assets and actors

Assets: the application and recovery firmware images, the update
journal (which slot is active/pending, per-slot version, size and
SHA-256, and the highest confirmed version), the user's project and
sample data on the DATA partition, and the device's availability as a
musical instrument.

Actors considered:

1. **Network attacker** — can reach the Ethernet control endpoint
   (TCP 16700 transfer / 16701 control) but has no physical access and
   no signing key.
2. **Bus attacker** — can speak to the USB-C CDC / UART control
   endpoint (GBX1), which in practice requires physical possession of
   the device or of a host it is plugged into.
3. **SD attacker** — can remove the SD card and write arbitrary
   sectors of it offline.
4. **Accidental corruption** — power loss mid-write, worn media,
   truncated transfers. Not an attacker, but the journal design spends
   most of its machinery here.

## What is authenticated, and when

- **Update packages are signature-verified at install time.**
  `Manager::begin()` requires the GUP1 header's Ed25519 signature
  (over the header's first 128 bytes) to verify against the pinned
  public key, and the header's key fingerprint to equal SHA-256 of
  that key. The package version must exceed the journal's `highest`
  (anti-rollback by version counter). The received image's SHA-256
  must match the signed header, and the image is re-read from storage
  and re-hashed before the journal records it as Ready.
- **Slot images are re-verified before every boot of that slot.**
  `trial()`, `boot_active()` and `rollback_confirmed()` re-hash the
  stored image and compare against the hash recorded in the journal
  at install time. A slot whose bytes changed after install is never
  booted by the manager.
- **Recovery is a separate, minimal image** on its own partition. It
  runs the same manager; a failed or unconfirmed trial returns to the
  recovery/stable path and never replaces the confirmed active slot.

So "signature verified" claims exactly this: *a package delivered
through the update channel was produced by the holder of the signing
key, and the bytes booted from a slot are the bytes that were
verified at install time — provided the journal itself is intact*
(see below).

## The journal is integrity-protected, not authenticated (M1)

The journal (`GJR1`, 512 bytes, two copies) carries an unkeyed
SHA-256 over its first 480 bytes, a sequence counter, dual copies
with write-then-readback verification in `persist()`. That detects
accidental corruption and torn writes (actor 4). It does not
authenticate: anyone who can write the journal sectors can forge a
state whose checksum is valid — for example swapping active/pending,
rewriting a slot's recorded hash to match a substituted image, or
lowering `highest` to re-admit an old (still validly signed) version.

How much that matters depends on the actor:

- Against the **network attacker** it does not matter: no network
  path writes journal sectors. The storage layer exposes no arbitrary
  path API; only the inactive image slot and the two journal sectors
  are writable, and only through the manager's verified flow.
- Against the **SD attacker** it also does not matter, because the
  journal is not the weakest link: the Raspberry Pi 4 boot chain in
  this product does not verify the boot partition's kernel images.
  An attacker who can write the SD can replace the application or
  recovery image directly, without touching the journal at all.
- The gap is real only for a hypothetical actor who can write the
  journal sectors but *not* the boot images or the firmware — no such
  partial write path exists in the current system.

## Journal authentication decision

Options from the review: (a) add a MAC or signature to the journal,
(b) re-verify journal-held hashes at boot (already partly present —
see the slot re-verification above), or (c) both.

**Decision: defer journal authentication until the trust boundary changes.** A journal MAC needs
a device secret; this hardware has no secure element or protected
key storage, so the key would live in the firmware image on the same
SD card — extractable by exactly the SD attacker the MAC would be
meant to stop, and useless against the direct image replacement that
attacker prefers anyway. A journal signature has the same problem
(the device must hold the signing key to record its own state
transitions). Spending either mechanism now would buy protection only
against the non-existent partial-write actor. Revisit if any of
these change: a network-reachable path to journal sectors appears,
protected key storage becomes available, or the boot chain gains
image verification (at which point the journal becomes the weakest
link and should be authenticated first).

What is done instead, and already true: corruption resistance
(dual copies, sequence, write-verify), install-time signature
verification, and per-boot slot re-hashing — so the *operational*
claim "the device only boots images that were signed at install
time" holds under journal integrity, and journal integrity holds
against every actor except one who already owns the boot media.

## GBX1 control plane

The GBX1 line protocol (USB-C CDC and TCP 16701) has **no
authentication**. Its commands include update control (ENTER / TRIAL
/ CONFIRM / BOOT / CANCEL), hardware test workflows, and BENCHMARK,
which rewrites the DATA tree to the benchmark layout. Possession of
GBX1 access is therefore operator-equivalent: anyone who can speak
GBX1 can stop playback, reboot the device, stage and confirm updates
(the packages still need valid signatures), and destroy user data
via the benchmark cleanup.

The accepted trust model, consistent with the setup documentation,
is: GBX1 is exposed only on physically controlled interfaces
(UART / USB to a trusted host) and on trusted networks — the Ethernet
endpoint is for the owner's LAN, and the docs already require
limiting it to a trusted network. Remote control is opt-in
(`REMOTE.ON` on the DATA partition) and off by default. This model is
accepted for this private deployment; if the device is ever exposed to untrusted
networks, GBX1 needs authentication before that exposure, not after.
