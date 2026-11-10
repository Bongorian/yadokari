#!/usr/bin/env python3
"""Copy factory assets into a host boot tree without replacing existing data."""
import filecmp
import os
import shutil
import sys
import tempfile
from pathlib import Path

source, target = map(Path, sys.argv[1:3])
target.mkdir(parents=True, exist_ok=True)
for src in sorted(source.rglob('*')):
    relative=src.relative_to(source)
    name=src.name
    if name.endswith(('.bak','.tmp','.part')) or any(token in name for token in ('.bak-','.factory-','.corrupt-','.rejected-','.new')):
        continue
    if src.is_symlink():
        raise SystemExit(f'Factory asset symlink is not copied: {src}')
    if src.is_dir():
        (target/relative).mkdir(parents=True, exist_ok=True)
        continue
    if not src.is_file():
        continue
    # Build catalogs and measurements stay on the host, not on DATA.
    if src.suffix.lower() in {".csv", ".tsv", ".log", ".png", ".bmp", ".html", ".pdf"}:
        continue
    dest=target/relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    if os.path.lexists(dest):
        if dest.is_symlink() or not dest.is_file() or not filecmp.cmp(src,dest,shallow=False):
            raise SystemExit(f'Existing boot asset differs; preserved {dest}. Use a new BOOT_OUTPUT_DIR.')
        continue
    fd,temp=tempfile.mkstemp(prefix='.copy-asset-',dir=dest.parent)
    try:
        with os.fdopen(fd,'wb') as out,src.open('rb') as inp:
            shutil.copyfileobj(inp,out)
            out.flush()
            os.fsync(out.fileno())
        try:
            os.link(temp,dest)
        except FileExistsError:
            if dest.is_symlink() or not dest.is_file() or not filecmp.cmp(src,dest,shallow=False):
                raise SystemExit(f'Concurrent boot asset differs; preserved {dest}.')
    finally:
        os.unlink(temp)
