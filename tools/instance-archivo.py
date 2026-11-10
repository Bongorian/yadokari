#!/usr/bin/env python3
"""Instance the pinned Archivo variable font into the four static styles
the atlas generator consumes. Generation conditions (SPEC section 5: a
Width-axis choice must be fixed and recorded): wdth pinned to 90, wght
per style (Light 300 / Regular 400 / Semibold 600 / Bold 700), and the
tnum feature frozen into the digit glyphs so tabular figures are the
default (design FONTS.md: Archivo's digit advance variance makes tnum
baking mandatory)."""
from pathlib import Path
import copy
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
root=Path(__file__).resolve().parents[1]
src=root/'.deps/fonts/Archivo-VF.ttf'
STYLES=[('Light',300),('Regular',400),('Semibold',600),('Bold',700)]
WDTH=90

def freeze_tnum(font):
 gsub=font['GSUB'].table
 mapping={}
 for record in gsub.FeatureList.FeatureRecord:
  if record.FeatureTag!='tnum': continue
  for index in record.Feature.LookupListIndex:
   lookup=gsub.LookupList.Lookup[index]
   for sub in lookup.SubTable:
    if hasattr(sub,'mapping'): mapping.update(sub.mapping)
 glyf=font['glyf']; hmtx=font['hmtx']
 for base,variant in mapping.items():
  if base in glyf and variant in glyf:
   glyf[base]=copy.deepcopy(glyf[variant])
   hmtx[base]=hmtx[variant]
 return mapping

for name,wght in STYLES:
 font=TTFont(str(src))
 instantiateVariableFont(font,{'wdth':WDTH,'wght':wght},inplace=True)
 assert 'fvar' not in font, 'axes not fully pinned'
 frozen=freeze_tnum(font)
 out=root/f'.deps/fonts/Archivo-{name}.otf'
 font.save(str(out))
 cmap=font.getBestCmap()
 advances={font['hmtx'][cmap[ord(d)]][0] for d in '0123456789'}
 assert len(advances)==1, (name,advances)
 print(f'Archivo-{name}: wght={wght} wdth={WDTH} tnum frozen ({len(frozen)} glyphs), digit advance {advances}')
