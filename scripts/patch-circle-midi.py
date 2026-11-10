#!/usr/bin/env python3
"""Expose the owning USB device and reject short MIDI host writes in Circle."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
header = root / 'include/circle/usb/usbmidi.h'
device = root / 'lib/usb/usbmidi.cpp'
host = root / 'lib/usb/usbmidihost.cpp'
h, d, s = header.read_text(), device.read_text(), host.read_text()
marker = 'GROOVE_MIDI_USB_IDENTITY'
if marker in h:
    if 'GROOVE_MIDI_EXACT_LENGTH' not in s or 'm_pUSBDevice (pUSBDevice)' not in d:
        raise SystemExit('Incomplete Circle MIDI patch; refusing to continue')
    print('Circle MIDI identity and exact-length guards already applied')
    raise SystemExit(0)

def once(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Unexpected pinned Circle MIDI source')
    return text.replace(old, new, 1)

h = once(h, 'class CUSBMIDIDevice : public CDevice',
         'class CUSBDevice;\n\nclass CUSBMIDIDevice : public CDevice')
h = once(h, 'CUSBMIDIDevice (CDevice *pParent = nullptr);',
         'CUSBMIDIDevice (CDevice *pParent = nullptr, const CUSBDevice *pUSBDevice = nullptr);')
h = once(h, '\t~CUSBMIDIDevice (void);',
         '\t~CUSBMIDIDevice (void);\n\n\t// GROOVE_MIDI_USB_IDENTITY: nullptr for non-host MIDI devices.\n\tconst CUSBDevice *GetUSBDevice (void) const { return m_pUSBDevice; }')
h = once(h, '\tTMIDIPacketHandlerEx *m_pPacketHandler;',
         '\tconst CUSBDevice *m_pUSBDevice;\n\tTMIDIPacketHandlerEx *m_pPacketHandler;')
d = once(d, 'CUSBMIDIDevice::CUSBMIDIDevice (CDevice *pParent)\n:\tCDevice (pParent),',
         'CUSBMIDIDevice::CUSBMIDIDevice (CDevice *pParent, const CUSBDevice *pUSBDevice)\n:\tCDevice (pParent),\n\tm_pUSBDevice (pUSBDevice),')
s = once(s, 'm_pInterface = new CUSBMIDIDevice (this);',
         'm_pInterface = new CUSBMIDIDevice (this, GetDevice ());')
s = once(s, '\treturn pThis->GetHost ()->Transfer (pThis->m_pEndpointOut, Buffer, nLength) >= 0;',
         '\t// GROOVE_MIDI_EXACT_LENGTH: retain MIDI messages on a short transfer.\n\treturn pThis->GetHost ()->Transfer (pThis->m_pEndpointOut, Buffer, nLength) == (int) nLength;')
header.write_text(h)
device.write_text(d)
host.write_text(s)
print('Circle MIDI host now exposes USB identity and rejects short writes')
