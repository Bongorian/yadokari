#!/usr/bin/env python3
"""Generate the one current renderer pack, its type audit and page/theme index.

Build manual-render first. Historical artifacts are never overwritten here.
"""
from pathlib import Path
import csv,json,hashlib,subprocess,shutil,html,re,os
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/manual';OUT.mkdir(exist_ok=True)
previous_pages={row['page'] for row in json.loads((OUT/'screens.json').read_text()).get('frames',[])} if (OUT/'screens.json').exists() else set()
EVIDENCE=Path(os.environ.get('MGX_SCREEN_EVIDENCE_ROOT',ROOT/'build/wave11/screens'));EVIDENCE.mkdir(exist_ok=True)
THEME_NAMES=list(json.loads((ROOT/'docs/reference/themes9.json').read_text()))
THEMES=[name.lower().replace(' ','-') for name in THEME_NAMES]
DEFAULT=THEMES.index('cyber-2')
# Replace only the owned current theme pack; historical captures stay archived.
if (OUT/'themes').exists():shutil.rmtree(OUT/'themes')
manifest={'source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'renderer':'tools/manual_render.cpp','physical_images':False,'default_theme':THEMES[DEFAULT],'themes':THEMES,'frames':[]}
manifest['renderer_source_sha256']=hashlib.sha256((ROOT/'tools/manual_render.cpp').read_bytes()).hexdigest()
manifest['renderer_binary_sha256']=hashlib.sha256((ROOT/'build/manual-render').read_bytes()).hexdigest()
manifest['reused_renderer_outputs']=bool(os.environ.get('MGX_SCREEN_REUSE_RENDER'))
audit={'cap_center_tolerance_px':1,'integer_text_origins':True,'themes':[],'checks':{}}
reference=None;icon_masks=None;alignment_failures=[];grammar_failures=[];grammar_summary=[]
icon_names=re.search(r"enum Id:uint8_t \{([^}]+)\}",(ROOT/"src/ui/icons.h").read_text()).group(1).split(",")[1:-1]
for number,theme in enumerate(THEMES):
 if not os.environ.get('MGX_SCREEN_REUSE_RENDER'):
  with (EVIDENCE/f'render-{theme}.log').open('w') as log:subprocess.run([str(ROOT/'build/manual-render'),'--theme',str(number)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
 else:
  assert (ROOT/f'build/manual-themes/{number}/frames.tsv').exists(),('missing cached renderer fixture',theme)
 source=ROOT/f'build/manual-themes/{number}';target=OUT/'themes'/theme;target.mkdir(parents=True,exist_ok=True)
 frames=list(csv.DictReader((source/'frames.tsv').open(),delimiter='\t'));names={row['frame'] for row in frames};modes={row['mode'] for row in frames}
 assert len(modes)==20,(theme,'main page coverage',modes)
 if reference is None:reference=names
 assert names==reference,(theme,'page set differs')
 for row in frames:
  name=row['frame'];path=target/f'{name}.png'
  if name.startswith("tracker"):assert row["tracker"]=="1",(theme,name,"not TRACKER")
  with Image.open(source/f'{name}.ppm') as picture:assert picture.size==(960,272);picture.save(path,optimize=True)
  manifest['frames'].append({'page':name,'mode':row['mode'],'engine':int(row['engine']),'engine_page':int(row['page']),'type':row['type'],'left':row['left'],'right':row['right'],'title':row['title'],'theme':theme,'file':str(path.relative_to(OUT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
  if number==DEFAULT:shutil.copyfile(path,OUT/path.name);shutil.copyfile(source/f'{name}.ppm',ROOT/f'build/manual-{name}.ppm')
 path=target/'icons.png'
 with Image.open(source/'icons.ppm') as picture:assert picture.size==(960,272);picture.save(path,optimize=True)
 with Image.open(path) as picture:
  background=picture.getpixel((0,0));masks=[]
  for n in range(len(icon_names)+3):
   x=(n%6)*160+10;y=26+(n//6)*30
   mask=tuple(pixel!=background for pixel in picture.crop((x,y,x+14,y+14)).getdata());assert any(mask),(theme,n,'invisible icon');masks.append(mask)
  assert len(set(masks[:len(icon_names)]))==len(icon_names),(theme,'ambiguous monochrome shapes')
  if icon_masks is None:icon_masks=masks
  assert masks==icon_masks,(theme,'icon geometry changed with theme')
 manifest['frames'].append({'page':'icons','mode':'Catalogue','engine':-1,'engine_page':-1,'theme':theme,'file':str(path.relative_to(OUT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
 if number==DEFAULT:shutil.copyfile(path,OUT/path.name);shutil.copyfile(source/'icons.ppm',ROOT/'build/manual-icons.ppm')
 placements=list(csv.DictReader((source/'type-placements.tsv').open(),delimiter='\t'))
 roles=json.loads((ROOT/'assets/fonts/atlas.json').read_text())['fonts'];metrics={i:r for i,r in enumerate(r for r in roles if r['family']=='Barlow Semi Condensed')}
 centered=0;semantic=0;hero_count=0;section_centres=set()
 for row in placements:
  if int(row.get('errors',0)):alignment_failures.append({'theme':theme,**row})
  if int(row.get('kind',0)):semantic+=1
  if int(row.get('kind',0))==5:hero_count+=1
  if int(row.get('kind',0))==4:section_centres.add(2*int(row['band_y'])+int(row['band_h']))
  for key in ('x','y','width','band_y','band_h'):int(row[key])
  if int(row['band_h']):
   face=metrics[int(row['role'])];top=int(row['y'])+(face['line_height']-face['cap_height'])//2;error=2*top+face['cap_height']-(2*int(row['band_y'])+int(row['band_h']));centered+=1
   if abs(error)>2:alignment_failures.append({'theme':theme,'cap_error':error,**row})
 for name in names:
  titles=[r for r in placements if r['frame']==name and int(r['x']) in (10,30) and int(r['role'])==2 and int(r['y'])<64]
  assert {r['side'] for r in titles}=={'0','1'},(theme,name,'missing shared header title')
  assert len({int(r['y']) for r in titles})==1,(theme,name,'left/right title alignment')
 # Compare actual rendered header labels beside a digit-count change.
 a=Image.open(target/'type-099.png');b=Image.open(target/'type-100.png')
 assert ImageChops.difference(a.crop((4,242,116,256)),b.crop((4,242,116,256))).getbbox() is None,(theme,'fixed TEMPO legend moved')
 # Header marquee and clock are independently animated; pixel comparisons exclude y<64.
 audit['themes'].append({'theme':theme,'main_pages':len(modes),'frames':len(frames),'placements':len(placements),'cap_centered_bands':centered,'semantic_fields':semantic,'centred_hero_captions':hero_count,'section_cap_centres_twice':sorted(section_centres),'header_left_right':'same cap-centered y','digit_count_change':'TEMPO legend-name pixels unchanged; header excluded'})
 control_source=source/'controls.tsv'
 if not os.environ.get('MGX_SCREEN_REUSE_RENDER'):shutil.copyfile(ROOT/'build/manual-controls.tsv',control_source)
 assert control_source.exists(),('missing per-theme controls; rerender required',theme)
 controls=list(csv.DictReader(control_source.open(),delimiter='\t'))
 empty=[];knobs={}
 for control in controls:
  kind=control['kind'];label=control['label'];number=int(control['number']);typ=control['type']
  if kind=='button' and label:
   if int(control['label_width'])>107:grammar_failures.append({'theme':theme,'reason':'Top label exceeds native field',**control})
   exception=control.get('placement_exception')=='1'
   if exception and not (typ=='sound' and control['mode']=='SYNTH' and ((number==3 and label=='USE CHILD') or (number==4 and label=='REMOVE'))):grammar_failures.append({'theme':theme,'reason':'undeclared Top placement exception',**control})
   if not exception and typ not in ('selection','transient') and ((number<=4 and control['work']!='selection') or (number>4 and control['work']=='selection')):grammar_failures.append({'theme':theme,'reason':'Top work on wrong LCD',**control})
   if typ in ('selection','settings','diagnostics') and label in ('STORE SOUND','RECALL SOUND'):grammar_failures.append({'theme':theme,'reason':'sound action on management face',**control})
  if kind=='knob':
   key=(control['screen'],control['shift']);knobs[key]=knobs.get(key,0)+bool(label)
   if typ in ('instrument','writing') and number==1 and label!='TEMPO':grammar_failures.append({'theme':theme,'reason':'K1 must be TEMPO',**control})
   if typ=='settings' and number>1 and label:grammar_failures.append({'theme':theme,'reason':'only selected settings item may own K1',**control})
   if label in ('PREVIEW','FAVORITE','PAGE: CORE','CLEAR ZONES','INVERT'):grammar_failures.append({'theme':theme,'reason':'action assigned to a knob',**control})
 for key,count in knobs.items():
  if count==0:empty.append({'screen':key[0],'shift':key[1]})
 grammar_summary.append({'theme':theme,'rows':len(controls),'zero_knob_faces':empty})
 for row in frames:
  match=re.fullmatch(r'engine-(\d+)-page-(\d+)',row['frame'])
  if match and int(match[1])!=int(row['engine']):grammar_failures.append({'theme':theme,'reason':'engine fixture differs from actual mounted engine',**row})
 shutil.copyfile(control_source,EVIDENCE/f'controls-{theme}.tsv')
 shutil.copyfile(source/'type-placements.tsv',EVIDENCE/f'type-{theme}.tsv');shutil.copyfile(source/'frames.tsv',EVIDENCE/f'frames-{theme}.tsv')
(EVIDENCE/'grammar-audit.json').write_text(json.dumps({'themes':grammar_summary,'failures':grammar_failures,'exceptions':['Layer list uses left Top3 USE CHILD / Top4 REMOVE, declared by the shared page descriptor.','Transient confirmation/name/repeat layers use their shared component grammar; selection faces permit left execution.']},indent=2)+'\n')
(EVIDENCE/'alignment-failures.json').write_text(json.dumps(alignment_failures,indent=2)+'\n')
if grammar_failures or alignment_failures:raise AssertionError(f'{len(grammar_failures)} grammar / {len(alignment_failures)} alignment violations; all themes collected before stopping')
# Additional input workflows are emitted by the real controller/renderer test.
# Their file list and control declarations are evidence, never repaint instructions.
input_frames=os.environ.get('MGX_INPUT_FRAMES')
if input_frames:
 source=Path(input_frames);engine_names=re.search(r'enum class Instrument[^\{]*\{([^}]+)\}',(ROOT/'src/instrument_types.h').read_text()).group(1).split(',');va=next(i for i,name in enumerate(engine_names) if name.strip()=='VirtualAnalog')
 fixtures=[('input-step','step'),('input-keyboard','keyboard'),('input-chords','chords'),('step-chord-edit','keyboard'),('kits','browser'),('chords-held','chords')]
 for number,theme in enumerate(THEMES):
  for fixture,(name,base) in enumerate(fixtures):
   row=dict(next(r for r in manifest['frames'] if r['page']==base and r['theme']==theme));target=OUT/'themes'/theme/f'{name}.png';ppm=source/f'theme-{number}-case-{fixture}.ppm'
   with Image.open(ppm) as picture:assert picture.size==(960,272);picture.save(target,optimize=True)
   controls=list(csv.DictReader((source/f'theme-{number}-case-{fixture}-controls.tsv').open(),delimiter='\t'))
   for control in controls:
    if control['kind']!='top' or not control['label'] or row['type'] in ('selection','transient'):continue
    index=int(control['index']);assert (control['work']=='selection') if index<4 else (control['work']!='selection'),(theme,name,control)
   shutil.copyfile(source/f'theme-{number}-case-{fixture}-controls.tsv',EVIDENCE/f'{theme}-{name}-controls.tsv')
   row.update(page=name,engine=va,engine_page=0,tracker=0,file=str(target.relative_to(OUT)),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),fixture='tests/wave12_input_test.cpp')
   if fixture==3:row['right']='selected step chord'
   if fixture==4:row.update(title='KITS',left='user/factory kits',right='kit preview and mount')
   manifest['frames']=[r for r in manifest['frames'] if (r['page'],r['theme'])!=(name,theme)]
   manifest['frames'].append(row)
   if number==DEFAULT:shutil.copyfile(target,OUT/target.name);shutil.copyfile(ppm,ROOT/f'build/manual-{name}.ppm')
 manifest['input_fixture']={'renderer':'tests/wave12_input_test.cpp','states':len(fixtures),'themes':len(THEMES),'physical_images':False,'control_grammar_audited':True}

# Explicit file-transfer states from the real controller/renderer test.
transfer_frames=os.environ.get('MGX_TRANSFER_FRAMES')
if transfer_frames:
 source=Path(transfer_frames)
 for number,theme in enumerate(THEMES):
  for state,name in enumerate(('transfer-ready','transfer-receiving','transfer-error','transfer-usb')):
   ppm=source/f'theme-{number}-transfer-{state}.ppm';target=OUT/'themes'/theme/f'{name}.png'
   with Image.open(ppm) as picture:
    assert picture.size==(960,272)
    picture.save(target,optimize=True)
   manifest['frames'].append({'page':name,'mode':'FILE TRANSFER','engine':-1,'engine_page':-1,'type':'transient','left':'network and physical connection code','right':'address, transfer state and expiry','title':'FILE TRANSFER','theme':theme,'file':str(target.relative_to(OUT)),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'fixture':'tests/ui_grammar_test.cpp'})
   if number==DEFAULT:shutil.copyfile(target,OUT/target.name);shutil.copyfile(ppm,ROOT/f'build/manual-{name}.ppm')
 manifest['transfer_fixture']={'renderer':'tests/ui_grammar_test.cpp','states':4,'themes':len(THEMES),'physical_images':False}

# One source of truth, with a generated human-readable view of it.
assert len(manifest['frames'])==len({(r['page'],r['theme']) for r in manifest['frames']}),'duplicate current page/theme identity'
for obsolete in previous_pages-{row['page'] for row in manifest['frames']}:
 path=OUT/(obsolete+'.png')
 if path.exists():path.unlink()
(OUT/'screens.json').write_text(json.dumps(manifest,indent=2)+'\n')
by_page={}
for row in manifest['frames']:by_page.setdefault(row['page'],{})[row['theme']]=row
parts=['<!doctype html><meta charset="utf-8"><title>Current renderer screens</title><style>body{background:#101017;color:#eee;font:16px sans-serif}a{color:#bca0ff}table{border-collapse:collapse}td,th{padding:8px;border:1px solid #444}img{width:320px;height:auto}</style><h1>Current renderer / Screen grammar</h1><p>Host renderer images; physical LCD colors and controls are unverified. Generated from screens.json.</p><table><tr><th>Page</th>']
parts += [f'<th>{html.escape(t)}</th>' for t in THEME_NAMES];parts+=['</tr>']
for name,rows in sorted(by_page.items()):
 parts += [f'<tr><th>{html.escape(name)}</th>']
 for theme in THEMES:
  path=rows[theme]['file'];parts+=[f'<td><a href="{path}"><img loading="lazy" src="{path}" alt="{html.escape(name)} / {theme}"></a></td>']
 parts+=['</tr>']
parts+=['</table><h2>Motion evidence</h2><p><a href="motion/index.html">Seven actual DSP fixtures / fixed hero ROI / STOP at frame 12</a></p>'];(OUT/'screens.html').write_text('\n'.join(parts)+'\n')
audit['checks']={'all_main_pages_all_themes':True,'semantic_field_alignment':True,'hero_caption_centres':True,'band_cap_center':True,'integer_origins':True,'shared_header_y':True,'numeric_adjacent_labels_stable':True,'all_icon_masks_visible_unique_and_theme_invariant':True};(EVIDENCE/'type-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
shutil.copyfile(ROOT/f'build/manual-themes/{DEFAULT}/controls.tsv',ROOT/'build/manual-controls.tsv')
subprocess.run(['python3','tools/build-manual.py','--publish-assets'],cwd=ROOT,check=True)
print(f'{len(by_page)} page/state frames × {len(THEMES)} themes = {len(manifest["frames"])} current images; type audit passed')

archive=ROOT/'docs/archive/screens/pre-wave11/manual'
comparison=[]
# Runtime engine ordinals may compact; compare the same persistent engine identity.
for before_page,after_page,side in [('engine-22-page-0','engine-18-page-0',1),('engine-05-page-0','engine-03-page-0',1),('engine-06-page-0','engine-04-page-0',1),('engine-23-page-0','engine-19-page-0',1),('engine-26-page-0','engine-22-page-0',1)]+[(p,p,side) for p,side in [('insert-1',1),('settings',0),('browser',0),('mixer',0),('tracker',0),('looper',1),('diagnostics',0)]]:
 before=archive/(before_page+'.png');after=OUT/(after_page+'.png')
 if before.exists() and after.exists():
  roi=(side*480+20,72,side*480+460,242);a=Image.open(before).crop(roi);b=Image.open(after).crop(roi);diff=ImageChops.difference(a,b);comparison.append({'before_page':before_page,'after_page':after_page,'side':side,'roi':roi,'changed':bool(diff.getbbox()),'difference_bbox_within_roi':diff.getbbox()})
(EVIDENCE/'before-after.json').write_text(json.dumps(comparison,indent=2)+'\n')
