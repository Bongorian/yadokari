import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'update'))
import hardware_capture
import remote

class FakeLink:
    def __init__(self, host):
        self.status = self.logs = 0
    def command(self, command, timeout):
        if command == 'STATUS':
            self.status += 1
            if self.status == 1:
                raise ConnectionResetError('test gap')
            return 'GBX1 STATUS version=1'
        self.logs += 1
        if self.logs == 1:
            raise TimeoutError('test gap')
        return 'GBX1 LOG -'
    def close(self):
        pass

with tempfile.TemporaryDirectory() as directory:
    destination = Path(directory) / 'capture'
    args = ['hardware_capture.py', '--network', 'test', '--output-dir', str(destination), '--duration', '0.8', '--status-interval', '0.1']
    with patch.object(remote, 'NetworkLink', FakeLink), patch.object(sys, 'argv', args), contextlib.redirect_stdout(io.StringIO()):
        hardware_capture.main()
    records = [json.loads(line) for line in (destination / 'status.jsonl').read_text().splitlines()]
    assert any(r.get('error') == 'ConnectionResetError' and r.get('command') == 'STATUS' for r in records)
    assert any(r.get('error') == 'TimeoutError' and r.get('command') == 'LOG' for r in records)
    assert any(r.get('response', '').startswith('GBX1 STATUS') for r in records)
