"""Rebuild the immutable tables using the production converter on original single cycles."""
import argparse
import tempfile
from pathlib import Path
import math,struct,wave,sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('converter_tools',type=Path,help='directory containing wavetable.py')
args=parser.parse_args()
scratch=tempfile.TemporaryDirectory()
ev=Path(scratch.name)
sys.path.insert(0,str(args.converter_tools.resolve()))
from wavetable import convert
out=Path(__file__).resolve().parents[2]/'src/generated'
for index,name in enumerate(('reed','prism','probe')):
    frames=3 if index<2 else 2
    values=[]
    for f in range(frames):
        t=f/(frames-1)
        for n in range(256):
            p=2*math.pi*n/256
            if index==0:
                x=sum((.7/h)*(1 if h==1 else t**(h/9)) *math.sin(h*p) for h in range(1,16,2))
            elif index==1:
                x=.45*math.sin(p)+sum(.13*math.exp(-.45*abs(h-2-7*t))*math.cos(h*p+.13*h) for h in range(2,13))
            else:
                x=.55*math.sin(p)+.19*t*math.cos(3*p)+.11*(1-t)*math.sin(5*p)
            values.append(round((x*(1-.28*t)+.18*t*math.cos(p))*30000))
    src=ev/(name+'-source.wav')
    with wave.open(str(src),'wb') as w:
        w.setparams((1,2,48000,0,'NONE',''));w.writeframes(struct.pack('<'+'h'*len(values),*values))
    asset=ev/(name+'.wav');convert(src,asset,256)
    with wave.open(str(asset),'rb') as w:pcm=w.readframes(w.getnframes())
    words=struct.unpack('<'+'h'*(len(pcm)//2),pcm)
    (out/('wavetable_'+name+'.inc')).write_text('\n'.join(','.join(map(str,words[i:i+16]))+',' for i in range(0,len(words),16))+'\n')
