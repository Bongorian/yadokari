#!/usr/bin/env python3
"""Exercise preparation without a Pi, a real key, or repository mutations."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/update'))
spec = importlib.util.spec_from_file_location('prepare_update', ROOT / 'tools/update/prepare.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory(prefix='groove-prepare-') as temp:
    root = Path(temp)
    (root / 'firmware/recovery').mkdir(parents=True)
    keys = root / 'keys'
    keys.mkdir()
    key = bytes(range(1, 33))
    (keys / 'public.bin').write_bytes(key)
    (keys / 'private.pem').write_bytes(b'test-only')
    header = root / 'firmware/update_key.h'
    header.write_text('#pragma once\nstatic const unsigned char UpdatePublicKey[32]={' + ','.join(map(str, key)) + '};\n#define GROOVE_FW_VERSION 10\n')
    marker = b'GBX-FW-ID-v1\0\0\0\0'

    def fake_build(command, cwd, env, check):
        assert command == ['make', 'firmware'] and cwd == root and check
        assert env['FIRMWARE_VERSION'] == '11' and Path(env['UPDATE_PUBLIC_KEY']) == (keys / 'public.bin')
        for path, kind in ((root / 'firmware/kernel8-rpi4.img', 1), (root / 'firmware/recovery/kernel8-rpi4.img', 2)):
            path.write_bytes(marker + (11).to_bytes(4, 'little') + key + kind.to_bytes(4, 'little'))

    def fake_git(command, cwd, text):
        return 'abcdef\n' if command[-1] == 'HEAD' else ''

    def fake_package(image, private, version, label, require_identity):
        assert module.identity(image) == (11, key, 1) and private == keys / 'private.pem'
        assert version == 11 and require_identity
        return b'GUP1-test'

    args = SimpleNamespace(key_dir=keys, version=11, output_dir=root / 'out', label='', timeout=1, tcp=None, serial=None, initial_ab=False, data_mib=64, remote_control=False)
    with patch.object(module, 'ROOT', root), patch.object(module, 'public_key', return_value=key), \
         patch.object(module.subprocess, 'run', side_effect=fake_build), \
         patch.object(module.subprocess, 'check_output', side_effect=fake_git), \
         patch.object(module, 'package', side_effect=fake_package), patch.object(module, 'send') as sender:
        module.prepare(args)
        assert not sender.called
        manifest = json.loads((args.output_dir / 'manifest.json').read_text())
        assert manifest['firmware_version'] == 11 and not manifest['working_tree_dirty']
        assert set(manifest['files']) == {'application.img', 'recovery.img', 'update.gup'}
        before = (args.output_dir / 'update.gup').read_bytes()
        try:
            module.prepare(args)
        except FileExistsError:
            pass
        else:
            raise AssertionError('Existing output overwritten')
        assert (args.output_dir / 'update.gup').read_bytes() == before
        args.output_dir = root / 'wrong-key'
        (keys / 'public.bin').write_bytes(bytes(reversed(key)))
        try:
            module.prepare(args)
        except ValueError:
            pass
        else:
            raise AssertionError('Mismatched key accepted')
        assert not args.output_dir.exists()
        (keys / 'public.bin').write_bytes(key)
        args.output_dir = root / 'with-ab'
        args.initial_ab = True
        args.remote_control = True
        args.tcp = '127.0.0.1:16700'
        def all_steps(command, cwd, env, check):
            if command == ['make', 'firmware']:
                return fake_build(command, cwd, env, check)
            assert cwd == root and check
            if command == [str(root / 'scripts/prepare-sd.sh')]:
                Path(env['BOOT_OUTPUT_DIR']).mkdir()
            elif command == ['make', 'update-simulator']:
                pass
            else:
                assert command[1] == str(root / 'tools/update/provision.py')
                assert '--data-mib' in command and command[command.index('--data-mib') + 1] == '64'
                assert '--remote-control' in command
                Path(command[2]).write_bytes(b'virtual image')
        with patch.object(module.subprocess, 'run', side_effect=all_steps):
            module.prepare(args)
        assert sender.call_count == 1 and sender.call_args.args[0].package == args.output_dir / 'update.gup'
        manifest = json.loads((args.output_dir / 'manifest.json').read_text())
        assert 'initial-ab.img' in manifest['files'] and (args.output_dir / 'initial-ab.img').is_file()
print('Update preparation: identity, artifacts, provenance, and overwrite guards passed')
