#!/usr/bin/env python3
"""Drain bounded Circle log chunks over the opt-in USB-C CDC control link."""
import argparse
import sys
import time
from usb_remote import UsbLink


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Stable USB CDC path')
    parser.add_argument('--output', help='Append decoded log bytes to this file')
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--follow', action='store_true', help='Continue polling until interrupted')
    args = parser.parse_args()
    link = UsbLink(args.serial)
    output = None
    try:
        output = open(args.output, 'ab') if args.output else sys.stdout.buffer
        while True:
            reply = link.command('LOG', args.timeout)
            text = reply.removeprefix('GBX1 LOG ')
            if text == '-':
                if not args.follow:
                    break
                time.sleep(0.25)
                continue
            data = bytes.fromhex(text)
            if len(data) > 64:
                raise ValueError('Oversized device log chunk')
            output.write(data)
            output.flush()
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError) as exc:
        print('USB log retrieval stopped:', exc, file=sys.stderr)
        return 1
    finally:
        link.close()
        if args.output and output:
            output.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
