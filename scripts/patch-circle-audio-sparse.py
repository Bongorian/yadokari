#!/usr/bin/env python3
"""Search bounded sparse audio subdevice numbers after re-enumeration."""
from pathlib import Path
import sys
path=Path(sys.argv[1])/'lib/sound/usbsoundbasedevice.cpp'
s=path.read_text()
marker='GROOVE_AUDIO_SPARSE_SUBDEVICES'
if marker in s:
 print('Circle sparse audio scan already applied');raise SystemExit(0)
a='''\tfor (unsigned nInterface = 0; TRUE; nInterface++)
\t{
\t\tCString USBDeviceName;
\t\tUSBDeviceName.Format ("uaudio%u-%u", m_nDevice+1, nInterface+1);

\t\tCDevice *pDevice = CDeviceNameService::Get ()->GetDevice (USBDeviceName, FALSE);
\t\tif (!pDevice)
\t\t{
\t\t\tLOGWARN ("USB audio streaming device not found (%cX)", bTX ? 'T' : 'R');

\t\t\tbreak;
\t\t}
'''
b='''\t// GROOVE_AUDIO_SPARSE_SUBDEVICES: subdevice IDs are monotonic and may
\t// have gaps after removal; keep the same bound as the MK3 discovery scan.
\tfor (unsigned nInterface = 0; nInterface < 16; nInterface++)
\t{
\t\tCString USBDeviceName;
\t\tUSBDeviceName.Format ("uaudio%u-%u", m_nDevice+1, nInterface+1);

\t\tCDevice *pDevice = CDeviceNameService::Get ()->GetDevice (USBDeviceName, FALSE);
\t\tif (!pDevice) continue;
'''
if s.count(a)!=1:raise SystemExit('Unexpected pinned Circle streaming lookup')
path.write_text(s.replace(a,b,1))
print('Circle bounded sparse audio subdevice lookup patched; hardware unverified')
