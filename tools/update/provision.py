#!/usr/bin/env python3
"""Build a NEW four-partition SD image; never opens a physical block device."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from update import identity

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--factory', required=True, type=Path)
parser.add_argument('--recovery', required=True, type=Path)
parser.add_argument('--boot-files', required=True, type=Path)
parser.add_argument('--version', type=int, required=True)
parser.add_argument('--data-mib', type=int, default=1024, help='Protected DATA capacity; 64..32768 MiB')
parser.add_argument('--offline-package', type=Path)
parser.add_argument('--early-pi4-eeprom-writable', action='store_true', help='Acknowledge physical verification on Pi4 rev1.0/1.1; does not change EEPROM')
parser.add_argument('--remote-control', action='store_true', help='Enable physically connected UART update commands on this new image')
args = parser.parse_args()
if not 64 <= args.data_mib <= 32768:
    parser.error('DATA capacity must be 64..32768 MiB for FAT32')
if args.output.exists() or str(args.output.resolve()).startswith('/dev/'):
    parser.error('Output must be a new regular file outside /dev')
for image in (args.factory, args.recovery):
    if not image.is_file() or not 0 < image.stat().st_size <= 8 * 1024 * 1024:
        parser.error('Missing or oversized kernel')
factory_version, factory_key, factory_kind = identity(args.factory.read_bytes())
_, recovery_key, recovery_kind = identity(args.recovery.read_bytes())
if factory_version != args.version or factory_kind != 1 or recovery_kind != 2 or factory_key != recovery_key or not any(factory_key):
    parser.error('Factory version, app/recovery identities, and nonzero trusted public keys must match')
boot_files = ['start4.elf', 'fixup4.dat', 'bcm2711-rpi-4-b.dtb', 'armstub8-rpi4.bin', 'LICENCE.broadcom', 'COPYING.linux']
for name in boot_files + ['config.txt']:
    if not (args.boot_files / name).is_file():
        parser.error('Missing boot file: ' + name)
config = (args.boot_files / 'config.txt').read_text()
config += '\n[all]\nkernel=KERNEL.IMG\nkernel_watchdog_timeout=15\nkernel_watchdog_partition=1\n'
cmdline_file = args.boot_files / 'cmdline.txt'
# Retain the caller's validated hardware profile in all boot partitions.
# Legacy boot bundles without cmdline keep the old conservative default.
cmdline = cmdline_file.read_text() if cmdline_file.is_file() else 'loglevel=4 soundopt=24\n'
if '\x00' in cmdline or len(cmdline.encode()) > 4096 or len(cmdline.splitlines()) != 1:
    parser.error('cmdline.txt must contain one bounded line')
cmdline = cmdline.rstrip('\r\n') + '\n'
run = lambda *items: subprocess.run([str(item) for item in items], check=True)
sim = root / 'build/update-sim'
run(sim, 'init', args.output, args.factory, args.version, args.data_mib)
with tempfile.TemporaryDirectory() as temp:
    staging = Path(temp)
    def put(partition, name, source):
        run(sim, 'put', args.output, f'{partition}:/{name}', source)
    for index in range(3):
        for name in boot_files:
            put(index, name, args.boot_files / name)
        for name, content in [('config.txt', config), ('tryboot.txt', config), ('cmdline.txt', cmdline)]:
            path = staging / name
            path.write_text(content)
            put(index, name, path)
    put(0, 'KERNEL.IMG', args.recovery)
    # Both app partitions are bootable immediately; only A is initially confirmed.
    put(2, 'KERNEL.IMG', args.factory)
    autoboot = staging / 'autoboot.txt'
    autoboot.write_text('[all]\ntryboot_a_b=1\nboot_partition=1\n')
    put(0, 'autoboot.txt', autoboot)
    if args.early_pi4_eeprom_writable:
        acknowledgement = staging / 'EEPROM.OK'
        acknowledgement.write_text('Operator acknowledged EEPROM write protection is disabled\n')
        put(0, 'EEPROM.OK', acknowledgement)
    for directory in ('ASSETS', 'FACTORY', 'PROJECTS', 'PRESETS', 'KITS', 'RECORDINGS', 'GROOVE', 'FIRMWARE', 'LICENSES'):
        source = args.boot_files / directory
        if source.is_dir():
            for file in sorted(source.rglob('*')):
                if file.is_file():
                    put(3, str(file.relative_to(args.boot_files)), file)
    for name in ('START-HERE.md','FIRMWARE-UPDATES.md','manifest.json'):
        source = args.boot_files / name
        if source.is_file():
            put(3, name, source)
    config_source = args.boot_files / 'wpa_supplicant.conf'
    if config_source.is_file():
        put(3, 'wpa_supplicant.conf', config_source)
    if args.offline_package:
        put(3, 'UPDATE.GUP', args.offline_package)
        marker = staging / 'RECOVERY.ON'
        marker.write_text('Stay in recovery; select PAD 3 to verify UPDATE.GUP\n')
        put(3, 'RECOVERY.ON', marker)
    if args.remote_control:
        marker = staging / 'REMOTE.ON'
        marker.write_text('Enable physical UART update control; access to UART can stop and reboot this device.\n')
        put(3, 'REMOTE.ON', marker)
print('Created', args.output, '(recovery + A + B + protected data). Physical boot remains unverified.')
