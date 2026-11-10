#!/usr/bin/env python3
"""Expose stopped HID polling and release a rejected Pi 4 interrupt request."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);header=root/'include/circle/usb/usbhiddevice.h';source=root/'lib/usb/usbhiddevice.cpp'
h=header.read_text();s=source.read_text();marker='GROOVE_HID_POLL_STOPPED'
if marker in h or marker in s:
    if marker not in h or s.count(marker)!=2:raise SystemExit('Incomplete HID poll restart patch')
    print('Circle HID poll stop notification already applied');raise SystemExit(0)
anchor='\tvirtual void ReportTransferError (TUSBError) {}\n'
if h.count(anchor)!=1:raise SystemExit('HID error hook must be installed first')
h=h.replace(anchor,anchor+'\t// GROOVE_HID_POLL_STOPPED: no request remains queued.\n\tvirtual void ReportPollingStopped (void) {}\n',1)
start=s.index('boolean CUSBHIDDevice::StartRequest (');end=s.index('\nvoid CUSBHIDDevice::CompletionRoutine (',start)
part=s[start:end];anchor='\treturn GetHost ()->SubmitAsyncRequest (pURB);'
replacement='''\t// GROOVE_HID_POLL_STOPPED: Pi 4 interrupt IN is one xHCI TRB.
\tboolean bOK = GetHost ()->SubmitAsyncRequest (pURB);
#if RASPPI == 4
\tif (!bOK) delete pURB;
#endif
\treturn bOK;'''
if part.count(anchor)!=1:raise SystemExit('Unexpected HID StartRequest anchor')
s=s[:start]+part.replace(anchor,replacement,1)+s[end:]
start=s.index('void CUSBHIDDevice::CompletionRoutine (');end=s.index('\nvoid CUSBHIDDevice::CompletionStub (',start)
part=s[start:end];anchor='''\t\tCLogger::Get ()->Write (FromUSBHID, LogError, "Cannot restart request");
\t}
}'''
replacement='''\t\tCLogger::Get ()->Write (FromUSBHID, LogError, "Cannot restart request");
\t\tReportPollingStopped ();
\t}
\telse if (!bRestart)
\t{
\t\t// GROOVE_HID_POLL_STOPPED: repeated errors stopped polling.
\t\tReportPollingStopped ();
\t}
}'''
if part.count(anchor)!=1:raise SystemExit('Unexpected HID restart failure anchor')
s=s[:start]+part.replace(anchor,replacement,1)+s[end:]
header.write_text(h);source.write_text(s)
print('Circle Pi 4 HID stopped polling is reported; hardware unverified')
