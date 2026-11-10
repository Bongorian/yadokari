#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
tool=root/'tools/seed-boot-session.py'
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);source=p/'factory';destination=p/'data'/'SESS.MGX';source.write_bytes(b'factory')
    subprocess.run([sys.executable,str(tool),str(source),str(destination)],check=True)
    assert destination.read_bytes()==b'factory'
    destination.write_bytes(b'user project, not a valid schema')
    subprocess.run([sys.executable,str(tool),str(source),str(destination)],check=True)
    assert destination.read_bytes()==b'user project, not a valid schema'
    destination.unlink();os.symlink(p/'missing',destination)
    subprocess.run([sys.executable,str(tool),str(source),str(destination)],check=True)
    assert destination.is_symlink() and not (p/'missing').exists()
    assert not list(destination.parent.glob('.seed-session-*'))
print('boot session seeding preserves existing files and symlinks')

with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);source=p/"source";dest=p/"dest";source.mkdir();dest.mkdir()
    (source/"catalog.csv").write_text("not audio");(source/"NOTICE.txt").write_text("license")
    (source/"a.wav").write_bytes(b"factory");(source/"b.wav").write_bytes(b"new");(source/"ignore.wav.factory-000").write_bytes(b"candidate")
    (dest/"a.wav").write_bytes(b"user")
    tool=root/"tools/copy-boot-assets.py"
    assert subprocess.run([sys.executable,str(tool),str(source),str(dest)],capture_output=True).returncode!=0
    assert (dest/"a.wav").read_bytes()==b"user" and not (dest/"b.wav").exists()
    (dest/"a.wav").write_bytes(b"factory")
    subprocess.run([sys.executable,str(tool),str(source),str(dest)],check=True)
    assert (dest/"b.wav").read_bytes()==b"new" and not (dest/"ignore.wav.factory-000").exists()
    assert not list(dest.glob(".copy-asset-*"))
    assert not (dest/"catalog.csv").exists() and (dest/"NOTICE.txt").exists()
print("boot asset copying preserves conflicts")

# Manifest reflects compiled identity and preserves unknown existing sessions.
import json,struct
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'assets').mkdir();(p/'GROOVE').mkdir();(p/'GROOVE'/'SESS.MGX').write_bytes(b'USER data');(p/'GROOVE'/'CURRENT.TXT').write_text('GROOVE/SESS.MGX')
    marker=b'GBX-FW-ID-v1\0\0\0\0'
    def image(key):return marker+struct.pack('<I',125)+key+struct.pack('<I',1)
    (p/'kernel8-rpi4.img').write_bytes(image(bytes(32)))
    tool=root/'tools/boot-manifest.py'
    subprocess.run([sys.executable,str(tool),str(p)],check=True,capture_output=True)
    manifest=json.loads((p/'manifest.json').read_text());assert manifest['firmware_version']==125 and not manifest['signed_updates_enabled'] and manifest['initial_session_schema']=='USER'
    (p/'kernel8-rpi4.img').write_bytes(image(bytes([1])*32))
    subprocess.run([sys.executable,str(tool),str(p)],check=True,capture_output=True)
    manifest=json.loads((p/'manifest.json').read_text());assert manifest['signed_updates_enabled'] and manifest['initial_session_schema']=='USER'
    os.symlink(p/'outside',p/'assets'/'link')
    assert subprocess.run([sys.executable,str(tool),str(p)],capture_output=True).returncode!=0
print('boot manifest records trust and session identity without following symlinks')
