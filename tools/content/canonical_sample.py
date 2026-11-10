#!/usr/bin/env python3
"""Prepare the owned operational sample at its canonical DATA shelf.

assets/ remains the repository's source/provenance tree. ASSETS/ is generated
development DATA, not a renamed user collection. Do not touch other files.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = root / 'assets/samples/demo-slices.wav'
raw = source.read_bytes()
# FACTORY is the operational copy. ASSETS is a host-only fixture for existing
# authored references; this script never runs against a mounted device DATA.
for target in (root / 'FACTORY/DEBUG/demo-slices.wav', root / 'ASSETS/SAMPLES/demo-slices.wav'):
    if not target.exists() or target.read_bytes() != raw:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
