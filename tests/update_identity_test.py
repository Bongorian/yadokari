#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'scripts').mkdir();(p/'firmware').mkdir()
    script=p/'scripts'/'update-public-key.py';shutil.copyfile(root/'scripts'/'update-public-key.py',script)
    header=p/'firmware'/'update_key.h';key=p/'public';key.write_bytes(bytes(range(1,33)))
    env=os.environ.copy()
    for name in ('UPDATE_PUBLIC_KEY','FIRMWARE_VERSION','ENABLE_UPDATE_WIFI'):env.pop(name,None)
    subprocess.run([sys.executable,str(script)],env=env,check=True,capture_output=True)
    assert '#define GROOVE_FW_VERSION 1' in header.read_text()
    env.update(UPDATE_PUBLIC_KEY=str(key),FIRMWARE_VERSION='125')
    subprocess.run([sys.executable,str(script)],env=env,check=True,capture_output=True)
    # 'used' alone does not survive --gc-sections. The signed identity is a
    # loader contract, even though no C++ function reads this constant.
    source=p/'identity.cpp';source.write_text('#include "firmware/update_key.h"\nint main(){return 0;}\n')
    binary=p/'identity';subprocess.run(['c++','-O3','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--undefined=GrooveUpdateIdentity',str(source),'-o',str(binary)],check=True)
    assert binary.read_bytes().count(b'GBX-FW-ID-v1\0\0\0\0')==1
    first=header.read_bytes();env.pop('UPDATE_PUBLIC_KEY');env.pop('FIRMWARE_VERSION')
    subprocess.run([sys.executable,str(script)],env=env,check=True,capture_output=True);assert header.read_bytes()==first
    env['FIRMWARE_VERSION']='126';subprocess.run([sys.executable,str(script)],env=env,check=True,capture_output=True)
    assert '#define GROOVE_FW_VERSION 126' in header.read_text() and 'UpdatePublicKey[32]={1,2,3' in header.read_text()
    previous=header.read_bytes();env['UPDATE_PUBLIC_KEY']='';assert subprocess.run([sys.executable,str(script)],env=env,capture_output=True).returncode!=0;assert header.read_bytes()==previous
    env.pop('UPDATE_PUBLIC_KEY');header.write_bytes(b'unknown user file');assert subprocess.run([sys.executable,str(script)],env=env,capture_output=True).returncode!=0;assert header.read_bytes()==b'unknown user file'
    header.write_bytes(b'\xff\xfe');assert subprocess.run([sys.executable,str(script)],env=env,capture_output=True).returncode!=0 and header.read_bytes()==b'\xff\xfe'
    header.unlink();os.symlink(p/'not-present',header);assert subprocess.run([sys.executable,str(script)],env=env,capture_output=True).returncode!=0 and header.is_symlink()
print('update build preserves existing identity and rejects invalid overrides')
