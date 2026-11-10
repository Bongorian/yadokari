#!/usr/bin/env python3
"""Avoid reading beyond the original 17-entry crossfade tables at full wet.
The endpoint is the existing table[16], not an epsilon-adjusted parameter.
"""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
s=a.source.read_bytes();manifest=json.loads(Path(__file__).resolve().parents[2].joinpath('third_party/mutable/resonator-texture-sources.json').read_text())
assert hashlib.sha256(s).hexdigest()==manifest['files']['clouds/dsp/granular_processor.cc']
for table in ('lut_xfade_in','lut_xfade_out'):
 old=f'Interpolate({table}, dry_wet, 16.0f)'.encode();assert s.count(old)==1
 s=s.replace(old,f'(dry_wet >= 1.0f ? {table}[16] : Interpolate({table}, dry_wet, 16.0f))'.encode())
a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_bytes()!=s:a.output.write_bytes(s)
