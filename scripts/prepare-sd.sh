#!/usr/bin/env bash
# Assemble a boot tree only; never formats, mounts, or writes a physical device.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
circle=${CIRCLEHOME:-"$root/.deps/circle-multicore-4m"}
prefix=${PREFIX64:-aarch64-none-elf-}
firmware_revision=0641c5bf25d185d5bf25d8dcafd1da6dc8fdbf68
cache="$root/.deps/pi-boot/$firmware_revision"
destination="${BOOT_OUTPUT_DIR:-$root/build/sdcard}"
test -s "$root/firmware/kernel8-rpi4.img" || { echo 'Run make firmware first' >&2; exit 1; }
python3 "$root/tools/update/pi_boot.py" --cache "$cache" --revision "$firmware_revision"
# factory-library first: assets/factory/library.json is a
# generated file (gitignored) that factory-check reads; a
# clean clone does not have it yet.
# factory-catalog is the maintained distribution generator
# (provenance-locked); the legacy canonical-catalog tool read
# uncommitted fixture shelves and is not part of assembly.
make -C "$root" factory-library factory-content factory-check factory-catalog
mkdir -p "$cache" "$destination/ASSETS/SAMPLES" "$destination/GROOVE"
for file in start4.elf fixup4.dat bcm2711-rpi-4-b.dtb LICENCE.broadcom COPYING.linux; do
 cp "$cache/$file" "$destination/$file"
done
make -C "$circle/boot" armstub64 PREFIX64="$prefix"
cp "$circle/boot/armstub8-rpi4.bin" "$destination/"
cp "$circle/boot/config64.txt" "$destination/config.txt"
cat >> "$destination/config.txt" <<'CONFIG'

[all]
enable_uart=1
CONFIG
mkdir -p "$destination/LICENSES"
cp "$root/LICENSE" "$destination/LICENSES/Groovebox-GPL-3.txt"
cp "$circle/LICENSE" "$destination/LICENSES/Circle-GPL-3.txt"
cp "$root/third_party/daisysp/LICENSE" "$destination/LICENSES/DaisySP-MIT.txt"
cp "$root/third_party/mutable/PROVENANCE.md" "$destination/LICENSES/Mutable-Provenance.md"
cp "$root/third_party/monocypher/LICENCE.md" "$destination/LICENSES/Monocypher.md"
cp "$root/assets/fonts/LICENSE.txt" "$destination/LICENSES/UI-Font.txt"
cp "$root/assets/fonts/atlas.json" "$destination/LICENSES/UI-Font.json"
cp "$root/third_party/msfa/LICENSE" "$destination/LICENSES/msfa-Apache-2.txt"
cp "$root/third_party/tinysoundfont/LICENSE" "$destination/LICENSES/TinySoundFont-MIT.txt"
cp "$root/third_party/mutable/LICENSE-MIT.txt" "$destination/LICENSES/Mutable-MIT.txt"
cp "$root/third_party/akwf/LICENSE" "$destination/LICENSES/AKWF.txt"
cp "$root/THIRD_PARTY_NOTICES.md" "$destination/LICENSES/Third-Party-Notices.md"
for directory in ASSETS FACTORY PROJECTS PRESETS KITS RECORDINGS; do
 python3 "$root/tools/copy-boot-assets.py" "$root/build/factory-data/$directory" "$destination/$directory"
done
cp "$root/docs/device-start.md" "$destination/START-HERE.md"
cp "$root/docs/firmware-updates.md" "$destination/FIRMWARE-UPDATES.md"
if [ "${ENABLE_UPDATE_WIFI:-0}" = 1 ]; then python3 "$root/tools/update/wifi.py" "$destination"; fi
# Observed on the connected bcdDevice 1.46 unit and the host audio driver.
# The firmware still validates the actual descriptors before using this pair.
# Set MK3_FEEDBACK_PROFILE=none to produce a configuration without a pair.
case "${MK3_FEEDBACK_PROFILE:-observed}" in
 observed) printf '%s\n' 'loglevel=4 soundopt=24 mk3fbout=1 mk3fbin=129' > "$destination/cmdline.txt" ;;
 none) printf '%s\n' 'loglevel=4 soundopt=24' > "$destination/cmdline.txt" ;;
 *) echo 'MK3_FEEDBACK_PROFILE must be observed or none' >&2; exit 1 ;;
esac
cp "$root/firmware/kernel8-rpi4.img" "$destination/"
python3 "$root/tools/boot-manifest.py" "$destination"
(cd "$destination" && sha256sum kernel8-rpi4.img armstub8-rpi4.bin start4.elf fixup4.dat bcm2711-rpi-4-b.dtb > SHA256SUMS)
echo "Boot files assembled at $destination; physical boot and audio format remain unverified."
