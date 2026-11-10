#!/usr/bin/env python3
"""Expose an optional task-level EMMC wait pulse (no IRQ/DSP callback)."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'addon/SDCard/emmc.cpp'
s=p.read_text();marker='GROOVE_STORAGE_UI_WAIT'
if marker in s:raise SystemExit(0)
a='int CEMMCDevice::TimeoutWait (unsigned long reg, unsigned mask, int value, unsigned usec)'
assert s.count(a)==1
s=s.replace(a,'// '+marker+'; absent in immutable recovery.\nextern "C" void GrooveStorageUiWait () __attribute__((weak));\n\n'+a)
a='\twhile ((read32 (reg) & mask) ? !value : value)\n\t{'
assert s.count(a)==1
s=s.replace(a,a+'\n\t\tif (GrooveStorageUiWait) GrooveStorageUiWait ();')
p.write_text(s)
print('Optional recording UI pulse installed in task-level EMMC wait')
