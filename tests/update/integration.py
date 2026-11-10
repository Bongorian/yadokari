#!/usr/bin/env python3
"""End-to-end update tests using real MBR/FAT32 images and the shipping sender."""
import importlib.util
import hashlib
import os
from pathlib import Path
import pty
import select
import socket
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('update_tool', root / 'tools/update/update.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)
recovery_spec = importlib.util.spec_from_file_location('recovery_tool', root / 'tools/update/recover.py')
recovery_tool = importlib.util.module_from_spec(recovery_spec)
recovery_spec.loader.exec_module(recovery_tool)
wifi_spec = importlib.util.spec_from_file_location('wifi_tool', root / 'tools/update/wifi.py')
wifi_tool = importlib.util.module_from_spec(wifi_spec)
wifi_spec.loader.exec_module(wifi_tool)
config = wifi_tool.configuration('test"\nssid', 'test-only-secret', 'JP')
assert 'psk=test-only-secret' not in config and config.count('network={') == 1
assert 'ssid=74657374220a73736964' in config
try:
    wifi_tool.configuration('test', 'short', 'JP')
except ValueError:
    pass
else:
    raise AssertionError('Invalid WPA credentials accepted')
binary = root / 'build/update-sim'


def run(*args):
    return subprocess.run([str(x) for x in args], check=True, text=True, capture_output=True).stdout


def partition_hash(path, partition):
    with path.open('rb') as image:
        mbr = image.read(512)
        import struct
        first, sectors = struct.unpack_from('<II', mbr, 446 + partition * 16 + 8)
        image.seek(first * 512)
        digest = hashlib.sha256()
        remaining = sectors * 512
        while remaining:
            block = image.read(min(1024 * 1024, remaining))
            assert block
            digest.update(block)
            remaining -= len(block)
        return digest.digest()


with tempfile.TemporaryDirectory(prefix='groove-update-') as temp:
    directory = Path(temp)
    private = directory / 'key.pem'
    run('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', private)
    public = directory / 'public.bin'
    public.write_bytes(tool.public_key(private))
    compiled = b'GBX-FW-ID-v1\0\0\0\0' + (2).to_bytes(4, 'little') + public.read_bytes() + (1).to_bytes(4, 'little')
    assert tool.package(compiled, private, 2, require_identity=True)[:4] == b'GUP1'
    # Provisioning must not silently lose an explicitly selected audio profile.
    boot = directory / 'boot'
    boot.mkdir()
    for name in ('start4.elf', 'fixup4.dat', 'bcm2711-rpi-4-b.dtb', 'armstub8-rpi4.bin', 'LICENCE.broadcom', 'COPYING.linux', 'config.txt'):
        (boot / name).write_text('test\n')
    profile = 'loglevel=4 soundopt=24 mk3fbout=1 mk3fbin=129\n'
    (boot / 'cmdline.txt').write_text(profile)
    app = directory / 'app.img'
    recovery = directory / 'recovery.img'
    app.write_bytes(compiled)
    recovery.write_bytes(compiled[:-4] + (2).to_bytes(4, 'little'))
    provisioned = directory / 'provisioned.img'
    run('python3', root / 'tools/update/provision.py', provisioned, '--factory', app, '--recovery', recovery, '--boot-files', boot, '--version', 2, '--data-mib', 64)
    for partition in range(3):
        extracted_cmdline = directory / f'cmdline-{partition}.txt'
        run(binary, 'get', provisioned, f'{partition}:/cmdline.txt', extracted_cmdline)
        assert extracted_cmdline.read_text() == profile
    print('provisioning: selected audio profile preserved in all boot partitions PASS')
    for version, payload in ((3, compiled), (2, compiled[:-4] + (2).to_bytes(4, 'little'))):
        try:
            tool.package(payload, private, version, require_identity=True)
        except ValueError:
            pass
        else:
            raise AssertionError('Version mismatch or recovery image accepted as application')
    factory = directory / 'factory.img'
    factory.write_bytes(bytes((i * 17) % 256 for i in range(4097)))
    package = directory / 'update.gup'
    package.write_bytes(tool.package(bytes((i * 31 + 7) % 256 for i in range(65539)), private, 2, 'integration-2'))
    image = root / 'build/update-test.img'
    if image.exists():
        image.unlink()
    run(binary, 'init', image, factory, 1)
    protected = {partition: partition_hash(image, partition) for partition in (0, 1, 3)}
    print(run(binary, 'exercise', image, public, package).strip())
    assert all(partition_hash(image, partition) == digest for partition, digest in protected.items())
    print('recovery, original A, and full DATA partitions: byte-for-byte preservation PASS')
    screen = root / 'build/update-screen.ppm'
    run(binary, 'screen', image, screen)
    assert screen.read_bytes().startswith(b'P6\n960 272\n255\n')
    restored = directory / 'restored.img'
    run(binary, 'init', restored, factory, 1)
    # Deliberately damage both boot journals; recovery copies only DATA to a
    # separately provisioned card and never requires the normal application.
    with image.open('r+b') as damaged:
        for sector in (16, 32):
            damaged.seek(sector * 512)
            damaged.write(bytes(512))
    assert len(recovery_tool.copy_data(image, restored)) == 64
    extracted = directory / 'protected.dat'
    run(binary, 'get', restored, '3:/PROTECT.DAT', extracted)
    assert extracted.read_bytes() == bytes([1, 3, 3, 7, 9])
    print('offline recovery: both journals lost, separate SD rebuilt, full DATA readback SHA256 PASS')
    cycle_image = directory / 'cycle.img'
    third = directory / 'third.gup'
    third.write_bytes(tool.package(bytes((i * 13 + 9) % 256 for i in range(32771)), private, 3, 'integration-3'))
    run(binary, 'init', cycle_image, factory, 1)
    unchanged_data = partition_hash(cycle_image, 3)
    print(run(binary, 'cycle', cycle_image, public, package, third).strip())
    assert partition_hash(cycle_image, 3) == unchanged_data
    for transport in ('tcp', 'serial'):
        target = directory / (transport + '.img')
        run(binary, 'init', target, factory, 1)
        if transport == 'tcp':
            server = subprocess.Popen([str(binary), 'serve', str(target), str(public), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            assert select.select([server.stdout], [], [], 10)[0], 'Server startup timed out'
            port = server.stdout.readline().strip().split()
            assert port[0] == 'PORT', port
            with socket.create_connection(('127.0.0.1', int(port[1])), 5) as interrupted:
                with interrupted.makefile('rb') as replies:
                    payload = package.read_bytes()
                    interrupted.sendall(payload[:192])
                    assert replies.readline() == b'READY\n'
                    interrupted.sendall(payload[192:192+4096])
                    assert replies.readline() == b'OK\n'
            out, err = server.communicate(timeout=10)
            assert server.returncode == 1 and 'Transport disconnected' in err, (out, err)
            state = run(binary, 'status', target)
            assert 'active=0 version=1 phase=1' in state, state
            server = subprocess.Popen([str(binary), 'serve', str(target), str(public), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            assert select.select([server.stdout], [], [], 10)[0]
            port = server.stdout.readline().strip().split()
            assert port[0] == 'PORT'
            args = ['--tcp', '127.0.0.1:' + port[1]]
        else:
            master, slave = pty.openpty()
            server = subprocess.Popen([str(binary), 'serial', str(target), str(public), str(master)], pass_fds=(master,), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            args = ['--serial', os.ttyname(slave)]
        try:
            run('python3', root / 'tools/update/update.py', 'send', package, *args, '--timeout', '10')
            out, err = server.communicate(timeout=15)
            assert server.returncode == 0, (out, err)
            assert 'phase=2' in out, out  # Ready, never remotely committed/rebooted
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
            if transport == 'serial':
                os.close(master)
                os.close(slave)
        print('update sender:', transport, 'real stream + signed package + FAT readback PASS')
print('update integration PASS; sparse virtual SD retained at build/update-test.img')
