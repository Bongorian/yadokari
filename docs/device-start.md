# YADOKARI — device start

This card runs on a Raspberry Pi 4 or 5 (4 GB RAM or more) as a bare-metal AArch64 Circle instrument — not Linux, and not Native Instruments software.

Connect MASCHINE MK3 to Pi USB-A with suitable power. Connect LINE OUT to monitors and PHONES for Cue. Start at low monitoring volume. PAD MODE plays sounds; STEP edits notes; PLAY/STOP controls transport.

FILE opens PROJECT. SAVE writes the current song; NEW followed by SAVE asks for a name. SAVE AS creates another named song. OPEN selects an existing song. Wait for the completion message before switching off. COLLECT gathers the song and its referenced samples; copy the entire export folder.

SETTINGS → SYSTEM → FILE TRANSFER opens file transfer over Ethernet (Wi-Fi is disabled in this build). Press START, then open the displayed address and enter the connection code. Add files to their automatic content shelves, download or delete user content. BROWSER or CLOSE ends transfer; the mode also expires after ten minutes. Playback is stopped during transfer.

USB-C and Ethernet provide private development/update control, not general PC audio, MIDI or disk sharing. Online updates preserve DATA and recovery. Do not rewrite the whole card for an ordinary update.

Manual, build instructions and known limitations:
https://github.com/Bongorian/yadokari/tree/main/docs
