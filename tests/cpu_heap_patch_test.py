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
def extract(d):
 for p in paths:
  target=d/p;target.parent.mkdir(parents=True,exist_ok=True)
  target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':'+p]))
with tempfile.TemporaryDirectory() as directory:
 d=Path(directory)
 extract(d)
 for name in ['fallible-heap','cpu-heap']:
  subprocess.run(['python3',str(root/f'scripts/patch-circle-{name}.py'),str(d)],check=True)
 # Preprocess the actual AArch64 include route, rather than its AArch32 sibling.
 text=subprocess.check_output(['g++','-E','-dM','-x','c++','-DAARCH=64','-DRASPPI=4','-DGROOVE_CPU_HEAP','-I'+str(d/'include'),'-I'+str(circle/'include'),'-include','circle/sysconfig.h','/dev/null'],text=True)
 assert '#define MEM_CPU_HEAP_START 0xC0000000ULL' in text
 assert '#define MEM_CPU_HEAP_END 0xFC000000ULL' in text
 assert '#define MEM_PCIE_RANGE_START_VIRTUAL MEM_PCIE_RANGE_START' in text
 # Pi 5: capacity-only arena (top of DRAM, carved out of the high heap);
 # the fixed Pi 4 window macros must not leak into a Pi 5 build.
 text5=subprocess.check_output(['g++','-E','-dM','-x','c++','-DAARCH=64','-DRASPPI=5','-DGROOVE_CPU_HEAP','-I'+str(d/'include'),'-I'+str(circle/'include'),'-include','circle/sysconfig.h','/dev/null'],text=True)
 assert '#define MEM_CPU_HEAP_SIZE (960ULL * MEGABYTE)' in text5
 assert 'MEM_CPU_HEAP_START' not in text5
 reference={p:(d/p).read_bytes() for p in paths}
 subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],check=True)
 assert reference=={p:(d/p).read_bytes() for p in paths}
 # Partial patches must fail before writing any file.
 (d/'lib/memory64.cpp').write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':lib/memory64.cpp']))
 partial={p:(d/p).read_bytes() for p in paths}
 result=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],capture_output=True)
 assert result.returncode and b'Partial CPU heap' in result.stderr
 assert partial=={p:(d/p).read_bytes() for p in paths}
 # Migration: a tree patched by the 0.9.0 script (marker in all six files,
 # none of the current definitions) must be migrated to exactly the current
 # shape, never silently skipped -- a stale CI cache restore is that tree.
 old_script=None
 env_old=os.environ.get('OLD_CPU_HEAP_SCRIPT')
 if env_old and Path(env_old).exists():
  old_script=Path(env_old)
 else:
  probe=subprocess.run(['git','-C',str(root),'show','v0.9.0:scripts/patch-circle-cpu-heap.py'],capture_output=True)
  if probe.returncode==0 and b'GROOVE_CPU_HEAP_PATCH' in probe.stdout:
   old_script=Path(directory)/'patch-circle-cpu-heap-090.py';old_script.write_bytes(probe.stdout)
 if old_script is None:
  print('CPU heap migration replay skipped: 0.9.0 patch script unavailable (set OLD_CPU_HEAP_SCRIPT)')
 else:
  with tempfile.TemporaryDirectory() as stale_dir:
   d2=Path(stale_dir)
   extract(d2)
   subprocess.run(['python3',str(root/'scripts/patch-circle-fallible-heap.py'),str(d2)],check=True,capture_output=True)
   subprocess.run(['python3',str(old_script),str(d2)],check=True,capture_output=True)
   assert all(b'GROOVE_CPU_HEAP_PATCH' in (d2/p).read_bytes() for p in paths)
   assert b'IsCpuHeapPointer' not in (d2/'include/circle/memory.h').read_bytes()
   migrated=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d2)],capture_output=True)
   assert migrated.returncode==0 and b'migrated' in migrated.stdout
   assert reference=={p:(d2/p).read_bytes() for p in paths}
   subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d2)],check=True)
   assert reference=={p:(d2/p).read_bytes() for p in paths}
  print('CPU heap: 0.9.0-patched tree migrates to the current shape PASS')
print('CPU heap: pinned replay, idempotence and atomic partial-patch rejection PASS')
