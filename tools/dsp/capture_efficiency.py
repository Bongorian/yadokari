#!/usr/bin/env python3
"""Run a fixed authored-song audit once; retain partial data without automatic reruns."""
import argparse
import datetime
import json
import os
import re
import select
import sys
import threading
import time
import tty
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'update'))
from remote import ReconnectingSerial, NetworkLink, parse_status

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--version', type=int, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--ip', default='10.42.0.46')
subset = parser.add_mutually_exclusive_group()
subset.add_argument('--tail', action='store_true', help='Fixed T3 onward (T3/T4 in revision1) after interrupted baseline')
subset.add_argument('--sampler', action='store_true', help='Fixed T4 onward (T4 in revision1), without repeating T3')
parser.add_argument('--attach', action='store_true')
parser.add_argument('--round3', action='store_true', help='Seven original songs and EFF revision3 event attribution')
parser.add_argument('--round2', action='store_true', help='Six original songs, EFF revision2 and owned concurrent save')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
if args.round3:args.round2=True
command, expected = ('EFFSAMPLER', [4,5,6] if args.round2 else [4]) if args.sampler else ('EFFTAIL', [3,4,5,6] if args.round2 else [3,4]) if args.tail else ('EFFICIENCY', [1, 2, 3, 4, 5, 6] if args.round2 else [1, 2, 3, 4])
if args.round3:expected.append(7)
report = {'version': args.version, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'command': command, 'expected_songs': expected, 'revision': 3 if args.round3 else 2 if args.round2 else 1,
          'transport': 'USB bootstrap; Ethernet16701 measurement; passive CDC panic mirror only',
          'read_only_transport_faults': [], 'mutating_request_retried': False}
wire = (args.output / 'wire.log').open('xb')
raw = (args.output / 'raw.log').open('xb')
chunks = (args.output / 'chunks.jsonl').open('x')
text = bytearray()
link = NetworkLink(args.ip, wire)
stop = threading.Event()
panic = threading.Event()
reader = None
serial_path = '/dev/serial/by-id/usb-Circle_CDC_Gadget-if00'


def save():
    (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n')


def passive_panic_reader():
    # The bootstrap descriptor is closed before this one reader starts. It sends
    # no command and is closed before any following deployment/bootstrap.
    fd = None
    with (args.output / 'panic-cdc.log').open('xb') as output:
        while not stop.is_set():
            try:
                if fd is None:
                    fd = os.open(serial_path, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
                    tty.setraw(fd)
                if select.select([fd], [], [], .1)[0]:
                    data = os.read(fd, 4096)
                    if not data:
                        raise OSError('CDC disconnected')
                    output.write(data)
                    output.flush()
                    if b'\x1b[1m' in data or b'\x1b[91m' in data:
                        panic.set()
            except OSError:
                if fd is not None:
                    os.close(fd)
                    fd = None
                stop.wait(.1)
        if fd is not None:
            os.close(fd)


try:
    if not args.attach:
        usb = ReconnectingSerial(serial_path, wire)
        try:
            report['before'] = parse_status(usb.command('STATUS', 5))
            save()
            assert report['before']['version'] == args.version and not report['before']['error'] and not report['before']['active']
            report['start_reply'] = usb.command(command, 10)
            save()
            assert report['start_reply'] == f'GBX1 RESULT command={35 if args.sampler else 34 if args.tail else 33} ok=1'
        finally:
            usb.close()
    reader = threading.Thread(target=passive_panic_reader, daemon=True)
    reader.start()
    started = time.monotonic()
    deadline = started + (2400 if args.round3 else 2100 if args.round2 else 1500)
    fault_delay = .1
    save()
    while time.monotonic() < deadline:
        if panic.is_set():
            report['panic_detected'] = True
            raise RuntimeError('Panic mirror received; partial data retained, no automatic rerun')
        try:
            reply = link.command('LOG', 6)
            fault_delay = .1
            if reply != 'GBX1 LOG -':
                data = bytes.fromhex(reply.removeprefix('GBX1 LOG '))
                raw.write(data)
                raw.flush()
                text.extend(data)
                chunks.write(json.dumps({'seconds': time.monotonic() - started, 'end_offset': len(text)}) + '\n')
                chunks.flush()
            begin = text.find(f'EFF revision {report["revision"]} begin'.encode())
            if begin >= 0 and b'Circle 51.' in text[begin:]:
                report['reset_detected'] = True
                raise RuntimeError('Target rebooted during measurement; partial raw retained, no automatic rerun')
            if b'EFF requested ok 0' in text:
                raise RuntimeError('Diagnostic admission refused; current content retained')
            if re.search(rb'EFF revision [123] completed failed [01] restored [01]', text):
                break
            time.sleep(.1)
        except (OSError, TimeoutError) as error:
            report['read_only_transport_faults'].append({'seconds': time.monotonic() - started, 'error': repr(error)})
            save()
            # A synchronous load can leave the one pending control client
            # unserviced. Back off boundedly instead of filling TCP admission.
            stop.wait(fault_delay)
            fault_delay = min(1.0, fault_delay * 2)
    report['elapsed_seconds'] = time.monotonic() - started
    report['ending'] = parse_status(link.command('STATUS', 8))
    report['hardware'] = link.command('HARDWARE', 8)
    decoded = text.decode(errors='replace')
    report['loads'] = [{'song': int(m[1]), 'ok': int(m[2])} for m in re.finditer(r'EFF song (\d+) load (\d+)', decoded)]
    report['completed'] = bool(re.search(r'EFF revision [123] completed failed 0 restored 1', decoded))
    # LOG consumes bytes when the target builds a response. A synchronous
    # load may delay that response past a TCP timeout, losing a partial notice.
    # Full sealed PLAY windows plus failed0/restored1 prove fixed-hash admission;
    # retain missing notifications explicitly instead of repeating audio work.
    windows = {int(m[1]): int(m[2]) for m in re.finditer(r'EFF row (\d+) wall-us \d+ blocks (\d+)', decoded)}
    report['complete_play_songs'] = [song for song in expected
        if windows.get((song - 1) * 8 + 2) == (48000 * 60 * 4 * 48 // [140, 138, 132, 124, 138, 124, 96][song - 1] + 23) // 24]
    report['missing_load_notifications'] = sorted(set(expected) - {x['song'] for x in report['loads']})
    report['fileplay_ok'] = not args.round2 or 1 not in expected or bool(re.search(r'EFF fileplay saved 1 known 1 cleaned 1',decoded))
    report['functional_ok'] = (report['fileplay_ok'] and report['completed'] and report['complete_play_songs'] == expected
                               and all(x['ok'] for x in report['loads']) and report['ending']['version'] == args.version
                               and not report['ending']['error'] and not report['ending']['active'])
    save()
    assert report['functional_ok'], 'Audit incomplete or failed; raw retained, no automatic rerun'
except BaseException as error:
    report['exception'] = repr(error)
    save()
    raise
finally:
    stop.set()
    if reader:
        reader.join(2)
    link.close()
    wire.close()
    raw.close()
    chunks.close()
print(json.dumps(report, indent=2))
