#!/usr/bin/env python3
"""Stage a signed update and request trial boot using a trusted physical UART."""
import argparse
import hashlib
import os
from pathlib import Path
import socket
import struct
import sys
import termios
import threading
import time
import tty
from types import SimpleNamespace

from uart_control import COMMANDS, transact
from update import send


def parse_status(line):
    prefix = 'GBX1 STATUS '
    if not line.startswith(prefix):
        raise ValueError('Invalid UART status')
    fields = {}
    for entry in line[len(prefix):].split():
        key, sep, value = entry.partition('=')
        if not sep or key in fields:
            raise ValueError('Invalid UART status fields')
        fields[key] = value
    for key in ('remote', 'active', 'ready', 'armed', 'phase', 'version', 'highest', 'partition', 'trial', 'error', 'ip'):
        if key not in fields:
            raise ValueError('Incomplete UART status')
    for key in ('remote', 'active', 'ready', 'armed', 'phase', 'version', 'highest', 'partition', 'trial', 'error'):
        fields[key] = int(fields[key])
    for key in ('journal_trial', 'diagnostic_busy'):
        if key in fields:
            fields[key] = int(fields[key])
    return fields


def package_version(path):
    data = path.read_bytes()
    if len(data) < 193 or data[:4] != b'GUP1':
        raise ValueError('Invalid update package')
    version, size = struct.unpack_from('<II', data, 12)
    if len(data) != 192 + size or hashlib.sha256(data[192:]).digest() != data[24:56]:
        raise ValueError('Package size or payload hash mismatch')
    return version


class TrialObserver:
    """Read the optional rejected-trial Ethernet diagnostic in parallel."""
    def __init__(self, host):
        self.host = host
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self.observe, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        self.thread.join(timeout=2)

    def observe(self):
        while not self.stop_event.is_set():
            try:
                with socket.create_connection((self.host, 16701), timeout=0.5) as peer:
                    peer.settimeout(0.5)
                    data = bytearray()
                    while len(data) < 1024 and data.count(b'\n') < 2:
                        chunk = peer.recv(1024 - len(data))
                        if not chunk:
                            break
                        data.extend(chunk)
                    lines = data.decode('ascii').splitlines()
                    if lines and lines[0].startswith('GBX1 STATUS '):
                        parse_status(lines[0])
                        print('Rejected-trial Ethernet status:', lines[0], flush=True)
                        if len(lines) > 1 and lines[1].startswith('GBX1 BOOTDIAG '):
                            print('Rejected-trial boot diagnostics:', lines[1], flush=True)
                        return
            except (OSError, UnicodeError, ValueError):
                pass
            self.stop_event.wait(0.5)


class ReconnectingSerial:
    def __init__(self, path, log=None):
        self.path, self.log, self.fd = path, log, None

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def command(self, command, timeout):
        if self.fd is None:
            fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            try:
                tty.setraw(fd)
                config = termios.tcgetattr(fd)
                config[4] = config[5] = termios.B115200
                termios.tcsetattr(fd, termios.TCSANOW, config)
            except BaseException:
                os.close(fd)
                raise
            self.fd = fd
        try:
            return transact(self.fd, command, timeout, self.log)
        except (OSError, TimeoutError):
            self.close()
            raise


class NetworkLink:
    def __init__(self, host, log=None):
        self.host, self.log = host, log

    def close(self):
        pass

    def command(self, command, timeout):
        if command not in COMMANDS or timeout <= 0:
            raise ValueError('Invalid command or timeout')
        with socket.create_connection((self.host, 16701), timeout=timeout) as peer:
            peer.settimeout(timeout)
            peer.sendall(('!GBX1 ' + command + '\n').encode('ascii'))
            data = bytearray()
            while len(data) < 4096:
                part = peer.recv(1024)
                if not part:
                    raise ConnectionError('Network control disconnected')
                if self.log:
                    self.log.write(part)
                    self.log.flush()
                data.extend(part)
                while b'\n' in data:
                    line, _, rest = data.partition(b'\n')
                    data = bytearray(rest)
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
            raise ValueError('Oversized network response')


def wait_status(link, timeout, condition, rejected=None):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            state = parse_status(link.command('STATUS', min(2, max(0.1, deadline-time.monotonic()))))
            last = state
            if condition(state):
                return state
            if rejected and rejected(state):
                raise RuntimeError('Trial returned to recovery without confirmation; previous version remains selected')
        except (TimeoutError, OSError, ValueError):
            link.close()
            pass
        time.sleep(0.2)
    raise TimeoutError('Target status not observed; no update success assumed; last=' + str(last))


def run(args):
    version = package_version(args.package)
    log = None
    link = None
    try:
        if args.log:
            log = open(args.log, 'ab')
        if getattr(args, 'network', None):
            link = NetworkLink(args.network, log)
        elif getattr(args, 'raw_usb', False):
            from raw_usb_control import RawUsbLink
            link = RawUsbLink(log=log)
        else:
            link = ReconnectingSerial(args.serial, log)
        state = parse_status(link.command('STATUS', args.timeout))
        if not state['remote'] or not state['ready'] or version <= state['highest']:
            raise ValueError('Remote disabled, update layout unavailable, or package version not newer')
        if not state['active']:
            link.command('ENTER', args.timeout)
        link.command('ETHERNET', args.timeout)
        state = wait_status(link, args.timeout, lambda s: s['active'] and s['ip'] not in ('none', '0.0.0.0'))
        print('Update listener address:', state['ip'], flush=True)
        send(SimpleNamespace(package=args.package, tcp=state['ip'] + ':16700', serial=None, timeout=args.timeout))
        sys.stdout.flush()
        wait_status(link, args.timeout, lambda s: s['phase'] == 2)
        observer = None if getattr(args, 'network', None) else TrialObserver(state['ip'])
        if observer:
            observer.start()
        try:
            try:
                link.command('TRIAL', min(args.timeout, 5))
            except (TimeoutError, OSError):
                # A reboot may prevent a response. Only the next status proves a trial.
                pass
            finally:
                link.close()
            state = wait_status(link, args.timeout, lambda s: s['phase'] == 3 and (s['trial'] == 1 or s.get('journal_trial') == 1) and s['version'] == version,
                                lambda s: s['partition'] == 1 and s['phase'] == 0 and s['highest'] < version)
        finally:
            if observer:
                observer.stop()
        print('Trial identity observed:', state, flush=True)
        if not args.confirm_identity_only:
            print('Trial remains unconfirmed. Identity does not prove audible audio, display, or controls.')
            return
        link.command('ENTER', args.timeout)
        link.command('CONFIRM', args.timeout)
        wait_status(link, args.timeout, lambda s: s['phase'] == 0 and s['highest'] == version)
        try:
            link.command('BOOT', min(args.timeout, 5))
        except (TimeoutError, OSError):
            pass
        finally:
            link.close()
        wait_status(link, args.timeout, lambda s: s['phase'] == 0 and s['version'] == version and s['trial'] == 0 and s['partition'] in (2, 3))
        print('Confirmed and booted on identity-only checks; audio, display and control health were not proven.')
    finally:
        if link:
            link.close()
        if log:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--serial', help='CDC serial path; omitted with --raw-usb')
    methods = parser.add_mutually_exclusive_group()
    methods.add_argument('--raw-usb', action='store_true', help='Use Circle CDC bulk endpoints through PyUSB')
    methods.add_argument('--network', help='IPv4 address of the Ethernet control endpoint')
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--log')
    parser.add_argument('--confirm-identity-only', action='store_true', help='Explicitly confirm based on trial identity only, without claiming hardware health')
    args = parser.parse_args()
    if not args.raw_usb and not args.network and not args.serial:
        parser.error('--serial is required without --raw-usb or --network')
    try:
        run(args)
    except (OSError, ValueError, RuntimeError, TimeoutError) as exc:
        print('Remote update stopped:', exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
