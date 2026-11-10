#!/usr/bin/env python3
"""Expose running separately from the draining lifetime of a USB sound device."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hpath=root/'include/circle/sound/usbsoundbasedevice.h';path=root/'lib/sound/usbsoundbasedevice.cpp'
h=hpath.read_text();s=path.read_text();marker='GROOVE_AUDIO_RUNNING_STATUS'
if marker in s:
 if marker not in h:raise SystemExit('Incomplete Circle running status patch')
 print('Circle audio running status already applied');raise SystemExit(0)
old='\tboolean IsActive (void) const override;';new=old+'\n\n\t// GROOVE_AUDIO_RUNNING_STATUS: canceled callbacks may still be draining.\n\tboolean IsRunning (void) const;'
if h.count(old)!=1:raise SystemExit('Unexpected Circle IsActive declaration')
h=h.replace(old,new,1)
old='CSoundController *CUSBSoundBaseDevice::GetController (void)';new='''// GROOVE_AUDIO_RUNNING_STATUS
boolean CUSBSoundBaseDevice::IsRunning (void) const
{
\tm_SpinLock.Acquire ();
\tboolean bRunning = m_State == StateRunning;
\tm_SpinLock.Release ();
\treturn bRunning;
}

'''+old
if s.count(old)!=1:raise SystemExit('Unexpected Circle GetController definition')
s=s.replace(old,new,1);hpath.write_text(h);path.write_text(s)
print('Circle audio running status patched (hardware unverified)')
