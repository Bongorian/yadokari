#!/usr/bin/env python3
"""Rebuild boot/recovery/journal into a NEW image, preserving DATA byte-for-byte."""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess


def data_region(path):
    if not Path(path).is_file() or str(Path(path).resolve()).startswith('/dev/'):
        raise ValueError('Only regular image files are accepted')
    with open(path, 'rb') as source:
        mbr = source.read(512)
        source.seek(8 * 512)
        marker = source.read(512)
    if mbr[510:512] != b'\x55\xaa' or marker[:4] != b'GSD1' or marker[4:36] != hashlib.sha256(mbr[446:510]).digest():
        raise ValueError('Unknown layout; refusing to guess where user data resides')
    start, sectors = struct.unpack_from('<II', mbr, 446 + 3 * 16 + 8)
    if sectors < 32768 or (start + sectors) * 512 > Path(path).stat().st_size:
        raise ValueError('Invalid DATA bounds')
    return start * 512, sectors * 512


def copy_data(source, destination):
    if Path(source).resolve() == Path(destination).resolve():
        raise ValueError('Recovery requires a separate output image')
    source_offset, length = data_region(source)
    target_offset, target_length = data_region(destination)
    if length != target_length:
        raise ValueError('DATA sizes differ; no truncation or automatic migration is performed')
    digest = hashlib.sha256()
    with open(source, 'rb') as original, open(destination, 'r+b') as result:
        original.seek(source_offset)
        result.seek(target_offset)
        remaining = length
        while remaining:
            block = original.read(min(1024 * 1024, remaining))
            if not block:
                raise ValueError('Unexpected end of DATA')
            result.write(block)
            digest.update(block)
            remaining -= len(block)
        result.flush()
        os.fsync(result.fileno())
        result.seek(target_offset)
        verification = hashlib.sha256()
        remaining = length
        while remaining:
            block = result.read(min(1024 * 1024, remaining))
            if not block:
                raise ValueError('Recovery readback failed')
            verification.update(block)
            remaining -= len(block)
    if digest.digest() != verification.digest():
        raise ValueError('DATA readback mismatch')
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('damaged_image', type=Path)
    parser.add_argument('new_image', type=Path)
    parser.add_argument('--factory', required=True, type=Path)
    parser.add_argument('--recovery', required=True, type=Path)
    parser.add_argument('--boot-files', required=True, type=Path)
    parser.add_argument('--version', required=True, type=int)
    args = parser.parse_args()
    _, length = data_region(args.damaged_image)  # Validate before creating output
    if length % (1024 * 1024):
        raise ValueError('DATA capacity must be a whole MiB for this provisioner')
    script = Path(__file__).with_name('provision.py')
    subprocess.run(['python3', str(script), str(args.new_image), '--factory', str(args.factory), '--recovery', str(args.recovery), '--boot-files', str(args.boot_files), '--version', str(args.version), '--data-mib', str(length // (1024 * 1024))], check=True)
    print('Protected DATA SHA256:', copy_data(args.damaged_image, args.new_image))
    print('Original image was opened read-only. Boot/journal rebuilt in', args.new_image)


if __name__ == '__main__':
    main()
