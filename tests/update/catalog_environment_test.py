"""Wrong generator environments must fail before writing catalog data."""
import json, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
script=ROOT/'tools/content/catalog_environment.py'
subprocess.run(['python3',str(script)],check=True)
lock=json.loads((ROOT/'tools/content/catalog_environment.lock.json').read_text())
with tempfile.TemporaryDirectory() as d:
    altered=Path(d)/'environment.json';lock['fingerprint']['machine']='unsupported-test-host';altered.write_text(json.dumps(lock))
    result=subprocess.run(['python3',str(script),'--lock',str(altered)],text=True,capture_output=True)
    assert result.returncode and 'Do not regenerate' in result.stderr
print('Factory generator environment accepted reference and rejected altered environment without manifest changes')
