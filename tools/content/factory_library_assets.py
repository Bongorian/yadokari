#!/usr/bin/env python3
"""Stage the factory library's shared audio for a new host DATA tree."""
import argparse, hashlib, json, os, shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args()
    manifest=json.loads((ROOT/'assets/factory-library/manifest.json').read_text())
    for asset in manifest['assets']:
        source=ROOT/asset['source'] if asset.get('source') else Path(os.environ.get('FACTORY_SOUNDFONT',str(ROOT/asset['path'])))
        if not source.is_file():raise SystemExit(f"Missing factory asset source: {source}; supply FACTORY_SOUNDFONT for FluidR3 GM")
        with source.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
        if digest!=asset['sha256']:raise SystemExit(f'Factory asset hash mismatch: {source}')
        destination=a.output/asset['path'];destination.parent.mkdir(parents=True,exist_ok=True)
        if destination.exists():
            with destination.open('rb') as f:existing=hashlib.file_digest(f,'sha256').hexdigest()
            if existing!=digest:raise SystemExit(f'Existing staged asset differs; preserved {destination}')
        else:shutil.copyfile(source,destination)
    print(f"Factory library shared audio staged: {len(manifest['assets'])} files; source files unchanged")
if __name__=='__main__':main()
