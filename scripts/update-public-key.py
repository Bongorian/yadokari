#!/usr/bin/env python3
"""Generate the public update identity, preserving configured build values."""
from pathlib import Path
import os
import hashlib
import re
import tempfile

root = Path(__file__).resolve().parents[1]
target = root / 'firmware/update_key.h'
if target.is_symlink():
    raise SystemExit('Refusing to replace a symlink at firmware/update_key.h')
previous_key = None
previous_version = None
if target.exists():
    try:
        old = target.read_text()
    except UnicodeError:
        raise SystemExit('Existing firmware/update_key.h is not UTF-8; preserved without changes')
    key_match = re.search(r'UpdatePublicKey\[32\]\s*=\s*\{([0-9,\s]+)\}\s*;', old)
    version_match = re.search(r'^#define\s+GROOVE_FW_VERSION\s+(\d+)\s*$', old, re.M)
    if not key_match or not version_match:
        raise SystemExit('Existing firmware/update_key.h is unrecognized; preserved without changes')
    try:
        values = [int(value.strip()) for value in key_match.group(1).split(',')]
        previous_key = bytes(values)
        previous_version = int(version_match.group(1))
    except (ValueError, OverflowError):
        raise SystemExit('Existing firmware/update_key.h has invalid values; preserved without changes')
    if len(previous_key) != 32 or not 0 < previous_version < 2**32:
        raise SystemExit('Existing firmware/update_key.h has invalid identity; preserved without changes')

source = os.environ.get('UPDATE_PUBLIC_KEY')
if source is not None:
    if not source:
        raise SystemExit('UPDATE_PUBLIC_KEY must name a raw 32-byte Ed25519 public key')
    key = Path(source).read_bytes()
    if len(key) != 32 or not any(key):
        raise SystemExit('UPDATE_PUBLIC_KEY must be a nonzero raw 32-byte Ed25519 public key')
else:
    key = previous_key if previous_key is not None else bytes(32)
try:
    version = int(os.environ['FIRMWARE_VERSION']) if 'FIRMWARE_VERSION' in os.environ else (previous_version if previous_version is not None else 1)
except ValueError:
    raise SystemExit('FIRMWARE_VERSION must be 1..4294967295')
if not 0 < version < 2**32:
    raise SystemExit('FIRMWARE_VERSION must be 1..4294967295')
text = '#pragma once\nstatic const unsigned char UpdatePublicKey[32]={' + ','.join(str(b) for b in key) + '};\n'
text += '#define GROOVE_FW_VERSION ' + str(version) + '\n'
text += '#define GROOVE_UPDATE_WIFI_BUILD ' + ('1' if os.environ.get('ENABLE_UPDATE_WIFI', '1') == '1' else '0') + '\n'
identity = b'GBX-FW-ID-v1\0\0\0\0' + version.to_bytes(4, 'little') + key
text += '#ifdef GROOVE_RECOVERY\n'
text += 'extern "C" const unsigned char GrooveUpdateIdentity[56] __attribute__((used))={' + ','.join(str(b) for b in identity + (2).to_bytes(4, 'little')) + '};\n'
text += '#else\n'
text += 'extern "C" const unsigned char GrooveUpdateIdentity[56] __attribute__((used))={' + ','.join(str(b) for b in identity + (1).to_bytes(4, 'little')) + '};\n#endif\n'
if not target.exists() or target.read_text() != text:
    fd, temp = tempfile.mkstemp(prefix='.update-key-', dir=target.parent)
    try:
        with os.fdopen(fd, 'w') as output:
            output.write(text)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp, target)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)
print('Updater trust key:', source if source else ('preserved configured public key' if any(key) else 'NOT CONFIGURED (signed installs disabled)'))
print('Public key SHA-256:', hashlib.sha256(key).hexdigest())
print(f'Firmware version: {version}; specify FIRMWARE_VERSION explicitly for a newer update')
