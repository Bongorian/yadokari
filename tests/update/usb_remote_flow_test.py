import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/update'))
spec = importlib.util.spec_from_file_location('usb_remote', root / 'tools/update/usb_remote.py')
remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(remote)

with tempfile.TemporaryDirectory(prefix='groove-usb-remote-') as directory:
    payload = b'test firmware payload'
    header = bytearray(192)
    header[:4] = b'GUP1'
    struct.pack_into('<II', header, 12, 2, len(payload))
    header[24:56] = hashlib.sha256(payload).digest()
    path = Path(directory) / 'test.gup'
    path.write_bytes(header + payload)
    for confirm in (False, True):
        state = dict(remote=1, active=0, ready=1, armed=0, phase=0, version=1, highest=1, partition=2, trial=0, error=0, ip='none')
        commands = []
        class Link:
            def __init__(self, name, log):
                assert name == '/dev/test-usb'
            def close(self):
                pass
            def wait(self, timeout, predicate):
                assert predicate(state), state
                return dict(state)
            def command(self, command, timeout):
                commands.append(command)
                if command == 'ENTER':
                    state.update(active=1, armed=1)
                elif command == 'TRIAL':
                    state.update(active=0, armed=0, phase=3, version=2, partition=3,
                                 trial=1 if confirm else 0, journal_trial=0 if confirm else 1)
                elif command == 'CONFIRM':
                    state.update(phase=0, highest=2)
                elif command == 'BOOT':
                    state.update(active=0, armed=0, phase=0, version=2, trial=0, partition=3)
        def send(args):
            assert args.package == path and args.serial == '/dev/test-usb' and args.tcp is None
            state['phase'] = 2
        args = SimpleNamespace(package=path, serial='/dev/test-usb', timeout=2, log=None, confirm_identity_only=confirm)
        with patch.object(remote, 'UsbLink', Link), patch.object(remote, 'send', side_effect=send):
            remote.run(args)
        assert 'TRIAL' in commands
        assert ('CONFIRM' in commands) == confirm and ('BOOT' in commands) == confirm
print('USB flow keeps trial unconfirmed by default and only confirms when explicitly selected')
