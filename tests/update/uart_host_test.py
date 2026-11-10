import importlib.util
import os
from pathlib import Path
import pty
import sys
import threading
import tty

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('uart_control', root / 'tools/update/uart_control.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

master, slave = pty.openpty()
tty.setraw(slave)

def device():
    received = b''
    while not received.endswith(b'\n'):
        received += os.read(master, 64)
    assert received == b'!GBX1 STATUS\n'
    os.write(master, b'other boot log\r\nGBX1 STATUS remote=1 active=0 phase=0\r\n')
    received = b''
    while not received.endswith(b'\n'):
        received += os.read(master, 64)
    assert received == b'!GBX1 ENTER\n'
    os.write(master, b'GBX1 RESULT command=2 ok=0\n')
    received = b''
    while not received.endswith(b'\n'):
        received += os.read(master, 64)
    assert received == b'!GBX1 LOG\n'
    os.write(master, b'GBX1 LOG 74657374\n')
    received = b''
    while not received.endswith(b'\n'):
        received += os.read(master, 64)
    assert received == b'!GBX1 HARDWARE\n'
    os.write(master, b'GBX1 HARDWARE arm_hz=600000000\n')

thread = threading.Thread(target=device)
thread.start()
assert 'remote=1' in tool.transact(slave, 'STATUS', 2)
try:
    tool.transact(slave, 'ENTER', 2)
except RuntimeError:
    pass
else:
    raise AssertionError('Rejected command reported success')
assert tool.transact(slave, 'LOG', 2) == 'GBX1 LOG 74657374'
assert tool.transact(slave, 'HARDWARE', 2) == 'GBX1 HARDWARE arm_hz=600000000'
thread.join()
os.close(master)
os.close(slave)
print('UART host framing, log filtering and rejection passed')


# The shared host whitelist and firmware declaration must stay in agreement.
import re
from pathlib import Path
_declaration=(Path(__file__).resolve().parents[2]/'src/update/serial_control.h').read_text()
_enum=re.search(r'enum class SerialCommand:uint8_t \{([^}]+)\}',_declaration).group(1)
_values={name.strip():i for i,name in enumerate(_enum.split(','))}
_pairs=re.findall(r'\{"!GBX1 ([A-Z0-9]+)",SerialCommand::([A-Za-z0-9]+)\}',_declaration)
assert tool.COMMANDS=={name:_values[value] for name,value in _pairs},'Host command whitelist differs from firmware'
