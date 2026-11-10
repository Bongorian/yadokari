"""Lost replies and rejected probes never turn into duplicate mutations."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('deployment_runner', root / 'tools/testing/deployment_check.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class Peer:
    def __init__(self, mode):
        self.mode = mode
        self.mutations = []
        self.log_reads = 0
        self.guard_requests = []

    def command(self, command, timeout):
        if command == 'STATUS':
            version = 565 if self.mode == 'wrong-version' else 566
            busy = int(self.mode == 'guard' and self.log_reads == 1)
            return f'GBX1 STATUS remote=1 active=0 ready=1 armed=0 phase=0 version={version} highest={version} partition=3 trial=0 journal_trial=0 error=0 ip=10.42.0.46 diagnostic_busy={busy}'
        if command == 'HARDWARE':
            return 'GBX1 HARDWARE uptime_s=200'
        if command == 'LOG':
            if not self.mutations:
                return 'GBX1 LOG -'
            self.log_reads += 1
            if self.mode == 'lost-reply' and self.log_reads == 1:
                raise TimeoutError('temporary log delay')
            if self.mode == 'guard' and self.log_reads == 1:
                return 'GBX1 LOG ' + b'working\n'.hex()
            text = 'Deploy functional completed passed 21 ok 1 restored 1\n'
            if self.mode == 'rejected':
                text = 'Deploy functional requested started 0\n'
            if self.mode == 'chained-ui':
                text = 'UI-only diagnostic completed idle 1; no DSP or Fluid chaining\nDSP suite unexpected\n'
            return 'GBX1 LOG ' + text.encode().hex()
        if self.mode == 'guard' and command in ('ENTER', 'PRESETCHECK'):
            self.guard_requests.append(command)
            raise RuntimeError('Device rejected command; query STATUS')
        self.mutations.append(command)
        if self.mode == 'lost-reply':
            raise TimeoutError('reply lost after accepted request')
        return 'GBX1 RESULT command=36 ok=1'

    def close(self):
        pass


for mode, expected in [('success', 0), ('lost-reply', 0), ('rejected', 1), ('wrong-version', 1), ('chained-ui', 1), ('guard', 0)]:
    with tempfile.TemporaryDirectory() as temp:
        peer = Peer(mode)
        output = Path(temp) / 'result.json'
        command = 'UITEST' if mode == 'chained-ui' else 'DEPLOYCHECK'
        args = ['deployment_check.py', '--network', 'test', '--version', '566', '--output', str(output), '--command', command]
        if mode == 'guard':
            args.append('--verify-admission')
        with patch.object(runner, 'file_open_rejected', return_value={'rejected': True, 'error': 'BUSY', 'status': 2}), patch.object(sys, 'argv', args), patch.object(runner, 'NetworkLink', return_value=peer), patch.object(runner.time, 'sleep'), contextlib.redirect_stdout(io.StringIO()):
            assert runner.main() == expected
        result = json.loads(output.read_text())
        assert len(peer.mutations) == (0 if mode == 'wrong-version' else 1)
        assert result['mutations_sent'] == len(peer.mutations)
        if mode == 'lost-reply':
            assert result['ok'] and result['reply_error'] and len(result['transport_errors']) == 1
        if mode == 'guard':
            assert peer.guard_requests == ['ENTER', 'PRESETCHECK'] and len(result['admission']) == 3
        if mode == 'rejected':
            assert 'Diagnostic rejected' in result['failure']
print('deployment runner: success/lost reply/read timeout/rejection/version/chaining; no duplicate mutations')
