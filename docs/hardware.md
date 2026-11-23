# Hardware

The target hardware is a Pi 4 or Pi 5 / AArch64 running Circle, and the MASCHINE MK3. The Pi 4 is the reference environment; the Pi 5 is supported with measured, scaled-up polyphony budgets (see [known limitations](known-limitations.md)). The PC SDL backend is for development only.

| Connection | Purpose | Basis / limits |
|---|---|---|
| Pi USB-A → MK3 | HID input, LEDs, two LCDs, USB audio, MIDI | [USB implementation](../firmware/mk3_usb.cpp), [audio](../firmware/mk3_audio.cpp), [MIDI](../firmware/mk3_midi.cpp) |
| MK3 LINE OUT / PHONES | Main / Cue output | Analog output of the MK3 |
| MK3 LINE IN | External audio recording / monitoring | Analog input of the MK3; the recording / monitoring source |
| Pi USB-C → development PC | Power supply, GBX1 limited control | Not an implementation that presents USB audio / MIDI / storage to the PC |
| Ethernet → development PC | Signed updates and limited control | TCP 16700 transfer, 16701 control. No SSH / no Linux |
| Ethernet → computers with a DAW | Network MIDI in and out (RTP-MIDI), up to 4 hosts | UDP 5004 / 5005, off by default: **SETTINGS → MIDI → NETWORK MIDI**; see [Network MIDI](#network-midi) |

Operating procedures are in the [manual](user-manual.ja.md).

The USB-C 1209:0001 identifier is a private test value, not a product ID unique to this unit. Use a suitable external power supply for the MK3, and sufficient power plus a data-capable cable for the Pi. A stall before USB initialization cannot always be diagnosed over CDC alone; keep physical access to the microSD card available.

## USB notes

For current implementation values, refer to the code. On a failed transfer, do not advance the sent state of LEDs / frames. Do not add a Pad release timeout or MPE dimensions derived from Pressure by speculation.

RGB/BGR565 is handled through an explicit profile. Strip wire output stays disabled until a calibration profile is deliberately chosen. This is a separate setting from operational Strip input.

## USB transfer ownership

LEDs are accepted through a [latest-state queue](../src/latest_led_state.h) and sent to a single asynchronous HID OUT request. A report in flight is immutable; only an older waiting state is replaced. An identical, already-succeeded state is not resent. On a short write / failure, the ACK is invalidated and the latest state is kept and retried; on timeout, the incomplete DMA is not reused and reconnection is awaited. Acceptance and USB success are distinguished, and no synchronous LED transmission is left in the task loop.

Each LCD keeps its existing packet / snapshot pair, and up to two requests — the maximum across both displays — are submitted simultaneously. The original byte sequence / compression / framing is not split; the corresponding shadow advances only after each packet succeeds. Frame completion is counted at the last differential ACK for each LCD, or at a full-match scan. Preparation / transmission of one display proceeds while the other waits for its ACK, and a rejected request does not commit the other display's state. The 30 Hz pacing / gate and the capture-anchored audio implicit cadence are unchanged.

The [request-reuse patch](../scripts/patch-circle-request-reuse.py) adds a post-completion reset to CUSBRequest: audio secures a fixed set of 8 requests/streams corresponding to the existing 8 buffers, HID IN a fixed 1 request, the displays a fixed 2 requests, and LEDs a fixed 1 request, all at setup time. Ordinary target callbacks do not create / destroy requests. LEDs / displays / DMA and the HID IN tombstone during disconnection are held until their callbacks complete; release happens in the task / at the next setup. A transfer that never returns is held for the duration of the boot. The unused generic Circle SendToEndpointOutAsync keeps its conventional allocate / release behavior, but MK3 output does not call it. Control/setup, MIDI and CDC are outside the scope of this request reuse.

## DIN MIDI

A user-connected **MK3 MIDI OUT → MK3 MIDI IN** cable forms a short round trip: transmit USB cable 0 → DIN OUT → DIN IN → receive USB cable 0. These are port numbers distinct from MIDI channels 1–16. The [NI rear-panel terminal documentation](https://docs.native-instruments.com/online-guides/maschine-mk3-manual/en/quick-reference) and [USB MIDI 1.0 §4](https://www.usb.org/sites/default/files/midi10.pdf) were consulted.

Input accepts only the MIDI interface that belongs to the same USB parent device as the MK3 HID. [mk3_midi.cpp](../firmware/mk3_midi.cpp) manages the receive queue and the disconnection generation, and in normal operation passes data from the [input router](../src/midi_input_router.h) to the Engine. Transmission proceeds from the [output pump](../src/midi_output_pump.h) to the same backend. Clock / Start / Continue / Stop are sent with USB CIN F. Because the pinned Circle `SendPlainMIDI` classifies these as CIN 5, this short transmission only is corrected through `SendEventPackets` (USB MIDI 1.0 Table 4-2).

To run the loopback check, finish recording and saving with the loopback connection in place, enable remote control, and run the following. For `--network`, give the Pi's IP as confirmed by STATUS; for `--output-dir`, give a directory that does not exist yet.

```sh
python3 tools/update/midi_loopback_test.py --network 10.42.0.46 \
  --output-dir build/din-midi-check
```

[MIDITEST](../src/midi_loopback_probe.h) stops playback and compares 261 messages one by one. Covered: Note On/Off on each channel, velocity 0, Poly / Channel Pressure, CC1 / 64 / 120 / 123, Program, Pitch Bend minimum / center / maximum, the CC sequence forming NRPN/RPN on ch1, 24 Clocks, Start / Stop / Continue, and Song Position Pointer. Reception during the test is not passed to the sound engine or transport, and normal MIDI transmission is also stopped after settling, to avoid re-transmission through the loop. After the test, normal processing resumes, but playback does not restart automatically. Saved song values are not changed.

This command is **for a direct loopback only**. Running it with an external sound source connected sends notes, Programs and CCs to all channels. RTT is the round-trip time including USB transfer and task processing.

### DIN loopback and normal transport

Received MIDI Start / Stop must not be echoed back to the output. With a DIN loopback connected, echoing received transport messages makes PLAY / STOP toggle rapidly; disconnecting the cable settles it. Received transport is therefore not re-sent. This applies to the normal input path and is distinct from the diagnostic MIDITEST 261-message round trip.

## Network MIDI

**SETTINGS → MIDI → NETWORK MIDI** turns it on; the setting is kept with the other device settings, so it stays on across restarts. The first time it is on, the firmware starts Ethernet (DHCP) and then runs **RTP-MIDI** ([RFC 6295](https://www.rfc-editor.org/rfc/rfc6295)) with the AppleMIDI session protocol on UDP 5004 (control) and 5005 (data). It is announced over mDNS as `YADOKARI` (`_apple-midi._udp`), so it appears in macOS Audio MIDI Setup → MIDI Network Setup, in rtpMIDI on Windows and in rtpmidid on Linux; any of them can also add it by IP and port 5004. Up to four sessions can be open at once, and a fifth host is refused. Turning the setting off ends every session (BY), releases their notes, closes the ports and withdraws the mDNS announcement; Ethernet itself stays up until the next restart.

**Reconnecting.** A host connects first. YADOKARI then remembers it (up to four hosts, newest first, in `GROOVE/NETHOSTS.CFG` on the card) and invites it again whenever its session is gone without a goodbye: after the Pi restarts, after a session timeout, or when the setting is turned on again. It sends the AppleMIDI invitation (IN) to the host's control port and then to its data port, 1 second after the loss and then at growing intervals, up to one every 30 seconds, while the host does not answer; a session YADOKARI opened keeps the clock in sync every 10 seconds. A host that ends the session itself (BY, for example Disconnect in Audio MIDI Setup) or refuses the invitation (NO) is forgotten until it connects again on its own, unless the goodbye came right after a long pause of the main loop (below). On macOS set the receiving session's **Who may connect to me** (Connection policy) to *Anyone*. *Directory entries* also lets sessions connect; with *No one* macOS refuses the invitation, and, for a session the Mac opened, accepts YADOKARI's packets without passing them to applications. With *Directory entries* macOS also refuses YADOKARI's invitations for at least 7 minutes after it has dropped a session itself (after about 50 seconds without answers, for example while a large file is checked), so only with *Anyone* does such a session come back by itself.

**NET LATENCY** on the same page (0–50 ms, default 5) is how long received messages wait after the time their RTP timestamp names, to absorb network jitter; a change applies at once. The right screen of the page shows the state under NETWORK MIDI: `Off`, the address and port with the number of connected hosts, `Waiting for DHCP`, or why Ethernet or the UDP ports are unavailable.

[The session logic](../src/rtp_midi_session.h) answers clock synchronisation (CK) and starts it when a host is quiet for 15 seconds; a session ended with BY, or silent for 60 seconds, ends and releases its notes. A pulled cable or a host that sleeps or crashes sends no BY, so while a session's notes or sustain pedal sound, a quiet host is asked every second, and one that has answered such a probe before but now stays silent for 3 seconds has its notes released; the session stays open and plays again when the host returns. Losing the Ethernet link on the Pi releases every session's notes at once. After the main loop has stood still for more than 5 seconds (a long file check during FILE TRANSFER, for example), packets that waited meanwhile look fresh, so every host that answers probes is asked again and a session whose host does not answer within 3 seconds ends; the host is then invited again as above. A goodbye (BY) read as the main loop resumes, or within 3 seconds after, counts as such a loss too (`bye after a pause` in the log): macOS ends a session after about 50 seconds without answers, so during a long pause the computer says goodbye on its own. Both directions carry a [recovery journal](../src/rtp_midi_journal.h) (chapters P, C, M, W, N, E, T, A for channels and Q for the sequencer): after a lost packet the receiver restores notes, controllers, programs, RPN/NRPN parameters, pitch bend, pressure and transport from the next packet. Journals are trimmed with receiver feedback (RS); a loss the journal cannot cover releases that host's notes.

**Input.** Each session is its own logical cable (12–15) in the same [input router](../src/midi_input_router.h) as MK3 MIDI, so Notes, CC, Program, Pressure, Pitch Bend, Clock, Start / Stop / Continue and Song Position Pointer behave as from DIN, including **AUTO → MIDI → K8 SLAVE CLOCK**, and a host that leaves releases only its own notes. Timing comes from the RTP timestamps, not from packet arrival: a jitter buffer follows the fastest path through the network, adds the latency above and schedules each message at that local time. Clock, transport and Song Position Pointer pass through the router with that time converted to an audio sample, so they reach the same clock estimator as DIN clock and act on that sample; channel messages are released by the main loop at that time and act at the next audio block. SysEx is consumed and ignored.

**Output.** Everything the engine sends to MK3 MIDI OUT (Clock and transport when it is the master, track MIDI output) is also sent to every open session, stamped with the time its audio sample was rendered.

The [firmware glue](../firmware/net_midi.h) logs each session's start and end, and every 60 seconds per open session: packets and messages in each direction, lost, repaired and late packets, uncovered losses, releases after host silence, guard packets, acknowledgements and round-trip time.

## 4-D direction key / main encoder

The `INPUTTEST` diagnostic logs changed HID reports and UI acceptance results for 120 seconds after the request, up to 128 events. It does not trigger playback or automatic screen switching.

