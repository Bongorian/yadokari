import importlib.util
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'scripts/check-kernel-bounds.py'
spec=importlib.util.spec_from_file_location('kernel_bounds',p);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
base='00080000 T _start\n00280000 A groove_kernel_max_size\n'
assert mod.check_symbols(base+'002fffff B _end\n')==(0x27ffff,0x280000)
for text in (base+'00300001 B _end\n', '00080000 T _start\n00290000 B _end\n', base+'00070000 B _end\n', base+'00090000 B _end\n00090000 B _end\n'):
    try:mod.check_symbols(text)
    except ValueError:pass
    else:raise AssertionError('invalid kernel layout accepted')
print('Kernel bounds include BSS, exact limit and missing symbols')
