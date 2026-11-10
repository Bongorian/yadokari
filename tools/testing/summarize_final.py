#!/usr/bin/env python3
"""Summarize raw final device evidence. Empty windows never qualify."""
import argparse,json,re
from pathlib import Path

def rows(raw,pattern,keys):
 return [dict(zip(keys,map(int,m.groups()))) for m in re.finditer(pattern,raw)]
def summarize(path):
 x=json.loads(path.read_text());raw=x.get('raw_log','')
 workers=rows(raw,r'Native case (\d+) worker blocks (\d+) avg-us (\d+) max-us (\d+) over500 (\d+) missing-frames (\d+)',('case','blocks','mean_us','max_us','over500','missing_frames'))
 rings=rows(raw,r'Native case (\d+) callbacks (\d+) avg-us (\d+) max-us (\d+) over125 (\d+) over250 (\d+) bad (\d+) underrun (\d+) overrun (\d+)',('case','callbacks','mean_us','max_us','over125','over250','bad','underrun','overrun'))
 streams=rows(raw,r'Audio case (\d+) stream-misses (\d+) recording-failed (\d+)',('case','stream_misses','recording_failed'))
 ui=rows(raw,r'Native case (\d+) UI window-us (\d+) frames (\d+)/(\d+) gap-us (\d+)/(\d+) over100ms (\d+)/(\d+) discarded (\d+)/(\d+) input-service-gap-us (\d+) over-frame (\d+)',('case','window_us','left_frames','right_frames','left_gap_us','right_gap_us','left_over100ms','right_over100ms','left_discarded','right_discarded','input_service_gap_us','input_over_frame'))
 for u in ui:
  u['valid_window']=u['window_us']>=9_900_000
  u['left_fps']=round(u['left_frames']*1e6/u['window_us'],4) if u['window_us'] else 0
  u['right_fps']=round(u['right_frames']*1e6/u['window_us'],4) if u['window_us'] else 0
  u['qualified']=u['valid_window'] and min(u['left_fps'],u['right_fps'])>=29 and not (u['left_over100ms'] or u['right_over100ms'])
 admission=rows(raw,r'Native admission active-mask (\d+) refused-mask (\d+) average-budget-us (\d+) burst-budget-us (\d+)',('active_mask','refused_mask','average_budget_us','burst_budget_us'))
 for w in workers:
  ring=next((r for r in rings if r['case']==w['case']),{})
  stream=next((r for r in streams if r['case']==w['case']),{})
  w['valid_window']=bool(w['blocks'] and ring.get('callbacks') and (x.get('command')!='AUDIOTEST' or stream))
  w['continuous']=w['valid_window'] and not any([w['missing_frames'],ring.get('bad',1),ring.get('underrun',1),ring.get('overrun',1),stream.get('stream_misses',0),stream.get('recording_failed',0)])
 recorded=rows(raw,r'Sampler record saved (\d+) verified (\d+) assigned (\d+) frames (\d+) slot (\d+) peak-uv (\d+)/(\d+) input-drops (\d+)',('saved','verified','assigned','frames','slot','peak_l_u','peak_r_u','input_drops'))
 display=rows(raw,r'Display frames window-us (\d+) left (\d+) right (\d+) gap-us (\d+)/(\d+) over100ms (\d+)/(\d+) discarded (\d+)/(\d+) record (\d+)',('window_us','left_frames','right_frames','left_gap_us','right_gap_us','left_over100ms','right_over100ms','left_discarded','right_discarded','recording'))
 fx=rows(raw,r'FX worker blocks (\d+) avg-us (\d+) max-us (\d+) over500 (\d+) missing-frames (\d+)',('blocks','mean_us','max_us','over500','missing_frames'))
 count=re.search(r'Final device completed passed (\d+) failed (\d+) restored (\d+)',raw)
 return {'evidence':str(path),'command':x.get('command'),'version':x.get('before',{}).get('version'),'completion_observed':x.get('matched'),'reset_observed':x.get('reset_observed'),'continuity_status_available':bool(x.get('hardware_before') and x.get('hardware_after')),'communication_errors':len(x.get('errors',[])),'display_windows':display,'fx_workers':fx,'ui_windows':ui,'admission_in_case_order':admission,'workers':workers,'rings':rings,'streams':streams,'recordings':recorded,'final_checks':dict(zip(('passed','failed','restored'),map(int,count.groups()))) if count else None,'failed_checks':re.findall(r'Final check ([^\n]+) ok 0',raw)}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=[]
 for path in sorted(a.directory.rglob('*.json')):
  if path==a.output:continue
  x=json.loads(path.read_text())
  if isinstance(x,dict) and 'raw_log' in x:out.append(summarize(path))
 a.output.write_text(json.dumps(out,indent=2)+'\n')
 print('Evidence',len(out),'windows',sum(len(x['workers']) for x in out))
if __name__=='__main__':main()
