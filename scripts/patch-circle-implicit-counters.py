#!/usr/bin/env python3
"""Snapshot cadence counters on rejection without logging in the hot path."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/sound/usbsoundbasedevice.cpp'
source = path.read_text()
if 'GROOVE_IMPLICIT_COUNTERS' not in source:
    anchor = '\t\tconst unsigned nDraining = unsigned (m_bImplicitDraining);'
    snapshot = anchor + '''
\t\t// GROOVE_IMPLICIT_COUNTERS: all counters are protected by this lock.
\t\tconst unsigned nCaptures = m_ImplicitCadence.captures ();
\t\tconst unsigned nTakes = m_ImplicitCadence.takes ();
\t\tconst unsigned nCompletions = m_ImplicitCadence.completions ();
\t\tconst unsigned nZeros = m_ImplicitCadence.zero_captures ();'''
    warning = '\t\t\tCallbackFinished ();\n\t\t\treturn;\n\t\t}\n\t}\n\tif (!ReceiveChunk ())'
    replacement = '''\t\t\tLOGWARN ("Implicit counts capture %u take %u complete %u zero %u",
\t\t\t\t nCaptures, nTakes, nCompletions, nZeros);
''' + warning
    if source.count(anchor) != 1 or source.count(warning) != 1:
        raise SystemExit('Unexpected implicit counter anchor')
    path.write_text(source.replace(anchor, snapshot, 1).replace(warning, replacement, 1))
