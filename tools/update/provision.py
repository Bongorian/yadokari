#!/usr/bin/env python3
"""Build a NEW four-partition SD image; never opens a physical block device."""
import argparse
import json
import hashlib
import os
import struct
from pathlib import Path
import subprocess
import tempfile
from update import check_release_image, identity

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--factory', required=True, type=Path)
parser.add_argument('--recovery', required=True, type=Path)
parser.add_argument('--boot-files', required=True, type=Path)
parser.add_argument('--version', type=int, required=True)
parser.add_argument('--board', choices=('pi4', 'pi5'), default='pi4',
                    help='Board matching both kernel images; default: pi4')
parser.add_argument('--data-mib', type=int, default=1024, help='Protected DATA capacity; 64..32768 MiB')
parser.add_argument('--offline-package', type=Path)
parser.add_argument('--development', action='store_true', help='Provision development-only firmware for bench use')
parser.add_argument('--early-pi4-eeprom-writable', action='store_true', help='Acknowledge physical verification on Pi4 rev1.0/1.1; does not change EEPROM')
parser.add_argument('--early-pi5-eeprom-writable', action='store_true', help='Acknowledge physical verification on Pi 5 PCB rev 1.0; does not change EEPROM')
parser.add_argument('--remote-control', action=argparse.BooleanOptionalAction, default=True, help='Include DATA/REMOTE.ON')
args = parser.parse_args()
if (args.early_pi4_eeprom_writable and args.board != 'pi4' or
        args.early_pi5_eeprom_writable and args.board != 'pi5'):
    parser.error('EEPROM acknowledgement must match the selected board')
if not 64 <= args.data_mib <= 32768:
    parser.error('DATA capacity must be 64..32768 MiB for FAT32')
if args.output.exists() or str(args.output.resolve()).startswith('/dev/'):
    parser.error('Output must be a new regular file outside /dev')
for image in (args.factory, args.recovery):
    if not image.is_file() or not 0 < image.stat().st_size <= 8 * 1024 * 1024:
        parser.error('Missing or oversized kernel')
factory = args.factory.read_bytes()
recovery = args.recovery.read_bytes()
if not args.development:
    try:
        check_release_image(factory)
        check_release_image(recovery)
        if args.offline_package:
            with args.offline_package.open('rb') as package:
                package.seek(192)  # GUP1 header and signature
                check_release_image(package.read(8 * 1024 * 1024))
    except ValueError as error:
        parser.error(str(error))
factory_version, factory_key, factory_kind = identity(factory)
_, recovery_key, recovery_kind = identity(recovery)
if factory_version != args.version or factory_kind != 1 or recovery_kind != 2 or factory_key != recovery_key or not any(factory_key):
    parser.error('Factory version, app/recovery identities, and nonzero trusted public keys must match')
board_files = {
    'pi4': ['start4.elf', 'fixup4.dat', 'bcm2711-rpi-4-b.dtb', 'armstub8-rpi4.bin'],
    'pi5': ['bcm2712-rpi-5-b.dtb', 'overlays/bcm2712d0.dtbo'],
}
boot_files = board_files[args.board] + ['LICENCE.broadcom', 'COPYING.linux']
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
final_image=args.output.resolve()
workspace=tempfile.TemporaryDirectory(prefix='.provision-',dir=final_image.parent)
args.output=Path(workspace.name)/'card.img'
run(sim, 'init-release', args.output, args.factory, args.version, args.data_mib)
with tempfile.TemporaryDirectory() as temp:
    staging = Path(temp)
    volumes=('BOOT','A','B','DATA')
    files={'A/KERNEL.IMG':hashlib.sha256(args.factory.read_bytes()).hexdigest()}
    def put(partition, name, source):
        with source.open('rb') as stream:
            files[volumes[partition]+'/'+name]=hashlib.file_digest(stream,'sha256').hexdigest()
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
    if args.early_pi4_eeprom_writable or args.early_pi5_eeprom_writable:
        acknowledgement = staging / 'EEPROM.OK'
        acknowledgement.write_text('Operator acknowledged EEPROM write protection is disabled\n')
        put(0, 'EEPROM.OK', acknowledgement)
    for directory in ('ASSETS', 'FACTORY', 'PROJECTS', 'PRESETS', 'KITS', 'RECORDINGS', 'GROOVE', 'FIRMWARE', 'LICENSES'):
        source = args.boot_files / directory
        if source.is_dir():
            for file in sorted(source.rglob('*')):
                if file.is_file():
                    put(3, str(file.relative_to(args.boot_files)), file)
    for name in ('START-HERE.md','FIRMWARE-UPDATES.md'):
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
        marker.write_text('Enable USB-C update control. Physical access can stop and reboot this device.\n')
        put(3, 'REMOTE.ON', marker)
    manifest=staging/'manifest.json'
    manifest.write_text(json.dumps({'board':args.board,'firmware_version':args.version,'files':files},indent=2,sort_keys=True)+'\n')
    put(3,'manifest.json',manifest)
# Only a fully provisioned image may opt into first-boot expansion. Save
# immutable geometry separately from both redundant update journal sectors.
with args.output.open('r+b',buffering=0) as disk:
    mbr=disk.read(512)
    data_start=struct.unpack_from('<I',mbr,446+3*16+8)[0]
    disk.seek(data_start*512)
    bpb=disk.read(512)
    intent=bytearray(512)
    intent[:4]=b'GRW1'
    intent[4:36]=hashlib.sha256(mbr).digest()
    intent[36:68]=hashlib.sha256(bpb).digest()
    intent[480:]=hashlib.sha256(intent[:480]).digest()
    disk.seek(40*512)
    disk.write(mbr+bpb+intent)
    os.fsync(disk.fileno())
os.link(args.output,final_image)
workspace.cleanup()
print('Created', final_image, '(recovery + A + B + protected data). Physical boot remains unverified.')
