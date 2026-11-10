#!/usr/bin/env python3
"""Use a descriptor-selected output ordinal during construction and start."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';cp=root/'lib/sound/usbsoundbasedevice.cpp'
h=hp.read_text();s=cp.read_text();marker='GROOVE_AUDIO_TX_SELECTION'
if marker in h:
 if marker not in s:raise SystemExit('Incomplete output selection patch')
 print('Circle output selection already applied');raise SystemExit(0)
def once(src,a,b):
 if src.count(a)!=1:raise SystemExit('Unexpected output selection anchor: '+a[:120])
 return src.replace(a,b,1)
h=once(h,'unsigned nRXInterface  = 0);','unsigned nRXInterface  = 0,\n\t\t\t     unsigned nTXInterface  = 0); // GROOVE_AUDIO_TX_SELECTION')
s=once(s,'unsigned nDevice, unsigned nRXInterface)\n:', 'unsigned nDevice, unsigned nRXInterface, unsigned nTXInterface)\n:')
s=once(s,'\tm_nTXInterface (0),','\tm_nTXInterface (nTXInterface), // GROOVE_AUDIO_TX_SELECTION')
hp.write_text(h);cp.write_text(s)
print('Circle descriptor-selected output ordinal patched; physical operation unverified')
