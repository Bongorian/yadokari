#!/usr/bin/env python3
"""MGF1 v1 file CLI over Circle CDC bulk; no tty and no GBX1 file commands."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'update'))
from raw_usb_control import RawUsbLink

HEADER = struct.Struct('<4sBBHIIII')
ERRORS = ['OK','CLOSED','BUSY','INVALID_PATH','UNSUPPORTED_TYPE','IN_USE','LENGTH_MISMATCH','HASH_MISMATCH','INVALID_CONTENT','IO_ERROR','EXISTS','LIMIT']
SHELVES = ['projects', 'presets', 'kits', 'samples', 'soundfonts', 'midi', 'recordings']

def checksum(data):
    value = 2166136261
    for b in data:
        value = ((value ^ b) * 16777619) & 0xffffffff
    return value

class FileError(RuntimeError):
    def __init__(self, status, details=b''):
        self.status = status
        self.details = details
        message=ERRORS[status] if status<len(ERRORS) else {128:'INVALID_FRAME',129:'UNSUPPORTED_VERSION'}.get(status,str(status))
        if status==128 and len(details)==24:
            reason,expected,actual,length,offset=struct.unpack('<IIIIQ',details)
            message+=f' reason={reason} expected={expected:08x} received={actual:08x} payload={length} accepted_offset={offset}'
        super().__init__(message)

class Client:
    def __init__(self, link=None, timeout=120):
        self.link = link or RawUsbLink()
        self.timeout = timeout
        self.sequence = 0
        self.buffer = bytearray()
        self.opened = False
    def read(self, size):
        deadline = time.monotonic() + self.timeout
        while len(self.buffer) < size:
            remaining = max(1, int((deadline-time.monotonic())*1000))
            if time.monotonic() >= deadline:
                raise TimeoutError('USB file reply timeout; outcome unknown')
            self.buffer.extend(bytes(self.link.device.read(0x83, 4096, timeout=remaining)))
        out = bytes(self.buffer[:size]); del self.buffer[:size]
        return out
    def request(self, op, data=b'', version=1):
        if len(data) > 4096:
            raise ValueError('Oversized request')
        self.link.open()
        self.sequence += 1
        head = HEADER.pack(b'MGF1',version,op,0,self.sequence,len(data),checksum(data),0)
        frame = head[:20] + struct.pack('<I',checksum(head[:20])) + data
        if self.link.device.write(0x02,frame,timeout=int(self.timeout*1000)) != len(frame):
            raise OSError('Short USB file write')
        head = self.read(24)
        magic,ver,reply,status,seq,n,body_sum,head_sum = HEADER.unpack(head)
        if magic != b'MGF1' or ver != 1 or reply != op|0x80 or seq != self.sequence or n > 32776 or head_sum != checksum(head[:20]):
            raise ValueError('Invalid USB file reply header')
        body = self.read(n)
        if checksum(body) != body_sum:
            raise ValueError('Invalid USB file reply checksum')
        if status:
            raise FileError(status,body)
        return body
    def __enter__(self):
        try:
            self.request(1);self.opened=True
            return self
        except BaseException:
            self.link.close();raise
    def close(self):
        try:
            if self.opened:self.request(2)
        finally:
            self.opened=False;self.link.close()
    def __exit__(self,*args):self.close()
    def list(self,shelf):
        result=[];first=0
        while True:
            body=self.request(3,struct.pack('<BI',shelf,first));total,next_=struct.unpack_from('<II',body);at=8
            while at<len(body):
                size,length=struct.unpack_from('<QB',body,at);at+=9
                path=body[at:at+length].decode('utf-8');at+=length
                result.append({'path':path,'bytes':size})
            if next_==total:return result
            if next_<=first:raise ValueError('Nonadvancing list')
            first=next_
    def put(self,path,name=None):
        path=Path(path)
        with path.open('rb') as source:digest=hashlib.file_digest(source,'sha256').digest()
        self.request(5,struct.pack('<Q',path.stat().st_size)+digest+(name or path.name).encode('utf-8')+b'\0')
        at=0
        with path.open('rb') as source:
            while data:=source.read(4088):
                self.request(6,struct.pack('<Q',at)+data);at+=len(data)
        target=self.request(7).rstrip(b'\0').decode('utf-8')
        return {'path':target,'bytes':at,'sha256':digest.hex()}
    def get(self,path,output):
        output=Path(output);temp=output.with_name(output.name+'.part');at=0;digest=hashlib.sha256()
        if temp.exists():raise FileExistsError(str(temp))
        try:
            with temp.open('xb') as dest:
                while True:
                    body=self.request(4,struct.pack('<QI',at,32752)+path.encode('utf-8')+b'\0');total,offset=struct.unpack_from('<QQ',body);data=body[16:]
                    if offset!=at or at+len(data)>total or (not data and at<total):raise ValueError('Invalid GET extent')
                    dest.write(data);digest.update(data);at+=len(data)
                    if at==total:break
            temp.replace(output)
        except BaseException:
            temp.unlink(missing_ok=True);raise
        return {'path':path,'output':str(output),'bytes':at,'sha256':digest.hexdigest()}
    def delete(self,path):
        self.request(8,path.encode('utf-8')+b'\0');return {'deleted':path}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--timeout',type=float,default=120)
    sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('list');p.add_argument('shelf',choices=SHELVES)
    p=sub.add_parser('put');p.add_argument('file',type=Path);p.add_argument('--name')
    p=sub.add_parser('get');p.add_argument('path');p.add_argument('output',type=Path)
    p=sub.add_parser('delete');p.add_argument('path')
    args=parser.parse_args();started=time.monotonic()
    try:
        with Client(timeout=args.timeout) as client:
            if args.command=='list':result={'files':client.list(SHELVES.index(args.shelf))}
            elif args.command=='put':result=client.put(args.file,args.name)
            elif args.command=='get':result=client.get(args.path,args.output)
            else:result=client.delete(args.path)
        result['seconds']=round(time.monotonic()-started,3);print(json.dumps(result,ensure_ascii=False));return 0
    except (OSError,RuntimeError,ValueError,TimeoutError) as exc:
        print(json.dumps({'error':str(exc)}),file=sys.stderr);return 1
if __name__=='__main__':sys.exit(main())
