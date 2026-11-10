#!/usr/bin/env python3
"""Render the LED reference from real PadPalette test output and wire profile."""
import hashlib, json, re, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
data=json.loads(Path(sys.argv[1]).read_text())
profile=(root/'src/mk3_led_transport.h').read_text()
colors=re.search(r'native\[\]\s*=\s*\{([^}]+)',profile).group(1)
colors=[int(value,16) for value in re.findall(r'0x[0-9a-f]+',colors)]
reference=data['themes'][0]
for theme in data['themes']:
 assert [r['wire'] for r in theme['ground']]==[r['wire'] for r in reference['ground']]
 assert theme['events']==reference['events']
parts=["<svg xmlns='http://www.w3.org/2000/svg' width='1000' height='650' viewBox='0 0 1000 650'>",
 "<rect width='1000' height='650' fill='#090a0d'/><g font-family='sans-serif' fill='#dce9f5'>",
 "<text x='28' y='38' font-size='24'>FIXED PAD COLORS / HOST WIRE PREVIEW</text>",
 "<text x='28' y='64' font-size='13'>Same wire colors in all nine themes. Physical hue and brightness remain uncalibrated.</text>"]
for x,text in ((28,'OBJECT'),(300,'MUTED / LOW'),(530,'READY / MEDIUM'),(770,'SELECTED / WHITE')):
 parts.append(f"<text x='{x}' y='101' font-size='14'>{text}</text>")
for n,name in enumerate(('SYNTH','SAMPLER','KIT','LAYER','AUDIO / LOOPER','MIDI','EMPTY','NEUTRAL')):
 row=reference['ground'][n];y=128+n*46
 parts.append(f"<text x='28' y='{y+19}' font-size='15'>{name}</text>")
 for col,color in enumerate(row['rgb']):
  code=row['wire'][col];x=300+col*230
  parts.append(f"<rect x='{x}' y='{y}' width='38' height='28' rx='3' fill='#{color[-6:]}'/><text x='{x+49}' y='{y+19}' font-size='13'>token {code>>2} / level {code & 3}</text>")
for n,(name,code) in enumerate(zip(('TRIGGER / PLAY','ARM / RECORD','QUEUE / WARNING'),reference['events'])):
 x=28+n*320;rgb=colors[code>>2]&0xffffff
 parts.append(f"<rect x='{x}' y='525' width='38' height='28' rx='3' fill='#{rgb:06x}'/><text x='{x+49}' y='544' font-size='14'>{name}</text>")
parts.append("<text x='28' y='590' font-size='14'>ROOT / TONIC: white, level 3. Scale notes: object color, level 2. Events take priority.</text>")
parts.append("<text x='28' y='620' font-size='13'>Low/Medium device brightness caps levels. No theme-to-pad RGB quantization. Strip behavior unchanged.</text></g></svg>")
(root/'docs/images/ui-led-legend.svg').write_text('\n'.join(parts)+'\n')
print('Fixed LED legend: nine-theme wire equality verified; profile SHA256',hashlib.sha256(profile.encode()).hexdigest())
