#!/usr/bin/env python3
"""Issue one bounded update-control command over the Pi's physical UART."""
import argparse
import os
import select
import sys
import termios
import time
import tty

COMMANDS = {'STATUS': 1, 'ENTER': 2, 'ETHERNET': 3, 'WIFI': 4, 'USB': 5,
            'TRIAL': 6, 'CONFIRM': 7, 'BOOT': 8, 'CANCEL': 9, 'LOG': 10, 'BENCHMARK': 11, 'HARDWARE': 12, 'PLSTRESS': 13, 'PLFAST': 14, 'DRUMDEMO': 15, 'LOOPTEST': 16, 'SAMPLETEST': 17, 'FXSTRESS': 18, 'SAVETEST': 19, 'KITSWAP': 20, 'CHOPTEST': 21, 'KITREC': 22, 'UITEST': 23, 'MIDITEST': 24, 'NATIVETEST': 25, 'AUDIOTEST': 26, 'INPUTTEST': 27, 'PRESETCHECK': 28, 'WAVE4TEST': 29, 'FINALTEST': 30, 'DSPTEST': 31, 'DSPSTRESS': 32, 'EFFICIENCY': 33, 'EFFTAIL': 34, 'EFFSAMPLER': 35, 'DEPLOYCHECK': 36, 'OPTTEST': 37}


def transact(fd, command, timeout, log=None):
    if command not in COMMANDS or timeout <= 0:
        raise ValueError('Invalid command or timeout')
    frame = ('!GBX1 ' + command + '\n').encode('ascii')
    end = time.monotonic() + timeout
    written = 0
    while written < len(frame):
        delay = end - time.monotonic()
        if delay <= 0 or not select.select([], [fd], [], delay)[1]:
            raise TimeoutError('UART send timed out; outcome unknown')
        try:
            n = os.write(fd, frame[written:])
        except BlockingIOError:
            continue
        if n <= 0:
            raise ConnectionError('UART disconnected')
        written += n
    line = bytearray()
    while True:
        delay = end - time.monotonic()
        if delay <= 0 or not select.select([fd], [], [], delay)[0]:
            raise TimeoutError('No matching reply; reboot or outcome unknown')
        try:
            part = os.read(fd, 256)
        except BlockingIOError:
            continue
        if not part:
            raise ConnectionError('UART disconnected; outcome unknown')
        if log:
            log.write(part)
            log.flush()
        for byte in part:
            if byte == 10:
                text = line.decode('ascii', errors='replace').strip()
                line.clear()
                if command == 'STATUS' and text.startswith('GBX1 STATUS '):
                    return text
                if command == 'HARDWARE' and text.startswith('GBX1 HARDWARE '):
                    return text
                if command == 'LOG' and text.startswith('GBX1 LOG '):
                    return text
                prefix = f'GBX1 RESULT command={COMMANDS[command]} ok='
                if command != 'STATUS' and text.startswith(prefix):
                    if text == prefix + '1':
                        return text
                    if text == prefix + '0':
                        raise RuntimeError('Device rejected command; query STATUS')
            elif len(line) < 1024:
                line.append(byte)
            else:
                line.clear()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Physical UART, for example /dev/serial/by-id/...')
    parser.add_argument('--timeout', type=float, default=10)
    parser.add_argument('--log', help='Append observed UART bytes to this file')
    parser.add_argument('command', type=str.upper, choices=COMMANDS)
    args = parser.parse_args()
    fd = None
    log = None
    try:
        fd = os.open(args.serial, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(fd)
        config = termios.tcgetattr(fd)
        config[4] = config[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, config)
        if args.log:
            log = open(args.log, 'ab')
        print(transact(fd, args.command, args.timeout, log))
    except (OSError, ValueError, TimeoutError, RuntimeError) as exc:
        print('UART command failed:', exc, file=sys.stderr)
        return 1
    finally:
        if log:
            log.close()
        if fd is not None:
            os.close(fd)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
