#!/usr/bin/env bash
# Package a fresh regular image for each board from completed signed-update builds.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?Usage: release-sd.sh NEW_OUTPUT_DIRECTORY [both|pi4|pi5]}
if (( $# > 2 )); then echo "Unexpected release arguments" >&2; exit 2; fi
case ${2-both} in
 pi5) boards=(pi5) ;;
 both|pi4) echo "Pi 4 release is blocked pending Wave B WAVETABLE voice admission. Select pi5 for a Pi 5-only release." >&2; exit 1 ;;
 *) echo "Expected both, pi4 or pi5" >&2; exit 2 ;;
esac
mkdir -p "$(dirname "$out")"
mkdir "$out"
out=$(cd "$out" && pwd)
staging=$(mktemp -d "$out/.stage-XXXXXX")
trap 'rm -r -- "$staging"' EXIT
mkdir "$staging/shared" "$staging/kernels"
for board in "${boards[@]}"; do
 if [[ $board == pi5 ]]; then script=build-firmware-pi5.sh; kernel=kernel_2712.img; else script=build-firmware.sh; kernel=kernel8-rpi4.img; fi
 "$root/scripts/$script"
 cp "$root/firmware/$kernel" "$staging/kernels/$board-app.img"
 cp "$root/firmware/recovery/$kernel" "$staging/kernels/$board-recovery.img"
done
CIRCLEHOME="${CIRCLEHOME:-$root/.deps/circle-pi5}" BOOT_OUTPUT_DIR="$staging/shared" "$root/scripts/prepare-sd.sh"
make -C "$root" update-simulator
for board in "${boards[@]}"; do
 name="yadokari-${FIRMWARE_VERSION:?firmware version}-$board"
 python3 "$root/tools/update/provision.py" "$out/$name.img" --board "$board" \
  --factory "$staging/kernels/$board-app.img" --recovery "$staging/kernels/$board-recovery.img" \
  --version "$FIRMWARE_VERSION" --boot-files "$staging/shared" --data-mib "${DATA_MIB:-1024}"
 (cd "$out" && zip -q "$name.img.zip" "$name.img" && sha256sum "$name.img.zip" >> SHA256SUMS)
done
