#!/usr/bin/env python3
"""Run one bounded Ethernet diagnostic; observe through delays, never resend it."""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'update'))
from remote import NetworkLink, parse_status

MARKERS = {
    'DEPLOYCHECK': r'Deploy functional completed passed (\d+) ok 1 restored 1',
    'UITEST': r'UI-only diagnostic completed idle 1; no DSP or Fluid chaining',
    'PRESETCHECK': r'Preset functional check saved mounted 12/12 restored cleaned ok 1',
    'SAVETEST': r'Project probe byte-readback 1 cleaned 1 path GROOVE/CHECK-\d+\.mgxs',
}


def file_open_rejected():
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'files'))
    from usb_files import Client, FileError
    try:
        with Client(timeout=20):
            return {'rejected': False}
    except FileError as exc:
        return {'rejected': exc.status in (1, 2), 'error': str(exc), 'status': exc.status}


def verify_admission(link):
    checks = []
    for command in ('ENTER', 'PRESETCHECK'):
        try:
            link.command(command, 20)
        except RuntimeError as exc:
            checks.append({'command': command, 'rejected': True, 'error': str(exc)})
        else:
            raise RuntimeError('Unexpected admission during diagnostic: ' + command)
    result = file_open_rejected()
    assert result['rejected'], 'File service opened during diagnostic'
    checks.append({'command': 'MGF1 OPEN', **result})
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--network', required=True)
    parser.add_argument('--version', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--command', choices=MARKERS, default='DEPLOYCHECK')
    parser.add_argument('--seconds', type=float, default=180)
    parser.add_argument('--verify-admission', action='store_true', help='Verify busy rejects ENTER, PRESETCHECK and MGF1 OPEN; needs USB access')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Evidence exists; use another output path')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    link = NetworkLink(args.network)
    report = {'command': args.command, 'mutations_sent': 0, 'statuses': [], 'transport_errors': []}
    raw = bytearray()
    started = time.monotonic()

    def save():
        report['seconds'] = time.monotonic() - started
        report['raw_log'] = raw.decode(errors='replace')
        args.output.write_text(json.dumps(report, indent=2) + '\n')

    def status():
        current = parse_status(link.command('STATUS', 20))
        assert current['version'] == args.version and current['highest'] == args.version
        assert current['ready'] and not current['error'] and not current['active'] and not current['phase']
        return current

    try:
        report['before'] = status()
        assert report['before'].get('diagnostic_busy') == 0, 'Diagnostic owner is busy'
        report['hardware_before'] = link.command('HARDWARE', 20)
        history = bytearray()
        for _ in range(2048):
            line = link.command('LOG', 20)
            if line == 'GBX1 LOG -':
                break
            history.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
        else:
            raise RuntimeError('History did not drain; no diagnostic sent')
        args.output.with_suffix('.history.log').write_bytes(history)
        report['mutations_sent'] = 1
        save()
        # Even if the reply is lost, do not issue this mutation again.
        try:
            report['reply'] = link.command(args.command, 20)
        except (OSError, TimeoutError) as exc:
            report['reply_error'] = repr(exc)
        next_status = 0
        while time.monotonic() - started < args.seconds:
            try:
                line = link.command('LOG', 20)
                if line != 'GBX1 LOG -':
                    raw.extend(bytes.fromhex(line.removeprefix('GBX1 LOG ')))
                else:
                    time.sleep(.04)
                if time.monotonic() >= next_status:
                    current = status()
                    report['statuses'].append({'seconds': time.monotonic() - started, **current})
                    if args.verify_admission and current['diagnostic_busy'] and 'admission' not in report:
                        # Use this same synchronous client, avoiding a second
                        # control reader racing the diagnostic observer.
                        report['admission'] = verify_admission(link)
                    next_status = time.monotonic() + .2
                text = raw.decode(errors='replace').rsplit('\n', 1)[0]
                if 'Application firmware ' in text or 'Circle 51' in text:
                    raise RuntimeError('Unexpected reboot')
                if re.search(MARKERS[args.command], text):
                    current = status()
                    if not current['diagnostic_busy']:
                        report['after'] = current
                        report['matched'] = True
                        break
                if re.search(r'Deploy functional completed passed \d+ ok 0|Preset functional check saved mounted \d+/\d+ restored cleaned ok 0|Deploy functional requested started 0|Preset functional check started 0', text):
                    raise RuntimeError('Diagnostic rejected or failed; no mutation retry')
            except (OSError, TimeoutError) as exc:
                report['transport_errors'].append({'seconds': time.monotonic() - started, 'error': repr(exc)})
                time.sleep(.1)
            save()
        assert report.get('matched'), 'Completion/idle not observed before deadline'
        if args.verify_admission:
            assert len(report.get('admission', [])) == 3, 'Busy admission was not observed'
        if args.command == 'UITEST':
            assert 'DSP suite' not in raw.decode(errors='replace'), 'Unexpected diagnostic chaining'
        report['hardware_after'] = link.command('HARDWARE', 20)
        before = int(re.search(r'uptime_s=(\d+)', report['hardware_before'])[1])
        after = int(re.search(r'uptime_s=(\d+)', report['hardware_after'])[1])
        assert after >= before, 'Uptime reset'
        report['ok'] = True
    except Exception as exc:
        report['failure'] = repr(exc)
        try:
            report['ending_status'] = parse_status(link.command('STATUS', 10))
        except Exception as ending:
            report['ending_status_error'] = repr(ending)
    finally:
        save()
        link.close()
    print(json.dumps({k: v for k, v in report.items() if k not in ('raw_log', 'statuses')}, indent=2))
    return 0 if report.get('ok') else 1


if __name__ == '__main__':
    raise SystemExit(main())
