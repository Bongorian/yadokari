#!/usr/bin/env python3
"""Publish default-theme frames and controls from the current manual renderer."""
import csv
import hashlib
import html
import json
import shutil
import subprocess
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
output = root / 'docs/manual'
themes = list(json.loads((root / 'docs/reference/themes9.json').read_text()))
index = themes.index('CYBER 2')
source = root / 'build/manual-themes' / str(index)
subprocess.run([root / 'build/manual-render', '--theme', str(index)], cwd=root, check=True)
with (source / 'frames.tsv').open() as stream:
    rows = list(csv.DictReader(stream, delimiter='\t'))
frames = []
for row in rows + [dict(frame='icons', mode='CATALOG', engine=-1, page=0, title='Icons')]:
    name = row['frame']
    target = output / (name + '.png')
    with Image.open(source / (name + '.ppm')) as img:
        img.save(target)
    frames.append(dict(page=name, mode=row['mode'], engine=int(row['engine']), engine_page=int(row['page']),
                       title=row['title'], theme='cyber-2', file=target.name,
                       sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
if len({frame['page'] for frame in frames}) != len(frames):
    raise ValueError('duplicate renderer frame names')
listing = {'renderer':'tools/manual_render.cpp','physical_images':False,'default_theme':'cyber-2',
           'themes':['cyber-2'],'frames':frames}
(output/'screens.json').write_text(json.dumps(listing, indent=2)+'\n')
links = [f'<li><a href="{html.escape(f["file"])}">{html.escape(f["page"])}</a> · {html.escape(f["title"])}</li>' for f in frames]
(output/'screens.html').write_text('<!doctype html><meta charset="utf-8"><title>Screen index</title><h1>Screen index · CYBER 2</h1><ul>\n'+'\n'.join(links)+'\n</ul>\n')
shutil.copyfile(root/'build/manual-controls.tsv', output/'controls.tsv')
# Both current indexes describe exactly the PNG bytes published above.
manifest_path = output/'manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['renderer_source_sha256'] = hashlib.sha256((root/'tools/manual_render.cpp').read_bytes()).hexdigest()
manifest['screens'] = sorted(({'file': f['file'], 'sha256': f['sha256']} for f in frames), key=lambda f: f['file'])
manifest_path.write_text(json.dumps(manifest, indent=2)+'\n')
print(f'{len(frames)} indexed images and matching controls')
