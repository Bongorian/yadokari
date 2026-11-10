import sys
from pathlib import Path
from unittest.mock import patch
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools/update'))
import benchmark
class FakeLink:
    def __init__(self,host): self.log_reads=0;self.commands=[];self.status_reads=0;self.fast=False;self.drums=False
    def command(self,command,timeout):
        self.commands.append(command)
        if command=='STATUS':
            self.status_reads+=1
            if self.status_reads==1: raise TimeoutError('transient status delay')
            return 'GBX1 STATUS remote=1 active=0 ready=1 armed=0 phase=0 version=1 highest=1 partition=2 trial=0 error=0 ip=10.42.0.46'
        if command=='PLSTRESS': return 'GBX1 RESULT command=13 ok=1'
        if command=='PLFAST': self.fast=True;return 'GBX1 RESULT command=14 ok=1'
        if command=='DRUMDEMO': self.drums=True;return 'GBX1 RESULT command=15 ok=1'
        if command=='LOG':
            self.log_reads+=1
            if self.log_reads==1: raise TimeoutError('transient log delay')
            if self.log_reads==2: return 'GBX1 LOG -'
            return 'GBX1 LOG '+(b'Stress DATA cleanup files 1 dirs 2 ok 1\n'+(b'Daisy drum demo playing six published voices\nDaisy drum demo loaded and playback requested\n' if self.drums else (b'Plaits stress tempo 300\n' if self.fast else b'')+b'Plaits stress loaded and playback requested\n')).hex()
        raise AssertionError(command)
link=FakeLink('host')
with patch.object(benchmark,'NetworkLink',return_value=link),patch.object(benchmark.time,'sleep'),patch.object(sys,'argv',['benchmark.py','--network','10.42.0.46','--plaits-stress']): benchmark.main()
assert link.commands==['STATUS','STATUS','LOG','LOG','PLSTRESS','LOG']

serial=FakeLink('serial')
with patch.object(benchmark,'ReconnectingSerial',return_value=serial),patch.object(benchmark.time,'sleep'),patch.object(sys,'argv',['benchmark.py','--serial','/dev/ttyACM0','--plaits-stress']): benchmark.main()
assert serial.commands==['STATUS','STATUS','LOG','LOG','PLSTRESS','LOG']

fast=FakeLink('fast')
with patch.object(benchmark,'NetworkLink',return_value=fast),patch.object(benchmark.time,'sleep'),patch.object(sys,'argv',['benchmark.py','--network','10.42.0.46','--plaits-stress','--tempo','300']): benchmark.main()
assert fast.commands==['STATUS','STATUS','LOG','LOG','PLFAST','LOG']

drums=FakeLink('drums')
with patch.object(benchmark,'NetworkLink',return_value=drums),patch.object(benchmark.time,'sleep'),patch.object(sys,'argv',['benchmark.py','--network','10.42.0.46','--daisy-drums']): benchmark.main()
assert drums.commands==['STATUS','STATUS','LOG','LOG','DRUMDEMO','LOG']
