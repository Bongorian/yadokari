#!/usr/bin/env python3
"""Read-only MK3 USB/HID observations on a Linux development host.

No feature or output report is written. Optional capture creates a new folder;
serial strings and audio are never read. This is not a Circle hardware test.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path


def report_lengths(data):
    sizes = {}
    size = count = report_id = 0
    stack = []
    pos = 0
    while pos < len(data):
        prefix = data[pos]
        pos += 1
        if prefix == 0xfe:
            if pos + 2 > len(data):
                raise ValueError('truncated long item')
            length = data[pos]
            pos += 2
            if pos + length > len(data):
                raise ValueError('truncated long payload')
            pos += length
            continue
        length = (0, 1, 2, 4)[prefix & 3]
        if pos + length > len(data):
            raise ValueError('truncated HID item')
        value = int.from_bytes(data[pos:pos + length], 'little')
        pos += length
        kind, tag = (prefix >> 2) & 3, prefix >> 4
        if kind == 1:
            if tag == 7:
                size = value
            elif tag == 8:
                if not 0 < value < 256:
                    raise ValueError('invalid report ID')
                report_id = value
            elif tag == 9:
                count = value
            elif tag == 10:
                if len(stack) == 8:
                    raise ValueError('global stack overflow')
                stack.append((size, count, report_id))
            elif tag == 11:
                if not stack:
                    raise ValueError('global stack underflow')
                size, count, report_id = stack.pop()
        elif kind == 0 and tag in (8, 9, 11) and report_id:
            key = (report_id, {8: 'input', 9: 'output', 11: 'feature'}[tag])
            sizes[key] = sizes.get(key, 0) + size * count
            if sizes[key] > 65528:
                raise ValueError('report too large')
    if stack or any(bits % 8 for bits in sizes.values()):
        raise ValueError('invalid global state or non-byte report')
    return {key: bits // 8 + 1 for key, bits in sizes.items()}


def locate():
    found = []
    for node in sorted(Path('/sys/class/hidraw').glob('hidraw*')):
        parent = (node / 'device').resolve()
        for device in (parent, *parent.parents):
            vendor, product = device / 'idVendor', device / 'idProduct'
            if vendor.exists() and product.exists():
                if vendor.read_text().strip().lower() == '17cc' and product.read_text().strip().lower() == '1600':
                    found.append((node, device))
                break
    if len(found) != 1:
        raise RuntimeError(f'expected one MK3 HID interface; found {len(found)}')
    return found[0]


def audio_configuration(data):
    """Summarize UAC2 descriptors; candidates are not proven stream pairings."""
    if len(data) < 9 or data[:2] != b'\x09\x02' or int.from_bytes(data[2:4], 'little') != len(data):
        raise ValueError('invalid configuration')
    terminals, streams = {}, {}
    interface = None
    pos = 0
    while pos < len(data):
        length = data[pos]
        if length < 2 or pos + length > len(data):
            raise ValueError('truncated USB descriptor')
        desc = data[pos:pos + length]
        pos += length
        if desc[1] == 4:
            if length < 9:
                raise ValueError('short interface descriptor')
            interface = (desc[2], desc[3], desc[5], desc[6], desc[7])
        elif interface and interface[2:] == (1, 1, 0x20) and desc[1] == 0x24 and length >= 9:
            if desc[2] == 2:
                terminals[desc[3]] = desc[7]
            elif desc[2] == 3:
                terminals[desc[3]] = desc[8]
        elif interface and interface[2:] == (1, 2, 0x20) and interface[1] != 0:
            stream = streams.setdefault(interface[:2], {'interface': interface[0], 'alternate': interface[1]})
            if desc[1] == 0x24 and length >= 16 and desc[2] == 1:
                stream.update(terminal=desc[3], channels=desc[10], pcm=bool(int.from_bytes(desc[6:10], 'little') & 1))
            elif desc[1] == 0x24 and length >= 6 and desc[2] == 2 and desc[3] == 1:
                stream.update(subslot=desc[4], bits=desc[5])
            elif desc[1] == 5 and length >= 7:
                stream.setdefault('endpoints', []).append({'address': desc[2], 'attributes': desc[3], 'packet_bytes': int.from_bytes(desc[4:6], 'little') & 0x7ff, 'interval': desc[6]})
    candidates = []
    for stream in streams.values():
        stream['clock'] = terminals.get(stream.get('terminal'))
    for tx in streams.values():
        for output in tx.get('endpoints', []):
            if output['address'] & 0x80 or (output['attributes'] & 0x3f) != 0x05:
                continue
            for rx in streams.values():
                for capture in rx.get('endpoints', []):
                    if not capture['address'] & 0x80 or (capture['attributes'] & 0x33) != 0x21:
                        continue
                    if tx['interface'] == rx['interface'] or not tx['clock'] or tx['clock'] != rx['clock']:
                        continue
                    if output['interval'] != capture['interval'] or not output['interval'] or not output['packet_bytes'] or not capture['packet_bytes']:
                        continue
                    if not tx.get('pcm') or not rx.get('pcm') or not tx.get('channels') or not rx.get('channels') or tx.get('bits') != 24 or rx.get('bits') != 24 or tx.get('subslot') not in (3, 4) or rx.get('subslot') not in (3, 4):
                        continue
                    candidates.append({'output': output['address'], 'input': capture['address'], 'clock': tx['clock']})
    return {'streams': list(streams.values()), 'implicit_candidates': candidates}


def observe(capture):
    node, usb = locate()
    descriptor = (node / 'device' / 'report_descriptor').read_bytes()
    lengths = report_lengths(descriptor)
    raw = (usb / 'descriptors').read_bytes()
    if len(raw) < 27 or raw[0] < 18 or raw[18:20] != b'\x09\x02':
        raise RuntimeError('unexpected USB configuration bytes')
    total = int.from_bytes(raw[20:22], 'little')
    if total < 9 or 18 + total > len(raw):
        raise RuntimeError('truncated USB configuration')
    configuration = raw[18:18 + total]
    summary = {'vid_pid': '17cc:1600', 'usb_speed_mbps': (usb / 'speed').read_text().strip(),
               'configuration_bytes': len(configuration), 'report_descriptor_bytes': len(descriptor),
               'hid_reports': {f'{kind}:{rid:02x}': length for (rid, kind), length in sorted(lengths.items())},
               'audio_descriptors': audio_configuration(configuration),
               'reads': {}}
    captures = {'config.bin': configuration, 'hid-descriptor.bin': descriptor}
    fd = os.open('/dev/' + node.name, os.O_RDONLY | os.O_NONBLOCK)
    try:
        for report_id, kind, ioctl_number in ((1, 'input', 0x0a), (2, 'input', 0x0a), (0x20, 'input', 0x0a), (0xfe, 'feature', 0x07), (0xff, 'feature', 0x07)):
            length = lengths.get((report_id, kind), 0)
            entry = {'declared_bytes': length}
            if not 1 <= length <= 4096:
                entry['error'] = 'missing or unsupported declared length'
            else:
                buf = bytearray(length)
                buf[0] = report_id
                request = (3 << 30) | (length << 16) | (ord('H') << 8) | ioctl_number
                try:
                    actual = fcntl.ioctl(fd, request, buf, True)
                    if actual < 0 or actual > length or (actual and buf[0] != report_id):
                        raise ValueError('invalid returned report')
                    payload = bytes(buf[:actual])
                    entry.update(actual_bytes=actual, sha256=hashlib.sha256(payload).hexdigest(), complete=actual == length)
                    captures[f'{kind}-{report_id:02x}.bin'] = payload
                except (OSError, ValueError) as error:
                    entry['error'] = str(error)
            summary['reads'][f'{kind}:{report_id:02x}'] = entry
    finally:
        os.close(fd)
    if capture:
        capture.mkdir(parents=True, exist_ok=False)
        for name, data in captures.items():
            (capture / name).write_bytes(data)
        (capture / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, help='Create a new directory for raw descriptors and returned reports')
    args = parser.parse_args()
    try:
        print(json.dumps(observe(args.capture), indent=2))
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Observation failed: {error}\n')
