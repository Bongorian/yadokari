#!/usr/bin/env python3
"""Convert immutable DSP probe logs and declared workload into auditable rows."""
import argparse,json,re,math,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--log',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--allow-partial',action='store_true');a=p.parse_args()
text=a.log.read_text(errors='replace');manifest=json.loads(a.manifest.read_text());cases={c['case']:dict(c,bins=[]) for c in manifest['cases']};started=set();skips=[];rejected=[]
frequency_match=re.search(r'DSP suite revision [123] begin .* freq (\d+)',text)
assert frequency_match,'missing architectural clock frequency'
frequency=int(frequency_match[1])
fx_count=22 if re.search(r'DSP suite revision [23] begin',text) else 20
system_base=1024+5*fx_count
stage_base=system_base+12
for m in re.finditer(r'DSP case (\d+) start kind',text):started.add(int(m[1]))
for m in re.finditer(r'DSP case (\d+) SKIPPED[^\n]*',text):skips.append({'case':int(m[1]),'line':m[0]})
for m in re.finditer(r'DSP case (\d+) REJECTED[^\n]*',text):rejected.append({'case':int(m[1]),'line':m[0]})
header=r'DSP profile case (\d+) freq (\d+) blocks (\d+) sampled (\d+) main-peak (\d+) poly-peak (\d+) control-calls (\d+) control-ticks (\d+) control-max (\d+) snapshot-max (\d+)'
for m in re.finditer(header,text):
 v=list(map(int,m.groups()));c=cases[v[0]];c['profile']={'freq':v[1],'blocks':v[2],'sampled':v[3],'main_peak':v[4],'poly_peak':v[5],'control_calls':v[6],'control_ticks':v[7],'control_max_ticks':v[8],'snapshot_max_ticks':v[9]};c['profile']['control_avg_us']=v[7]*1e6/(v[1]*v[6]) if v[6] else None;c['profile']['control_max_us']=v[8]*1e6/v[1];c['profile']['snapshot_max_us']=v[9]*1e6/v[1]
# v475 can overrun Circle's text ring when a complete case is dumped at once.
# A surviving header suffix belongs to its preceding, unique case-start marker.
# Frequency is suite-global and block/control counts are identical by contract.
current=None
for line in text.splitlines():
 start=re.search(r'DSP case (\d+) start kind',line)
 if start:current=int(start[1])
 suffix=re.fullmatch(r'sampled (\d+) main-peak (\d+) poly-peak (\d+) control-calls (\d+) control-ticks (\d+) control-max (\d+) snapshot-max (\d+)',line)
 if suffix and current is not None and 'profile' not in cases[current]:
  sampled,main,poly,calls,ticks,maximum,snapshot=map(int,suffix.groups())
  cases[current]['profile']={'freq':frequency,'blocks':calls,'sampled':sampled,'main_peak':main,'poly_peak':poly,'control_calls':calls,'control_ticks':ticks,'control_max_ticks':maximum,'snapshot_max_ticks':snapshot,'control_avg_us':ticks*1e6/(frequency*calls),'control_max_us':maximum*1e6/frequency,'snapshot_max_us':snapshot*1e6/frequency,'header_recovery':'case from preceding start; frequency from suite; blocks equal control calls; surviving suffix retained'}
for m in re.finditer(r'DSP bin case (\d+) id (\d+) calls (\d+) frames (\d+) ticks (\d+) active (\d+) max-block (\d+) max-voice-q16 (\d+)',text):
 cid,bin_id,calls,frames,ticks,active,peak,ratio=map(int,m.groups());c=cases[cid];freq=frequency;kind='engine' if bin_id<1024 else 'insert' if bin_id<system_base else 'system' if bin_id<stage_base else 'stage'
 row={'bin':bin_id,'kind':kind,'calls':calls,'active_frames':frames,'all_ticks':ticks,'active_ticks':active,'peak_ticks':peak,'voice_q16':ratio,'all_us':ticks*1e6/freq,'max_block_us':peak*1e6/freq,'mean_active_24_us':active*24e6/(freq*frames) if frames else None,'max_active_24_us':ratio*24e6/(freq*65536) if frames else None}
 if kind=='engine':row.update(engine=bin_id//32,variant=bin_id%32)
 elif kind=='insert':row.update(path=(bin_id-1024)//fx_count,type=(bin_id-1024)%fx_count)
 elif kind=='system':row['system']=bin_id-system_base
 else:row['stage']=bin_id-stage_base
 c['bins'].append(row)
for c in cases.values():
 if c['bins'] and 'profile' not in c:
  total=next((b for b in c['bins'] if b['kind']=='stage' and b['stage']==4),None)
  c['profile']={'freq':frequency,'blocks':None,'sampled':total['calls'] if total else None,'main_peak':None,'poly_peak':None,'control_avg_us':None,'control_max_us':None,'snapshot_max_us':None,'header_recovery':'missing header: only frequency from suite and sampled from Total.calls; other fields remain unknown'}
result=r'DSP result case (\d+) window-ms (\d+) blocks (\d+) avg-us (\d+) max-us (\d+) over500 (\d+) window-missing (\d+) condition-missing (\d+) native-active (\d+) refused (\d+) callbacks (\d+) callback-max (\d+) bad (\d+)'
for m in re.finditer(result,text):
 v=list(map(int,m.groups()));cases[v[0]]['deadline']=dict(zip(['window_ms','blocks','avg_us','max_us','over500','window_missing','condition_missing','native_active','refused','callbacks','callback_max','bad'],v[1:]));cases[v[0]]['deadline'].update(average_deadline_percent=v[3]/5,mean_headroom_us=500-v[3],max_headroom_us=500-v[4])
completed=[cases[i] for i in sorted(started) if 'deadline' in cases[i]];table=[]
for c in completed:
 if c['kind']!=1:continue
 row=next((r for r in c['bins'] if r['kind']=='engine' and r['engine']==c['engine'] and r['variant']==c['variant']),None)
 if row is None:row=next((r for r in c['bins'] if r['kind']=='engine' and r['engine']==c['engine']),None)
 entry={k:c[k] for k in ('case','engine','name','variant','declared_cost','independent','native_avg_us','native_burst_us')};entry.update(deadline=c['deadline'],control=c.get('profile'),measurement=row,measurement_unit='track DSP' if c['native_avg_us'] else 'independent voice',calibrated=bool(row and row['active_frames']))
 if row and row['mean_active_24_us'] is not None:
  mean=row['mean_active_24_us'];maximum=row['max_active_24_us']
  if c['native_avg_us']:entry.update(native_reference_avg=math.ceil(mean*1.25),native_reference_burst=math.ceil(row['max_block_us']*1.25))
  else:entry.update(reference_mean_cost=math.ceil(mean*1.25/6.25),reference_peak_cost=math.ceil(maximum*1.25/6.25),reference_safe_poly_slots=min(32,math.floor(300/(maximum*1.25))) if maximum and c['independent'] else None)
 table.append(entry)
finish=re.search(r'DSP suite revision [123] completed aborted (\d+) elapsed-ms (\d+) total-missing (\d+) restored (\d+)',text)
report={'log_sha256':hashlib.sha256(a.log.read_bytes()).hexdigest(),'manifest_sha256':hashlib.sha256(a.manifest.read_bytes()).hexdigest(),'scope':'six-second deadline window; voice/FX profile covers candidate generation incl warmup; coarse control/condition/suite include setup; no hypothetical quality/voice reduction','started':sorted(started),'completed':completed,'skipped':skips,'rejected':rejected,'cost_table':table,'suite':dict(zip(['aborted','elapsed_ms','total_missing','restored'],map(int,finish.groups()))) if finish else None}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
if not a.allow_partial:
 assert finish and not int(finish[1]) and int(finish[4])==1,'incomplete or aborted suite'
 assert not rejected and len(completed)+len(skips)==len(started)+len(skips),'missing condition results'
 assert all(c['profile']['sampled'] for c in completed),'unsampled condition'
print(json.dumps({'started':len(started),'completed':len(completed),'skipped':len(skips),'rejected':len(rejected),'suite':report['suite']}))
