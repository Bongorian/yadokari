#!/usr/bin/env python3
"""Fetch pinned Pi boot files and verify both new downloads and existing cache."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
from urllib.request import urlopen

LOCK = Path(__file__).resolve().parents[2]/'assets/boot/pi-boot-sha256.json'

def ensure_file(cache, name, digest, url):
    target = cache/name
    if target.exists():
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest()!=digest:
            raise ValueError('Pi boot cache SHA256 mismatch: '+str(target))
        return target
    cache.mkdir(parents=True,exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix='.'+name+'-',suffix='.download',dir=cache)
    try:
        checksum = hashlib.sha256()
        with os.fdopen(fd,'wb') as output, urlopen(url,timeout=30) as source:
            while data:=source.read(65536):
                checksum.update(data);output.write(data)
        if checksum.hexdigest()!=digest:
            raise ValueError('Pi boot download SHA256 mismatch: '+name)
        os.replace(temp,target)
    finally:
        Path(temp).unlink(missing_ok=True)
    return target

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cache',type=Path,required=True);p.add_argument('--revision',required=True);a=p.parse_args()
    lock=json.loads(LOCK.read_text())
    if a.revision!=lock['revision']:p.error('Boot revision differs from pinned SHA256 manifest')
    try:
        for name,digest in lock['sha256'].items():
            ensure_file(a.cache,name,digest,f"https://raw.githubusercontent.com/raspberrypi/firmware/{a.revision}/boot/{name}")
    except (OSError,ValueError) as e:raise SystemExit(str(e))
    print('Pi boot: all5 pinned downloads/cache entries verified by SHA256')

if __name__=='__main__':main()
