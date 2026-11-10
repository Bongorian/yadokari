#!/usr/bin/env python3
"""GUP1 signed-kernel packages and bounded TCP/USB-CDC sender (host only)."""
import argparse
import hashlib
import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import tempfile
import time


def public_key(private):
    der = subprocess.check_output(['openssl', 'pkey', '-in', str(private), '-pubout', '-outform', 'DER'])
    prefix = bytes.fromhex('302a300506032b6570032100')
    if len(der) != 44 or not der.startswith(prefix):
        raise ValueError('An Ed25519 key is required')
    return der[len(prefix):]


def identity(image):
    marker = b'GBX-FW-ID-v1\0\0\0\0'
    if image.count(marker) != 1:
        raise ValueError('Exactly one compiled firmware identity is required')
    offset = image.index(marker) + len(marker)
    if offset + 40 > len(image):
        raise ValueError('Truncated firmware identity')
    return struct.unpack_from('<I', image, offset)[0], image[offset+4:offset+36], struct.unpack_from('<I', image, offset+36)[0]


def package(image, private, version, label='', require_identity=False):
    if not 0 < len(image) <= 8 * 1024 * 1024 or not 0 < version < 2**32:
        raise ValueError('Invalid version or image size')
    key = public_key(private)
    if require_identity:
        compiled_version, compiled_key, kind = identity(image)
        if compiled_version != version or compiled_key != key or kind != 1:
            raise ValueError('Package version/key must match a compiled application, not the recovery image')
    name = label.encode('ascii')
    if len(name) > 31:
        raise ValueError('Label is limited to 31 ASCII bytes')
    header = bytearray(128)
    struct.pack_into('<4sIIIII', header, 0, b'GUP1', 1, 0x34414152, version, len(image), 1)
    header[24:56] = hashlib.sha256(image).digest()
    header[56:72] = hashlib.sha256(key).digest()[:16]
    header[72:72+len(name)] = name
    with tempfile.TemporaryDirectory() as temp:
        message = Path(temp) / 'manifest'
        message.write_bytes(header)
        signature = subprocess.check_output(['openssl', 'pkeyutl', '-sign', '-rawin', '-inkey', str(private), '-in', str(message)])
    if len(signature) != 64:
        raise ValueError('Invalid Ed25519 signature length')
    return bytes(header) + signature + image


class Connection:
    def __init__(self, args):
        self.sock = None
        if args.tcp:
            host, port = args.tcp.rsplit(':', 1)
            self.sock = socket.create_connection((host, int(port)), args.timeout)
            self.fd = self.sock.fileno()
        else:
            import termios
            import tty
            self.fd = os.open(args.serial, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            tty.setraw(self.fd)
            config = termios.tcgetattr(self.fd)
            config[4] = config[5] = termios.B115200
            termios.tcsetattr(self.fd, termios.TCSANOW, config)
        self.timeout = args.timeout

    def write(self, data):
        end = time.monotonic() + self.timeout
        while data:
            wait = end - time.monotonic()
            if wait <= 0 or not select.select([], [self.fd], [], wait)[1]:
                raise TimeoutError('Send timed out; candidate remains uncommitted')
            try:
                count = os.write(self.fd, data)
            except BlockingIOError:
                continue
            if not count:
                raise ConnectionError('Disconnected')
            data = data[count:]

    def line(self):
        data = bytearray()
        end = time.monotonic() + self.timeout
        while len(data) < 256:
            wait = end - time.monotonic()
            if wait <= 0 or not select.select([self.fd], [], [], wait)[0]:
                raise TimeoutError('Reply timed out; do not assume installation succeeded')
            byte = os.read(self.fd, 1)
            if not byte:
                raise ConnectionError('Disconnected')
            data += byte
            if byte == b'\n':
                return data.decode('ascii').strip()
        raise ValueError('Oversized response')

    def close(self):
        if self.sock:
            self.sock.close()
        else:
            os.close(self.fd)


def send(args):
    data = Path(args.package).read_bytes()
    if len(data) < 193 or data[:4] != b'GUP1' or len(data) != 192 + struct.unpack_from('<I', data, 16)[0]:
        raise ValueError('Invalid package size')
    conn = Connection(args)
    try:
        conn.write(data[:192])
        reply = conn.line()
        if reply != 'READY':
            raise ValueError(reply)
        for offset in range(192, len(data), 4096):
            chunk = data[offset:offset+4096]
            conn.write(chunk)
            reply = conn.line()
            expected = 'VERIFIED' if offset + len(chunk) == len(data) else 'OK'
            if reply != expected:
                raise ValueError(reply)
            print(f'{offset + len(chunk) - 192}/{len(data)-192}', flush=True)
        print('Verified and staged. Trial boot has not yet been requested.')
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    key = commands.add_parser('keygen')
    key.add_argument('directory', type=Path)
    pack = commands.add_parser('pack')
    pack.add_argument('image', type=Path)
    pack.add_argument('--key', required=True, type=Path)
    pack.add_argument('--version', required=True, type=int)
    pack.add_argument('--label', default='')
    pack.add_argument('--out', required=True, type=Path)
    upload = commands.add_parser('send')
    upload.add_argument('package', type=Path)
    upload.add_argument('--timeout', type=float, default=30)
    transport = upload.add_mutually_exclusive_group(required=True)
    transport.add_argument('--tcp', help='IPv4-address:16700')
    transport.add_argument('--serial', help='/dev/ttyACM0')
    args = parser.parse_args()
    if args.command == 'keygen':
        args.directory.mkdir(mode=0o700, parents=True, exist_ok=False)
        private = args.directory / 'private.pem'
        old = os.umask(0o077)
        try:
            subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', str(private)], check=True)
        finally:
            os.umask(old)
        (args.directory / 'public.bin').write_bytes(public_key(private))
        print('Public key SHA256:', hashlib.sha256(public_key(private)).hexdigest())
    elif args.command == 'pack':
        data = package(args.image.read_bytes(), args.key, args.version, args.label, require_identity=True)
        with args.out.open('xb') as out:
            out.write(data)
        print('Package SHA256:', hashlib.sha256(data).hexdigest())
    else:
        send(args)


if __name__ == '__main__':
    main()
