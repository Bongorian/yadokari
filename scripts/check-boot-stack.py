#!/usr/bin/env python3
"""Guard the measured startup/reset chain; not a whole-program stack proof."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--nm',required=True);p.add_argument('--elf',required=True);p.add_argument('--output',type=Path);p.add_argument('usage',nargs='+',type=Path);a=p.parse_args()
symbols=subprocess.check_output([a.nm,'-n',a.elf],text=True);limit=None
for line in symbols.splitlines():
 fields=line.split()
 if len(fields)==3 and fields[2]=='groove_kernel_stack_size':limit=int(fields[0],16)
if not limit:raise SystemExit('Missing actual kernel stack size symbol')
rows=[]
for path in a.usage:
 for line in path.read_text().splitlines():
  name,size,kind=line.rsplit('\t',2);rows.append({'function':name,'bytes':int(size),'kind':kind,'source':str(path)})
def exact(label):
 matches=[r for r in rows if r['function'].endswith(label)]
 if not matches:raise SystemExit('Missing stack usage for '+label)
 return max(r['bytes'] for r in matches)
def templates(label):
 matches=[r for r in rows if label in r['function']]
 if not matches:raise SystemExit('Missing stack usage for '+label)
 return max(r['bytes'] for r in matches)
run=exact('CKernel::Run()');storage=exact('CProjectStorage::PrepareContent()');make=exact('make_empty_project(ProjectState&)');reset=templates('reset_to_default(');gate=templates('prepare_factory_content(');install=templates('install_factory_content(')
critical={'Run':run,'PrepareContent':storage,'prepare_factory_content':gate,'install_factory_content':install,'make_empty_project':make,'max_reset_to_default':reset,'library_margin':4096}
# Some helpers are inlined into an ancestor; double-counting remains conservative.
report={'limit':limit,'critical_frames':critical,'conservative_chain_bytes':sum(critical.values()),'scope':'Known boot -> factory -> reset chain; not all call paths or runtime high-water measurement'}
if a.output:a.output.write_text(json.dumps(report,indent=2)+'\n')
if make>4096 or reset>4096 or sum(critical.values())>limit:raise SystemExit('Boot/reset stack budget exceeded: '+json.dumps(report))
print('Boot/reset stack:',sum(critical.values()),'/',limit,'bytes; reset frame',reset,'make_empty frame',make)
