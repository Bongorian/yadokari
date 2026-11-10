#!/usr/bin/env python3
"""Package current documentation: real rendered PNGs, schematic panel, offline HTML.

Run after tools/manual_render.cpp. Requires Pillow and markdown-it-py.
Does not read/write the physical device or user projects.
"""
from pathlib import Path
import argparse
import base64
import hashlib
import html
import json
import re
from PIL import Image
from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pdf', action='store_true', help='Also render a PDF (requires WeasyPrint)')
parser.add_argument('--publish-assets', action='store_true', help='Update tracked docs/manual assets explicitly')
args = parser.parse_args()
EXPORT = ROOT / 'build/manual'
EXPORT.mkdir(parents=True, exist_ok=True)
OUT = ROOT / ('docs/manual' if args.publish_assets else 'build/manual')
OUT.mkdir(exist_ok=True)
screens = []
index=ROOT/'docs/manual/screens.json'
# Manual packaging consumes the canonical pack; stale build PPMs never re-enter it.
if index.exists():
    pack=json.loads(index.read_text());names={Path(row['file']).name.removesuffix('.png') for row in pack['frames'] if row['theme']==pack['default_theme']}
    sources=[ROOT/'build'/f'manual-{name}.ppm' for name in sorted(names)]
else:
    sources=sorted((ROOT/'build').glob('manual-*.ppm'))
for source in sources:
    target = OUT / (source.stem.removeprefix('manual-') + '.png')
    with Image.open(source) as picture:
        assert picture.size == (960, 272), source
        picture.save(target, optimize=True)
    screens.append({'file': target.name, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
if not screens:
    raise SystemExit('Run build/manual-render first')
control_rows=[]
for line in (ROOT / 'build/manual-controls.tsv').read_text().splitlines():
    fields=line.split('\t')
    # Performance exposes the active dialog controls as well as page controls.
    if not fields[-1]:
        fields[-1]='n/a'
    control_rows.append('\t'.join(fields))
(OUT / 'controls.tsv').write_text('\n'.join(control_rows)+'\n')
(OUT / 'synth-control-audit.tsv').write_bytes((ROOT / 'build/synth-control-audit.tsv').read_bytes())
(OUT / 'review-probes.txt').write_bytes((ROOT / 'build/manual-review-probes.txt').read_bytes())

# Panel map: original vector diagram; screenshots embedded without UI repainting.
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="1230" viewBox="0 0 1440 1230">',
       '<rect width="1440" height="1230" fill="#111a23"/>',
       '<style>text{font-family:Arial,sans-serif;fill:#edf5f9;font-size:16px}.hint{fill:#9bb4c5;font-size:13px}.accent{fill:#65dfc4;font-size:22px}</style>']
def label(x, y, text, css=''):
    svg.append(f'<text x="{x}" y="{y}" class="{css}">{html.escape(text)}</text>')
def button(x, y, w, text, hint=''):
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="47" rx="7" fill="#263746" stroke="#597082"/>')
    label(x+9, y+20, text)
    if hint:
        label(x+9, y+37, hint, 'hint')
label(30, 35, 'MK3 / PANEL GUIDE', 'accent')
label(30, 59, 'Schematic placement / actual shared-renderer screens / not a hardware photograph', 'hint')
left = [('CHANNEL','ROUTING / FX'),('PLUG-IN','SYNTH / ENGINE'),('ARRANGER','SONG'),('MIXER','MIXER'),('BROWSER','FILES / SAMPLES'),('SAMPLING','SAMPLER'),('PAGE LEFT','BACK / HOME'),('PAGE RIGHT','FORWARD / RETURN'),('FILE','SHIFT: SAVE'),('SETTINGS','SETTINGS'),('AUTO','MODULATION'),('MACRO','MACROS')]
for i, (name, hint) in enumerate(left):
    button(30+(i%2)*155, 90+(i//2)*57, 145, name, hint)
for i in range(8):
    button(380+i*126, 90, 117, f'Top{i+1}')
screen = base64.b64encode((OUT/'step.png').read_bytes()).decode()
svg.append(f'<image x="380" y="150" width="1000" height="283" href="data:image/png;base64,{screen}"/>')
for i in range(8):
    x = 439+i*125
    svg.append(f'<circle cx="{x}" cy="475" r="27" fill="#30495b" stroke="#86a6b8"/>')
    label(x-12, 520, f'K{i+1}')
label(380, 553, 'Top: icon + action / K: name + current value; active cell adds arc', 'hint')
svg.append('<circle cx="120" cy="575" r="64" fill="#30495b" stroke="#86a6b8"/>')
label(77,575,'4-D PUSH'); label(58,598,'TURN / UP / DOWN','hint')
for i, name in enumerate(['VOLUME','SWING','TEMPO']):
    button(210, 510+i*56, 125, name, 'HOLD + 4-D')
button(365,580,175,'NOTE REPEAT','SHIFT: ARP'); button(365,638,175,'LOCK')
for i, name in enumerate(['PITCH','MOD','PERFORM','NOTES']):
    button(30+i*132,690,123,name)
svg.append('<rect x="30" y="755" width="520" height="35" rx="6" fill="#080e14" stroke="#597082"/>')
label(30,816,'SMART STRIP: PITCH / MOD / NOTES / PERFORM (header symbol)','hint')
for i, name in enumerate('ABCDEFGH'):
    button(30+(i%4)*132,845+(i//4)*57,123,name,f'T{i+1} / T{i+9} STICKY')
for i, name in enumerate(['RESTART','ERASE','TAP','FOLLOW','PLAY','REC','STOP','SHIFT']):
    button(30+(i%4)*132,1010+(i//4)*59,123,name, 'SHIFT: NEXT BAR' if name=='RESTART' else 'TOUCH: RESET' if name=='ERASE' else '')
button(578,580,135,'FIXED VEL')
for i,name in enumerate(['PAD MODE','KEYBOARD','CHORDS','STEP']):
    button(755+i*157,580,146,name,'TAP: VIEW / HOLD' if name=='STEP' else '')
for i,name in enumerate(['SCENE','PATTERN','EVENTS','VARIATION','DUPLICATE','SELECT','SOLO','MUTE']):
    button(578,645+i*59,135,name,'UNDO / SH: REDO' if name=='EVENTS' else '')
for row in range(4):
    for col in range(4):
        n=(3-row)*4+col+1;x=755+col*157;y=655+row*118
        svg.append(f'<rect x="{x}" y="{y}" width="146" height="105" rx="10" fill="#276d68" stroke="#74bead"/>')
        label(x+20,y+42,f'Pad{n}')
label(30,1195,'Pad1 = bottom left / Pad16 = top right. A-H are tracks, not the NI Group hierarchy.','hint')
svg.append('</svg>')
(OUT/'panel.svg').write_text('\n'.join(svg))

source = ROOT/'docs/user-manual.ja.md'
md = MarkdownIt('commonmark', {'html':True}).enable('table')
body = md.render(source.read_text())
toc=[]
def heading(m):
    number=len(toc)+1; anchor=f'section-{number}'
    toc.append(f'<a href="#{anchor}">{m.group(1)}</a>')
    return f'<h2 id="{anchor}">{m.group(1)}</h2>'
body = re.sub(r'<h2>(.*?)</h2>',heading,body)
def embed(m):
    path=(OUT / Path(m.group(1)).name) if m.group(1).startswith('manual/') else source.parent/m.group(1)
    if not path.is_file():
        raise RuntimeError(f'Missing image: {path}')
    mime='image/svg+xml' if path.suffix=='.svg' else 'image/png'
    return 'src="data:'+mime+';base64,'+base64.b64encode(path.read_bytes()).decode()+'"'
body=re.sub(r'src="([^":]+)"',embed,body)
def figures(m):
    return re.sub(r'(<img [^>]*alt="([^"]*)"[^>]*>)', lambda image: '<figure>'+image.group(1)+'<figcaption>'+image.group(2)+'</figcaption></figure>', m.group(1))
body=re.sub(r'<p>((?:<img[^>]+>\s*)+)</p>', figures, body)
# Keep ancillary references useful when the standalone HTML is moved elsewhere.
def absolute_link(m):
    target=m.group(1)
    if target.startswith(('https:', 'http:', '#', 'data:')):
        return m.group(0)
    if target == 'user-manual.ja.html':
        return 'href="#"'
    return 'href="https://github.com/Bongorian/maschine-mk3-bare-metal-groovebox/blob/master/docs/'+html.escape(target,quote=True)+'"'
body=re.sub(r'href="([^"]+)"',absolute_link,body)
css='''
:root{color-scheme:light;--ink:#182f40;--accent:#08786f}*{box-sizing:border-box}
body{margin:0;background:#eef2f4;color:var(--ink);font:16px/1.85 system-ui,-apple-system,"Noto Sans CJK JP",sans-serif}
nav{position:fixed;inset:0 auto 0 0;width:265px;padding:24px 18px;background:#152c3c;color:white;overflow:auto}
nav strong{display:block;font-size:20px;margin-bottom:16px}nav a{display:block;color:#d2eeea;text-decoration:none;font-size:13px;padding:6px 0;border-bottom:1px solid #31505e}
main{margin-left:265px;max-width:1260px;padding:38px 46px 90px;background:white;min-height:100vh}
h1{font-size:32px;line-height:1.45;margin:0 0 20px}h2{margin:64px 0 20px;border-top:4px solid var(--accent);padding-top:18px;font-size:25px;scroll-margin-top:20px}h3{margin-top:34px;font-size:20px}
a{color:#076f85;text-underline-offset:3px}strong{color:#123d51}img{display:block;width:100%;height:auto;margin:24px 0;border:1px solid #d3dfe4;border-radius:6px}
figure{margin:26px 0}figure img{margin:0}figcaption{font-size:13px;color:#526a77;margin-top:7px}
table{border-collapse:collapse;table-layout:fixed;width:100%;font-size:14px;margin:22px 0;overflow-wrap:anywhere}th{background:#e4f1ef;text-align:left}th,td{padding:10px 12px;border:1px solid #cbd9df;vertical-align:top}th:first-child,td:first-child{width:22%}tr:nth-child(even){background:#f7fafb}
li{margin:10px 0}code{font-size:.91em;background:#eef4f6;padding:2px 4px;border-radius:3px}pre{padding:14px;background:#eef4f6;overflow:auto}.toolbar{font-size:13px;color:#586f7b;margin-bottom:22px}
@media screen and (max-width:1000px){nav{position:static;width:auto}nav a{display:inline-block;padding:5px 10px}main{margin:0;padding:24px 18px}table{font-size:12px}th,td{padding:6px}}
@media print{@page{size:A4;margin:15mm;@bottom-right{content:"MK3 / " counter(page);font-size:8pt;color:#657985}}body{background:white;font-size:10pt;line-height:1.65}nav,.toolbar{display:none}main{margin:0;padding:0;max-width:none}h1{font-size:23pt}h2{break-before:page;font-size:18pt;margin:0 0 15px}h3{break-after:avoid;font-size:13pt}figure{break-inside:avoid}img{max-height:190mm;object-fit:contain;break-inside:avoid}tr{break-inside:avoid}table{font-size:8pt}th,td{padding:5px}a{color:inherit}pre{white-space:pre-wrap}}
'''
document='<!doctype html><html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>MK3 Groovebox 取扱説明書</title><style>'+css+'</style></head><body><nav aria-label="目次"><strong>MK3 GROOVEBOX<br>操作ガイド</strong>'+''.join(toc)+'</nav><main><div class="toolbar">画像はすべて内包。オフライン閲覧可能。ブラウザーの印刷からPDFへ保存できます。</div>'+body+'</main></body></html>'
(EXPORT/'user-manual.html').write_text(document)
(OUT/'manifest.json').write_text(json.dumps({'renderer_source_sha256':hashlib.sha256((ROOT/'tools/manual_render.cpp').read_bytes()).hexdigest(), 'physical_capture':False,'status':'current shared renderer; page/theme index is screens.json','generator':'tools/manual_render.cpp','scope':'Host shared renderer + DSP fixture; no physical capture. Browser listing is a documentation fixture. Panel is an original schematic.','screens':screens},indent=2)+'\n')
print(f'{len(screens)} real renderer screens; standalone HTML {(len(document.encode())/1024):.0f} KiB')
if args.pdf:
    from weasyprint import HTML
    HTML(string=document, base_url=str(source.parent)).write_pdf(EXPORT/'user-manual.pdf')
    print('Printable PDF generated')
