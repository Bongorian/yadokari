#!/usr/bin/env python3
"""Keep the original source immutable; define its Cortex-M shift-by-32 result.

ARMv7-M LSR (register), ARM DDI 0403E Shift_C/LSR_C: shifts >= 32
produce zero. AArch64 variable shifts mask the amount instead. C++ leaves
this expression undefined, and UBSan correctly rejects the pristine source.
Only the zero-offset boundary is changed; all other arithmetic is identical.
"""
import argparse,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
s=a.source.read_bytes()
assert hashlib.sha256(s).hexdigest()=='f2b929b8cbd92031f207c377aa862a46ec910ce2520549eb5fa1be8c5dc134fb'
old=b'destination_bits |= destination[i + 1] >> (32 - offset_bits);'
new=b'if (offset_bits) destination_bits |= destination[i + 1] >> (32 - offset_bits);'
assert s.count(old)==1
result=s.replace(old,new)
# The original SWAR count and popcount have exactly the same integer result.
start=result.index(b'    count = count - ((count >> 1) & 0x55555555);')
end=result.index(b'    xcorr += count;',start)
result=result[:start]+b'    count = __builtin_popcount(count);\n'+result[end:]

a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_bytes()!=result:a.output.write_bytes(result)
