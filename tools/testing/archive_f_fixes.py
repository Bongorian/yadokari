#!/usr/bin/env python3
"""Archive qualification evidence using an explicit non-content file allowlist."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--versions', type=int, nargs='+', required=True)
    p.add_argument('--runs', type=Path, default=Path('build/f-fixes'))
    a = p.parse_args()
    out = a.output
    out.mkdir(parents=True, exist_ok=True)

    def copy(src, dst):
        if src.is_file():
            target = out / dst
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, target)

    for f in sorted(a.runs.rglob('*')):
        if f.suffix in ('.json', '.jsonl', '.log', '.txt', '.stderr') or f.name.endswith('.json.partial'):
            copy(f, Path('runs') / f.relative_to(a.runs))
        elif f.parent.name == 'screens' and f.suffix == '.png':
            copy(f, Path('screens') / f.name)
    for version in a.versions:
        copy(Path(f'build/prepared-update-{version}/manifest.json'), Path('firmware') / f'{version}-manifest.json')
        root = Path(f'build/deploy-{version}')
        for f in sorted(root.rglob('*')):
            if f.suffix in ('.json', '.jsonl', '.log'):
                copy(f, Path('firmware') / str(version) / f.relative_to(root))
    if a.runs == Path('build/f-fixes'):
        for f in sorted(Path('/tmp').glob('f-fixes-*.log')):
            copy(f, Path('extra') / f.name)
    paths = [p for folder in ('src', 'firmware') for p in Path(folder).rglob('*')
             if p.is_file() and p.suffix in ('.cpp', '.h')]
    source = {
        'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'worktree': subprocess.check_output(['git', 'status', '--short'], text=True).splitlines(),
        'runtime_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)},
        'note': 'Per-run manifests and logs identify tested builds. This inventory is the archive-time source, not a claim that every run used this commit.'
    }
    (out / 'source.json').write_text(json.dumps(source, indent=2) + '\n')
    hashes = {str(f.relative_to(out)): hashlib.sha256(f.read_bytes()).hexdigest()
              for f in sorted(out.rglob('*')) if f.is_file() and f.name != 'SHA256SUMS.json'}
    (out / 'SHA256SUMS.json').write_text(json.dumps(hashes, indent=2) + '\n')
    print('Archived', len(hashes), 'evidence files; no firmware binaries, signing keys, songs or audio')


if __name__ == '__main__':
    main()
