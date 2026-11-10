#!/usr/bin/env python3
"""Check the assembled firmware/content identities; write a hashed manifest."""
import hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'update'))
from update import identity
root=Path(sys.argv[1]);version,public,kind=identity((root/'kernel8-rpi4.img').read_bytes())
assert kind==1
paths=sorted(root.rglob('*'))
for p in paths:
    if p.is_symlink(): raise SystemExit(f'Boot tree contains a symlink, preserved but not packaged: {p}')
files={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file() and p.name not in ('manifest.json','SHA256SUMS')}
format_header=(Path(__file__).resolve().parent.parent/'src/project_format_version.h').read_text()
format_version=int(re.search(r'kFormatVersion\s*=\s*(\d+)',format_header)[1])
for folder in ('ASSETS','FACTORY','PROJECTS','PRESETS','KITS','RECORDINGS'):
    for p in (root/folder).rglob('*'):
        if p.is_file() and p.suffix.lower() in ('.mgx','.mgxs','.mgxk','.mgxp'):
            header=p.read_bytes()[:23]
            assert header[:6] == b'MGXC'+format_version.to_bytes(2,'little'), str(p)
            assert header[10:14] == b'TYPE' and header[22] in (1,2,3), str(p)
# Existing current-file locators may retain the untyped extension. Never infer kind from it.
session = root/'FACTORY/DEBUG/Benchmark.mgxs'
locator=root/'GROOVE/CURRENT.TXT'
if locator.exists():
    candidate=root/locator.read_text().strip()
    if candidate.is_file() and candidate.resolve().is_relative_to(root.resolve()): session=candidate
session_magic=session.read_bytes()[:4]
session_schema=session_magic.decode('ascii') if len(session_magic)==4 and all(32<=c<127 for c in session_magic) else session_magic.hex()
(root/'manifest.json').write_text(json.dumps({'firmware_version':version,'project_schema':'MGXC','project_format':format_version,'platform':'Pi4/AArch64/Circle','hardware_verified':False,'public_key_sha256':hashlib.sha256(public).hexdigest(),'signed_updates_enabled':any(public),'initial_session_schema':session_schema,'files':files},indent=2)+'\n')
print(f'Boot package: firmware {version}, factory schema MGXC v{format_version}, existing session {session_schema}, {len(files)} hashed files; no physical boot assertion')
if not any(public): print('WARNING: no updater trust key in this firmware; signed installations disabled',file=sys.stderr)
