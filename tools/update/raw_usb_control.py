"""Use the Circle CDC bulk endpoints without opening the host tty driver."""
import time
from pathlib import Path

import usb.core
import usb.util

from uart_control import COMMANDS


def matches_cached_identity(device):
    # sysfs caches enumeration strings, avoiding a new EP0 string request when
    # the CDC control endpoint has stopped responding.
    for entry in Path('/sys/bus/usb/devices').iterdir():
        try:
            if (int((entry / 'busnum').read_text()) == device.bus
                    and int((entry / 'devnum').read_text()) == device.address
                    and (entry / 'idVendor').read_text().strip() == '1209'
                    and (entry / 'idProduct').read_text().strip() == '0001'
                    and (entry / 'manufacturer').read_text().strip() == 'Circle'
                    and (entry / 'product').read_text().strip() == 'CDC Gadget'):
                return True
        except (OSError, ValueError):
            continue
    return False


class RawUsbLink:
    def __init__(self, path=None, log=None):
        self.device = None
        self.detached = False
        self.kernel_interfaces = []
        self.log = log
        self.buffer = bytearray()

    def open(self):
        if self.device is not None:
            return
        devices = list(usb.core.find(find_all=True, idVendor=0x1209, idProduct=0x0001))
        devices = [d for d in devices if matches_cached_identity(d)]
        if len(devices) != 1:
            raise OSError('Expected exactly one Circle CDC test gadget')
        self.device = devices[0]
        try:
            self.kernel_interfaces = [i for i in (0, 1) if self.device.is_kernel_driver_active(i)]
            if 1 in self.kernel_interfaces:
                self.device.detach_kernel_driver(1)
                self.detached = True
            usb.util.claim_interface(self.device, 1)
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.device is None:
            return
        device, self.device = self.device, None
        try:
            usb.util.release_interface(device, 1)
        except usb.core.USBError:
            pass
        # Linux cdc_acm binds the control interface and claims the data
        # interface as a pair. Detaching data may unbind both. Restore control
        # first so the tty returns; attaching only data does not re-probe CDC.
        for interface in self.kernel_interfaces:
            try:
                if not device.is_kernel_driver_active(interface):
                    device.attach_kernel_driver(interface)
            except usb.core.USBError:
                pass
        self.kernel_interfaces = []
        self.detached = False
        self.buffer.clear()
        usb.util.dispose_resources(device)

    def command(self, command, timeout):
        if command not in COMMANDS or timeout <= 0:
            raise ValueError('Invalid command or timeout')
        try:
            self.open()
            deadline = time.monotonic() + timeout
            frame = ('!GBX1 ' + command + '\n').encode('ascii')
            if self.device.write(0x02, frame, timeout=max(1, int(timeout * 1000))) != len(frame):
                raise OSError('Short USB command write')
            while time.monotonic() < deadline:
                while b'\n' in self.buffer:
                    line, _, rest = self.buffer.partition(b'\n')
                    self.buffer = bytearray(rest)
                    text = line.decode('ascii', errors='replace').strip()
                    if command == 'STATUS' and text.startswith('GBX1 STATUS '):
                        return text
                    if command == 'HARDWARE' and text.startswith('GBX1 HARDWARE '):
                        return text
                    if command == 'LOG' and text.startswith('GBX1 LOG '):
                        return text
                    prefix = f'GBX1 RESULT command={COMMANDS[command]} ok='
                    if text == prefix + '1':
                        return text
                    if text == prefix + '0':
                        raise RuntimeError('Device rejected command; query STATUS')
                remaining = max(1, int((deadline - time.monotonic()) * 1000))
                data = bytes(self.device.read(0x83, 512, timeout=remaining))
                if self.log:
                    self.log.write(data)
                    self.log.flush()
                self.buffer.extend(data)
                if len(self.buffer) > 4096:
                    raise ValueError('Oversized USB response')
            raise TimeoutError('No matching USB reply; outcome unknown')
        except usb.core.USBTimeoutError as exc:
            self.close()
            raise TimeoutError('USB command timed out; outcome unknown') from exc
        except usb.core.USBError as exc:
            self.close()
            raise OSError('USB disconnected or transfer failed') from exc
