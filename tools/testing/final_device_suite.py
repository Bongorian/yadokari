#!/usr/bin/env python3
"""Run bounded final workloads sequentially; audio loopback only, no DIN stimulus."""
import argparse,json,subprocess,sys
from pathlib import Path
# FINALTEST must have generated LOADTEST.SF2 before this sequence.
STEPS=[
 ('WAVE4TEST',r'Wave4 draft restored stopped cleaned [01]',240),
 ('NATIVETEST',r'Native workload v1 completed 19 cases',260),
 ('AUDIOTEST',r'Audio workload v1 completed 19 cases',320),
 ('BENCHMARK',r'Benchmark loaded and playback requested',80),
 ('KITSWAP',r'Kit swap probe ok [01]',40),
 ('KITREC',r'Kit record playback hits \d+/\d+',35),
 ('CHOPTEST',r'Chop probe after 2s playing \d audio-valid \d',30),
 ('UITEST',r'UI probe completed',40),
 ('SAVETEST',r'Project save probe completed ok [01]',45),
 ('PRESETCHECK',r'Preset functional check saved mounted \d+/\d+ restored cleaned ok [01]',80),
 ('FXSTRESS',r'FX stress 30 seconds completed',65),
 ('PLSTRESS',None,35),('PLFAST',None,35),('DRUMDEMO',None,25),
 ('BENCHMARK',r'Benchmark loaded and playback requested',80),
 (None,None,120),
 ('SAVETEST',r'Project save probe completed ok [01]',45),
]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--network',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--from-step',type=int,default=0);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);results=[]
 for i,(cmd,marker,seconds) in enumerate(STEPS):
  if i<a.from_step:continue
  path=a.output/f'{i:02d}-{cmd or "CONTINUITY"}.json';argv=[sys.executable,str(Path(__file__).with_name('final_device.py')),'--network',a.network,'--output',str(path),'--seconds',str(seconds)]
  if cmd:argv+=['--command',cmd]
  if marker:argv+=['--until',marker]
  print(i,cmd,flush=True)
  with path.with_suffix('.log').open('w') as log:result=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT)
  results.append({'command':cmd,'seconds':seconds,'exit':result.returncode,'evidence':path.name});(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
  # Timeout/reset leaves state uncertain. Never overlap a following diagnostic.
  if result.returncode:return result.returncode
 return 0
if __name__=='__main__':raise SystemExit(main())
