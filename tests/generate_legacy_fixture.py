# Immutable MGX1 fixture, built independently of the C++ MGX2 codec.
from pathlib import Path
import struct
import zlib
payload = bytearray(struct.pack('<HBB',137,4,2))
payload += bytes(16 * 18) # scene routes and mute masks
step = struct.pack('<BBBBBhH',0,60,100,100,1,0,96) + struct.pack('<HfB',0,0,0)*4
track = struct.pack('<BBBBBBBff',16,0,0,0,0,60,0,0.8,0) + struct.pack('<5H',*([8192]*5)) + step*64
for _ in range(16):
    payload += b'\0' + track*16
Path('tests/fixtures/legacy-v1.mgx').write_bytes(b'MGX1'+payload+struct.pack('<I',zlib.crc32(payload)))
