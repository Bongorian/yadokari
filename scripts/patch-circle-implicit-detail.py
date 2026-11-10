#!/usr/bin/env python3
"""Capture outstanding transfers and drain state when feedback queues fill."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/sound/usbsoundbasedevice.cpp'
source = path.read_text()
marker = 'GROOVE_IMPLICIT_FAILURE_DETAIL'
if marker not in source:
    old = '\t\tconst unsigned nReady = m_ImplicitCadence.ready ();'
    new = old + '''
\t\t// GROOVE_IMPLICIT_FAILURE_DETAIL: snapshot while holding the lock.
\t\tconst unsigned nOutstanding = unsigned (m_nOutstanding);
\t\tconst unsigned nCallbacks = m_nCallbacksActive;
\t\tconst unsigned nDraining = unsigned (m_bImplicitDraining);'''
    warning = '''\t\t\tLOGWARN ("Implicit feedback rejected: bytes %u frame %u queued %u ready %u state %u",
\t\t\t\t nBytesTransferred, nFrameSize, nQueued, nReady, nState);'''
    replacement = '''\t\t\tLOGWARN ("Implicit rejected bytes %u frame %u queued %u ready %u state %u outstanding %u callbacks %u draining %u",
\t\t\t\t nBytesTransferred, nFrameSize, nQueued, nReady, nState, nOutstanding, nCallbacks, nDraining);'''
    if source.count(old) != 1 or source.count(warning) != 1:
        raise SystemExit('Unexpected implicit feedback detail anchor')
    path.write_text(source.replace(old, new, 1).replace(warning, replacement, 1))
