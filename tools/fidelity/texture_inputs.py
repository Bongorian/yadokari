#!/usr/bin/env python3
"""Original deterministic stereo drum/pad WAV stimuli (no external sound assets)."""
from pathlib import Path
import wave, hashlib, json
import numpy as np
root=Path('build/fidelity');root.mkdir(parents=True,exist_ok=True)
rate=32000;n=rate*4;t=np.arange(n)/rate;rng=np.random.default_rng(20261103)
phase=t%0.5
kick=np.sin(2*np.pi*(48*phase+22*(1-np.exp(-phase*35))))*np.exp(-phase*18)
snare=rng.uniform(-1,1,n)*np.exp(-((t-.25)%0.5)*35)
hat=rng.uniform(-1,1,n)*np.exp(-(t%.125)*120)
drum=np.column_stack((.5*kick+.22*snare+.1*hat,.5*kick+.18*snare-.1*hat))
env=np.minimum(t/0.3,1)*np.minimum((4-t)/0.3,1)
pad=np.column_stack([sum(np.sin(2*np.pi*f*t+p)*.12 for f,p in zip([130.8128,164.8138,195.9977],[channel*.3,.2,.4]))*env for channel in (0,1)])
report={}
for name,data in [('drum',drum),('pad',pad)]:
 path=root/f'texture-input-{name}.wav'
 with wave.open(str(path),'wb') as w:
  w.setparams((2,2,rate,0,'NONE','not compressed'));w.writeframes(np.rint(data*32767).astype('<i2').tobytes())
 report[name]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'frames':n,'rate':rate,'channels':2,'pcm_bits':16}
(root/'texture-inputs.json').write_text(json.dumps(report,indent=2)+'\n')
