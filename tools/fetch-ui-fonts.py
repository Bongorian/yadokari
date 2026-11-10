#!/usr/bin/env python3
"""Fetch only pinned, OFL licensed primary-source font inputs for atlas generation."""
from pathlib import Path
import hashlib,json,urllib.request,urllib.parse
root=Path(__file__).resolve().parents[1]
manifest=root/'third_party/fonts/sources.json'
SPECS={
 'source-sans':{'repo':'adobe-fonts/source-sans','branch':'release','license':'third_party/fonts/source-sans/OFL.txt',
  'files':[('LICENSE.md','third_party/fonts/source-sans/OFL.txt')]+[(f'OTF/SourceSans3-{w}.otf',f'.deps/fonts/SourceSans3-{w}.otf') for w in ['Light','Regular','Semibold','Bold']]},
 'smooch-sans':{'repo':'google/fonts','branch':'main','license':'third_party/fonts/smooch-sans/OFL.txt',
  'files':[('ofl/smoochsans/OFL.txt','third_party/fonts/smooch-sans/OFL.txt'),('ofl/smoochsans/SmoochSans[wght].ttf','.deps/fonts/SmoochSans-VF.ttf')]},
 'barlow-semi-condensed':{'repo':'google/fonts','branch':'main','license':'third_party/fonts/barlow-semi-condensed/OFL.txt',
  'files':[('ofl/barlowsemicondensed/OFL.txt','third_party/fonts/barlow-semi-condensed/OFL.txt')]+[(f'ofl/barlowsemicondensed/BarlowSemiCondensed-{w}.ttf',f'.deps/fonts/BarlowSemiCondensed-{w}.ttf') for w in ['Regular','Medium','SemiBold','Bold']]},
 'archivo':{'repo':'google/fonts','branch':'main','license':'third_party/fonts/archivo/OFL.txt',
  'files':[('ofl/archivo/OFL.txt','third_party/fonts/archivo/OFL.txt'),('ofl/archivo/Archivo[wdth,wght].ttf','.deps/fonts/Archivo-VF.ttf')]},
}
if manifest.exists(): sources=json.loads(manifest.read_text())
else: sources={}
for key,spec in SPECS.items():
 if key!='barlow-semi-condensed':continue
 if key not in sources:
  request=urllib.request.Request(f"https://api.github.com/repos/{spec['repo']}/commits/{spec['branch']}",headers={'User-Agent':'groovebox-font-atlas'})
  sources[key]={'repo':spec['repo'],'revision':json.load(urllib.request.urlopen(request))['sha'],'files':{}}
 sources[key].setdefault('files',{})
for key,source in sources.items():
 source['usage']='ui' if key=='barlow-semi-condensed' else 'historical'
 if key!='barlow-semi-condensed':continue
 spec=SPECS[key]
 for relative,destination in spec['files']:
  path=root/destination;url=f"https://raw.githubusercontent.com/{source['repo']}/{source['revision']}/{urllib.parse.quote(relative)}"
  data=path.read_bytes() if path.exists() else urllib.request.urlopen(url).read()
  digest=hashlib.sha256(data).hexdigest();expected=source['files'].get(relative,{}).get('sha256')
  if expected and expected!=digest:raise SystemExit(f'Checksum mismatch: {path}')
  path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
  source['files'][relative]={'sha256':digest,'url':url,'local':destination}
 license_text=(root/spec['license']).read_text()
 if 'SIL OPEN FONT LICENSE' not in license_text:raise SystemExit('Unexpected font license')
manifest.write_text(json.dumps(sources,indent=2)+'\n')
print('Pinned font inputs verified; OFL notices retained.')
