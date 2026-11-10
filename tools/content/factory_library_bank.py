#!/usr/bin/env python3
"""Derive a second GBL1 layer for ROM; leave submitted masters/pack untouched."""
import argparse, hashlib, re
from pathlib import Path
from factory_library_pack import pack
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    source=(ROOT/'src/generated/factory_library.inc').read_text()
    prefix,rest=source.split('inline constexpr uint8_t kFactoryLibraryPacked[]={\n',1)
    body,suffix=rest.split('};\n',1)
    raw=bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]{2})',body));encoded=pack(raw)
    digest=hashlib.sha256(raw).digest()
    text='// Derived by tools/content/factory_library_bank.py; original submitted GBL1 pack stays unchanged.\n'+prefix
    text+=f'inline constexpr unsigned kFactoryLibraryBankBytes={len(raw)};\n'
    text+='inline constexpr uint8_t kFactoryLibraryBankHash[]={'+','.join(str(v) for v in digest)+'};\n'
    text+='inline constexpr uint8_t kFactoryLibraryPacked[]={\n'
    text+=''.join(' '+','.join(f'0x{v:02x}' for v in encoded[i:i+24])+',\n' for i in range(0,len(encoded),24)) or ' 0,\n'
    text+='};\n'+suffix;target=ROOT/'src/generated/factory_library_bank.inc'
    if args.write:target.write_text(text)
    elif not target.exists() or target.read_text()!=text:raise SystemExit('Factory ROM bank differs; regenerate with tools/content/factory_library_bank.py --write')
    print(f'Factory ROM bank: {len(raw)} -> {len(encoded)} bytes; SHA256 checked before inner unpack')
if __name__=='__main__':main()
