#!/usr/bin/env bash
# Package a fresh regular image for each board from completed signed-update builds.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?Usage: release-sd.sh NEW_OUTPUT_DIRECTORY}
mkdir -p "$(dirname "$out")"
mkdir "$out"
out=$(cd "$out" && pwd)
staging=$(mktemp -d "$out/.stage-XXXXXX")
trap 'rm -r -- "$staging"' EXIT
mkdir "$staging/shared" "$staging/kernels"
for board in pi5 pi4; do
 if [[ $board == pi5 ]]; then script=build-firmware-pi5.sh; kernel=kernel_2712.img; else script=build-firmware.sh; kernel=kernel8-rpi4.img; fi
 "$root/scripts/$script"
 cp "$root/firmware/$kernel" "$staging/kernels/$board-app.img"
 cp "$root/firmware/recovery/$kernel" "$staging/kernels/$board-recovery.img"
done
BOOT_OUTPUT_DIR="$staging/shared" "$root/scripts/prepare-sd.sh"
make -C "$root" update-simulator
for board in pi4 pi5; do
 name="yadokari-${FIRMWARE_VERSION:?firmware version}-$board"
 python3 "$root/tools/update/provision.py" "$out/$name.img" --board "$board" \
  --factory "$staging/kernels/$board-app.img" --recovery "$staging/kernels/$board-recovery.img" \
  --version "$FIRMWARE_VERSION" --boot-files "$staging/shared" --data-mib "${DATA_MIB:-1024}"
 (cd "$out" && zip -q "$name.img.zip" "$name.img" && sha256sum "$name.img.zip" >> SHA256SUMS)
done
