#!/usr/bin/env python3
"""Exercise the read-only observer without requiring a connected controller."""
import importlib.util
from pathlib import Path
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mk3_observe', root / 'tools/mk3_observe.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
fixture = root / 'tests/fixtures'
config = (fixture / 'mk3-actual-config-20261030.bin').read_bytes()
audio = module.audio_configuration(config)
assert audio['implicit_candidates'] == [{'output': 1, 'input': 129, 'clock': 2}]
assert len(audio['streams']) == 2 and audio['streams'][0]['channels'] == 4 and audio['streams'][1]['channels'] == 2
assert all(stream['bits'] == 24 and stream['subslot'] == 4 for stream in audio['streams'])
changed = bytearray(config)
changed[141] = 3  # Clock source of input-to-host terminal, in this captured fixture.
assert not module.audio_configuration(changed)['implicit_candidates']
try:
    module.audio_configuration(config[:-1])
except ValueError:
    pass
else:
    raise AssertionError('truncated USB configuration accepted')
report = (fixture / 'mk3-actual-hid-report-20261030.bin').read_bytes()
lengths = module.report_lengths(report)
assert lengths[(1, 'input')] == 42 and lengths[(2, 'input')] == 64
assert lengths[(0x80, 'output')] == 63 and lengths[(0x81, 'output')] == 42
for report_id in (0xfe, 0xff):
    observed = (fixture / f'mk3-actual-palette-{report_id:02x}-20261030.bin').read_bytes()
    assert lengths[(report_id, 'feature')] == 210 and len(observed) == 209 and observed[0] == report_id
for malformed in (b'\x85', b'\xfe\x02\x00\x01', b'\xb4'):
    try:
        module.report_lengths(malformed)
    except ValueError:
        pass
    else:
        raise AssertionError('invalid HID descriptor accepted')
print('read-only MK3 observer: captured descriptors and short feature reports PASS')
