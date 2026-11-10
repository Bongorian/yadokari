#!/usr/bin/env python3
"""Reject short synchronous HID OUT transfers in the pinned Circle source."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/usb/usbhiddevice.cpp'
source = path.read_text()
marker = 'GROOVE_HID_OUT_EXACT_LENGTH'
if marker in source:
    print('Circle HID exact-length guard already applied')
    raise SystemExit(0)

start = source.index('boolean CUSBHIDDevice::SendToEndpointOut (')
end = source.index('\nboolean CUSBHIDDevice::SendToEndpointOutAsync (', start)
part = source[start:end]
old = '''\tif (GetHost ()->Transfer (m_pEndpointOut, (void *) pBuffer, nBufSize, nTimeoutMs) < 0)
\t{
\t\treturn FALSE;
\t}

\treturn TRUE;'''
new = '''\t// GROOVE_HID_OUT_EXACT_LENGTH: do not acknowledge a partial LED report.
\treturn GetHost ()->Transfer (m_pEndpointOut, (void *) pBuffer, nBufSize, nTimeoutMs)
\t       == (int) nBufSize;'''
if part.count(old) != 1:
    raise SystemExit('Unexpected pinned Circle HID output implementation')
path.write_text(source[:start] + part.replace(old, new) + source[end:])
print('Circle HID synchronous output now rejects short transfers')
