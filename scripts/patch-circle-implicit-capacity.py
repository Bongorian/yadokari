#!/usr/bin/env python3
"""Size implicit-feedback TX buffers for the selected endpoint capacity.

Applied after the other pinned Circle audio patches. The descriptor's maximum
does not claim that the device will actually emit that many frames at 48 kHz.
"""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/sound/usbsoundbasedevice.cpp'
source = path.read_text()
marker = 'GROOVE_IMPLICIT_TX_CAPACITY'
if marker in source:
    print('Circle implicit TX capacity already applied')
    raise SystemExit(0)
anchor = '\t\t\tm_nTXChunkSizeBytes = m_pTXUSBDevice->GetChunkSizeBytes ();\n'
replacement = anchor + '''
\t\t\t// GROOVE_IMPLICIT_TX_CAPACITY: actual capture packet lengths can
\t\t\t// exceed twice the nominal output interval. The submit path still
\t\t\t// validates the exact packet against the descriptor and frame size.
\t\t\tif (m_bImplicitFeedback)
\t\t\t{
\t\t\t\tconst unsigned nPacketMax = m_pTXUSBDevice->GetDataMaxPacketSize ();
\t\t\t\tconst unsigned nHalfMax = nPacketMax / 2 + nPacketMax % 2;
\t\t\t\tif (nHalfMax > m_nTXChunkSizeBytes) m_nTXChunkSizeBytes = nHalfMax;
\t\t\t}
'''
if source.count(anchor) != 1:
    raise SystemExit('Unexpected Circle TX buffer capacity anchor')
path.write_text(source.replace(anchor, replacement, 1))
print('Circle implicit TX capacity follows selected endpoint (physical streaming unverified)')
