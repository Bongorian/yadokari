"""Require a pinned Circle checkout; missing integration inputs are failures."""
import os
from pathlib import Path
import subprocess

PIN = '7a81e1b480fe7d19fc1fc695b89bff0a0848f44b'

def require_circle(root, candidates=None):
    explicit = os.environ.get('CIRCLEHOME')
    paths = [Path(explicit)] if explicit else candidates if candidates is not None else [
        root/'.deps/circle-multicore-4m', root/'.deps/circle-build', root/'.deps/circle', Path('/tmp/mk3-circle')]
    for path in paths:
        probe = subprocess.run(['git','-C',str(path),'rev-parse','--show-toplevel','HEAD'],text=True,capture_output=True)
        lines = probe.stdout.splitlines()
        if probe.returncode == 0 and len(lines)==2 and Path(lines[0]).resolve()==path.resolve() and lines[1]==PIN:
            return path
    raise SystemExit('Pinned Circle integration input missing or wrong revision; run scripts/build-firmware.sh or set CIRCLEHOME to '+PIN+'. Checked: '+', '.join(map(str,paths)))
