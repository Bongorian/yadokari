#!/usr/bin/env python3
"""Check owned Markdown classification and non-archival local links (offline)."""
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'docs/catalog.json'
KINDS = {'current', 'reference', 'evidence', 'archive'}


def main():
    entries = json.loads(CATALOG.read_text())['documents']
    owned = {'README.md', 'AGENTS.md', 'assets/README.md'}
    owned.update(str(p.relative_to(ROOT)) for p in (ROOT / 'docs').rglob('*.md'))
    for directory in ('assets/factory-library', 'assets/factory-synth-drums'):
        owned.update(str(p.relative_to(ROOT)) for p in (ROOT / directory).rglob('*.md'))
    errors = []
    for name in sorted(owned - entries.keys()):
        errors.append(f'{name}: not classified in docs/catalog.json')
    for name, kind in sorted(entries.items()):
        path = ROOT / name
        if name not in owned or not path.is_file():
            errors.append(f'{name}: stale catalog entry')
            continue
        if kind not in KINDS:
            errors.append(f'{name}: invalid classification {kind}')
        if kind == 'archive':
            continue  # Historical code paths and claims are not current contracts.
        source = re.sub(r'^```[^\n]*\n.*?^```\s*$', '', path.read_text(), flags=re.M | re.S)
        links = re.findall(r'!?\[[^\]]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\)', source)
        links += re.findall(r'^\[[^\]]+\]:\s*(\S+)', source, flags=re.M)
        for link in links:
            target = urlsplit(link.strip('<>'))
            if target.scheme or target.netloc or not target.path:
                continue
            destination = (path.parent / unquote(target.path)).resolve()
            if not destination.is_relative_to(ROOT) or not destination.exists():
                errors.append(f'{name}: missing local link {link}')
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print(f'Docs OK: {len(entries)} classified Markdown files; current/reference/evidence local paths exist.')
    print('Historical links, URL reachability, fragments, and technical claims are outside this check.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
