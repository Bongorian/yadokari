#!/usr/bin/env python3
"""Notify USB audio removal while streaming endpoints still exist."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/device.h';cp=root/'lib/device.cpp';sp=root/'lib/usb/usbaudiostreaming.cpp'
h=hp.read_text();c=cp.read_text();s=sp.read_text();marker='GROOVE_AUDIO_EARLY_REMOVAL'
if marker in h:
 if marker not in c or marker not in s:raise SystemExit('Incomplete early removal patch')
 print('Circle early audio removal patch already applied');raise SystemExit(0)
def once(src,a,b):
 if src.count(a)!=1:raise SystemExit('Unexpected early removal anchor: '+a[:120])
 return src.replace(a,b,1)
h=once(h,'private:\n\tCPtrList m_RemovedHandlerList;','protected:\n\t// GROOVE_AUDIO_EARLY_REMOVAL: drain callbacks before derived resources die.\n\tvoid NotifyRemoved (void);\n\nprivate:\n\tCPtrList m_RemovedHandlerList;')
c=once(c,'CDevice::~CDevice (void)\n{\n\tTPtrListElement *pElement;','CDevice::~CDevice (void)\n{\n\tNotifyRemoved ();\n}\n\n// GROOVE_AUDIO_EARLY_REMOVAL: a later base destructor sees an empty list.\nvoid CDevice::NotifyRemoved (void)\n{\n\tTPtrListElement *pElement;')
a='''\tdelete m_pEndpointSync;
\tm_pEndpointSync = nullptr;

\tdelete m_pEndpointData;'''
b='''\t// GROOVE_AUDIO_EARLY_REMOVAL: consumers cancel and drain before the
\t// endpoint resources are destroyed. CDevice's destructor sees no handlers.
\tNotifyRemoved ();

'''+a
s=once(s,a,b);hp.write_text(h);cp.write_text(c);sp.write_text(s)
print('Circle audio removal notification moved before endpoint destruction; physical behavior unverified')
