#!/usr/bin/env python3
"""Inventory every test-run target, build shared prerequisites once, then run in isolation."""
import argparse,hashlib,json,os,re,signal,subprocess,time
from concurrent.futures import ThreadPoolExecutor,as_completed
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def command(argv,path,timeout):
 started=time.monotonic()
 with path.open('w') as log:
  proc=subprocess.Popen(argv,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  try:code=proc.wait(timeout=timeout)
  except subprocess.TimeoutExpired:
   os.killpg(proc.pid,signal.SIGTERM)
   try:proc.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
   code=124
 return {'exit':code,'seconds':round(time.monotonic()-started,3),'log':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--jobs',type=int,default=4);p.add_argument('--timeout',type=int,default=900);p.add_argument('--run-jobs',type=int,default=4);p.add_argument('--targets',nargs='*');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 make=(ROOT/'Makefile').read_text();inventory={name:deps.split() for name,deps in re.findall(r'^(test-run-[^ :]+):([^\n]*)',make,re.M)}
 targets=a.targets or list(inventory)
 if any(t not in inventory for t in targets):p.error('Unknown target')
 report={'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'inventory':inventory,'selected':targets,'timeout_per_target_s':a.timeout,'results':[]}
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 save();deps=sorted({d for t in targets for d in inventory[t]})
 report['build']=command(['make','--output-sync=target','-k',f'-j{a.jobs}',*deps],out/'build.log',7200);save();print('Build',report['build'],flush=True)
 def run(target):
  return {'target':target,**command(['make','test','TEST_JOBS=1','TEST_TARGETS='+target],out/(target+'.log'),a.timeout)}
 # Makefile already isolates test working directories for its parallel suite.
 with ThreadPoolExecutor(max_workers=a.run_jobs) as pool:
  for future in as_completed([pool.submit(run,target) for target in targets]):
   result=future.result();report['results'].append(result);save();print(result['target'],result['exit'],result['seconds'],flush=True)
 print('RESULTS',len(report['results']),'failed',sum(r['exit']!=0 for r in report['results']),flush=True)
 return int(any(r['exit']!=0 for r in report['results']))
if __name__=='__main__':raise SystemExit(main())
