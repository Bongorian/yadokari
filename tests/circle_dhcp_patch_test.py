#!/usr/bin/env python3
"""Check the pinned Circle DHCP patch applies once and rejects drift."""
import subprocess
import sys
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
circle = root / '.deps/circle-multicore-4m'
original = subprocess.check_output(['git', '-C', str(circle), 'show', 'HEAD:lib/net/dhcpclient.cpp'])
script = root / 'scripts/patch-circle-dhcp-retry.py'
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp) / 'lib/net/dhcpclient.cpp'
    p.parent.mkdir(parents=True)
    p.write_bytes(original)
    subprocess.run([sys.executable, str(script), tmp], check=True)
    first = p.read_bytes()
    subprocess.run([sys.executable, str(script), tmp], check=True)
    assert p.read_bytes() == first
    assert b'RESTART_DELAY\t\t5' in first and b's_GrooveDHCPDatagrams' in first
    p.write_bytes(original.replace(b'#define RESTART_DELAY', b'#define OTHER_DELAY'))
    assert subprocess.run([sys.executable, str(script), tmp], capture_output=True).returncode != 0
print('Circle DHCP patch idempotence and drift checks passed')
