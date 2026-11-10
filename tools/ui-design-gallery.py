#!/usr/bin/env python3
"""Assemble actual framebuffer renders and recorded semantic LED frames."""
from pathlib import Path
import csv,json
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1];out=root/'build/galleries/ui-design-gallery'
small=ImageFont.truetype(str(root/'.deps/fonts/SourceSans3-Semibold.otf'),14)
body=ImageFont.truetype(str(root/'.deps/fonts/SourceSans3-Regular.otf'),18)
colors={'bg':'#111A20','surface':'#22313B','text':'#EAF1EF','secondary':'#9DAEB6','teal':'#40CFB4','amber':'#F0B768','red':'#E46C77','blue':'#70A8DE'}
examples=[('FM','Two-operator FM'),('ACID','Acid bass'),('SAMPLER','WAV / slice playback'),('WAVETABLE','Wavetable terrain'),('MODAL','Modal resonator'),('MIXER','Mixer / live meters'),('MODULATION','LFO / pressure'),('PERFORM','Scene morph'),('CHORDS','Live chord pads'),('CHORD-INVERSION','Chord inversions'),('SCALE-EDITOR','Scale and note mask')]
gallery=Image.new('RGB',(1000,len(examples)*316+52),colors['bg']);d=ImageDraw.Draw(gallery);d.text((20,12),'MASCHINE MK3 / INSTRUMENT VISUAL SYSTEM',font=body,fill=colors['text'])
for i,(mode,title) in enumerate(examples):
 y=50+i*316;d.text((20,y),title,font=body,fill=colors['secondary']);im=Image.open(root/f'build/mk3-ui-{mode}.ppm');gallery.paste(im,(20,y+32))
gallery.save(out/'mk3-design-gallery.png')
# Hardware palette previews are approximate; intensity is explicitly quantized.
rows=list(csv.reader((root/'build/mk3-led-motion.csv').open()));frames=[]
for i,row in enumerate(rows):
 values=list(map(int,row));pads=values[1:17];buttons=values[17:79];strip=values[79:104]
 im=Image.new('RGB',(1238,348),colors['bg']);draw=ImageDraw.Draw(im);draw.text((16,10),'LIVE CORE / LCD + LED STATE',font=small,fill=colors['secondary']);im.paste(Image.open(root/f'build/mk3-ui-MOTION-{i:02}.ppm'),(12,40));draw.text((990,12),'LED palette preview',font=small,fill=colors['secondary'])
 for n,code in enumerate(pads):
  x=990+(n%4)*56;y=54+(n//4)*56;rgb=tuple((values[104+n]>>shift)&255 for shift in [16,8,0]);draw.rounded_rectangle((x,y,x+48,y+48),radius=7,fill=rgb);logical=(3-n//4)*4+n%4+1;draw.text((x+7,y+5),str(logical),font=small,fill=colors['text'])
 for n,level in enumerate(strip):
  x=990+n*9;draw.rounded_rectangle((x,291,x+5,294),radius=1,fill=colors['blue'] if level==3 else '#466171' if level else colors['surface'])
 for n,offset in enumerate([41,43,27]):
  draw.rounded_rectangle((992+n*77,313,1001+n*77,322),radius=2,fill=colors['teal'] if offset==41 and buttons[offset] else colors['amber'] if offset==27 and buttons[offset] else colors['text'] if buttons[offset] else colors['surface']);draw.text((1006+n*77,307),['Play','Stop','FX'][n],font=small,fill=colors['secondary'])
 frames.append(im)
frames[0].save(out/'mk3-live-motion.gif',save_all=True,append_images=frames[1:],duration=50,loop=0,optimize=True)
print('Wrote design gallery and 42-frame shared-core LCD / semantic LED animation')
