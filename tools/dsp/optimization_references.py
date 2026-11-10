#!/usr/bin/env python3
"""Record explicit references and the shared dispatch lower bound, not dead-code claims."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--phase',choices=['before','after'],default='before');args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
registry=(root/'src/engine_registry.h').read_text()
engines=re.findall(r'\{\{Instrument::(\w+),"[^"]+"[^\n]*?\},\{"(mk3\.[^"]+)"',registry)
rows=[]
scopes={'factory':['tools/content','src/future_factory_presets.h','src/benchmark_project.h','src/debug_song.h'],'host_tools':['tools','examples'],'tests':['tests'],'runtime':['src','firmware']}
for name,guid in engines:
 row={'engine':name,'guid':guid,'references':{}}
 for scope,paths in scopes.items():
  hits=[]
  for raw in paths:
   p=root/raw
   files=list(p.rglob('*')) if p.is_dir() else [p]
   for f in files:
    if not f.is_file() or f.suffix not in ('.cpp','.h','.inc','.py'):continue
    if f.name in ('engine_registry.h','optimization_audit.cpp','optimization_references.py'):continue
    for n,line in enumerate(f.read_text(errors='replace').splitlines(),1):
     if f'Instrument::{name}' in line or guid in line:hits.append({'file':str(f.relative_to(root)),'line':n})
  unique={(h['file'],h['line']) for h in hits};row['references'][scope]={'lines':len(unique),'locations':[{'file':p,'line':n} for p,n in sorted(unique)]}
 row['content_files']=[]
 for base in ['assets/presets','assets/kits','assets/demo-projects','assets/validation']:
  for f in (root/base).rglob('*'):
   if f.is_file() and f.suffix in ('.mgx','.mgxs','.mgxp','.mgxk') and guid.encode() in f.read_bytes():row['content_files'].append(str(f.relative_to(root)))
 row['reachable']=True # Every descriptor is exposed by SOURCE; absence from content != unreachable.
 rows.append(row)
out=root/'build/optimization-refactor';out.mkdir(parents=True,exist_ok=True)
raw=subprocess.check_output(['nm','-S','--size-sort','-C',str(root/'firmware/kernel8-rpi4.elf')],text=True)
(out/('symbols-'+args.phase+'.txt')).write_text(raw)
# These are named component lower bounds; shared dispatch, helpers and tables
# are accounted separately rather than divided arbitrarily among descriptors.
components={'PolySaw':['groove::PolyBlepSaw::'],'Sampler':['groove::WavVoice::'],'DaisySynth':['groove::DaisySynth::'],'PlaitsMacro':['groove::PlaitsVoice::'],'FM':['daisysp::Fm2::'],'Resonator':['groove::ResonatorVoice::','rings::'],'Texture':['groove::TextureVoice::'],'Wavetable':['groove::WavetableVoice::'],'VirtualAnalog':['groove::VirtualAnalogVoice::','groove::VaModOscillator::'],'Fm6':['groove::Fm6Voice::','msfa::'],'SoundFont':['groove::SoundFontVoice::','tsf_voice_','tsf_render_'],'Additive':['groove::AdditiveVoice::'],'PhaseDist':['groove::PhaseDistVoice::'],'Formant':['groove::FormantVoice::'],'Granular':['groove::GranularVoice::']}
for row in rows:
 hits=[];seen=set()
 for line in raw.splitlines():
  f=line.split()
  if len(f)<4 or f[2] not in ('t','T','W','w'):continue
  name=' '.join(f[3:])
  key=(f[0],f[1])
  if key not in seen and any(token in name for token in components.get(row['engine'],[])):
   seen.add(key);hits.append({'symbol':name,'bytes':int(f[1],16)})
 row['named_component_code_bytes']=sum(h['bytes'] for h in hits);row['component_symbols']=hits
shared=[]
for line in raw.splitlines():
 if any(n in line for n in ['Engine::render_voice','Engine::trigger(','Engine::render_ports','TrackInsertSlot::process(']):
  fields=line.split();shared.append({'symbol':' '.join(fields[3:]),'bytes':int(fields[1],16)})
(out/('references.json' if args.phase=='before' else 'references-after.json')).write_text(json.dumps({'engines':rows,'count':len(rows),'shared_firmware_symbols':shared,'method':'Explicit enum/GUID source-line references; raw GUID presence in content, including embedded presets. No claim of runtime use frequency. SOURCE reaches every descriptor. Generic iteration in tools also reaches all registrations; explicit lines are a lower bound. Shared dispatch symbols cannot be charged independently to each engine.'},indent=2)+'\n')
print('Recorded',len(rows),'descriptors;',len(shared),'shared code symbols')
