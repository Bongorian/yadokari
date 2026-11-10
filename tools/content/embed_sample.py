#!/usr/bin/env python3
"""Embed the pinned benchmark sample; no DSP/download dependency at boot."""
import hashlib
import sys
from pathlib import Path
source, target = map(Path, sys.argv[1:])
data = source.read_bytes()
assert len(data) == 96044
text = '// Generated from ' + source.name + '; SHA256 ' + hashlib.sha256(data).hexdigest() + '\n'
text += 'static const unsigned char kEmbeddedDemoSample[]={\n'
text += '\n'.join(','.join(str(x) for x in data[i:i+32])+',' for i in range(0,len(data),32))
text += '\n};\n'
target.parent.mkdir(parents=True,exist_ok=True)
if not target.exists() or target.read_text() != text:
    target.write_text(text)
