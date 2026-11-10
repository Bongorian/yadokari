#!/usr/bin/env python3
"""Reject non-PCM USB Audio formats before treating samples as signed PCM."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'lib/usb/usbaudiostreaming.cpp';s=p.read_text();marker='GROOVE_AUDIO_PCM_FORMAT'
if marker in s:print('Circle USB PCM format guard already applied');raise SystemExit(0)
anchor='\t// GROOVE_AUDIO_SUBSLOT: 24 valid PCM bits may use three or four bytes.'
insert='''\t// GROOVE_AUDIO_PCM_FORMAT: format type I is not necessarily signed PCM.
\t// UAC1 PCM has format tag 1; UAC2 PCM is bit zero in bmFormats.
\tif (GetInterfaceProtocol () == USB_PROTO_AUDIO_VER_200)
\t{
\t\tconst u8 *pFormats = reinterpret_cast<const u8 *> (&pAudioInterfaceDesc->Ver200.bmFormats);
\t\tif (!(pFormats[0] & 1)) return FALSE;
\t}
\telse
\t{
\t\tconst u8 *pTag = reinterpret_cast<const u8 *> (&pAudioInterfaceDesc->Ver100.wFormatTag);
\t\tif (pTag[0] != 1 || pTag[1] != 0) return FALSE;
\t}

'''
if s.count(anchor)!=1:raise SystemExit('Unexpected Circle PCM guard anchor')
p.write_text(s.replace(anchor,insert+anchor,1));print('Circle UAC1/UAC2 signed PCM format guard patched')
