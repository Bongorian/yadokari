#!/usr/bin/env python3
"""Replay the production patches against the exact pinned upstream sources."""
from pathlib import Path
import subprocess
import os
import tempfile
root=Path(__file__).resolve().parents[1]
circle=Path(os.environ.get('CIRCLEHOME',root/'.deps/circle-build'))
if not (circle/'.git').exists():
 print('CPU heap patch replay skipped: configure CIRCLEHOME for the pinned Circle integration check')
 raise SystemExit(0)
pin='7a81e1b480fe7d19fc1fc695b89bff0a0848f44b'
paths=['include/circle/heapallocator.h','lib/heapallocator.cpp','include/circle/memory.h','include/circle/memorymap64.h','lib/memory64.cpp','lib/translationtable64.cpp']
with tempfile.TemporaryDirectory() as directory:
 d=Path(directory)
 for p in paths:
  target=d/p;target.parent.mkdir(parents=True,exist_ok=True)
  target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':'+p]))
 for name in ['fallible-heap','cpu-heap']:
  subprocess.run(['python3',str(root/f'scripts/patch-circle-{name}.py'),str(d)],check=True)
 # Preprocess the actual AArch64 include route, rather than its AArch32 sibling.
 text=subprocess.check_output(['g++','-E','-dM','-x','c++','-DAARCH=64','-DRASPPI=4','-DGROOVE_CPU_HEAP','-I'+str(d/'include'),'-I'+str(circle/'include'),'-include','circle/sysconfig.h','/dev/null'],text=True)
 assert '#define MEM_CPU_HEAP_START 0xC0000000ULL' in text
 assert '#define MEM_CPU_HEAP_END 0xFC000000ULL' in text
 assert '#define MEM_PCIE_RANGE_START_VIRTUAL MEM_PCIE_RANGE_START' in text
 before={p:(d/p).read_bytes() for p in paths}
 subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],check=True)
 assert before=={p:(d/p).read_bytes() for p in paths}
 # Partial patches must fail before writing any file.
 (d/'lib/memory64.cpp').write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':lib/memory64.cpp']))
 partial={p:(d/p).read_bytes() for p in paths}
 result=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],capture_output=True)
 assert result.returncode and b'Partial CPU heap' in result.stderr
 assert partial=={p:(d/p).read_bytes() for p in paths}
print('CPU heap: pinned replay, idempotence and atomic partial-patch rejection PASS')
