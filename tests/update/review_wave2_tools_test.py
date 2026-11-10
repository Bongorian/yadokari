"""No silent Circle skips, verified Pi boot cache/downloads and UI/doc vocabulary."""
from pathlib import Path
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile
from unittest.mock import patch
root=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(root/'tools/testing'),str(root/'tools/update')]
from circle_source import require_circle
from pi_boot import ensure_file,LOCK
circle=require_circle(root)
with tempfile.TemporaryDirectory() as d:
    temp=Path(d)
    env=dict(os.environ,CIRCLEHOME=str(temp/'missing'))
    for script in ['tests/update/adapters.py','tests/circle_hook_test.py']:
        result=subprocess.run([sys.executable,str(root/script)],env=env,capture_output=True,text=True)
        assert result.returncode!=0 and 'skipped' not in result.stdout and 'scripts/build-firmware.sh' in result.stderr
    # The firmware builder's default placement must work, including a worktree.
    (temp/'.deps').mkdir();(temp/'.deps/circle-multicore-4m').symlink_to(circle.resolve(),target_is_directory=True)
    with patch.dict(os.environ):
        os.environ.pop('CIRCLEHOME',None)
        assert require_circle(temp).resolve()==circle.resolve()
    good=b'pinned boot bytes';digest=hashlib.sha256(good).hexdigest();cache=temp/'cache'
    with patch('pi_boot.urlopen',return_value=io.BytesIO(good)) as fetch:
        target=ensure_file(cache,'boot.bin',digest,'https://unused.invalid');assert target.read_bytes()==good and fetch.call_count==1
    with patch('pi_boot.urlopen',side_effect=AssertionError('cache must be verified without fetching')):
        ensure_file(cache,'boot.bin',digest,'https://unused.invalid')
        target.write_bytes(b'corrupt')
        try:ensure_file(cache,'boot.bin',digest,'https://unused.invalid')
        except ValueError:pass
        else:raise AssertionError('bad cache accepted')
    target.unlink()
    with patch('pi_boot.urlopen',return_value=io.BytesIO(b'corrupt')):
        try:ensure_file(cache,'boot.bin',digest,'https://unused.invalid')
        except ValueError:pass
        else:raise AssertionError('bad download accepted')
    assert not list(cache.iterdir()),'failed download published or temporary file leaked'
    with patch('pi_boot.urlopen',side_effect=OSError('network failed')):
        try:ensure_file(cache,'boot.bin',digest,'https://unused.invalid')
        except OSError:pass
    assert not list(cache.iterdir())
lock=json.loads(LOCK.read_text());assert len(lock['sha256'])==5 and all(len(h)==64 for h in lock['sha256'].values())
script=(root/'scripts/prepare-sd.sh').read_text();assert lock['revision'] in script and 'pi_boot.py' in script and 'curl ' not in script
ui=(root/'docs/ui-design-system.md').read_text();assert all(term not in ui for term in ['STORE SOUND','RECALL SOUND','SOURCE','19 main page types'])
assert 'LIBRARY / BROWSER / ENGINE / PROJECT' in ui and '20 main page types' in ui
for kind,n in [('Synth',10),('Sampler',7),('Kit',4),('Layer',14),('Audio',8),('MIDI',15),('Empty',17)]:assert f'{kind}={n}' in ui
assert 'K4 Sound Slot' not in (root/'docs/ui-pages.md').read_text()
print('Wave2 tools: both absent-Circle gates fail; default checkout found; pinned cache/download corruption rejected and cleaned; UI/docs match current vocabulary/palette passed')
