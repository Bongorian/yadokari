# Building YADOKARI

**Most people should not build anything.** Download the SD-card
image from [Releases](https://github.com/Bongorian/yadokari/releases), flash it, done — see the
README's Quick start.

Building is for developers. Everything below reproduces the exact
environment CI uses: Ubuntu 24.04, GCC 13.3, and a pinned Arm GNU
toolchain (versions are locked in
`tools/content/catalog_environment.lock.json` and the dev
container definition).

## Linux (native)

```sh
sudo apt install <packages from the lock file / container definition>
rustup toolchain install 1.100.0 --profile minimal --target aarch64-unknown-none
make test               # host test suite (no hardware needed)
make simulator          # PC virtual MK3 (needs SDL2)
scripts/build-firmware.sh   # firmware: fetches Circle at a pinned
                            # commit and applies the patch series
# Build board-specific BOOT/A/B/DATA images in a new directory.
# Set UPDATE_PUBLIC_KEY to the trusted raw 32-byte public key.
FIRMWARE_VERSION=10000 UPDATE_PUBLIC_KEY=/path/to/public.bin scripts/release-sd.sh build/release-100 pi5
```

The host suite checks moving wavetable controls against both reference oscillators. Run `make wavetable-motion-gate` for that check alone. Pi 4 and combined release packaging is blocked pending Pi 4 mixed-load qualification. For a Pi 4 bench SD or signed bench update, pass `--development` to `tools/update/provision.py` or `tools/update/update.py pack`.

The release builder verifies each factory asset against the locked SHA-256 in
`assets/factory-library/manifest.json`. Install the matching FluidR3 GM
SoundFont at `/usr/share/sounds/sf2/FluidR3_GM.sf2`, or set
`FACTORY_SOUNDFONT` to its path. The build also needs Python 3.11 or newer
and the `zip` utility. `DATA_MIB` controls the initial DATA capacity.
`scripts/prepare-sd.sh` assembles the shared files and runs the 187-original
mount gate in an empty `BOOT_OUTPUT_DIR`.

## Mac

Use the dev container — it is the same definition CI builds with,
so what passes here is what passes there:

```sh
# Docker Desktop (or another OCI runtime) required
docker build -f docker/Dockerfile -t yadokari-dev .
docker run --rm -it -v "$PWD":/src -w /src yadokari-dev bash
# inside the container: the Linux commands above
```

(VS Code users: `.devcontainer/` describes the same environment.)

Apple Silicon works natively (the locked environment is arm64).
Intel Macs cannot run the full build: the factory-content
environment is locked to native arm64, and the environment gate
fails by design under emulation. On an Intel Mac, use the release
SD image, or build on an arm64 machine.

## Windows

Best effort: **WSL2** with Ubuntu 24.04, then follow the Linux
steps; or the Docker route above under Docker Desktop. Native
Windows builds are not supported — the toolchain and image
assembly assume a Linux environment, and the container *is* that
environment.

## Update signing

Builds embed **your own** Ed25519 public key for update
verification (`UPDATE_PUBLIC_KEY`); there is no key material in
this repository. Official release images are signed with the
maintainer's release key, so a self-built unit and an official
image do not cross-update — switching means re-flashing the SD
card. See the README's "Update signing" section.
