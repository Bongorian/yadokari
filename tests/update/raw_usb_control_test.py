import sys
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/update'))
import raw_usb_control as raw


class Device:
    iManufacturer = 1
    iProduct = 2
    def __init__(self):
        self.responses = [b'ignored\nGBX1 STA', b'TUS remote=1\n', b'GBX1 RESULT command=2 ok=0\n']
        self.detached = self.attached = False
        self.active = {0: True, 1: True}
    def is_kernel_driver_active(self, interface):
        return self.active[interface]
    def detach_kernel_driver(self, interface):
        self.detached = True
        self.active = {0: False, 1: False}
    def attach_kernel_driver(self, interface):
        self.attached = True
        assert interface == 0
        self.active = {0: True, 1: True}
    def write(self, endpoint, data, timeout):
        assert endpoint == 2
        return len(data)
    def read(self, endpoint, size, timeout):
        assert endpoint == 0x83
        return self.responses.pop(0)


device = Device()
with patch.object(raw.usb.core, 'find', return_value=[device]), \
     patch.object(raw, 'matches_cached_identity', return_value=True), \
     patch.object(raw.usb.util, 'claim_interface'), patch.object(raw.usb.util, 'release_interface'), \
     patch.object(raw.usb.util, 'dispose_resources'):
    link = raw.RawUsbLink()
    assert link.command('STATUS', 1) == 'GBX1 STATUS remote=1'
    try:
        link.command('ENTER', 1)
        raise AssertionError('Rejected command was accepted')
    except RuntimeError:
        pass
    link.close()
    assert device.detached and device.attached
print('Raw USB framing, rejection and driver handoff passed')
