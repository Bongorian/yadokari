#!/usr/bin/env python3
"""Export actual shared-core framebuffer pixels at native resolution."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
names=['wave-sequence','chip-clock','tape-micro','west-coast','spectral-freeze','step16','step64','gesture','lab']
out=Path('build/galleries/creative-gallery');out.mkdir(parents=True,exist_ok=True)
gallery=Image.new('RGB',(1000,316*len(names)), '#111a20');d=ImageDraw.Draw(gallery)
for i,name in enumerate(names):
 im=Image.open(f'build/creative-{name}.ppm');assert im.size==(960,272)
 im.save(out/f'creative-{name}.png');d.text((20,i*316+9),name.replace('-',' ').upper(),fill='#eaf1ef');gallery.paste(im,(20,i*316+34))
gallery.save(out/'creative-gallery.png')
print('Native 480x272x2 creative UI frames exported')
