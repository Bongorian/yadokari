#!/usr/bin/env python3
"""Distinguish unavailable replies from actual uptime resets; never resend mutations."""
import argparse,json,re,time
from pathlib import Path
from remote import NetworkLink,parse_status

def uptime(reply):
    m=re.search(r'\buptime_s=(\d+)',reply)
    return int(m[1]) if m else None

def reset_between(before,after,raw):
    a,b=uptime(before),uptime(after)
    return (a is not None and b is not None and b<a) or 'Application firmware ' in raw or 'Circle 51' in raw

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--network',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--functional-checks',action='store_true');p.add_argument('--seconds',type=float,default=10);a=p.parse_args()
    if a.output.exists():p.error('Evidence output already exists')
    link=NetworkLink(a.network);status=parse_status(link.command('STATUS',5));assert status['ready'] and not status['active'] and not status['phase'] and not status['error']
    history=bytearray()
    for _ in range(1024):
        line=link.command('LOG',5)
        if line=='GBX1 LOG -':break
        history.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
    else:raise RuntimeError('Old log not drained; no mutation requested')
    result={'before':status,'history_before_observation':history.decode(errors='replace'),'checks':[],'scope':'Functional transitions only, not the historical heavy workload or a load qualification'}
    sequence=[('PRESETCHECK','Preset functional check saved mounted 8/8 restored cleaned ok 1'),('SAVETEST','Project save probe completed ok 1'),('SAVETEST','Project save probe completed ok 1')] if a.functional_checks else [(None,None)]
    for command,marker in sequence:
        before=link.command('HARDWARE',5);entry={'command':command,'hardware_before':before};raw=bytearray();errors=[];started=time.monotonic()
        if command:
            try:entry['reply']=link.command(command,5)
            except (OSError,TimeoutError) as e:entry['reply_error']=str(e)
        deadline=started+(60 if command else a.seconds)
        while time.monotonic()<deadline:
            try:
                line=link.command('LOG',3)
                if line!='GBX1 LOG -':raw.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
                else:time.sleep(.05)
                text=raw.decode(errors='replace')
                if (marker and marker in text and '\n' in text[text.index(marker):]) or reset_between(before,before,text):break
            except (OSError,TimeoutError,ValueError) as e:errors.append(str(e));time.sleep(.1)
        text=raw.decode(errors='replace');entry.update(raw_log=text,errors=errors,elapsed_s=time.monotonic()-started,completion_observed=bool(marker and marker in text))
        try:entry['hardware_after']=link.command('HARDWARE',5)
        except (OSError,TimeoutError) as e:entry['hardware_after_error']=str(e)
        entry['reset_observed']=reset_between(before,entry.get('hardware_after',''),text);result['checks'].append(entry)
        if entry['reset_observed'] or (command and not entry['completion_observed']):break
    try:result['after']=parse_status(link.command('STATUS',5))
    except (OSError,TimeoutError,ValueError) as e:result['after_error']=str(e)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');link.close();print(json.dumps([{k:v for k,v in c.items() if k!='raw_log'} for c in result['checks']],indent=2))
    return 1 if any(c['reset_observed'] or (c['command'] and not c['completion_observed']) for c in result['checks']) else 0
if __name__=='__main__':raise SystemExit(main())
