#!/usr/bin/env python3
"""Decode the DATA cleanup metadata log without reading removed content."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);a=p.parse_args();intents={};removed=[];malformed=0
for line in a.log.read_text(errors='replace').splitlines():
 try:
  row=json.loads(line);path=bytes.fromhex(row.pop('path_hex')).decode('utf-8',errors='replace');row['path']=path
  if row['action']=='intent':intents[path]=row
  elif row['action']=='removed':removed.append(row);intents.pop(path,None)
 except (ValueError,KeyError,TypeError):malformed+=1
print(json.dumps({'removed':removed,'pending_or_interrupted':list(intents.values()),'malformed_lines':malformed},ensure_ascii=False,indent=2))
