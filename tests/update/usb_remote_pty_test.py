"""Exercise the real host control and GUP sender on a single virtual CDC port."""
import hashlib
import importlib.util
import os
from pathlib import Path
import pty
import select
import struct
import sys
import tempfile
import threading
import tty
from types import SimpleNamespace

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/update'))
spec = importlib.util.spec_from_file_location('usb_remote_pty', root / 'tools/update/usb_remote.py')
remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(remote)

with tempfile.TemporaryDirectory(prefix='groove-usb-pty-') as directory:
    payload = bytes(range(256)) * 20
    header = bytearray(192)
    header[:4] = b'GUP1'
    struct.pack_into('<II', header, 12, 2, len(payload))
    header[24:56] = hashlib.sha256(payload).digest()
    path = Path(directory) / 'test.gup'
    path.write_bytes(header + payload)
    master, slave = pty.openpty()
    tty.setraw(slave)
    serial = os.ttyname(slave)
    state = dict(remote=1, active=0, ready=1, armed=0, phase=0, version=1, highest=1, partition=2, trial=0, error=0, ip='none')
    stopped = threading.Event()
    failure = []
    commands = []

    def device():
        buffer = bytearray()
        mode = 'control'
        body = bytearray()
        try:
            while not stopped.is_set():
                if not select.select([master], [], [], 0.1)[0]:
                    continue
                data = os.read(master, 8192)
                if not data:
                    continue
                buffer.extend(data)
                while buffer:
                    if mode == 'control' and buffer[0] == ord('G'):
                        mode = 'header'
                    if mode == 'header':
                        if len(buffer) < 192:
                            break
                        assert bytes(buffer[:192]) == header
                        del buffer[:192]
                        os.write(master, b'READY\n')
                        mode = 'body'
                    elif mode == 'body':
                        wanted = min(4096, len(payload)-len(body))
                        if len(buffer) < wanted:
                            break
                        body.extend(buffer[:wanted])
                        del buffer[:wanted]
                        if len(body) == len(payload):
                            assert body == payload
                            state['phase'] = 2
                            os.write(master, b'VERIFIED\n')
                            mode = 'control'
                        else:
                            os.write(master, b'OK\n')
                    else:
                        if b'\n' not in buffer:
                            break
                        line, _, rest = buffer.partition(b'\n')
                        buffer[:] = rest
                        assert line.startswith(b'!GBX1 ')
                        command = line[6:].decode()
                        commands.append(command)
                        if command == 'STATUS':
                            text = 'GBX1 STATUS ' + ' '.join(f'{k}={v}' for k,v in state.items()) + '\n'
                        elif command == 'ENTER':
                            state.update(active=1, armed=1)
                            text = 'GBX1 RESULT command=2 ok=1\n'
                        elif command == 'TRIAL':
                            state.update(active=0, armed=0, phase=3, version=2, partition=3, trial=1)
                            text = 'GBX1 RESULT command=6 ok=1\n'
                        else:
                            raise AssertionError('unexpected ' + command)
                        os.write(master, text.encode())
        except BaseException as exc:
            failure.append(exc)
    thread = threading.Thread(target=device)
    thread.start()
    try:
        args = SimpleNamespace(package=path, serial=serial, timeout=5, log=None, confirm_identity_only=False)
        remote.run(args)
        assert state['phase'] == 3 and 'CONFIRM' not in commands
    finally:
        stopped.set()
        thread.join(timeout=3)
        os.close(master)
        os.close(slave)
    assert not thread.is_alive() and not failure, failure
print('USB host control and bounded GUP transfer on one virtual CDC stream passed')
