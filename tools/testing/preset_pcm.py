#!/usr/bin/env python3
"""Render every factory preset in an isolated real-DSP process and compare float PCM hashes."""
import argparse, concurrent.futures, hashlib, json, os, resource, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data', type=Path, default=ROOT/'build/factory-data')
    p.add_argument('--renderer', type=Path, default=ROOT/'build/render-preset')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--compare', type=Path)
    p.add_argument('--jobs', type=int, default=4)
    a = p.parse_args()
    data, renderer = a.data.resolve(), a.renderer.resolve()
    paths = sorted((data/'FACTORY/PRESETS/LIBRARY').rglob('*.mgxp'))
    if len(paths) != 163:
        p.error(f'Expected 163 presets, found {len(paths)}')
    resource.setrlimit(resource.RLIMIT_STACK, (262144*1024, resource.RLIM_INFINITY))
    env = dict(os.environ, UBSAN_OPTIONS='halt_on_error=1')
    def render(path):
        run = subprocess.run([str(renderer), str(data), str(path.relative_to(data))],
                             capture_output=True, text=True, env=env, timeout=180)
        row = {'file': str(path.relative_to(data)), 'file_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
               'exit': run.returncode, 'stderr': run.stderr}
        if run.returncode == 0:
            row.update(json.loads(run.stdout))
        else:
            row['stdout'] = run.stdout
        return row
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        rows = list(pool.map(render, paths))
    report = {'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'renderer_sha256': hashlib.sha256(renderer.read_bytes()).hexdigest(),
              'method': '48000 Hz float32 Main/Cue; 24 frames; note60 velocity100 hold48000/release12000; stereo deterministic two-tone input; real CRC-verified assets; fresh process/seed per preset',
              'presets': rows}
    failed = [r['file'] for r in rows if r['exit']]
    if a.compare:
        before = json.loads(a.compare.read_text())['presets']
        keys = ('file', 'file_sha256', 'exit', 'main_sha256', 'cue_sha256', 'frames', 'rms', 'peak', 'nonzero')
        mismatch = [r['file'] for r, b in zip(rows, before) if any(r.get(k) != b.get(k) for k in keys)]
        if len(rows) != len(before):
            mismatch.append('inventory length differs')
        report['mismatches'] = mismatch
        failed += mismatch
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report, indent=2)+'\n')
    print(f'presets={len(rows)} failures={len(failed)} nonzero={sum(r.get("nonzero",0)>0 for r in rows)}')
    for name in failed:
        print(name)
    return int(bool(failed))
if __name__ == '__main__':
    raise SystemExit(main())
