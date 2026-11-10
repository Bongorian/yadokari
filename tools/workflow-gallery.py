#!/usr/bin/env python3
"""Export actual shared-renderer workflow screens, without UI reimplementation."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import subprocess
root=Path(__file__).resolve().parents[1]
if not (root/'.deps/fonts/SourceSans3-Regular.otf').exists():subprocess.run(['python3',str(root/'tools/fetch-ui-fonts.py')],check=True)
out=root/'build/galleries/workflow-gallery';out.mkdir(parents=True,exist_ok=True)
font=ImageFont.truetype(str(root/'.deps/fonts/SourceSans3-Regular.otf'),18)
pages=[('step','Step note / gate / locks'),('step-arp','Step chord arpeggiator'),('automation','Grid-clocked automation'),('macro-map','Macro route / range / scope'),('macros','Eight performance macros'),('midi','External MIDI / CC / clock'),('song-position','MIDI Song Position / stopped locate'),('compressor','Master compressor / sidechain'),('track-compressor','Track insert compressor'),('new-confirm','Project creation / confirmation')]
gallery=Image.new('RGB',(1000,50+len(pages)*310),'#111A20');d=ImageDraw.Draw(gallery);d.text((20,12),'GROOVEBOX / INTEGRATED PLAYING WORKFLOW',font=font,fill='#EAF1EF')
for n,(name,title) in enumerate(pages):
 image=Image.open(root/f'build/workflow-{name}.ppm').convert('RGB');image.save(out/f'workflow-{name}.png');y=50+n*310;d.text((20,y),title,font=font,fill='#9DAEB6');gallery.paste(image,(20,y+27))
gallery.save(out/'workflow-gallery.png')
print('Workflow UI screens and gallery exported')
