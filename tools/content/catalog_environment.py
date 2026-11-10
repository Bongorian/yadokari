#!/usr/bin/env python3
"""Enforce the reference native PCM generator environment before any output."""
import argparse, hashlib, json, platform, shlex, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
LOCK=ROOT/'tools/content/catalog_environment.lock.json'
def fingerprint(compiler):
    argv=shlex.split(compiler)
    if not argv:raise ValueError('Missing generator compiler')
    def query(option):return subprocess.check_output([*argv,option],text=True).strip()
    driver=Path(shutil.which(argv[0]) or argv[0]).resolve()
    frontend=Path(query('-print-prog-name=cc1plus')).resolve()
    libraries={}
    for line in Path('/proc/self/maps').read_text().splitlines():
        name=line.split()[-1]
        if name.startswith('/') and Path(name).name in ('libm.so.6','libc.so.6'):
            libraries[Path(name).name]=hashlib.sha256(Path(name).read_bytes()).hexdigest()
    if len(libraries)!=2:raise ValueError('Reference Linux glibc/libm mappings unavailable')
    return {'machine':platform.machine(),'target':query('-dumpmachine'),'gcc':query('-dumpfullversion'),
            'driver_sha256':hashlib.sha256(driver.read_bytes()).hexdigest(),
            'frontend_sha256':hashlib.sha256(frontend.read_bytes()).hexdigest(),
            'extra_compiler_arguments':argv[1:],'libraries':libraries,
            'profile':['-std=c++17','-O2','-Wall','-Wextra','-Werror']}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='g++');p.add_argument('--lock',type=Path,default=LOCK);p.add_argument('--capture',action='store_true');a=p.parse_args()
    try:
        actual=fingerprint(a.compiler)
        if a.capture:
            packages=subprocess.check_output(['dpkg-query','-W','g++-13','gcc-13','gcc-13-base','cpp-13','libgcc-13-dev','libstdc++-13-dev','libc6','libstdc++6'],text=True).splitlines()
            a.lock.write_text(json.dumps({'reference':'v556/56f78ea7 catalog243 unchanged; Ubuntu24.04 AArch64 GCC13.3 native generation','packages':packages,'fingerprint':actual},indent=2)+'\n')
        expected=json.loads(a.lock.read_text())['fingerprint']
        if actual!=expected:raise ValueError('Compiler/architecture/libm differs from the pinned PCM generator environment')
    except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as e:
        raise SystemExit(str(e)+'; use tools/content/catalog_environment.lock.json reference packages. Do not regenerate the factory manifest for another host.')
    print('Factory generation environment pinned: '+actual['machine']+' GCC'+actual['gcc']+'; compiler/frontend/libm/libc hashes match')
if __name__=='__main__':main()
