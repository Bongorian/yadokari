#!/usr/bin/env python3
"""Expose Circle CDC's actual serial interface without guessing device numbers."""
from pathlib import Path
import sys
path = Path(sys.argv[1]) / 'include/circle/usb/gadget/usbcdcgadget.h'
source = path.read_text()
marker = 'GROOVE_CDC_INTERFACE_ACCESSOR'
if marker not in source:
    anchor = '\t~CUSBCDCGadget (void);'
    if source.count(anchor) != 1:
        raise SystemExit('Unexpected Circle CDC header; refusing to overwrite edits')
    source = source.replace(anchor, anchor + '\n\n\t// ' + marker + '\n\tCUSBSerialDevice *GetSerialDevice (void) const { return m_pInterface; }')
    path.write_text(source)
