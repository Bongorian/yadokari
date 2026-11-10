#!/usr/bin/env python3
"""Run the one-second Line Out -> Line In recording probe and verify its WAV."""
import argparse
import re
import time

from remote import NetworkLink, ReconnectingSerial, parse_status


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
        for _ in range(512):
            if link.command('LOG', 5) == 'GBX1 LOG -':
                break
        else:
            raise SystemExit('Could not drain the old log')
        reply = link.command('LOOPTEST', 5)
        if 'ok=1' not in reply:
            raise SystemExit(f'Loopback request rejected: {reply}')
        log = bytearray()
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            line = link.command('LOG', 5)
            hex_data = line.removeprefix('GBX1 LOG ')
            if hex_data == '-':
                time.sleep(0.2)
                continue
            log.extend(bytes.fromhex(hex_data))
            if len(log) > 65536:
                del log[:-32768]
            if b'Loopback one-second input WAV requested ok 0' in log:
                raise SystemExit('Firmware could not start the recording probe')
            match = re.search(rb'Loopback WAV saved (\d+) verified (\d+) cleaned (\d+) frames (\d+) rms-uv (\d+)/(\d+) peak-uv (\d+)/(\d+) tone-uv (\d+)/(\d+) input-drops (\d+)', log)
            peaks = re.search(rb'Loopback USB peaks main (\d+) phones (\d+) input-right (\d+)/(\d+) uFS', log)
            if match and peaks:
                saved, verified, cleaned, frames, rms_l, rms_r, peak_l, peak_r, tone_l, tone_r, dropped = map(int, match.groups())
                main, phones, right_l, right_r = map(int, peaks.groups())
                print(f'WAV saved={saved} verified={verified} cleaned={cleaned} frames={frames} '
                      f'RMS L/R={rms_l}/{rms_r} uFS peak L/R={peak_l}/{peak_r} uFS '
                      f'440 Hz L/R={tone_l}/{tone_r} uFS input drops={dropped}')
                print(f'USB output peaks Main={main} Phones={phones} uFS; '
                      f'input alternate right-justified peaks L/R={right_l}/{right_r} uFS')
                if not (saved == verified == cleaned == 1 and frames >= 8000 and dropped == 0):
                    raise SystemExit('Recording or WAV validation failed')
                if main < 1000:
                    raise SystemExit('No diagnostic tone was rendered to Main output')
                if min(rms_l, rms_r) < 1000:
                    raise SystemExit('Line input has no measurable loopback signal on both channels')
                if min(tone_l, tone_r) < 1000 or tone_l * 2 < peak_l or tone_r * 2 < peak_r:
                    raise SystemExit('Line input lacks the expected 440 Hz loopback tone')
                if max(peak_l, peak_r) >= 980000:
                    raise SystemExit('Line input appears clipped; reduce output or input gain')
                return
        raise SystemExit('Loopback recording result was not observed')
    finally:
        link.close()


if __name__ == '__main__':
    main()
