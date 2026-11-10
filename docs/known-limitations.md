# Known limitations

Honest, release-time facts about what YADOKARI does not do (yet).
No test records live in this repository — this page is the whole
story, kept current per release.

## Connectivity

- **Wi-Fi is disabled in the 0.9.0 build.** The WLAN support
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

## Content and formats

- **No SoundFont is bundled.** The SF2 engine is built in, but
  banks are yours to add (licensing). The Browser shows a clear
  "bank missing" state when a preset references a bank that is
  not installed.
- **Current formats only.** Project/preset/kit files use the
  current format family (.mgxs/.mgxk/.mgxp). Files written for
  earlier, pre-release format revisions are not migrated; the
  device reports a clear format error instead.

## Performance ceiling

- The engine is real-time software on a Pi 4. The heaviest
  combinations — large layered patches with full effects at high
  polyphony — can reach the CPU ceiling; FREEZE (bounce a track
  to audio) and TWIN (sample a heavy track in advance) exist
  precisely for that.

## Hardware scope

- Raspberry Pi 4 (4 GB RAM or more) only. The MASCHINE MK3 must be stock and
  unmodified (that is the point), and is used over USB exactly as
  a computer would use it.
