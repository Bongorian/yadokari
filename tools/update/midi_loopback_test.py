#!/usr/bin/env python3
"""Check a physical MK3 DIN OUT -> IN cable through firmware MIDITEST.

Saves actual TX/RX logs and a byte-for-byte host comparison. Does not measure
standalone DIN wire latency or prove the instrument's MIDI input routing.
"""
import argparse
import datetime
import json
from pathlib import Path
import re
import time
from remote import NetworkLink, ReconnectingSerial, parse_status

TX = re.compile(r'DIN MIDI tx seq (\d+) ok (\d+) cable (\d+) len (\d+) bytes ([0-9a-fA-F]{2}) ([0-9a-fA-F]{2}) ([0-9a-fA-F]{2})')
RX = re.compile(r'DIN MIDI rx seq (\d+) match (\d+) cable (\d+) len (\d+) bytes ([0-9a-fA-F]{2}) ([0-9a-fA-F]{2}) ([0-9a-fA-F]{2}) rtt-us (\d+)')
RESULT = re.compile(r'DIN MIDI result pass (\d+) sent (\d+) received (\d+) unexpected (\d+) timeout (\d+) send-error (\d+) disconnect (\d+) dropped (\d+) rx-mask (\d+)')
TIMING = re.compile(r'DIN MIDI timing min-us (\d+) avg-us (\d+) max-us (\d+) missing (\d+) underrun (\d+) overrun (\d+)')


def analyze(log):
    result, timing = RESULT.search(log), TIMING.search(log)
    tx, rx = TX.findall(log), RX.findall(log)
    report = {'complete': bool(result and timing), 'tx_logged': len(tx), 'rx_logged': len(rx)}
    if not result or not timing:
        return report
    report['firmware'] = dict(zip(['pass', 'sent', 'received', 'unexpected', 'timeout', 'send_error', 'disconnect', 'dropped', 'rx_mask'], map(int, result.groups())))
    report['timing'] = dict(zip(['min_us', 'avg_us', 'max_us', 'worker_missing', 'usb_underrun', 'usb_overrun'], map(int, timing.groups())))
    expected = {int(row[0]): row for row in tx}
    received = {int(row[0]): row for row in rx if row[1] == '1'}
    n = report['firmware']['sent']
    exact = n == 261 and len(tx) == len(rx) == n and set(expected) == set(received) == set(range(n))
    for i, row in expected.items():
        actual = received.get(i)
        length = int(row[3])
        exact = exact and row[1] == '1' and actual is not None
        if actual:
            exact = exact and row[3] == actual[3] and row[4:4+length] == actual[4:4+length]
    report['host_exact_match'] = bool(exact)
    report['tx_cables'] = sorted({int(row[2]) for row in tx})
    report['rx_cables'] = sorted({int(row[2]) for row in rx})
    report['passed'] = bool(exact and report['firmware']['pass'])
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    transport = p.add_mutually_exclusive_group(required=True)
    transport.add_argument('--network')
    transport.add_argument('--serial')
    transport.add_argument('--raw-usb', action='store_true')
    p.add_argument('--output-dir', required=True, type=Path)
    p.add_argument('--timeout', type=float, default=30)
    args = p.parse_args()
    if args.timeout <= 0:
        p.error('--timeout must be positive')
    args.output_dir.mkdir(parents=True, exist_ok=False)
    if args.raw_usb:
        from raw_usb_control import RawUsbLink
        link = RawUsbLink()
    else:
        link = NetworkLink(args.network) if args.network else ReconnectingSerial(args.serial)
    log = bytearray()
    report = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'wiring': 'User confirmed MK3 DIN OUT -> MK3 DIN IN',
              'scope': 'Physical transport bytes; task/USB round trip, not wire-only latency or engine routing'}
    try:
        report['status_before'] = parse_status(link.command('STATUS', 5))
        s = report['status_before']
        if not s['remote'] or not s['ready'] or s['active']:
            raise RuntimeError('Normal firmware with remote control is required')
        # Drain, but retain old observations separately from this run.
        with (args.output_dir / 'previous.log').open('wb') as old:
            for _ in range(1024):
                line = link.command('LOG', 5).removeprefix('GBX1 LOG ')
                if line == '-':
                    break
                old.write(bytes.fromhex(line))
            else:
                raise RuntimeError('Old log did not drain')
        report['request'] = link.command('MIDITEST', 5)
        deadline = time.monotonic() + args.timeout
        with (args.output_dir / 'circle.log').open('wb') as raw:
            while time.monotonic() < deadline:
                line = link.command('LOG', 5).removeprefix('GBX1 LOG ')
                if line == '-':
                    time.sleep(.01)
                    continue
                chunk = bytes.fromhex(line); log.extend(chunk); raw.write(chunk); raw.flush()
                text = log.decode(errors='replace')
                if 'DIN MIDI start ok 0' in text:
                    raise RuntimeError('Probe unavailable; inspect connected/blocked state')
                if TIMING.search(text):
                    break
        report.update(analyze(log.decode(errors='replace')))
        report['status_after'] = parse_status(link.command('STATUS', 5))
    except (OSError, ValueError, RuntimeError) as exc:
        report['error'] = str(exc)
    finally:
        link.close()
        (args.output_dir / 'result.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report.get('passed') else 1


if __name__ == '__main__':
    raise SystemExit(main())
