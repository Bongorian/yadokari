#!/usr/bin/env python3
"""Define fixed-point shifts and return the original table endpoint safely."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
s=a.source.read_bytes();manifest=json.loads(Path(__file__).resolve().parents[2].joinpath('third_party/mutable/resonator-texture-sources.json').read_text())
assert hashlib.sha256(s).hexdigest()==manifest['files']['clouds/dsp/looping_sample_player.h']
for name,count in [('delay_int',2),('position',4)]:
 old=(name+' << 4').encode();assert s.count(old)==count;s=s.replace(old,('static_cast<uint32_t>('+name+') << 4').encode())
a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_bytes()!=s:a.output.write_bytes(s)

# The upstream interpolator unconditionally reads index + 1, even at frac=0.
# Preserve the exact final table value at the two inclusive parameter endpoints.
for filename, expression, endpoint, condition in [
    ('grain.h', 'stmlib::Interpolate(lut_window, gain, 4096.0f)', 'lut_window[4096]', 'gain >= 1.0f'),
    ('granular_sample_player.h', 'Interpolate(lut_grain_size, parameters.size, 256.0f)', 'lut_grain_size[256]', 'parameters.size >= 1.0f'),
]:
    raw=a.source.with_name(filename).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==manifest['files']['clouds/dsp/'+filename]
    old=expression.encode();assert raw.count(old)==1
    raw=raw.replace(old,('('+condition+' ? '+endpoint+' : '+expression+')').encode())
    output=a.output.with_name(filename)
    if not output.exists() or output.read_bytes()!=raw:output.write_bytes(raw)
# This file is the dependency stamp for all generated headers.
a.output.touch()
