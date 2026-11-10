#!/usr/bin/env python3
"""Archive final-test evidence, never firmware keys, projects or audio assets."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=True)
 def copy(src,dst):
  if not src.is_file():return
  target=out/dst;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,target)
 for f in Path('build/final-test').rglob('*'):
  if f.suffix in ('.json','.jsonl','.log','.txt'):copy(f,Path('runs')/f.relative_to('build/final-test'))
 for f in Path('build/final-test').glob('ui-*.png'):copy(f,Path('screens')/f.name)
 for version in (429,431,433,434,435):
  copy(Path(f'build/prepared-update-{version}/manifest.json'),Path('firmware')/f'{version}-manifest.json')
  for f in Path(f'build/deploy-{version}').rglob('*'):
   if f.suffix in ('.json','.jsonl','.log'):copy(f,Path('firmware')/str(version)/f.relative_to(f'build/deploy-{version}'))
 for name in ('backend-audio','backend-audio-retry','backend-default-syntax','demos','demos-current','creative-corrected','readiness','update','tsan','extra-tests','async-retry','standalone','hardware-tool','sf2-host','factory-check','prepare-435','deploy-435','docs-check','docs-check-final'):
  copy(Path('/tmp')/f'final-{name}.log',Path('extra')/(name+'.log'))
 runtime={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for folder in ('src','firmware') for f in sorted(Path(folder).rglob('*')) if f.is_file() and f.suffix in ('.h','.cpp')}
 source={'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'worktree':subprocess.check_output(['git','status','--short'],text=True).splitlines(),'runtime_sha256':runtime,'workloads':{}}
 for name in ('src/benchmark_project.h','src/native_stress_project.h','src/wave4_workload.h','src/audio_stress_assets.h','tools/update/wave4_workload.json','firmware/load_sf2_generator.h','tools/content/generate-load-sf2.py'):
  f=Path(name)
  if f.exists():source['workloads'][name]=hashlib.sha256(f.read_bytes()).hexdigest()
 (out/'source.json').write_text(json.dumps(source,indent=2)+'\n')
 hashes={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(out.rglob('*')) if f.is_file() and f.name!='SHA256SUMS.json'}
 (out/'SHA256SUMS.json').write_text(json.dumps(hashes,indent=2)+'\n');print('Archived',len(hashes),'evidence files; no project/audio/firmware/key files; selected host screenshots only')
if __name__=='__main__':main()
