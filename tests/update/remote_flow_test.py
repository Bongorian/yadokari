import hashlib
import importlib.util
import os
from pathlib import Path
import pty
import struct
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/update'))
spec = importlib.util.spec_from_file_location('remote_update', root / 'tools/update/remote.py')
remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(remote)

with tempfile.TemporaryDirectory(prefix='groove-remote-') as directory:
    payload = b'test firmware payload'
    header = bytearray(192)
    header[:4] = b'GUP1'
    struct.pack_into('<II', header, 12, 2, len(payload))
    header[24:56] = hashlib.sha256(payload).digest()
    path = Path(directory) / 'test.gup'
    path.write_bytes(header + payload)
    master, slave = pty.openpty()
    serial = os.ttyname(slave)
    for confirm in (False, True):
        state = dict(remote=1, active=0, ready=1, armed=0, phase=0, version=1, highest=1, partition=2, trial=0, error=0, ip='none')
        commands = []
        retry = [False]
        def transact(fd, command, timeout, log):
            commands.append(command)
            if command == 'STATUS':
                if retry[0]:
                    retry[0] = False
                    raise OSError('CDC disconnected during reboot')
                return 'GBX1 STATUS ' + ' '.join(f'{k}={v}' for k, v in state.items())
            if command == 'ENTER':
                state.update(active=1, armed=1)
            elif command == 'ETHERNET':
                state['ip'] = '127.0.0.1'
            elif command == 'TRIAL':
                state.update(active=0, armed=0, phase=3, version=2, partition=3,
                             trial=1 if confirm else 0, journal_trial=0 if confirm else 1, ip='none')
                retry[0] = True
            elif command == 'CONFIRM':
                state.update(phase=0, highest=2)
            elif command == 'BOOT':
                state.update(active=0, armed=0, phase=0, version=2, trial=0, partition=3)
            return 'ok'
        def send(args):
            assert args.package == path and args.tcp == '127.0.0.1:16700'
            state['phase'] = 2
        args = SimpleNamespace(package=path, serial=serial, timeout=2, log=None, confirm_identity_only=confirm)
        with patch.object(remote, 'transact', side_effect=transact), patch.object(remote, 'send', side_effect=send):
            remote.run(args)
        assert 'TRIAL' in commands
        assert ('CONFIRM' in commands) == confirm and ('BOOT' in commands) == confirm
    os.close(master)
    os.close(slave)
print('Remote flow preserves unconfirmed trial by default; explicit identity-only path checked')

class RecoveryStatus:
    def command(self, command, timeout):
        assert command == 'STATUS'
        return 'GBX1 STATUS remote=1 active=1 ready=1 armed=1 phase=0 version=1 highest=1 partition=1 trial=0 error=0 ip=none'
    def close(self):
        pass

try:
    remote.wait_status(RecoveryStatus(), 1, lambda s: False,
                       lambda s: s['partition'] == 1 and s['phase'] == 0 and s['highest'] < 2)
    raise AssertionError('Recovery rollback must be reported')
except RuntimeError as exc:
    assert 'returned to recovery' in str(exc)

class DiagnosticSocket:
    def __init__(self):
        self.parts = [b'GBX1 STATUS remote=1 active=1 ready=0 armed=0 phase=3 version=2 highest=1 ',
                      b'partition=3 trial=0 error=11 ip=10.42.0.46\nGBX1 BOOTDIAG flags=0\n']
    def __enter__(self):
        return self
    def __exit__(self, *args):
        pass
    def settimeout(self, value):
        pass
    def recv(self, size):
        return self.parts.pop(0)

with patch.object(remote.socket, 'create_connection', return_value=DiagnosticSocket()), patch('builtins.print') as output:
    remote.TrialObserver('10.42.0.46').observe()
    assert output.call_count == 2
    assert 'GBX1 BOOTDIAG flags=0' in str(output.call_args)

class ControlSocket:
    def __init__(self):
        self.parts = [b'GBX1 STA', b'TUS remote=1\n']
        self.sent = b''
    def __enter__(self):
        return self
    def __exit__(self, *args):
        pass
    def settimeout(self, value):
        pass
    def sendall(self, data):
        self.sent += data
    def recv(self, size):
        return self.parts.pop(0)

peer = ControlSocket()
with patch.object(remote.socket, 'create_connection', return_value=peer):
    assert remote.NetworkLink('10.42.0.46').command('STATUS', 2) == 'GBX1 STATUS remote=1'
    assert peer.sent == b'!GBX1 STATUS\n'

peer = ControlSocket()
peer.parts = [b'GBX1 HARD', b'WARE arm_hz=600000000\n']
with patch.object(remote.socket, 'create_connection', return_value=peer):
    assert remote.NetworkLink('10.42.0.46').command('HARDWARE', 2) == 'GBX1 HARDWARE arm_hz=600000000'
    assert peer.sent == b'!GBX1 HARDWARE\n'
