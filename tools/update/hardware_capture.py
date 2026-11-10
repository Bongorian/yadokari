#!/usr/bin/env python3
"""Capture read-only CDC status and Circle logs with one serial owner."""
import argparse
import datetime
import json
from pathlib import Path
import time

from usb_remote import UsbLink


def timestamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', help='CDC serial path; omitted with --raw-usb')
    methods = parser.add_mutually_exclusive_group()
    methods.add_argument('--raw-usb', action='store_true', help='Use Circle CDC bulk endpoints through PyUSB')
    methods.add_argument('--network', help='IPv4 address of the Ethernet control endpoint')
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--duration', type=float, default=30)
    parser.add_argument('--status-interval', type=float, default=5)
    parser.add_argument('--timeout', type=float, default=5)
    args = parser.parse_args()
    if not args.raw_usb and not args.network and not args.serial:
        parser.error('--serial is required without --raw-usb or --network')
    if args.duration <= 0 or args.status_interval <= 0 or args.timeout <= 0:
        parser.error('durations must be positive')
    args.output_dir.mkdir(parents=True, exist_ok=False)
    if args.network:
        from remote import NetworkLink
        link = NetworkLink(args.network)
    elif args.raw_usb:
        from raw_usb_control import RawUsbLink
        link = RawUsbLink()
    else:
        link = UsbLink(args.serial)
    start = time.monotonic()
    next_status = start
    statuses = errors = 0
    try:
        with (args.output_dir / 'status.jsonl').open('x') as status, (args.output_dir / 'circle.log').open('xb') as logs:
            while time.monotonic() - start < args.duration:
                now = time.monotonic()
                command = 'STATUS' if now >= next_status else 'LOG'
                try:
                    reply = link.command(command, args.timeout)
                except (OSError, TimeoutError) as exc:
                    # A LOG response may have been lost after the device dequeued it.
                    # Keep the gap visible; do not infer a reboot or a clean interval.
                    errors += 1
                    status.write(json.dumps({'utc': timestamp(), 'command': command, 'error': type(exc).__name__}) + '\n')
                    status.flush()
                    time.sleep(min(0.2, max(0, args.duration - (time.monotonic() - start))))
                    continue
                if command == 'STATUS':
                    statuses += 1
                    status.write(json.dumps({'utc': timestamp(), 'response': reply}) + '\n')
                    status.flush()
                    next_status = time.monotonic() + args.status_interval
                    continue
                payload = reply.removeprefix('GBX1 LOG ')
                if payload == '-':
                    time.sleep(min(0.2, max(0, args.duration - (time.monotonic() - start))))
                else:
                    data = bytes.fromhex(payload)
                    if len(data) > 64:
                        raise ValueError('Oversized device log chunk')
                    logs.write(data)
                    logs.flush()
    finally:
        link.close()
    if not statuses:
        raise SystemExit(f'No device status captured in {args.output_dir}; connection errors: {errors}')
    print(f'Capture saved to {args.output_dir}; connection errors: {errors}; logs are best effort and may have gaps or have been overwritten.')


if __name__ == '__main__':
    main()
