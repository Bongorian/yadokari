#!/usr/bin/env python3
"""Record Line In to a sampler track on hardware, verify, and restore the benchmark."""
import argparse
import re
import time

from remote import NetworkLink, ReconnectingSerial, parse_status


def drain(link):
    for _ in range(512):
        if link.command('LOG', 5) == 'GBX1 LOG -':
            return
    raise RuntimeError('Could not drain the firmware log')


def wait_log(link, pattern, timeout):
    data = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = link.command('LOG', 5).removeprefix('GBX1 LOG ')
        if line == '-':
            time.sleep(0.1)
            continue
        data.extend(bytes.fromhex(line))
        if len(data) > 65536:
            del data[:-32768]
        match = re.search(pattern, data)
        if match:
            return match
    raise TimeoutError(f'Firmware log did not contain {pattern!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument('--network')
    choice.add_argument('--serial')
    choice.add_argument('--raw-usb', action='store_true')
    parser.add_argument('--timeout', type=float, default=40)
    args = parser.parse_args()
    if args.raw_usb:
        from raw_usb_control import RawUsbLink
        link = RawUsbLink()
    elif args.network:
        link = NetworkLink(args.network)
    else:
        link = ReconnectingSerial(args.serial)
    try:
        status = parse_status(link.command('STATUS', 5))
        if not status['remote'] or not status['ready'] or status['active']:
            raise SystemExit('Normal firmware and remote control are required')
        drain(link)
        if 'ok=1' not in link.command('SAMPLETEST', 5):
            raise SystemExit('Sample workflow request was rejected')
        report = wait_log(link, rb'Sampler record saved (\d+) verified (\d+) assigned (\d+) frames (\d+) slot (\d+) peak-uv (\d+)/(\d+) input-drops (\d+)', args.timeout)
        saved, verified, assigned, frames, slot, peak_l, peak_r, dropped = map(int, report.groups())
        print(f'Sampler record saved={saved} verified={verified} assigned={assigned} '
              f'frames={frames} slot={slot} peak L/R={peak_l}/{peak_r} uFS input drops={dropped}')
        if not (saved == verified == assigned == 1 and frames == 48000 and slot < 64 and dropped == 0):
            raise SystemExit('Sampler recording, WAV validation, or assignment failed')
        if min(peak_l, peak_r) < 1000:
            raise SystemExit('Recorded sample has no measurable Line In signal')
        drain(link)
        if 'ok=1' not in link.command('BENCHMARK', 5):
            raise SystemExit('Could not request benchmark restore and DATA cleanup')
        cleanup = wait_log(link, rb'Benchmark DATA cleanup files (\d+) dirs (\d+) ok (\d+)', args.timeout)
        files, dirs, ok = map(int, cleanup.groups())
        print(f'Benchmark DATA cleanup files={files} dirs={dirs} ok={ok}')
        if not ok or files < 1:
            raise SystemExit('Recorded test file was not cleaned')
    finally:
        link.close()


if __name__ == '__main__':
    main()
