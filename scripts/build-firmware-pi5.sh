#!/usr/bin/env bash
set -euo pipefail
# Pi 5 counterpart of build-firmware.sh (0.9.1 dual-platform
# release): identical flow against the Pi 5 Circle checkout and
# release Config (firmware/circle-pi5.mk: strict update gate,
# CDC gadget on by default), RASPPI=5, plus the Pi 5 CDC gadget
# port patch after the stock patch series.
root=$(cd "$(dirname "$0")/.." && pwd)
circle=${CIRCLEHOME:-"$root/.deps/circle-pi5"}
default_circle=0
[ -n "${CIRCLEHOME:-}" ] || default_circle=1
prefix=${PREFIX64:-aarch64-none-elf-}
if ! command -v "${prefix}g++" >/dev/null; then echo "Missing ${prefix}g++ (Arm GNU AArch64 toolchain)" >&2; exit 1; fi
if [ ! -d "$circle/.git" ]; then
 mkdir -p "$(dirname "$circle")"
 git clone https://github.com/rsta2/circle.git "$circle"
 git -C "$circle" checkout 7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
 if [ "$default_circle" = 1 ]; then cp "$root/firmware/circle-pi5.mk" "$circle/Config.mk"; fi
fi
if [ "$default_circle" = 1 ] && ! cmp -s "$root/firmware/circle-pi5.mk" "$circle/Config.mk"; then
 echo "Default Circle checkout has an unexpected Config.mk; use a fresh checkout or explicitly set CIRCLEHOME" >&2; exit 1
fi
make -C "$root" factory-boot-assets-check
expected=7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
if [ "$(git -C "$circle" rev-parse HEAD)" != "$expected" ]; then
 echo "Circle revision differs from tested $expected: $circle" >&2; exit 1
fi
python3 "$root/scripts/patch-circle-fallible-heap.py" "$circle"
python3 "$root/scripts/patch-circle-cpu-heap.py" "$circle"
python3 "$root/scripts/patch-circle-storage-ui.py" "$circle"
python3 "$root/scripts/patch-circle.py" "$circle"
python3 "$root/scripts/patch-circle-audio.py" "$circle"
python3 "$root/scripts/patch-circle-audio-completion.py" "$circle"
python3 "$root/scripts/patch-circle-audio-input.py" "$circle"
python3 "$root/scripts/patch-circle-audio-chunk.py" "$circle"
python3 "$root/scripts/patch-circle-audio-lifetime.py" "$circle"
python3 "$root/scripts/patch-circle-audio-disconnect.py" "$circle"
python3 "$root/scripts/patch-circle-audio-status.py" "$circle"
python3 "$root/scripts/patch-circle-audio-sparse.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-stream.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-sound.py" "$circle"
python3 "$root/scripts/patch-circle-audio-subslot.py" "$circle"
python3 "$root/scripts/patch-circle-audio-pcm.py" "$circle"
python3 "$root/scripts/patch-circle-audio-mixed.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-zero.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-sync.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-capacity.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-submit.py" "$circle"
python3 "$root/scripts/patch-circle-audio-rx-select.py" "$circle"
python3 "$root/scripts/patch-circle-audio-duplex-removal.py" "$circle"
python3 "$root/scripts/patch-circle-audio-early-removal.py" "$circle"
python3 "$root/scripts/patch-circle-audio-tx-select.py" "$circle"
python3 "$root/scripts/patch-circle-audio-rx-length.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-diagnostics.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-detail.py" "$circle"
python3 "$root/scripts/patch-circle-implicit-counters.py" "$circle"
python3 "$root/scripts/patch-circle-rate.py" "$circle"
python3 "$root/scripts/patch-circle-range-data.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-timeout.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-blocking-race.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-async-critical.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-queue.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-iso-diagnostics.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-preflight.py" "$circle"
python3 "$root/scripts/patch-circle-xhci-cache-before-lock.py" "$circle"
python3 "$root/scripts/patch-circle-iso-errors.py" "$circle"
python3 "$root/scripts/patch-circle-audio-deep-queue.py" "$circle"
python3 "$root/scripts/patch-circle-hid.py" "$circle"
python3 "$root/scripts/patch-circle-hid-error.py" "$circle"
python3 "$root/scripts/patch-circle-hid-restart.py" "$circle"
python3 "$root/scripts/patch-circle-midi.py" "$circle"
python3 "$root/scripts/patch-circle-midi-dma.py" "$circle"
python3 "$root/scripts/patch-circle-midi-cache.py" "$circle"
python3 "$root/scripts/patch-circle-request-reuse.py" "$circle"
python3 "$root/scripts/patch-circle-hid-recovery.py" "$circle"
python3 "$root/scripts/patch-update-fatfs.py" "$circle"
python3 "$root/scripts/patch-update-gadget.py" "$circle"
python3 "$root/scripts/patch-circle-gadget-rx-cache.py" "$circle"
python3 "$root/scripts/patch-circle-dhcp-retry.py" "$circle"
python3 "$root/scripts/patch-circle-pi5-cdc.py" "$circle"
python3 "$root/scripts/update-public-key.py"
profile=$(printf "%s\n" "$circle"; cat "$circle/Config.mk" 2>/dev/null || true; cat "$circle/Config2.mk" 2>/dev/null || true; cat "$root/scripts/patch-circle-cpu-heap.py")
profile_hash=$(printf "%s" "$profile" | sha256sum | cut -d " " -f 1)
stamp="$root/firmware/.circle-profile-stamp"
circle_stamp="$circle/.groove-profile-stamp"
if [ ! -f "$circle_stamp" ] || [ "$(cat "$circle_stamp")" != "$profile_hash" ]; then
 for dir in tools lib lib/usb lib/usb/gadget lib/input lib/fs lib/fs/fat lib/sched lib/net lib/sound addon/SDCard addon/fatfs addon/wlan; do
  make -C "$circle/$dir" clean AARCH=64 RASPPI=5 PREFIX64="$prefix"
 done
 if [ -f "$circle/addon/wlan/hostap/wpa_supplicant/Makefile.circle" ]; then
  make -C "$circle/addon/wlan/hostap/wpa_supplicant" -f Makefile.circle clean AARCH=64 RASPPI=5 PREFIX64="$prefix"
 fi
fi
if [ ! -f "$stamp" ] || [ "$(cat "$stamp")" != "$profile_hash" ]; then
 make -C "$root/firmware" clean CIRCLEHOME="$circle" AARCH=64 RASPPI=5 PREFIX64="$prefix"
 make -C "$root/firmware/recovery" clean CIRCLEHOME="$circle" AARCH=64 RASPPI=5 PREFIX64="$prefix"
fi
# A failed build may leave mixed-profile objects; only mark a fully built tree.
rm -f "$stamp" "$circle_stamp"
for dir in tools lib lib/usb lib/usb/gadget lib/input lib/fs lib/fs/fat lib/sched lib/net lib/sound addon/SDCard addon/fatfs; do
 make -C "$circle/$dir" -j"$(nproc)" AARCH=64 RASPPI=5 PREFIX64="$prefix"
done
if [ "${ENABLE_UPDATE_WIFI:-0}" = 1 ]; then
 echo 'WLAN enabled for the requested private-use build; upstream license notices are retained.'
 git -C "$circle" submodule update --init addon/wlan/hostap
 make -C "$circle/addon/wlan" -j"$(nproc)" AARCH=64 RASPPI=5 PREFIX64="$prefix"
 make -C "$circle/addon/wlan/hostap/wpa_supplicant" -f Makefile.circle -j"$(nproc)" AARCH=64 RASPPI=5 PREFIX64="$prefix"
fi
make -C "$root/firmware" ENABLE_UPDATE_WIFI="${ENABLE_UPDATE_WIFI:-0}" CIRCLEHOME="$circle" AARCH=64 RASPPI=5 PREFIX64="$prefix" -j"$(nproc)"
python3 "$root/scripts/check-kernel-bounds.py" --nm "${prefix}nm" "$root/firmware/kernel_2712.elf"
python3 "$root/scripts/check-boot-stack.py" --nm "${prefix}nm" --elf "$root/firmware/kernel_2712.elf" --output "$root/build/boot-stack-pi5.json" "$root/firmware/kernel.su" "$root/firmware/file_transfer.su" "$root/firmware/project_storage.su"

make -C "$root/firmware/recovery" ENABLE_UPDATE_WIFI="${ENABLE_UPDATE_WIFI:-0}" CIRCLEHOME="$circle" AARCH=64 RASPPI=5 PREFIX64="$prefix" -j"$(nproc)"
python3 "$root/scripts/check-kernel-bounds.py" --nm "${prefix}nm" "$root/firmware/recovery/kernel_2712.elf"

printf "%s\n" "$profile_hash" > "$circle_stamp"
printf "%s\n" "$profile_hash" > "$stamp"
