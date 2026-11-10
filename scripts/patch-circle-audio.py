#!/usr/bin/env python3
"""Pinned Circle audio startup: serialize priming and safely drain failed starts."""
import sys
from pathlib import Path
p=Path(sys.argv[1])/'lib/sound/usbsoundbasedevice.cpp'
s=p.read_text()
marker='GROOVE_AUDIO_PRIMING_GUARD'
if marker in s:
    print('Circle audio priming guard already applied');raise SystemExit(0)
start=s.index('boolean CUSBSoundBaseDevice::Start (void)')
end=s.index('\nvoid CUSBSoundBaseDevice::Cancel',start)
part=s[start:end]
prime='\tm_State = StateRunning;\n\n\tm_nOutstanding = 0;'
if part.count(prime)!=1:raise SystemExit('Unexpected pinned audio priming block')
part=part.replace(prime,'\t// '+marker+'\n\t// Protect both initial GetChunk calls from IRQ re-entry into the DSP.\n\tm_SpinLock.Acquire ();\n'+prime)
fail_tx='\t\t\tm_State = StateIdle;\n\n\t\t\treturn FALSE;'
fail_rx='''\t\t\tCancel ();

\t\t\twhile (IsActive ())
\t\t\t{
#ifdef NO_BUSY_WAIT
\t\t\t\tCScheduler::Get ()->Yield ();
#endif
\t\t\t}

\t\t\treturn FALSE;'''
# A timed-out cancelled object must stay alive until its outstanding requests
# complete; the application adapter quarantines it rather than deleting it.
drain='''\t\t\tm_State = m_nOutstanding ? StateCanceled : StateIdle;
\t\t\tm_SpinLock.Release ();
\t\t\tunsigned nDrainStart = CTimer::GetClockTicks ();
\t\t\twhile (IsActive () && CTimer::GetClockTicks () - nDrainStart < 2000000)
\t\t\t{
\t\t\t\tCTimer::Get ()->MsDelay (1);
\t\t\t}
\t\t\treturn FALSE;'''
if part.count(fail_tx)!=1 or part.count(fail_rx)!=1:raise SystemExit('Unexpected pinned audio failure blocks')
part=part.replace(fail_tx,drain).replace(fail_rx,drain)
if part.count('\n\treturn TRUE;')!=1:raise SystemExit('Unexpected audio return')
part=part.replace('\n\treturn TRUE;','\n\tm_SpinLock.Release ();\n\treturn TRUE;')
p.write_text(s[:start]+part+s[end:])
print('Circle audio priming and failed-start drain patched (physical behavior unverified)')
