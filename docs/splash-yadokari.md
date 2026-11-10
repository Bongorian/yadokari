# YADOKARI splash

## Assets and display

The canonical source is the [approved PNG](../assets/boot/splash-yadokari-480x272.png) (480×272 RGB, SHA256 `c563c54f63bc2f8c206be1294585faab71513f5cf2307f18fea4478e683e454a`). The [generator](../tools/ui/boot_splash.py) quantises each pixel of the original-size image by truncation to RGB565 and encodes it into the conventional MK3 pair RLE. There is no scaling, repositioning, redrawing of characters, or version number baked into the image. The two vector files are kept as the original artwork and are not used at runtime. The [provenance and hashes](../assets/boot/provenance.json), the [generation manifest](../assets/boot/splash-generated.json), and the [left/right preview](../assets/boot/splash-preview.png) are retained.

The left LCD is 32 bytes of all black; the right LCD is 31,748 bytes. The cap for the pair is 61,468 bytes. Theme and brightness settings are not applied. The first two packets are sent at the static-splash position, with an ACK wait of within one second, ahead of the factory marker judgement, the catalog, and the normal screens that follow.

## Runtime version number

[CKernel::Run](../firmware/kernel.cpp) passes `GrooveFirmwareVersion()`. This has the same source as the STATUS `GROOVE_FW_VERSION`. The [shared formatter and packet generation](../src/boot_splash_version.h) displays raw values below 900 as `v<raw>` (development numbering). From 900 up, values are packed semver — 0.9.0 = 900, 1.0.0 = 10000 — decoded as `major*10000 + minor*100 + patch` (900 → `v0.9.0`, 10000 → `v1.0.0`). 900 is the boundary between the two numbering schemes.

The string is not carried into the image or the generated include. The formatter covers the full uint32 range.

The 128×32 strip at x176/y240 in the lower centre of the right LCD is built from the original ACKed shadow, and the shared Barlow Semi Condensed Small (12 px) is drawn at top 244. Grey 168 is the flat colour of the tagline in the supplied PNG. The additional raw packet is 8,220 bytes, within the fixed cap. There is no additional transmission to the left side.

[CircleDisplay](../firmware/circle_display.h) permits the additional transmission only after successful static ACKs on both sides. It uses the existing packet page, Submit, and the ACK→shadow reflection path of raw rectangles, and does not rewrite the shadow before the ACK. The transmission is submitted at the start of Run, and a completed additional ACK is always reaped at the renderer handoff. A retry at this point happens only if the static ACK was delayed. There is no additional blocking wait or resident processing. After the first normal frame the addition is no longer possible, and if already submitted it is not sent twice. Failures, short ACKs, and stale epochs are passed to the conventional invalidation/redraw path.

## Behaviour

The generated image matches the PNG quantised to RGB565 across all 130,560 pixels, and the left side is all black. The version strip renders at the boundary and maximum version numbers within the packet cap. A partial static ACK, a refused transmission, or a failed, short, or stale ACK sends nothing extra; the strip is never submitted twice, and the caption disappears once normal frames begin. These behaviours are exercised by `boot_splash_test.py` against the actual production USB transfer and CircleDisplay.
