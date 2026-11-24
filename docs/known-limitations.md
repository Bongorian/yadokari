# Known limitations

Honest, release-time facts about what YADOKARI does not do (yet).
No test records live in this repository — this page is the whole
story, kept current per release.

## Connectivity

- **Wi-Fi is disabled in the release build.** The WLAN support
  available to this firmware includes Plan 9-derived code
  licensed under the GPL v2 only, and its combination with this
  GPL v3 firmware has not been cleared, so the official image is
  built with WLAN support off (`ENABLE_UPDATE_WIFI=0`). File
  transfer works over Ethernet and USB. In builds that enable
  Wi-Fi, its access point is open and its file transfer is
  plaintext HTTP — use it only on networks you trust.

- **Network MIDI is Ethernet-only, without SysEx.** RTP-MIDI
  takes up to four hosts at once and ignores SysEx in both
  directions. Received channel messages act at the start of the
  next audio block after their due time (clock and transport act
  on their exact sample). Output follows the MK3 MIDI OUT queue,
  so it pauses while that output waits to retry. Once NETWORK MIDI
  has started Ethernet it stays up until restart; in builds that
  enable Wi-Fi, Wi-Fi file transfer then needs a restart with the
  setting off. The session name
  is always `YADOKARI`; with two units on one network, add each by
  IP. Notes from a host that never answers YADOKARI's clock probe
  (CK) are released only when its session times out after 60
  seconds of silence, and such a host is not rechecked after a long
  pause of the main loop.
- **Network MIDI reconnects only to a remembered address.**
  YADOKARI invites a host again at the address and control port it
  last connected from; a host whose address changed (a new DHCP
  lease) has to connect again from the computer. A host that ended
  the session itself, or refused an invitation, is not invited
  until it connects again, so a computer that says goodbye when it
  shuts down is not invited after it starts. During FILE TRANSFER
  and updates received network MIDI is not played, and while a
  large file is checked the sessions are not served at all; they
  are checked afterwards and invited again if they were lost. A
  Disconnect pressed on the computer during that check counts as
  such a loss, so the session has to be disconnected again after it.
  A Mac whose connection policy is *Directory entries* drops the
  session during a long check and then refuses YADOKARI's
  invitations for at least 7 minutes, so the session comes back by
  itself only with *Anyone*; otherwise connect again from the Mac.

## Updates and signing

- **One baked-in update key, no rotation.** Official images trust
  the maintainer's release key; self-built images trust your own.
  The two do not cross-update, and moving between them means
  re-flashing the SD card (your songs and settings live on the
  card's data partition workflow — see the update guide).
  A lost or compromised key likewise means re-flashing; there is
  no remote key-revocation mechanism.
- **Distribution is a provision-built A/B image.** Releases ship
  as one board-specific image (BOOT / A / B / DATA partitions);
  the single-partition ZIP of earlier releases is retired. Moving
  from a single-partition card means flashing the new image —
  back up your songs first. The provisioned DATA volume starts
  small and Recovery expands it to the end of the card on first
  boot; an interrupted expansion resumes in Recovery.
- **Remote diagnostics are opt-in by marker.** On a prepared
  (A/B) card, USB file sessions, remote diagnostics and update commands require the
  DATA/REMOTE.ON marker; official images bundle the marker by
  default so open-source users can develop against the release
  build. Delete the marker and reboot to opt out. USB STATUS remains available when the gadget enumerates.

## Content and formats

- **SoundFont content.** The factory image includes FluidR3 GM for its factory presets. Other banks are user-supplied. Large-bank load time depends on the card and board.
- **No bundled demo song yet.** The default and recovery song
  slot (FACTORY/DEMOS/Startup.mgxs) is in place, but the official
  demo song has not been supplied yet; until it is, a card with
  no saved song boots to an empty project.
- **Current formats only.** Project/preset/kit files use the
  current format family (.mgxs/.mgxk/.mgxp). Files written for
  earlier, pre-release format revisions are not migrated; the
  device reports a clear format error instead.

## Performance ceiling

- The engine is real-time software on a Pi 4 (a Pi 5 raises
  the polyphony budget by a measured factor of about 3x;
  the ceiling is still there). The heaviest
  combinations — large layered patches with full effects at high
  polyphony — can reach the CPU ceiling; FREEZE (bounce a track
  to audio) and TWIN (sample a heavy track in advance) exist
  precisely for that.

- WAVETABLE uses additional DSP when WARP AMT or PM AMT is nonzero.
  The legacy path costs 2 units per oscillator and the extended path costs 11.
  The Pi 4 track allows four oscillators in total, including at most two extended
  oscillators. The board profile scales both limits. WAVETABLE tracks and Layer
  leaves share a 48-unit Pi 4 budget. When an edit, lock or morph needs more
  capacity than is available, the held voice continues on its admitted path
  and retries on a later control tick. Release crossfades retain their reservation.
  The extended reservation is 65.68 µs per 24 frames, derived from a 42.03 µs
  Pi 4 estimate with two 25% margins. The registry rounds up to 68.75 µs
  (11 units). Applying the same conversion and margins to the current Pi 5
  peak gives 68.24 µs per oscillator. Pi 4 distribution remains blocked
  pending mixed-load measurements on that board against the 500 µs deadline.

- High-register WAVETABLE patches with warp or PM use a band-limited sum of
  carrier harmonics and PM sidebands. Existing projects retain their settings;
  their upper-register timbre can change. With warp and PM both at zero,
  one oscillator uses the original table reader.

## Hardware scope

- Raspberry Pi 4 (4 GB RAM or more) is the recommended and
  reference environment; Raspberry Pi 5 (4 GB RAM or more)
  is also supported (see "Pi 5" below). The MASCHINE MK3
  must be stock and
  unmodified (that is the point), and is used over USB exactly as
  a computer would use it.

## Pi 5

- **Supported from 0.9.1, next to the recommended Pi 4.**
  Projects use the same file format on both boards. The Pi 5
  has higher polyphony and Layer budgets. A Layer note that
  fits on a Pi 5 can be refused during playback on a Pi 4.
  See [hardware profiles](hardware-profiles.md).
- **USB-C CDC is on by default on Pi 5**: the control/update
  channel enumerates with no opt-in file, for STATUS. (On Pi 4 the channel enumerates when
  the REMOTE.ON marker is present — official images bundle
  it — or from the update screen. Remote diagnostics on
  either board additionally require the prepared A/B layout
  and the marker; see "Updates and signing".)
- **A 16 GB Pi 5 currently gets no CPU-heap arena** (the
  Circle memory map covers at most 8 GiB of DRAM); it runs
  with the pre-arena memory layout.
- **Wi-Fi is disabled on Pi 5 builds too**, for the same
  licensing reason as on Pi 4 (see Connectivity).
