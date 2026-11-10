#!/usr/bin/env python3
"""Validate patch idempotence against the pinned, unmodified Circle sources."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0,str(root/'tools/testing'))
from circle_source import require_circle
circle = require_circle(root)
names = ('addon/fatfs/ffconf.h', 'addon/fatfs/diskio.cpp', 'include/circle/usb/gadget/usbcdcgadget.h')
with tempfile.TemporaryDirectory() as temp:
    tree = Path(temp)
    for name in names:
        target = tree / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(['git', '-C', str(circle), 'show', 'HEAD:' + name]))
    for script in ('patch-update-fatfs.py', 'patch-update-gadget.py'):
        command = ['python3', str(root / 'scripts' / script), str(tree)]
        subprocess.run(command, check=True)
        first = {name: (tree / name).read_bytes() for name in names}
        subprocess.run(command, check=True)
        assert first == {name: (tree / name).read_bytes() for name in names}
    assert '"emmc1-4"' in (tree / names[1]).read_text()
    assert 'GetSerialDevice' in (tree / names[2]).read_text()
print('Circle update FAT/CDC adapter patches: pinned-source idempotence PASS')
