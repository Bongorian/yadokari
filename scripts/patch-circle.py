#!/usr/bin/env python3
"""Install an optional weak interface hook into the pinned Circle checkout."""
import pathlib, subprocess, sys
root=pathlib.Path(sys.argv[1]); p=root/'lib/usb/usbdevice.cpp'; text=p.read_text()
marker='// GROOVE_OPTIONAL_INTERFACE_HOOK'
if marker in text: sys.exit(0)
if subprocess.run(['git','-C',str(root),'diff','--quiet','--','lib/usb/usbdevice.cpp']).returncode:
    raise SystemExit('Circle usbdevice.cpp has local changes; refusing to overwrite')
anchor='\t\tCUSBFunction *pChild = 0;\n'
if text.count(anchor)!=1: raise SystemExit('Circle source differs from expected hook location')
text=text.replace(anchor,anchor+'''\n\t\t// GROOVE_OPTIONAL_INTERFACE_HOOK: application may bind a composite interface.\n\t\tif (GrooveCreateUSBFunction)\n\t\t{\n\t\t\tpChild = GrooveCreateUSBFunction (m_pFunction[nFunction]);\n\t\t}\n''')
text=text.replace('\t\tif (nFunction == 0)\n','\t\tif (nFunction == 0 && pChild == 0)\n',1)
include='#include <circle/usb/usbdevice.h>'
if text.count(include)!=1: raise SystemExit('Circle source include differs from expected')
text=text.replace(include,include+'\n\nextern "C" CUSBFunction *GrooveCreateUSBFunction (CUSBFunction *) __attribute__((weak));')
p.write_text(text)
