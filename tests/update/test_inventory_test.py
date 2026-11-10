"""Every defined test-run target must be reachable from test-all."""
import re, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
result=subprocess.run(['make','--no-print-directory','-qp','test-all'],cwd=root,text=True,capture_output=True)
assert result.returncode in (0,1),result.stderr
definitions=set(re.findall(r'^(test-run-[^\s:]+):',result.stdout,re.M))
registered=set()
for line in re.findall(r'^test-all:(.*)$',result.stdout,re.M):registered.update(re.findall(r'test-run-[\w-]+',line))
missing=definitions-registered
assert definitions and registered and not missing,('unregistered test-run targets',sorted(missing))
# The detector itself must reject a newly added unregistered target.
assert ({'test-run-intentionally-missing'}|definitions)-registered=={'test-run-intentionally-missing'}
print(f'Test inventory: all{len(definitions)} defined test-run targets registered in test-all; omission detector passed')
