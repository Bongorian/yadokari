#!/usr/bin/env python3
"""Apply the user-authorized damping endpoint adapter to an unchanged vendor TU."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
name='rings/dsp/resonator.cc';raw=(a.root/name).read_bytes()
manifest=json.loads((a.root/'resonator-texture-sources.json').read_text())
assert hashlib.sha256(raw).hexdigest()==manifest['files'][name]
s=raw.decode();old='''Interpolate(
      lut_4_decades,
      damping_,
      256.0f)''';assert s.count(old)==1
s='#include "resonator_boundary.h"\n'+s.replace(old,'groove::resonator_damping_lookup(damping_)')
a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_text()!=s:a.output.write_text(s)
