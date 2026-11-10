#!/usr/bin/env python3
"""Report capture details when the implicit-feedback stream stops."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/sound/usbsoundbasedevice.cpp'
source = path.read_text()
marker = 'GROOVE_IMPLICIT_FAILURE_DIAGNOSTICS'
if marker not in source:
    old = '''\t\tboolean bQueued = m_State == StateRunning && nFrameSize
'''
    new = '''\t\t// GROOVE_IMPLICIT_FAILURE_DIAGNOSTICS: snapshot under the state lock.
\t\tconst unsigned nState = unsigned (m_State);
\t\tconst unsigned nQueued = m_ImplicitCadence.queued ();
\t\tconst unsigned nReady = m_ImplicitCadence.ready ();
\t\tboolean bQueued = m_State == StateRunning && nFrameSize
'''
    warning = '\t\t\tLOGWARN ("Implicit feedback input invalid or queue full");'
    replacement = '''\t\t\tLOGWARN ("Implicit feedback rejected: bytes %u frame %u queued %u ready %u state %u",
\t\t\t\t nBytesTransferred, nFrameSize, nQueued, nReady, nState);'''
    if source.count(old) != 1 or source.count(warning) != 1:
        raise SystemExit('Unexpected implicit feedback diagnostic anchor')
    source = source.replace(old, new, 1).replace(warning, replacement, 1)
    path.write_text(source)
