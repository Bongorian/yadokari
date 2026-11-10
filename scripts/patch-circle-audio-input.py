#!/usr/bin/env python3
"""Allow UAC2 implicit-feedback data IN as a capture endpoint in pinned Circle.

This only exposes capture; it does not select a playback feedback pairing.
"""
from pathlib import Path
import sys
p = Path(sys.argv[1]) / 'lib/usb/usbaudiostreaming.cpp'
s = p.read_text()
marker = 'GROOVE_AUDIO_IMPLICIT_CAPTURE'
if marker in s:
    print('Circle implicit capture descriptor support already applied')
    raise SystemExit(0)
old = '''\tif (   !pEndpointDesc
\t    || (pEndpointDesc->bmAttributes & 0x33) != 0x01) // Isochronous, Data'''
new = '''\t// GROOVE_AUDIO_IMPLICIT_CAPTURE: UAC2 input can carry both audio and
\t// implicit-feedback data. This does not associate it with any output.
\tif (   !pEndpointDesc
\t    || (   (pEndpointDesc->bmAttributes & 0x33) != 0x01
\t        && !(   m_bVer200
\t             && (pEndpointDesc->bEndpointAddress & 0x80)
\t             && (pEndpointDesc->bmAttributes & 0x33) == 0x21)))'''
if s.count(old) != 1:
    raise SystemExit('Unexpected pinned Circle capture descriptor check')
p.write_text(s.replace(old, new, 1))
print('Circle UAC2 implicit-feedback capture endpoint recognized (physical transfers unverified)')
