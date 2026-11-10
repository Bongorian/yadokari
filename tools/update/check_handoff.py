#!/usr/bin/env python3
"""Check the v178 handoff artifacts and optional separately transferred signing key."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys

from update import public_key

ROOT = Path(__file__).resolve().parents[2]
IMAGE = ROOT / 'releases/pi4-v178/maschine-mk3-pi4-v178-usbc-sd.zip'
PUBLIC = ROOT / 'releases/pi4-v178/update-public.bin'
IMAGE_HASH = '3174e41de29ab516f3245a7a7624bd847fbfed6f464309934f96555117800148'
PUBLIC_HASH = 'fc84a17a570e291f99626d0aef653991a1e305ae44f686c8b4845592da97df50'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--key-dir', type=Path, help='Existing directory with private.pem and public.bin; never printed')
    args = parser.parse_args()
    try:
        for path, expected in ((IMAGE, IMAGE_HASH), (PUBLIC, PUBLIC_HASH)):
            if sha(path) != expected:
                raise ValueError(f'Artifact checksum mismatch: {path}')
            print(f'OK: {path.relative_to(ROOT)}')
        if args.key_dir:
            key = (args.key_dir / 'public.bin').read_bytes()
            if hashlib.sha256(key).hexdigest() != PUBLIC_HASH or public_key(args.key_dir / 'private.pem') != key:
                raise ValueError('Signing key does not match v178; preserve the existing key')
            print('OK: private signing key matches v178 public key')
        else:
            print('Signing key not checked; it must be moved separately for future updates.')
        for binary in ('make', 'g++', 'python3', 'openssl', 'aarch64-none-elf-g++'):
            print(f'{binary}: {shutil.which(binary) or "not on PATH"}')
        print('No hardware, boot or update was tested by this check.')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f'Handoff check failed: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
