#!/usr/bin/env python3
"""Generate the bounded AKWF bank offline; firmware never Fourier-transforms assets."""
import argparse, hashlib, json, wave
from pathlib import Path
import numpy as np
from wavetable_pack import write_packed
ROOT=Path(__file__).resolve().parents[2]
BANK=ROOT/'third_party/akwf'
HARMONICS=[64,32,16,8,4,2,1]
SIZE=256

def generate():
    manifest=json.loads((BANK/'manifest.json').read_text())
    result=[]
    for entry in manifest['waves']:
        path=BANK/'waves'/Path(entry['source']).name
        assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
        with wave.open(str(path),'rb') as f:
            assert f.getnchannels()==1 and f.getsampwidth()==2
            x=np.frombuffer(f.readframes(f.getnframes()),'<i2').astype(np.float64)/32768
        assert len(x)>=256
        spectrum=np.fft.rfft(x)/len(x);spectrum[0]=0
        # Fundamental phase alignment makes adjacent-table scans less prone to cancellation.
        phase=np.angle(spectrum[1]) if abs(spectrum[1])>1e-8 else 0
        spectrum*=np.exp(-1j*phase*np.arange(len(spectrum)))
        levels=[]
        for harmonics in HARMONICS:
            bins=np.zeros(SIZE//2+1,dtype=np.complex128)
            bins[1:harmonics+1]=spectrum[1:harmonics+1]*SIZE
            levels.append(np.fft.irfft(bins,n=SIZE))
        gain=.94/max(np.max(np.abs(x)) for x in levels)
        result.append(np.rint(np.array(levels)*gain*32767).astype(np.int16))
    data=np.array(result)
    output=ROOT/'src/generated/akwf_tables.inc';output.parent.mkdir(exist_ok=True)
    packed_bytes,crc=write_packed(output,list(map(int,data.reshape(-1))))
    evidence={'waves':len(data),'samples':SIZE,'harmonics':HARMONICS,'pcm_bytes':data.nbytes,'packed_bytes':packed_bytes,'decoded_crc32':f'{crc:08x}','decoded_sha256':hashlib.sha256(data.astype('<i2').tobytes()).hexdigest(),'packing':'Original fourth-difference Rice, 16-sample groups, reset per mip','generated_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'maximum_quantized_out_of_band':0.0}
    # Quantization can reintroduce only bounded sub-LSB spectral residue.
    for levels in data:
        for values,h in zip(levels,HARMONICS):
            bins=np.fft.rfft(values.astype(np.float64)/32767)/SIZE
            tail=float(np.max(np.abs(bins[h+1:])))
            evidence['maximum_quantized_out_of_band']=max(evidence['maximum_quantized_out_of_band'],tail)
            assert tail<1.6e-5
    report=ROOT/'build/wavetable-bank.json';report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence))
if __name__=='__main__':generate()
