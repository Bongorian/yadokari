#!/usr/bin/env python3
"""Run the transient native-DSP workload once; retain identity and raw device logs."""
import argparse,hashlib,json,re,time
from pathlib import Path
from remote import NetworkLink,parse_status
p=argparse.ArgumentParser();p.add_argument('--network',required=True);p.add_argument('--mixed-audio',action='store_true');p.add_argument('--output',type=Path,required=True);p.add_argument('--timeout',type=float,default=260);a=p.parse_args()
link=NetworkLink(a.network);status=parse_status(link.command('STATUS',5))
assert status['ready'] and status['remote'] and not status['active'] and status['error']==0
assert status['phase']==0 and not status.get('trial') and not status.get('journal_trial'), '152-second workload requires a confirmed normal boot; trial expires at 120 seconds'
for _ in range(1024):
 if link.command('LOG',5)=='GBX1 LOG -':break
else:raise SystemExit('Old log did not drain; no request sent')
# Never repeat a mutating request after an ambiguous connection failure.
try:response=link.command('AUDIOTEST' if a.mixed_audio else 'NATIVETEST',10)
except (OSError,TimeoutError):response='response unavailable; observing logs only'
else:
 if 'ok=1' not in response:raise SystemExit(response)
print(response,flush=True);data=bytearray();end=time.monotonic()+a.timeout
while time.monotonic()<end:
 try:
  line=link.command('LOG',5)
  if line!='GBX1 LOG -':
   chunk=bytes.fromhex(line.removeprefix('GBX1 LOG '));data.extend(chunk);print(chunk.decode(errors='replace'),end='',flush=True)
   if b'Compiled version' in data:break  # Device reboot: retain the partial run.
   if b'Audio workload v1 completed 19 cases' in data or b'Native workload v1 completed 19 cases' in data or b'Native test aborted:' in data or b'load failed' in data or b'asset preparation failed' in data:break
  else:time.sleep(.05)
 except (OSError,TimeoutError):time.sleep(.25)
raw=data.decode(errors='replace');records=[]
pattern=r'Native case (\d+) callbacks (\d+) avg-us (\d+) max-us (\d+) over125 (\d+) over250 (\d+) bad (\d+) underrun (\d+) overrun (\d+)'
for match in re.finditer(pattern,raw):records.append(dict(zip(('case','callbacks','average_us','maximum_us','over_125us','over_250us','bad','usb_underrun','usb_overrun'),map(int,match.groups()))))
stages=[]
for match in re.finditer(r'Native case (\d+) stages calls (\d+) avg-tracks (\d+) peak-total (\d+) control (\d+) events (\d+) tracks (\d+) output (\d+)',raw):
 stages.append(dict(zip(('case','calls','average_tracks_us','peak_total_us','peak_control_us','peak_events_us','peak_tracks_us','peak_output_us'),map(int,match.groups()))))
workers=[]
for match in re.finditer(r'Native case (\d+) worker blocks (\d+) avg-us (\d+) max-us (\d+) over500 (\d+) missing-frames (\d+)',raw):
 workers.append(dict(zip(('case','blocks','average_us','maximum_us','over_500us','missing_frames'),map(int,match.groups()))))
workload=Path(__file__).resolve().parent/'workloads'/('audio-workload-v1.json' if a.mixed_audio else 'native-workload-v1-audo.json')
streams=[dict(zip(('case','stream_misses','recording_failed'),map(int,m.groups()))) for m in re.finditer(r'Audio case (\d+) stream-misses (\d+) recording-failed (\d+)',raw)]
recordings=[dict(zip(('saved','verified','assigned','frames','slot','peak_l_u','peak_r_u','input_drops'),map(int,m.groups()))) for m in re.finditer(r'Sampler record saved (\d+) verified (\d+) assigned (\d+) frames (\d+) slot (\d+) peak-uv (\d+)/(\d+) input-drops (\d+)',raw)]
try:after=parse_status(link.command('STATUS',10));status_error=None
except (OSError,TimeoutError,ValueError) as error:after=None;status_error=str(error)
result={'recordings':recordings,'mixed_audio':a.mixed_audio,'streams':streams,'workers':workers,'worker_block_frames':24,'worker_budget_us':500,'stage_sampling_interval':63,'workload_sha256':hashlib.sha256(workload.read_bytes()).hexdigest(),'stages':stages,'before':status,'after':after,'status_error':status_error,'command_response':response,'completed':len(records)==19 and len(workers)==19 and (not a.mixed_audio or len(streams)==19) and ('Audio workload v1 completed 19 cases' if a.mixed_audio else 'Native workload v1 completed 19 cases') in raw,'timing':records,'raw_log':raw,'scope':'Mixed v1 replaces native tracks 13-16 with two loopers and two Song AUDIO clips and records Main on track 13.' if a.mixed_audio else 'Actual Pi USB-audio callbacks; bounded workload, no listening or long-duration reliability claim.'}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
raise SystemExit(0 if result['completed'] else 1)
