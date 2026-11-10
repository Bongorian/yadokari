# Track kind colours and pad spatial mapping

## Current colours and spatial mapping

The fixed seven-kind LCD chips and the bottom-left-1 spatial mapping remain in effect. Pad LEDs use the fixed tokens of the UI organization and are not quantised per theme, even under MONOTONE. Layer = 14 purple, Empty = 17 dim white, root/tonic = 17 white. Event priority and the transmitted values are defined in the [LED canonical spec](led-behavior.md).

The level meter body also uses the same shared kind colour. The hold mark, warning/clip, and the neutral colours for non-Track elements are distinguished by the roles of Plan A, without changing hold times or theme values.

## Design

The ground colour is decided by passing the current EngineKind to a shared function. The fixed seven colours are maintained, and under MONOTONE the luminance of the same colour is projected to grey. The theme's existing event colours, neutral colours, lightness, and wire quantisation are unchanged. Sound, saved data, materials, and kit/logical pad numbers are unaffected.

The pad 4×4 is already bottom-left-1 in the current implementation. It is not inverted again; the same map is shown on screens that actually become TrackSelect while SELECT is held. The meanings and operations of Kit/Notes/Steps do not change. The eight horizontal mixer strips, the Tracker columns, the Scene summary, the MIDI channel list, and the Macro target mask are not pad position diagrams, so they keep reading order.

## Surfaces

| Surface | Treatment of colour and arrangement |
|---|---|
| Pad Track 4×4 | Already bottom-left-1. Numbers stay as they are; chips are unified to the current Track kind colour |
| SYNTH / FX / PAD / ROUTING / AUDIO / Modulation while SELECT is TrackSelect | Only the left screen temporarily shows the same 4×4. The right-hand details are kept, and the display returns on release |
| Normal diagrams of Routing / Audio / Modulation | They are diagrams of wiring, timelines, LFOs and the like, not position diagrams of the 16 Tracks, so they are kept. The common map can be shown with SELECT |
| Mixer 8 strips, Tracker columns, Scene composition summary, MIDI Ch table | Reading order is kept. They are lists for reading bank/channel/time, not diagrams of the same shape as the physical 4×4 |
| Macro MAP target mask | A mask in reading order. Colours are per Track kind, and SHIFT selection shows the common map even while SELECT is held |
| Kit / Notes / Steps / Clip / Scene | The meanings of logical Pads and performance are invariant. The existing 4×4 row inversion is kept, and the LED ground is the selected Track's kind |
| Files / Settings / Assets / Cells and the like | Neutral colours and operations are kept |

In the renderer, `theme.track[]` is actually referenced in five places. `LedStateEngine::encode` indices are generic intents such as the update screen, not Track references, and are out of scope together with buttons and strips.

On each Kit pad the chip reads KIT, representing the parent Track, so that the label matches the meaning of the ground colour. Pad numbers, material names, layout and shapes are kept, and the other existing screens are compared for wording and layout other than colour.

## MONOTONE and wire boundaries

The fixed seven colours take the existing values as canonical, and under MONOTONE the luminance `(54R + 183G + 19B + 128) >> 8` is projected onto RGB in common. The MONOTONE attribute is generated from the existing theme names, and no colour in themes9.json is changed. Accents, motion, danger, warn, and the button/strip grammar other than the ground colour and the chip background/foreground are unaffected. On the LED side this becomes the existing quantisation's white index 17, and that decoder's approximate RGB #eaf1ef is not a fully equal RGB. The physical emission is not falsely described as corrected to grey; it is left uncalibrated. To maintain the existing low-saturation judgement, LAYER is also approximated to white 17, and its wire hue overlaps EMPTY (each theme's values are in led-palette.json). On the LCD the original purple/grey is maintained. No claim is made that the seven kinds are fully distinguishable physically; numbers, names, and selection brightness are also used. This is an explicit constraint of the wire quantisation.
