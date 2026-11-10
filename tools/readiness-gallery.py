#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
out=Path('build/galleries/readiness-gallery');out.mkdir(parents=True,exist_ok=True)
names=['browser','sampler-trim','sampler-slices','routing','diagnostics','factory']
gallery=Image.new('RGB',(960,272*len(names)))
for i,name in enumerate(names):
    image=Image.open(f'build/readiness-{name}.ppm')
    image.save(out/f'readiness-{name}.png');gallery.paste(image,(0,i*272))
gallery.save(out/'readiness-gallery.png')
