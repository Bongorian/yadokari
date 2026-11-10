#!/usr/bin/env python3
"""Reject ELF images whose code, data or BSS overlaps Circle's stacks."""
import argparse
import subprocess


def check_symbols(output):
    wanted = {'_start', '_end', 'groove_kernel_max_size'}
    found = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) != 3 or fields[2] not in wanted:
            continue
        if fields[2] in found:
            raise ValueError('duplicate kernel bound symbol: ' + fields[2])
        found[fields[2]] = int(fields[0], 16)
    if set(found) != wanted:
        raise ValueError('missing kernel bound symbols: ' + ', '.join(sorted(wanted - set(found))))
    start, end, limit = found['_start'], found['_end'], found['groove_kernel_max_size']
    if not limit or end < start:
        raise ValueError('invalid Circle kernel bounds')
    used = end - start
    if used > limit:
        raise ValueError(f'kernel including BSS needs {used} bytes; Circle reserves {limit}')
    return used, limit


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--nm', required=True)
    p.add_argument('elf')
    p.add_argument('--minimum-free', type=int, default=65536, help='reserved image headroom in bytes')
    args = p.parse_args()
    try:
        output = subprocess.check_output([args.nm, '-n', args.elf], text=True)
        used, limit = check_symbols(output)
    except (OSError, subprocess.CalledProcessError, ValueError) as exc:
        raise SystemExit('Kernel bounds check failed: ' + str(exc)) from exc
    free = limit - used
    if args.minimum_free < 0 or free < args.minimum_free:
        raise SystemExit(f'Kernel margin check failed: free {free}, required {args.minimum_free}')
    print(f'Kernel including BSS: {used}/{limit} bytes; free {free}; required margin {args.minimum_free}')


if __name__ == '__main__':
    main()
