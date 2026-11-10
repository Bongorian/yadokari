#!/usr/bin/env python3
"""Add explicit SD data/A/B mappings; preserve existing volume names and drivers."""
from pathlib import Path
import sys
root = Path(sys.argv[1])
conf = root / 'addon/fatfs/ffconf.h'
disk = root / 'addon/fatfs/diskio.cpp'
text = conf.read_text()
if '"DATA","SLOTA","SLOTB"' not in text:
    old = '#define FF_VOLUMES\t\t7'
    names = '"SD","SD2","USB","USB2","USB3","FD","NVME"'
    if old not in text or names not in text:
        raise SystemExit('Unexpected FatFs configuration; refusing to overwrite local edits')
    text = text.replace(old, '#define FF_VOLUMES\t\t10').replace(names, names + ',"DATA","SLOTA","SLOTB"')
    conf.write_text(text)
text = disk.read_text()
if '"emmc1-4"' not in text:
    old = '"nvme1"\n'
    if old not in text:
        raise SystemExit('Unexpected disk mapping; refusing to overwrite local edits')
    disk.write_text(text.replace(old, '"nvme1",\n\t"emmc1-4",\n\t"emmc1-2",\n\t"emmc1-3"\n'))

# Reserve recording extents before the DSP starts producing PCM.
text=conf.read_text()
if '#define FF_USE_EXPAND\t0' in text:
    conf.write_text(text.replace('#define FF_USE_EXPAND\t0','#define FF_USE_EXPAND\t1'))
