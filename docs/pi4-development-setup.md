# Development setup

## Host

Prepare a C++17 compiler, make, Python 3, Node.js (for the transfer page's JavaScript / SHA256 checks), and the SDL2 development files. For dependency sources and licenses, see [third-party code](../third_party) and [assets](../assets/README.md).

```sh
make simulator
./scripts/run-virtual-mk3.sh --factory
# When not using the system SDL2
./scripts/setup-local-sdl.sh
```

`make test` runs with ASan/UBSan. To narrow the targets, specify test targets found in the Makefile in `TEST_TARGETS`. Audio/state demos are `make session-demo`, operation demos are `make workflow-demo` and the SDL `--workflow-test`, and sampler/data/routing is `make readiness-demo`. Do not repeatedly run the full suite when nothing has changed.

## Pi 4 / Circle

Prepare Circle and an AArch64 bare-metal toolchain and specify absolute paths that actually exist. Do not treat this PC's own `/tmp` or `.deps` as if they existed in another environment. The inputs are the [build configuration](../firmware/circle-pi4-multicore-4m.mk), the [dependency preparation scripts](../scripts), and the [firmware Makefile](../firmware/Makefile).

```sh
CIRCLEHOME=/absolute/path/to/circle PREFIX64=/absolute/path/to/bin/aarch64-none-elf- make firmware
```

Use this repository's profile for Circle's `Config.mk`. The heap's reusable buckets must reach 512MiB. With Circle's default 512KiB cap, even deleting a 46032612-byte Project cannot be reused. When the profile changes, the build script rebuilds the Circle libraries and the app/recovery entirely.

Build the app and the recovery. A successful build is not a hardware test. The hardware does not require Linux. Update an existing A/B card following the [update procedure](firmware-updates.md).

`scripts/prepare-sd.sh` is a tool that assembles a boot tree under `build/sdcard` and does not write to a physical disk. It bundles the factory assets and licenses. Do not confuse creating a first SD image with updating an existing card online. An online update does not replace existing DATA or the recovery.

## Documentation and screens

```sh
make docs-check
make -f Makefile -f tools/manual.mk manual-screens
python3 tools/build-manual.py
```

Uses Pillow and markdown-it-py. The distribution HTML is generated in `build/manual/`, and a PDF with `--pdf` (WeasyPrint required). Specify `--publish-assets` only when updating the canonical images. Generated images are host fixtures, not photographs of the hardware.
