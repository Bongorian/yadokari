#!/usr/bin/env python3
"""Draft workload plan and one-shot runner. No pass/fail performance thresholds."""
import argparse,hashlib,json,re,time
from pathlib import Path
from remote import NetworkLink,parse_status
MANIFEST=Path(__file__).with_name('wave4_workload.json')
def summarize(raw):
    workers=[dict(zip(('case','blocks','mean_us','max_us','over500','missing_frames'),map(int,m.groups()))) for m in re.finditer(r'Native case (\d+) worker blocks (\d+) avg-us (\d+) max-us (\d+) over500 (\d+) missing-frames (\d+)',raw)]
    rings=[dict(zip(('case','callbacks','mean_us','max_us','over125','over250','bad','underrun_delta','overrun_delta'),map(int,m.groups()))) for m in re.finditer(r'Native case (\d+) callbacks (\d+) avg-us (\d+) max-us (\d+) over125 (\d+) over250 (\d+) bad (\d+) underrun (\d+) overrun (\d+)',raw)]
    skips=re.findall(r'Wave4 case (\d+) SKIPPED: ([^\n]+)',raw)
    return {'workers':workers,'rings':rings,'skipped':[{'case':int(c),'reason':r} for c,r in skips],'restored_cleaned':'Wave4 draft restored stopped cleaned 1' in raw,'boot_observed':'Application firmware ' in raw or 'Circle 51' in raw,'empty_windows':[w['case'] for w in workers if not w['blocks']], 'qualification':'NOT EVALUATED'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true',help='Run only after jointly accepting this workload');p.add_argument('--network');p.add_argument('--output',type=Path);p.add_argument('--timeout',type=float,default=240);a=p.parse_args()
    data=MANIFEST.read_bytes();identity=hashlib.sha256(data).hexdigest()
    if not a.execute:print(json.dumps({'sha256':identity,'manifest':json.loads(data)},indent=2));return 0
    if not a.network or not a.output:p.error('--execute requires --network and --output')
    if a.output.exists():p.error('Output already exists; choose a new evidence path')
    link=NetworkLink(a.network);before=parse_status(link.command('STATUS',5));assert before['ready'] and before['remote'] and not before['active'] and not before['error'] and not before['phase'] and not before.get('journal_trial') and not before.get('trial')
    hardware=link.command('HARDWARE',5)
    for _ in range(1024):
        if link.command('LOG',5)=='GBX1 LOG -':break
    else:raise RuntimeError('Log did not drain; no request sent')
    try:reply=link.command('WAVE4TEST',10)
    except (OSError,TimeoutError):reply='Ambiguous reply; observing only, request is never retried'
    raw=bytearray();end=time.monotonic()+a.timeout;errors=[]
    while time.monotonic()<end:
        try:
            line=link.command('LOG',3)
            if line!='GBX1 LOG -':
                chunk=bytes.fromhex(line.removeprefix('GBX1 LOG '));raw.extend(chunk)
                if b'Wave4 draft restored stopped cleaned' in raw or b'Application firmware ' in raw:break
            else:time.sleep(.05)
        except (OSError,TimeoutError,ValueError) as e:errors.append(str(e));time.sleep(.2)
    text=raw.decode(errors='replace');result={'manifest_sha256':identity,'before':before,'hardware_before':hardware,'request_reply':reply,'errors':errors,'raw_log':text,**summarize(text)}
    for cmd,key in [('STATUS','after'),('HARDWARE','hardware_after')]:
        try:result[key]=link.command(cmd,5)
        except (OSError,TimeoutError) as e:result[key+'_error']=str(e)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');link.close();print(json.dumps({k:v for k,v in result.items() if k!='raw_log'},indent=2));return 0 if result['restored_cleaned'] else 1
if __name__=='__main__':raise SystemExit(main())
