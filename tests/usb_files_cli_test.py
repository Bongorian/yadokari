#!/usr/bin/env python3
"""Run the actual Python client against the shared C++ file service over pipes."""
import importlib.util
import os
from pathlib import Path
import select
import subprocess
import tempfile
import sys
import hashlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
spec=importlib.util.spec_from_file_location('usb_files',ROOT/'tools/files/usb_files.py');cli=importlib.util.module_from_spec(spec);spec.loader.exec_module(cli)
class Pipe:
    def __init__(self):self.proc=None;self.device=self
    def open(self):
        if self.proc is None:self.proc=subprocess.Popen([str(ROOT/'build/test-file-bulk'),'--server'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    def write(self,endpoint,data,timeout):self.proc.stdin.write(data);self.proc.stdin.flush();return len(data)
    def read(self,endpoint,size,timeout):
        if not select.select([self.proc.stdout],[],[],timeout/1000)[0]:raise TimeoutError('pipe')
        return os.read(self.proc.stdout.fileno(),size)
    def close(self):
        if self.proc:
            self.proc.stdin.close();assert self.proc.wait(timeout=60)==0;self.proc=None
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)/'Sample.wav'
    import wave
    with wave.open(str(path),'wb') as wav:
        wav.setparams((2,2,48000,0,'NONE','NONE'));wav.writeframes(bytes(32768*3))
    body=bytearray(path.read_bytes())
    with cli.Client(Pipe()) as client:
        result=client.put(path);assert result['path']=='ASSETS/SAMPLES/Sample.wav'
        assert any(e['path']==result['path'] for e in client.list(3))
        output=Path(directory)/'back.wav';assert client.get(result['path'],output)['sha256']==result['sha256'];assert output.read_bytes()==body
        body[-1]^=1;path.write_bytes(body);replaced=client.put(path);assert replaced['sha256']!=result['sha256'];assert client.get(result['path'],output)['sha256']==replaced['sha256']
        path.write_bytes(b'broken')
        try:client.put(path);raise AssertionError('accepted broken RIFF')
        except cli.FileError as exc:assert exc.status==8
        assert client.get(result['path'],output)['sha256']==replaced['sha256']
        for op,data in [(8,b'PROJECTS/Open.mgxs\0'),(8,b'ASSETS/SAMPLES/Locked.wav\0'),(8,b'../GROOVE/DEVICE.CFG\0')]:
            try:client.request(op,data);raise AssertionError('unprotected')
            except cli.FileError as exc:assert exc.status in (3,5)
        client.delete(result['path']);assert not any(e['path']==result['path'] for e in client.list(3))
print('PASS actual Python bulk client: split PUT, SHA GET, overwrite, invalid preservation, protection, list/delete and close')

# Kernel ownership is a control/data pair on Linux; restore the exact old pair.
from unittest.mock import patch
import raw_usb_control as transport
class KernelDevice:
    bus=1;address=4
    def __init__(self):self.active={0:True,1:True};self.attached=[]
    def is_kernel_driver_active(self,i):return self.active[i]
    def detach_kernel_driver(self,i):assert i==1;self.active={0:False,1:False}
    def attach_kernel_driver(self,i):
        self.attached.append(i)
        if i==0:self.active={0:True,1:True}
        else:self.active[i]=True
fake=KernelDevice()
with patch.object(transport.usb.core,'find',return_value=[fake]),patch.object(transport,'matches_cached_identity',return_value=True),patch.object(transport.usb.util,'claim_interface'),patch.object(transport.usb.util,'release_interface'),patch.object(transport.usb.util,'dispose_resources'):
    link=transport.RawUsbLink();link.open();assert not any(fake.active.values());link.close();assert fake.active=={0:True,1:True} and fake.attached==[0]
print('PASS Linux CDC control/data ownership restoration')
