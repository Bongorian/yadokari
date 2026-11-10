#!/usr/bin/env python3
"""Install a factory session only if no session already exists.

Create and flush a private staging file, then hard-link it into place.  The
link is atomic and fails if another process created the destination first.
This tool operates on the host build tree, not a mounted FAT volume.
"""
import os
import shutil
import sys
import tempfile
from pathlib import Path

source, destination = map(Path, sys.argv[1:3])
destination.parent.mkdir(parents=True, exist_ok=True)
if os.path.lexists(destination):
    print(f'Preserved existing session: {destination}')
    sys.exit(0)
fd, temp = tempfile.mkstemp(prefix='.seed-session-', dir=destination.parent)
try:
    with os.fdopen(fd, 'wb') as out, source.open('rb') as inp:
        shutil.copyfileobj(inp, out)
        out.flush()
        os.fsync(out.fileno())
    try:
        os.link(temp, destination)
    except FileExistsError:
        print(f'Preserved concurrently created session: {destination}')
    else:
        print(f'Installed initial factory session: {destination}')
finally:
    os.unlink(temp)
