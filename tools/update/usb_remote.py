#!/usr/bin/env python3
"""Stage a signed update and request trial over one USB-C CDC connection."""
import argparse
import os
from pathlib import Path
import termios
import time
import tty
from types import SimpleNamespace
import sys

from remote import package_version, parse_status
from uart_control import transact
from update import send


class UsbLink:
    def __init__(self, path, log=None):
        self.path = path
        self.log = log
        self.fd = None

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def open(self):
        if self.fd is None:
            fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            try:
                tty.setraw(fd)
                cfg = termios.tcgetattr(fd)
                cfg[4] = cfg[5] = termios.B115200
                termios.tcsetattr(fd, termios.TCSANOW, cfg)
            except BaseException:
                os.close(fd)
                raise
            self.fd = fd

    def command(self, command, timeout):
        self.open()
        try:
            return transact(self.fd, command, timeout, self.log)
        except OSError:
            self.close()
            raise

    def wait(self, timeout, predicate):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            try:
                last = parse_status(self.command('STATUS', min(2, max(0.1, deadline-time.monotonic()))))
                if predicate(last):
                    return last
            except (OSError, TimeoutError, ValueError):
                self.close()
            time.sleep(0.2)
        raise TimeoutError('Target status not observed; update outcome unknown; last=' + str(last))


def run(args):
    version = package_version(args.package)
    log = open(args.log, 'ab') if args.log else None
    link = UsbLink(args.serial, log)
    try:
        state = link.wait(args.timeout, lambda s: True)
        if not state['remote'] or not state['ready'] or version <= state['highest']:
            raise ValueError('Remote disabled, update layout unavailable, or package version not newer')
        # Also holds recovery's bounded autoboot while receiving an update.
        link.command('ENTER', args.timeout)
        # Release the port before the existing bounded GUP1 sender takes it.
        link.close()
        send(SimpleNamespace(package=args.package, tcp=None, serial=args.serial, timeout=args.timeout))
        link.wait(args.timeout, lambda s: s['phase'] == 2)
        try:
            link.command('TRIAL', min(args.timeout, 5))
        except (TimeoutError, OSError):
            link.close()
        state = link.wait(args.timeout, lambda s: s['phase'] == 3 and (s['trial'] == 1 or s.get('journal_trial') == 1) and s['version'] == version)
        print('Trial identity observed:', state, flush=True)
        if not args.confirm_identity_only:
            print('Trial remains unconfirmed; audio, display and controls were not proven.')
            return
        link.command('ENTER', args.timeout)
        link.command('CONFIRM', args.timeout)
        link.wait(args.timeout, lambda s: s['phase'] == 0 and s['highest'] == version)
        try:
            link.command('BOOT', min(args.timeout, 5))
        except (TimeoutError, OSError):
            link.close()
        link.wait(args.timeout, lambda s: s['phase'] == 0 and s['version'] == version and s['trial'] == 0 and s['partition'] in (2, 3))
        print('Confirmed and booted on identity-only checks; audio, display and controls were not proven.')
    finally:
        link.close()
        if log:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--serial', required=True, help='Stable USB CDC path, preferably /dev/serial/by-id/...')
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--log')
    parser.add_argument('--confirm-identity-only', action='store_true')
    args = parser.parse_args()
    try:
        run(args)
    except (OSError, ValueError, RuntimeError, TimeoutError) as exc:
        print('USB update stopped:', exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
