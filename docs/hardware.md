# Hardware

The target hardware is a Pi 4 or Pi 5 / AArch64 running Circle, and the MASCHINE MK3. The Pi 4 is the reference environment; the Pi 5 is supported with measured, scaled-up polyphony budgets (see [known limitations](known-limitations.md)). The PC SDL backend is for development only.

| Connection | Purpose | Basis / limits |
|---|---|---|
| Pi USB-A → MK3 | HID input, LEDs, two LCDs, USB audio, MIDI | [USB implementation](../firmware/mk3_usb.cpp), [audio](../firmware/mk3_audio.cpp), [MIDI](../firmware/mk3_midi.cpp) |
| MK3 LINE OUT / PHONES | Main / Cue output | Analog output of the MK3 |
| MK3 LINE IN | External audio recording / monitoring | Analog input of the MK3; the recording / monitoring source |
| Pi USB-C → development PC | Power supply, GBX1 limited control | Not an implementation that presents USB audio / MIDI / storage to the PC |
| Ethernet → development PC | Signed updates and limited control | TCP 16700 transfer, 16701 control. No SSH / no Linux |

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

## 4-D direction key / main encoder

The `INPUTTEST` diagnostic logs changed HID reports and UI acceptance results for 120 seconds after the request, up to 128 events. It does not trigger playback or automatic screen switching.

