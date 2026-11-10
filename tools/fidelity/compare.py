#!/usr/bin/env python3
"""Compare independent native renderers; retain float error and listening WAVs."""
import argparse
import hashlib
import json
from pathlib import Path
import wave
import numpy as np

p=argparse.ArgumentParser()
p.add_argument('name');p.add_argument('--rate',type=int,required=True)
p.add_argument('--frames-per-case',type=int,required=True)
p.add_argument('--raw-reference',type=Path)
p.add_argument('--cpu-reference',type=Path)
p.add_argument('--directory',type=Path,default=Path('build/fidelity'))
a=p.parse_args();reports=[]
paths=[a.directory/f'{a.name}-{kind}.f32' for kind in ('reference','adapter')]
ref,actual=[np.fromfile(path,dtype='<f4') for path in paths]
assert ref.size==actual.size and ref.size%(2*a.frames_per_case)==0
assert np.isfinite(ref).all() and np.isfinite(actual).all()
for i in range(ref.size//(2*a.frames_per_case)):
    start=i*2*a.frames_per_case;stop=start+2*a.frames_per_case
    x=ref[start:stop].astype(np.float64);y=actual[start:stop].astype(np.float64)
    peak=float(np.max(np.abs(x)));error=float(np.max(np.abs(x-y)))
    # Correlation is undefined for constant signals; do not report invented 1.0.
    corr=float(np.corrcoef(x,y)[0,1]) if np.std(x)>0 and np.std(y)>0 else None
    reports.append({'case':i,'peak':peak,'max_absolute_error':error,'correlation':corr})
result={'scope':'Native DSP boundary; same host/compiler and stimulus, pristine reference vs adapter. Not physical listening or complete engine routing.',
        'sample_rate':a.rate,'frames_per_case':a.frames_per_case,'cases':reports,
        'max_absolute_error':max(r['max_absolute_error'] for r in reports),
        'minimum_correlation':min(r['correlation'] for r in reports if r['correlation'] is not None),
        'byte_equal':paths[0].read_bytes()==paths[1].read_bytes(),
        'sha256':{path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
        'acceptance':'Every float sample exactly equal; no error tolerance.',
        'passed':bool(np.array_equal(ref,actual) and all(r['peak']>0 for r in reports))}
if a.raw_reference:
    raw=np.fromfile(a.raw_reference,dtype='<f4');assert raw.size==actual.size and np.isfinite(raw).all()
    result['unmodified_upstream_host']={'known_undefined_behavior':True,'sha256':hashlib.sha256(a.raw_reference.read_bytes()).hexdigest(),'byte_equal':a.raw_reference.read_bytes()==paths[1].read_bytes(),'max_absolute_error':float(np.max(np.abs(raw.astype(np.float64)-actual.astype(np.float64)))),'correlation':float(np.corrcoef(raw,actual)[0,1])}
if a.cpu_reference:
    cpu=np.fromfile(a.cpu_reference,dtype='<f4');assert cpu.size==actual.size and np.isfinite(cpu).all()
    result['cortex_shift_only_reference']={'sha256':hashlib.sha256(a.cpu_reference.read_bytes()).hexdigest(),'byte_equal':a.cpu_reference.read_bytes()==paths[1].read_bytes(),'max_absolute_error':float(np.max(np.abs(cpu.astype(np.float64)-actual.astype(np.float64))))}
    different=np.flatnonzero(cpu!=actual)
    result['cortex_shift_only_reference']['different_samples']=int(different.size)
    result['cortex_shift_only_reference']['differences']=[{'case':int(i//(2*a.frames_per_case)),'frame':int(i//2%a.frames_per_case),'channel':int(i%2),'reference':float(cpu[i]),'adapter':float(actual[i])} for i in different[:16]]
# A shared attenuation makes the listening files comparable and avoids clipping.
gain=0.9/max(1.0,float(np.max(np.abs(ref))),float(np.max(np.abs(actual))))
result['listening_gain']=gain
for path,data in zip(paths,(ref,actual)):
    with wave.open(str(path.with_suffix('.wav')),'wb') as out:
        out.setparams((2,2,a.rate,0,'NONE','not compressed'))
        out.writeframes(np.rint(data*gain*32767).astype('<i2').tobytes())
(a.directory/f'{a.name}-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='cases'},indent=2))
raise SystemExit(0 if result['passed'] else 1)
