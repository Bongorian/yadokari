#!/usr/bin/env python3
"""Pinned UI font instances and a pre-raster capacity estimate."""
from pathlib import Path
import copy,json,math,argparse
from fontTools.ttLib import TTFont
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
STYLES=[('Small',12,600,'SemiBold',14),('Body',14,500,'Medium',18),('Heading',20,600,'SemiBold',24),('Number',24,400,'Regular',28),('Bold',14,700,'Bold',18),('Knob',20,600,'SemiBold',24)]
TEXT=sorted(set(range(32,127))|{0xb0,0xd7,0x2013,0x2022,0x2026,0x2212})
def instances():
 for style in sorted({s[3] for s in STYLES}):
  font=TTFont(ROOT/f'.deps/fonts/BarlowSemiCondensed-{style}.ttf');mapping={}
  for feature in font['GSUB'].table.FeatureList.FeatureRecord:
   if feature.FeatureTag=='tnum':
    for index in feature.Feature.LookupListIndex:
     for table in font['GSUB'].table.LookupList.Lookup[index].SubTable:mapping.update(getattr(table,'mapping',{}))
  if not mapping:raise ValueError('tnum substitutions missing')
  for base,variant in mapping.items():
   font['glyf'][base]=copy.deepcopy(font['glyf'][variant]);font['hmtx'][base]=font['hmtx'][variant]
  cmap=font.getBestCmap();advances={font['hmtx'][cmap[ord(d)]][0] for d in '0123456789'}
  assert len(advances)==1,(style,advances)
  font.recalcTimestamp=False;font['head'].modified=font['head'].created;font.save(ROOT/f'.deps/fonts/BarlowTnum-{style}.ttf',reorderTables=True)
  print(f'Barlow {style}: tnum frozen; advance {next(iter(advances))}')
def faces():
 for role,size,weight,nweight,line in STYLES:
  yield role,False,size,line,ROOT/f'.deps/fonts/BarlowTnum-{nweight}.ttf',TEXT
def estimate():
 rows=[]
 for role,numeric,size,line,path,chars in faces():
  face=TTFont(path);missing=[hex(cp) for cp in chars if cp not in face.getBestCmap()]
  if missing:raise ValueError(f'{path.name}: missing {missing}; do not substitute fonts')
  font=ImageFont.truetype(str(path),size*4);coverage=0
  for cp in chars:
   l,t,r,b=font.getbbox(chr(cp),anchor='ls');coverage+=(max(0,math.ceil(r/4)-math.floor(l/4))*max(0,math.ceil(b/4)-math.floor(t/4))+1)//2
  rows.append({'role':role,'numeric':numeric,'coverage_bytes':coverage,'glyph_metadata_bytes':len(chars)*16})
 result={'coverage_bytes':sum(r['coverage_bytes'] for r in rows),'glyph_metadata_bytes':sum(r['glyph_metadata_bytes'] for r in rows),'kerning_upper_bound_bytes':6*95*95*4,'faces':rows}
 print(json.dumps(result,indent=2));return result
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--estimate',action='store_true');args=parser.parse_args();instances()
 if args.estimate:
  report=estimate();target=ROOT/'build/wave7/font-estimate.json';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n')
