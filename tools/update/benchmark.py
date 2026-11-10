#!/usr/bin/env python3
"""Request the firmware's fixed benchmark and wait for its completion log."""
import argparse
import time
from remote import NetworkLink, ReconnectingSerial, parse_status

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    methods = parser.add_mutually_exclusive_group(required=True)
    methods.add_argument('--network', help='Pi private-network IPv4 address')
    methods.add_argument('--serial', help='USB-C CDC serial device path')
    methods.add_argument('--raw-usb', action='store_true', help='USB-C CDC bulk endpoints through PyUSB')
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--plaits-stress', action='store_true', help='Run the transient 16-track Plaits stress project')
    parser.add_argument('--fx-stress', action='store_true', help='Run transient mixed pad, synth, insert and morph workload')
    parser.add_argument('--daisy-drums', action='store_true', help='Audition six DaisySP drum voices without saving a project')
    parser.add_argument('--tempo', type=int, choices=(120, 300), default=120, help='Tempo for the Plaits stress workload')
    args = parser.parse_args()
    if sum((args.plaits_stress, args.daisy_drums, args.fx_stress)) > 1: parser.error('Choose one transient workload')
    if args.tempo != 120 and not args.plaits_stress: parser.error('--tempo 300 requires --plaits-stress')
    if args.network:
        link = NetworkLink(args.network)
    elif args.raw_usb:
        from raw_usb_control import RawUsbLink
        link = RawUsbLink()
    else:
        link = ReconnectingSerial(args.serial)
    preflight_end = time.monotonic() + args.timeout
    def read_only(command):
        while time.monotonic() < preflight_end:
            try:
                return link.command(command, min(5, max(0.1, preflight_end-time.monotonic())))
            except (OSError, TimeoutError):
                time.sleep(min(0.25, max(0, preflight_end-time.monotonic())))
        raise SystemExit('Network preflight timed out; benchmark was not requested')
    status = parse_status(read_only('STATUS'))
    if not status['remote'] or not status['ready'] or status['active']:
        raise SystemExit('Benchmark requires the normal application and enabled remote control')
    # Discard old log entries so an earlier run cannot masquerade as success.
    for _ in range(512):
        line = read_only('LOG')
        if line == 'GBX1 LOG -':
            break
    else:
        raise SystemExit('Could not drain old log; benchmark was not requested')
    command = 'FXSTRESS' if args.fx_stress else 'DRUMDEMO' if args.daisy_drums else ('PLFAST' if args.tempo == 300 else 'PLSTRESS') if args.plaits_stress else 'BENCHMARK'
    success = b'FX stress loaded and playback requested' if args.fx_stress else b'Daisy drum demo loaded and playback requested' if args.daisy_drums else b'Plaits stress loaded and playback requested' if args.plaits_stress else b'Benchmark loaded and playback requested'
    failure = b'FX stress failed;' if args.fx_stress else b'Daisy drum demo failed;' if args.daisy_drums else b'Plaits stress failed;' if args.plaits_stress else b'Benchmark failed;'
    try:
        response = link.command(command, min(args.timeout, 30))
    except (OSError, TimeoutError):
        # The request may already be executing while the main task is busy
        # writing DATA; never issue it a second time on an ambiguous result.
        print('Request response unavailable; checking logs for its outcome.', flush=True)
    else:
        print(response, flush=True)
        if 'ok=1' not in response:
            raise SystemExit('Benchmark request rejected')
    end = time.monotonic() + args.timeout
    data = bytearray()
    while time.monotonic() < end:
        try:
            line = link.command('LOG', 5)
            hex_data = line.removeprefix('GBX1 LOG ')
            if hex_data != '-':
                data.extend(bytes.fromhex(hex_data))
                if len(data) > 65536:
                    del data[:-32768]
                if success in data:
                    if args.daisy_drums and b'Daisy drum demo playing six published voices' not in data:
                        raise SystemExit('Daisy drum demo marker not observed')
                    if args.plaits_stress and args.tempo == 300 and b'Plaits stress tempo 300' not in data:
                        raise SystemExit('Stress tempo confirmation not observed')
                    print('FX stress loaded; playback requested for 30 seconds.' if args.fx_stress else 'Daisy drum demo loaded; playback requested.' if args.daisy_drums else 'Plaits stress loaded; playback requested.' if args.plaits_stress else 'Benchmark file loaded; playback requested.', flush=True)
                    return
                if failure in data:
                    raise SystemExit('Firmware could not load benchmark; inspect project or sample asset')
            else:
                time.sleep(0.25)
        except (OSError, TimeoutError):
            time.sleep(1)
    raise SystemExit('Benchmark outcome was not observed before timeout; inspect device logs')

if __name__ == '__main__':
    main()
