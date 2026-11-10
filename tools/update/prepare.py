#!/usr/bin/env python3
"""Build and sign a new update without overwriting earlier artifacts or keys."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from update import identity, package, public_key, send

ROOT = Path(__file__).resolve().parents[2]


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def configured_identity(path):
    if not path.exists():
        return None
    if path.is_symlink():
        raise ValueError('Refusing a symlinked firmware/update_key.h')
    text = path.read_text(encoding='utf-8')
    k = re.search(r'UpdatePublicKey\[32\]\s*=\s*\{([0-9,\s]+)\}\s*;', text)
    v = re.search(r'^#define\s+GROOVE_FW_VERSION\s+(\d+)\s*$', text, re.M)
    if not k or not v:
        raise ValueError('Unrecognized existing build identity; preserved')
    try:
        key = bytes(int(x.strip()) for x in k.group(1).split(','))
        version = int(v.group(1))
    except (ValueError, OverflowError) as e:
        raise ValueError('Invalid existing build identity; preserved') from e
    if len(key) != 32 or not 0 < version < 2**32:
        raise ValueError('Invalid existing build identity; preserved')
    return version, key


def prepare(args):
    private = args.key_dir / 'private.pem'
    public = args.key_dir / 'public.bin'
    if not private.is_file() or not public.is_file():
        raise ValueError('Existing private.pem and public.bin are required; no key is created')
    key = public.read_bytes()
    if len(key) != 32 or not any(key) or public_key(private) != key:
        raise ValueError('Public and private Ed25519 keys do not match')
    if not 0 < args.version < 2**32:
        raise ValueError('Version must be between 1 and 4294967295')
    if not 64 <= args.data_mib <= 32768:
        raise ValueError('DATA capacity must be 64..32768 MiB')
    if args.remote_control and not args.initial_ab:
        raise ValueError('--remote-control applies only to a new --initial-ab image')
    if len(args.label.encode('ascii')) > 31:
        raise ValueError('Label is limited to 31 ASCII bytes')
    previous = configured_identity(ROOT / 'firmware/update_key.h')
    if previous and any(previous[1]) and previous[1] != key:
        raise ValueError('Configured build uses another public key; no files changed')
    if previous and args.version <= previous[0]:
        raise ValueError('Version must exceed the currently configured build version')
    target = args.output_dir.resolve()
    if str(target).startswith('/dev/') or target == Path('/dev'):
        raise ValueError('Output must be a new directory outside /dev')
    target.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['UPDATE_PUBLIC_KEY'] = str(public.resolve())
    env['FIRMWARE_VERSION'] = str(args.version)
    subprocess.run(['make', 'firmware'], cwd=ROOT, env=env, check=True)
    app = (ROOT / 'firmware/kernel8-rpi4.img').read_bytes()
    recovery = (ROOT / 'firmware/recovery/kernel8-rpi4.img').read_bytes()
    if identity(app) != (args.version, key, 1) or identity(recovery) != (args.version, key, 2):
        raise ValueError('Built image identity mismatch; package not sent')
    payload = package(app, private, args.version, args.label, require_identity=True)
    app_path, recovery_path, package_path = (target / 'application.img', target / 'recovery.img', target / 'update.gup')
    for path, data in ((app_path, app), (recovery_path, recovery), (package_path, payload)):
        with path.open('xb') as out:
            out.write(data)
    artifacts = [app_path, recovery_path, package_path]
    if args.initial_ab:
        env['BOOT_OUTPUT_DIR'] = str(target / 'boot-files')
        subprocess.run([str(ROOT / 'scripts/prepare-sd.sh')], cwd=ROOT, env=env, check=True)
        subprocess.run(['make', 'update-simulator'], cwd=ROOT, env=env, check=True)
        sd_image = target / 'initial-ab.img'
        provision = [sys.executable, str(ROOT / 'tools/update/provision.py'), str(sd_image),
                     '--factory', str(app_path), '--recovery', str(recovery_path),
                     '--boot-files', env['BOOT_OUTPUT_DIR'], '--version', str(args.version),
                     '--data-mib', str(args.data_mib)]
        if args.remote_control:
            provision.append('--remote-control')
        subprocess.run(provision, cwd=ROOT, env=env, check=True)
        artifacts.append(sd_image)
        print('Initial A/B image prepared as a regular file; no physical SD was written.', flush=True)
        print('Its same-version package is not an upgrade; future updates need a higher version.', flush=True)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    manifest = {'firmware_version': args.version, 'git_commit': revision, 'working_tree_dirty': dirty,
                'public_key_sha256': hashlib.sha256(key).hexdigest(),
                'files': {p.name: file_sha256(p) for p in artifacts},
                'physical_verification': False}
    with (target / 'manifest.json').open('x', encoding='utf-8') as out:
        json.dump(manifest, out, indent=2, sort_keys=True)
        out.write('\n')
    print('Prepared signed artifacts at', target, flush=True)
    print('Recorded source', revision, '(dirty)' if dirty else '(clean)', flush=True)
    if args.tcp or args.serial:
        args.package = package_path
        send(args)
    else:
        print('No transfer requested. Target update mode and trial confirmation remain manual.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--key-dir', type=Path, required=True, help='Existing private.pem and public.bin directory')
    parser.add_argument('--version', type=int, required=True)
    parser.add_argument('--output-dir', type=Path, required=True, help='New directory; existing files are never overwritten')
    parser.add_argument('--label', default='')
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--initial-ab', action='store_true', help='Also create a new four-partition SD image (never writes a device)')
    parser.add_argument('--remote-control', action='store_true', help='Enable physically connected UART and USB-C update commands on the new A/B image')
    parser.add_argument('--data-mib', type=int, default=1024, help='DATA size for --initial-ab (64..32768 MiB)')
    transport = parser.add_mutually_exclusive_group()
    transport.add_argument('--tcp', help='Send to an already-enabled update listener, IPv4:port')
    transport.add_argument('--serial', help='Send to an already-enabled USB CDC update device')
    args = parser.parse_args()
    try:
        prepare(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print('Preparation failed:', exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
