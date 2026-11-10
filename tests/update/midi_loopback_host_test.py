import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/update'))
from midi_loopback_test import analyze

lines=[]
for i in range(261):
    lines += [f'DIN MIDI tx seq {i} ok 1 cable 0 len 3 bytes 90 3c 50',
              f'DIN MIDI rx seq {i} match 1 cable 0 len 3 bytes 90 3c 50 rtt-us 1000']
lines += ['DIN MIDI result pass 1 sent 261 received 261 unexpected 0 timeout 0 send-error 0 disconnect 0 dropped 0 rx-mask 1',
          'DIN MIDI timing min-us 1000 avg-us 1000 max-us 1000 missing 0 underrun 0 overrun 0']
valid='\n'.join(lines)
assert analyze(valid)['passed']
assert not analyze(valid.replace('rx seq 0 match 1 cable 0 len 3 bytes 90 3c 50','rx seq 0 match 1 cable 0 len 3 bytes 90 3c 51'))['passed']
assert not analyze('\n'.join(lines[1:]))['passed']
assert not analyze(valid+'\n'+lines[1])['passed']
assert not analyze(valid.replace('rx seq 0 match 1','rx seq 0 match 0'))['passed']
assert not analyze(valid.replace('DIN MIDI result pass 1','DIN MIDI result pass 0'))['passed']
assert not analyze('\n'.join(lines[:-1]))['complete']
print('DIN host comparison rejects corruption, missing/duplicate packets, mismatch and incomplete results')
