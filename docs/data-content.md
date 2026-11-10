# DATA and Standard Content

Canonical definitions: [layout and exclusion rules](../src/data_layout.h), [drum assets](../src/factory_drum_assets.h), [restoration processing](../src/factory_content.h), [device storage](../firmware/project_storage.cpp). The saved byte structure is [MGXC format 13](project-format.md).

## Storage locations

| Inside DATA | Contents | Editing / saving |
|---|---|---|
| `FACTORY/DEMOS/` | Demo songs | Currently empty |
| `FACTORY/DEBUG/` | Benchmark.mgxs, Test-Tone.mgxp, 440Hz.wav, Sine.sf2 / SF2-Sine.mgxp | The originals for debugging. Reloading restores the initial state |
| `FACTORY/KITS/` | Circuit / Dust / Wood .mgxk, plus an optional Techno-01.mgxk | The three kits are always generated. Techno-01 only when its existing 16 WAVs are all present |
| `FACTORY/PRESETS/` | 16 .mgxp across WAVETABLE / VA DUAL / FM 6 / ADDITIVE / PHASE DIST / Layer | On a full install, the originals are verified and restored. Not automatically loaded into the pool |
| `FACTORY/DRUMS/` | Original WAVs, 3 kits × 6 types | Generated and verified on a full install. Save editing copies into PROJECTS |
| `PROJECTS/` | User songs, edited versions of demos, COLLECT output | Writable. The default destination for new saves |
| `RECORDINGS/` | WAV recordings | Writable. Incomplete WAV `.part` files are not deleted automatically |
| `PRESETS/` | Named presets as .mgxp | Where presets are saved. A name collision asks OVERWRITE / CANCEL. Saving does not consume the Project pool |
| `KITS/` | Kit sounds as .mgxk | The regular user Kit shelf. Old formats are not loaded |
| `ASSETS/SAMPLES/`, `ASSETS/SOUNDFONTS/`, `ASSETS/MIDI/` | WAV, SF2 and MIDI assets | License information and the actual audio are kept. New containers are .mgxs / .mgxk / .mgxp |
| `GROOVE/CURRENT.TXT` | The location of the last opened/saved song | Not shown in the Browser |
| `BENCH.MGXS` | The canonical system copy of the fixed evaluation song | Not shown in the Browser. Opened via the alias copy in Debug |
| Root REMOTE.ON / RECOVERY.ON / network settings, `FIRMWARE/` | Operational information for updates and communication | Not shown in the Browser |

The Browser shows supported `.mgx` / `.mgxs` / `.mgxk` / `.mgxp` / `.wav` / `.mid` / `.midi` files and recoverable recording `.part` files (PART). For MGX files it displays SONG / KIT / PATCH from the leading TYPE, and OPEN also follows the kind. Unknown extensions are not shown as WAV. The 64-entry list limit is applied after out-of-scope files are excluded; the folder list limit itself is 64 entries.

## Standard content

No demo songs are bundled, and the 256-slot preset pool is not pre-filled.

- **BROWSER → FACTORY → DEBUG → Benchmark.mgxs**: operational-check data that uses the 16 persistent tracks.
- **FACTORY → DEBUG → Test-Tone.mgxp / 440Hz.wav**: material for checking editing presets and PCM.
- **FACTORY → KITS**: the three kits Circuit / Dust / Wood. Pads 1–6 are Kick / Snare / Closed Hat / Open Hat / Clap / Tom. The 18 WAVs are generated from the firmware.

To keep the results of editing standard data, save with **PROJECT → SAVE AS** into PROJECTS. Re-OPENing the same original returns it to its initial state. If the boot destination is invalid, the device falls back to the Benchmark in Debug.

## Restoration and tidying

The Kit and Debug definitions live in the firmware; the files in DATA are copies for the user to open. On first boot, on a firmware version change, on an invalid install marker, or when files are missing or have the wrong size, everything is generated and content-verified before the audio worker starts, and only mismatches are replaced via temporary write → verification → replacement. A normal boot checks `GROOVE/FACTORY.CHK` and the existence and size of the target files, and skips generation and content reads. Same-size changes / bit rot are not detected until the next full install. See the boot gate. Writes, deletions and renames in the standard area are refused from normal save processing. User-saved copies are not targets of standard restoration.

Tidying at normal boot is limited to the FACTORY area only. Current and future containers and assets of the MGX family are kept; legacy factory names are removed only at exact-match paths, after replacement has been prepared. A normal LOAD also verifies the container CRC and state.

Tidying consists only of operations inside DATA via FatFs. It never formats a partition, writes raw blocks, or updates the immutable recovery. On a development boot from a single SD root, only the standard content is prepared; nothing is deleted.

The host's `make factory-catalog` generates the same Kits and Debug content into `build/factory-data/`. Build catalogs and measurement CSVs stay on the host and are excluded from what is copied to the SD card. `make factory-content` generates the 16 PCM files, the Techno debug Kit with fixed identities, the operational install and the benchmark. The 103 official preset masters are collected in `assets/factory-library`. Do not update standard content by hand-editing generated files; change the definition code and regenerate.

The standard for new saves is Song = `.mgxs`, Kit = `.mgxk`, Preset = `.mgxp`. All four extensions are enumerated case-insensitively, and even if the extension and the content disagree, the file is opened by ContentKind. The `.mgx` suffix is enumerated for discovery only; files in older formats are rejected with a clear error, and there is no migration. Existing user files are not renamed, and a SAVE to an existing name keeps that name. Future kinds also use "mgx + kind suffix"; the details are consolidated in [the format contract](project-format.md).

COLLECT gathers the assets referenced by the persistent tracks, the 256-slot pool, audio clips, freezes and sample locks. Asset entries that are not referenced are removed from the exported Song, but the original Project and the original WAVs are not changed. Tape Micro / Spectral / WAVESEQ can also save internal waveforms held in empty asset slots; a Preset whose designated WAV is missing is not mounted.

The BAKE-guided COLLECT outputs a single saved Preset, plus only the assets that Preset references, to `PRESETS/<UPPERCASED NAME>-EXPORT/PRESETS/preset.mgxp`. For a Song it is `PROJECTS/<UPPERCASED SONG NAME>-EXPORT/PROJECTS/project.mgxs`. Assets are gathered into `ASSETS/SAMPLES/` or `ASSETS/SOUNDFONTS/` within the same bundle and referenced from the container as `../ASSETS/...`. If you transfer the container plus each asset, the relative references within the same shelf are preserved.

## User file transfer

[Common transfer contract and USB CLI](reference/usb-files.md). In SETTINGS → SYSTEM → FILE TRANSFER, choose AP / STATION and press START. Open the displayed URL in a browser and enter the connection code. The shelves for Song / Preset / Kit / Samples / SoundFonts / MIDI / Recordings are shown with their capacity and add / download / delete operations. Playback is stopped during transfer; the session ends with CLOSE / BROWSER or after 10 minutes. A wired host can use the same transfer core over USB bulk via the USB CLI (see below).

PUT accepts only a file name and places the file on the correct shelf according to the content's TYPE and so on. Accepted types are `.mgxs/.mgxk/.mgxp/.wav/.mid/.syx/.sf2`, up to 1 GiB. MGX files are validated as MGXC13 containers, and even if the extension differs, the TYPE decides the shelf and the standard extension. A DX7 bulk file becomes 32 Presets under `PRESETS/DX7/<UPPERCASED BANK NAME>/`, and the original syx is not kept. The recording shelf is the destination for the device's own recordings; transferred WAVs go into SAMPLES.

An existing file with the same name is replaced only after the new candidate has been fully received and its stored hash and type have been verified. The current Project and all its declared assets are refused for both PUT and DELETE. Requests before authentication, outside the target shelves, with path traversal or absolute paths, or targeting FACTORY or operational files are refused. Only candidates that pass length, received/stored SHA256 and kind verification are promoted; an interruption processes that reception's candidate, and once replacement has begun, the verified transaction restores either the complete old or the complete new content. Temporary evacuation copies are erased on success or recovery, and no generational backups are kept. Unknown pre-existing tmp files are not deleted. Only directory notation is normalized to uppercase before boot; file names and shelf placement are never moved around on our own initiative. The source `assets/` are provenance inputs and are distinct from the distributed DATA.

COLLECT's asset copies get short index/CRC names. Long EXPORT folder names become a shortened name plus an identification hash; the file names of the original song, preset and assets are preserved. Check the actual destination in the completion display.

A wired development host can use the same core via list / get / put / delete in `tools/files/usb_files.py`. No authentication code is needed; during updates, recording or file processing the reply is BUSY, and the session ends with CLOSE / 30 s idle. See [USB procedure and wire](reference/usb-files.md) for details.
