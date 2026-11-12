#!/usr/bin/env python3
"""Port the Pi 5 CDC gadget support into Circle (measurement builds).

Applies scripts/circle-pi5-cdc.patch: the exact working-tree delta
between the stock-patched Circle checkout and the experiment tree's
CDC-verified checkout (DWC2 device-mode for BCM2712: slave/PIO pump,
PowerOn skip when the core is already live, GRXSTSP SETUP PKTSTS 6,
IRQ counters + polled-service fallback, gadget lib build for
RASPPI=5). The delta was captured file-by-file from the tree where
enumeration + GBX1 over CDC were verified on hardware (v1013/v1014);
applying it must reproduce those 13 files byte-for-byte.

Usage: patch-circle-pi5-cdc.py <circle-home>
Idempotent: a second run detects the applied state and exits 0.
"""
from pathlib import Path
import subprocess
import sys

FILES = [
    "include/circle/bcm2711int.h",
    "include/circle/bcm2835.h",
    "include/circle/usb/dwhcidevice.h",
    "include/circle/usb/dwhci.h",
    "include/circle/usb/dwhciregister.h",
    "include/circle/usb/gadget/dwusbgadgetendpoint0.h",
    "include/circle/usb/gadget/dwusbgadgetendpoint.h",
    "include/circle/usb/gadget/dwusbgadget.h",
    "lib/usb/dwhcidevice.cpp",
    "lib/usb/dwhciregister.cpp",
    "lib/usb/gadget/dwusbgadget.cpp",
    "lib/usb/gadget/Makefile",
    "lib/usb/gadget/dwusbgadgetendpoint.cpp",
]

SENTINEL_FILE = "lib/usb/gadget/dwusbgadget.cpp"
SENTINEL = "HandleRxFIFOLevel"


def run_patch(circle, patch_file, reverse=False, dry_run=False):
    cmd = ["patch", "-p1", "-d", str(circle), "-i", str(patch_file),
           "--no-backup-if-mismatch", "-f"]
    if reverse:
        cmd.append("-R")
    if dry_run:
        cmd.append("--dry-run")
    return subprocess.run(cmd, capture_output=True, text=True)


def main():
    circle = Path(sys.argv[1])
    patch_file = Path(__file__).resolve().with_name("circle-pi5-cdc.patch")
    if not patch_file.is_file():
        raise SystemExit("circle-pi5-cdc.patch missing next to this script")
    if run_patch(circle, patch_file, reverse=True, dry_run=True).returncode == 0:
        print("Circle Pi 5 CDC patch already applied")
        return 0
    dry = run_patch(circle, patch_file, dry_run=True)
    if dry.returncode != 0:
        sys.stdout.write(dry.stdout)
        sys.stderr.write(dry.stderr)
        raise SystemExit("Circle Pi 5 CDC patch does not apply cleanly; refusing")
    applied = run_patch(circle, patch_file)
    if applied.returncode != 0:
        sys.stdout.write(applied.stdout)
        sys.stderr.write(applied.stderr)
        raise SystemExit("Circle Pi 5 CDC patch failed while applying")
    if SENTINEL not in (circle / SENTINEL_FILE).read_text():
        raise SystemExit("Circle Pi 5 CDC patch applied but sentinel missing")
    print("Circle Pi 5 CDC gadget patch applied (13 files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
