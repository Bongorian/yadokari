#!/usr/bin/env python3
"""Run the frozen Wave 8 DSP fixture once and publish its measured motion."""
from pathlib import Path
import csv, hashlib, json, subprocess, sys, os
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=Path(os.environ.get('MGX_VISUAL_EVIDENCE_ROOT',ROOT/'build/wave8-followup')).resolve()
os.environ['MGX_VISUAL_EVIDENCE_ROOT']=str(EVIDENCE)
RAW=EVIDENCE/'motion'
OUT=ROOT/'docs/manual/motion'
retry='--texture-retry' in sys.argv
publish='--publish-existing' in sys.argv
if publish:
 if retry or not (RAW/'frames.tsv').is_file():raise SystemExit('Publish requires the existing original capture')
elif retry:
 replacement=EVIDENCE/'motion-texture-retry'
 if replacement.exists():raise SystemExit('Texture retry exists; do not repeat qualification')
 with (EVIDENCE/'motion-texture-retry.log').open('w') as log:subprocess.run([str(ROOT/'build/ui-visual-capture'),'texture-spectrum'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
else:
 if RAW.exists():raise SystemExit('Motion evidence exists; preserve it instead of rerunning')
 with (EVIDENCE/'motion-capture.log').open('w') as log:subprocess.run([str(ROOT/'build/ui-visual-capture')],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
OUT.mkdir(parents=True,exist_ok=True)
groups={}
for row in csv.DictReader((RAW/'frames.tsv').open(),delimiter='\t'):groups.setdefault(row['case'],[]).append(row)
if retry:
 for row in csv.DictReader((replacement/'frames.tsv').open(),delimiter='\t'):
  if row['frame']=='0':groups[row['case']]=[]
  groups[row['case']].append(row)
records=[];failures=[]
for name,rows in groups.items():
 assert len(rows)==24
 hashes=[row['hero_hash'] for row in rows]
 # Compare only the declared hero ROI, not header/marquee/time/notice.
 if len(set(hashes))<=1:failures.append([name,'no measured display change'])
 quiet=all(float(row['peak'])<.00001 for row in rows[-4:])
 if quiet and len(set(hashes[-4:]))!=1:failures.append([name,'PCM quiet but hero still changing'])
 if max(float(row['peak']) for row in rows[:12])<=.00001:failures.append([name,'no audible input fixture'])
 source=replacement if retry and name=='texture-spectrum' else RAW
 frames=[Image.open(source/f'{name}-{i:02}.ppm').convert('RGB') for i in range(24)]
 animation=OUT/f'{name}.gif';frames[0].save(animation,save_all=True,append_images=frames[1:],duration=33,loop=0,optimize=False)
 sheet=Image.new('RGB',(960*2,272*2))
 for i,n in enumerate((0,6,12,23)):sheet.paste(frames[n],((i%2)*960,(i//2)*272))
 sheet.save(OUT/f'{name}.png',optimize=True)
 records.append({'case':name,'animation':str(animation.relative_to(OUT.parent)),'contact':f'motion/{name}.png','frames':24,'stop_frame':12,'hero_roi':[20,82,460,242],'side':1,'active_hash_count':len(set(hashes[:12])),'quiet_pcm':quiet,'quiet_last_four_hash_count':len(set(hashes[-4:])),'sha256':hashlib.sha256(animation.read_bytes()).hexdigest()})
(EVIDENCE/'motion-check-errors.json').write_text(json.dumps(failures,indent=2)+'\n')
if failures:raise AssertionError(failures)
manifest=ROOT/'docs/manual/screens.json';data=json.loads(manifest.read_text());data['motion']=records;manifest.write_text(json.dumps(data,indent=2)+'\n')
html=['<!doctype html><meta charset=utf-8><title>Wave 11 measured motion</title><style>body{background:#101017;color:#eee;font:16px sans-serif}img{max-width:100%}a{color:#bca0ff}</style><h1>Actual DSP / Wave 11</h1><p>Host fixture, no physical capture. 24 frames at 33ms, STOP at frame 12. Contact sheet: 0 / 6 / 12 / 23. Source: screens.json.</p>']
for record in records:html.append(f'<h2>{record["case"]}</h2><img src="{record["case"]}.gif"><p><a href="{record["case"]}.png">Contact sheet</a></p>')
(OUT/'index.html').write_text('\n'.join(html)+'\n')
(EVIDENCE/'motion-summary.json').write_text(json.dumps({'cases':records,'checks':'real DSP changes; last four stopped hero ROI hashes identical; headers excluded'},indent=2)+'\n')
print('Seven real DSP motion fixtures passed; raw frames and hashes retained')
