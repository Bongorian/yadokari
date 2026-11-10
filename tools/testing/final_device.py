#!/usr/bin/env python3
"""One device action, raw evidence and uptime; no retry of ambiguous mutations."""
import argparse,json,re,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'update'))
from remote import NetworkLink,ReconnectingSerial,parse_status
from transition_check import reset_between

def main():
 p=argparse.ArgumentParser(description=__doc__);transport=p.add_mutually_exclusive_group(required=True);transport.add_argument('--network');transport.add_argument('--serial');p.add_argument('--output',type=Path,required=True);p.add_argument('--command');p.add_argument('--until');p.add_argument('--seconds',type=float,default=30);a=p.parse_args()
 if a.output.exists():p.error('Evidence exists; choose a new path')
 a.output.parent.mkdir(parents=True,exist_ok=True);link=NetworkLink(a.network) if a.network else ReconnectingSerial(a.serial);record={'transport':{'network':a.network,'serial':a.serial,'log_timeout_seconds':20 if a.serial else 3},'command':a.command,'completion_regex':a.until,'seconds_limit':a.seconds,'before':parse_status(link.command('STATUS',5)),'hardware_before':link.command('HARDWARE',5),'errors':[]};assert record['before']['ready'] and not record['before']['active'] and not record['before']['phase'] and not record['before']['error']
 history=bytearray()
 for _ in range(2048):
  line=link.command('LOG',5);time.sleep(.07)
  if line=='GBX1 LOG -':break
  history.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
 else:raise RuntimeError('Old log did not drain')
 record['history_before']=history.decode(errors='replace');raw=bytearray();start=time.monotonic();record['matched']=False
 def save():record['raw_log']=raw.decode(errors='replace');record['elapsed_s']=round(time.monotonic()-start,3);a.output.write_text(json.dumps(record,indent=2)+'\n')
 if a.command:
  try:record['reply']=link.command(a.command,10)
  except (OSError,TimeoutError,RuntimeError) as e:record['reply_error']=repr(e)
 save();tail=None
 while time.monotonic()-start<a.seconds:
  try:
   line=link.command('LOG',20 if a.serial else 3);time.sleep(.07)
   if line!='GBX1 LOG -':raw.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
   else:time.sleep(.08)
   text=raw.decode(errors='replace');complete=text.rsplit('\n',1)[0] if '\n' in text else ''
   if a.until and re.search(a.until,complete):
    record['matched']=True
    if tail is None:tail=time.monotonic()+1
   if reset_between(record['hardware_before'],record['hardware_before'],text):break
   if tail and time.monotonic()>tail:break
  except (OSError,TimeoutError,ValueError) as e:record['errors'].append(repr(e));time.sleep(.1)
  save()
 for command,key in [('STATUS','after'),('HARDWARE','hardware_after')]:
  try:
   answer=link.command(command,5);record[key]=parse_status(answer) if command=='STATUS' else answer
  except (OSError,TimeoutError,ValueError) as e:record[key+'_error']=repr(e)
 record['reset_observed']=reset_between(record['hardware_before'],record.get('hardware_after',''),raw.decode(errors='replace'));save();link.close()
 print(json.dumps({k:v for k,v in record.items() if k not in ['raw_log','history_before']},indent=2))
 return int(record['reset_observed'] or bool(a.until and not record['matched']) or 'reply_error' in record)
if __name__=='__main__':raise SystemExit(main())
