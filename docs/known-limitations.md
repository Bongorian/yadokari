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
  (A/B) card, remote diagnostic and update commands require the
  DATA/REMOTE.ON marker; official images bundle the marker by
  default so open-source users can develop against the release
  build. Delete the marker and reboot to opt out. Gadget
  enumeration for local file transfer and status is a separate
  gate and does not by itself enable diagnostics.

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
  channel enumerates with no opt-in file, for local file
  transfer and status. (On Pi 4 the channel enumerates when
  the REMOTE.ON marker is present — official images bundle
  it — or from the update screen. Remote diagnostics on
  either board additionally require the prepared A/B layout
  and the marker; see "Updates and signing".)
- **A 16 GB Pi 5 currently gets no CPU-heap arena** (the
  Circle memory map covers at most 8 GiB of DRAM); it runs
  with the pre-arena memory layout.
- **Wi-Fi is disabled on Pi 5 builds too**, for the same
  licensing reason as on Pi 4 (see Connectivity).
