#!/usr/bin/env python3
"""Notify HID subclasses when an interrupt-IN request fails."""
from pathlib import Path
import sys
root=Path(sys.argv[1]); header=root/'include/circle/usb/usbhiddevice.h'; source=root/'lib/usb/usbhiddevice.cpp'
h=header.read_text();s=source.read_text();marker='GROOVE_HID_INPUT_ERROR'
if marker in h or marker in s:
    if marker not in h or marker not in s: raise SystemExit('Incomplete HID input error patch')
    print('Circle HID input error notification already applied');raise SystemExit(0)
header_anchor='\t// cannot be called from ReportHandler() context\n'
header_replacement='\t// GROOVE_HID_INPUT_ERROR: subclass may release state after lost input.\n\tvirtual void ReportTransferError (TUSBError) {}\n\n'+header_anchor
if h.count(header_anchor)!=1:raise SystemExit('Unexpected HID header anchor')
start=s.index('void CUSBHIDDevice::CompletionRoutine (')
end=s.index('\nvoid CUSBHIDDevice::CompletionStub (',start)
section=s[start:end]
anchor='\telse\n\t{\n\t\tif (!GetHost ()->IsPlugAndPlay ())'
replacement='\telse\n\t{\n\t\t// GROOVE_HID_INPUT_ERROR: report every failed interrupt request.\n\t\tReportTransferError (pURB->GetUSBError ());\n\t\tif (!GetHost ()->IsPlugAndPlay ())'
if section.count(anchor)!=1:raise SystemExit('Unexpected HID completion anchor')
header.write_text(h.replace(header_anchor,header_replacement,1))
source.write_text(s[:start]+section.replace(anchor,replacement,1)+s[end:])
print('Circle HID input errors reported to subclass; physical behavior unverified')
