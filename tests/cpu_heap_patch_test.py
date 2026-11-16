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
 # Pi 5: capacity-only arena (top of DRAM, carved out of the high heap);
 # the fixed Pi 4 window macros must not leak into a Pi 5 build.
 text5=subprocess.check_output(['g++','-E','-dM','-x','c++','-DAARCH=64','-DRASPPI=5','-DGROOVE_CPU_HEAP','-I'+str(d/'include'),'-I'+str(circle/'include'),'-include','circle/sysconfig.h','/dev/null'],text=True)
 assert '#define MEM_CPU_HEAP_SIZE (960ULL * MEGABYTE)' in text5
 assert 'MEM_CPU_HEAP_START' not in text5
 before={p:(d/p).read_bytes() for p in paths}
 subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],check=True)
 assert before=={p:(d/p).read_bytes() for p in paths}
 # Partial patches must fail before writing any file.
 (d/'lib/memory64.cpp').write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':lib/memory64.cpp']))
 partial={p:(d/p).read_bytes() for p in paths}
 result=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],capture_output=True)
 assert result.returncode and b'Partial CPU heap' in result.stderr
 assert partial=={p:(d/p).read_bytes() for p in paths}
 # Stale shapes under the same marker must migrate or refuse loudly, never
 # silently skip (the marker survived a content rewrite once already).
 for p in paths:
  target=d/p;target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show',pin+':'+p]))
 for name in ['fallible-heap','cpu-heap']:
  subprocess.run(['python3',str(root/f'scripts/patch-circle-{name}.py'),str(d)],check=True)
 current={p:(d/p).read_bytes() for p in paths}
 # 0.9.2 shape: Pi 5 arena at the fixed 960 MiB, no runtime DRAM sizing.
 mc='lib/memory64.cpp'
 sized=(d/mc).read_text()
 runtime_block="""\t\t// RAM-scaled arena (perf investigation): 8 GiB boards carve
\t\t// 2 GiB; 4 GiB boards keep the 960 MiB baseline. Mirrors the
\t\t// rule in groove::hardware::decode (src/hardware_profile.h),
\t\t// so the reported profile and the real carve agree.
\t\tm_nMemSizeCpu = nRAMSize >= 8192 ? (2048ULL * 1024 * 1024) : MEM_CPU_HEAP_SIZE;"""
 assert sized.count(runtime_block)==1
 (d/mc).write_text(sized.replace(runtime_block,'\t\tm_nMemSizeCpu = MEM_CPU_HEAP_SIZE;'))
 result=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],capture_output=True)
 assert result.returncode==0 and b'migrated' in result.stdout
 assert current=={p:(d/p).read_bytes() for p in paths}
 # Unrecognised stale shape: marker everywhere, IsCpuHeapPointer stripped.
 mh='include/circle/memory.h'
 header=(d/mh).read_text()
 probe="""#ifdef GROOVE_CPU_HEAP
\tstatic boolean IsCpuHeapPointer (const void *pBlock)
\t{
#if RASPPI == 5
\t\treturn (uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu;
#else
\t\treturn (uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END;
#endif
\t}
#endif
"""
 assert header.count(probe)==1
 (d/mh).write_text(header.replace(probe,''))
 stale={p:(d/p).read_bytes() for p in paths}
 result=subprocess.run(['python3',str(root/'scripts/patch-circle-cpu-heap.py'),str(d)],capture_output=True)
 assert result.returncode and b'Unrecognised CPU heap patch state' in result.stderr
 assert stale=={p:(d/p).read_bytes() for p in paths}
print('CPU heap: stale-shape migration and unrecognised-shape rejection PASS')
print('CPU heap: pinned replay, idempotence and atomic partial-patch rejection PASS')
