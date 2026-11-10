#!/usr/bin/env python3
"""Pin a curated, redistributable subset of public sample libraries.

Run deliberately when changing source selection. Normal factory builds use the
resulting lock file and never depend on a moving GitHub branch.
"""
import hashlib
import json
import re
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools/content/factory_sources.lock.json"
REPOS = {
    "fischer808": ("tidalcycles/sounds-tr808-fischer", ["LICENSE"]),
    "octal": ("octalmusic/octal-samples", ["LICENSE", "LICENSES.md"]),
    "oramics": ("oramics/sampled", ["DM/CR-78/README.md", "DM/LM-2/README.md", "DM/TR-505/README.md"]),
    "vcsl": ("sgossner/VCSL", ["LICENSE"]),
    "vsco": ("sgossner/VSCO-2-CE", ["LICENSE", "Readme.txt"]),
}
ONE_SHOT_PREFIXES = {
    "fischer808": ("bd8/", "cb8/", "ch8/", "cl8/", "cp8/", "cy8/", "hc8/", "ht8/", "lc8/", "lt8/", "ma8/", "mc8/", "mt8/", "oh8/", "rs8/", "sd8/"),
    "octal": ("samples/",),
    "oramics": ("DM/CR-78/samples/", "DM/LM-2/samples/", "DM/TR-505/samples/"),
}
PERCUSSION = {
    "Bongo": "Membranophones/Struck Membranophones/Bongos/",
    "Conga": "Membranophones/Struck Membranophones/Conga/",
    "StudioKick": "Membranophones/Struck Membranophones/Bass Drum 2/",
    "StudioSnare": "Membranophones/Struck Membranophones/Snare Drum, Modern 1/",
    "Shaker": "Idiophones/Struck Idiophones/Shaker, Small/",
    "Tambourine": "Idiophones/Struck Idiophones/Tambourine 1/",
    "Cowbell": "Idiophones/Struck Idiophones/Cowbells/",
    "Claps": "Idiophones/Struck Idiophones/Claps/",
    "HiHat": "Idiophones/Struck Idiophones/Hi-Hat Cymbal/",
}
# One packed WAV per preset. C2-C5 are enough for a compact live instrument;
# the sampler repitches between zones. Repeated roots may carry velocity/RR.
PRESETS = {
    "Grand-Piano": ("vcsl", "Chordophones/Zithers/Grand Piano, Steinway B/NoSus/", (36, 48, 60, 72), 2),
    "Kawai-Piano": ("vcsl", "Chordophones/Zithers/Grand Piano, Kawai/Sustains/", (36, 48, 60, 72), 1),
    "Electric-Piano": ("vcsl", "Electrophones/TX81Z/FM Piano/", (36, 48, 60, 72), 2),
    "FM-Piano": ("vcsl", "Electrophones/TX81Z/Piano 1/", (36, 48, 60, 72), 1),
    "Clavisynth": ("vcsl", "Electrophones/TX81Z/Clavisynth/", (36, 48, 60, 72), 2),
    "Pipe-Organ": ("vcsl", "Aerophones/Edge-blown Aerophones/Pipe Organ/Quiet/", (36, 48, 60, 72), 1),
    "Vibraphone": ("vcsl", "Idiophones/Struck Idiophones/Vibraphone/Soft Mallets/", (48, 60, 72), 2),
    "Marimba": ("vcsl", "Idiophones/Struck Idiophones/Marimba/", (48, 60, 72), 2),
    "Xylophone": ("vcsl", "Idiophones/Struck Idiophones/Xylophone/Medium Mallets/", (48, 60, 72), 1),
    "Folk-Harp": ("vcsl", "Chordophones/Composite Chordophones/Folk Harp/", (36, 48, 60, 72), 1),
    "Kalimba": ("vcsl", "Idiophones/Plucked Idiophones/Kalimba, Tanzania/", (48, 60, 72), 1),
    "Solo-Violin": ("vsco", "Strings/Solo Violin/Arco Vib/", (57, 69, 81), 2),
    "Cello-Section": ("vsco", "Strings/Cello Section/susvib/", (45, 57, 69), 1),
    "Trumpet": ("vsco", "Brass/Trumpet/sus/", (48, 60, 72), 2),
    "Trombone": ("vsco", "Brass/Tenor Trombone/sus/", (36, 48, 60), 1),
    "Flute": ("vsco", "Woodwinds/Flute/susNV/", (60, 72, 84), 1),
}
NOTE = re.compile(r"(?:^|_)([A-G](?:#|b)?[0-8])(?:_|\.)", re.IGNORECASE)
VEL = re.compile(r"(?:^|_)(?:vl|v)([1-9])(?:_|\.)", re.IGNORECASE)


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "mk3-factory-source-lock"})
    with urllib.request.urlopen(req, timeout=30) as response:
        return response.read()


def midi(path):
    m = NOTE.search(Path(path).name)
    if not m:
        return None
    name = m[1].upper()
    semitone = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}[name[0]]
    if len(name) == 3:
        semitone += 1 if name[1] == "#" else -1
    return 12 * (int(name[-1]) + 1) + semitone


def velocity(path):
    m = VEL.search(Path(path).name)
    if m:
        return int(m[1])
    stem = Path(path).stem.lower()
    if stem.endswith("_p") or "_soft_" in stem:
        return 1
    if stem.endswith("_f") or "_loud_" in stem:
        return 4
    return 2


def main():
    repositories = {}
    all_files = {}
    for short, (repo, license_paths) in REPOS.items():
        commit = json.loads(get(f"https://api.github.com/repos/{repo}/commits/HEAD"))["sha"]
        tree = json.loads(get(f"https://api.github.com/repos/{repo}/git/trees/{commit}?recursive=1"))
        if tree.get("truncated"):
            raise SystemExit(f"truncated source tree: {repo}")
        files = {entry["path"]: entry for entry in tree["tree"] if entry["type"] == "blob"}
        licenses = []
        for path in license_paths:
            if path not in files:
                raise SystemExit(f"missing license statement: {repo}/{path}")
            data = get(f"https://raw.githubusercontent.com/{repo}/{commit}/{urllib.parse.quote(path)}")
            licenses.append({"path": path, "sha256": hashlib.sha256(data).hexdigest()})
        repositories[short] = {"repo": repo, "commit": commit, "license_evidence": licenses}
        all_files[short] = files
    selected = {}

    def add(repo, path, role, preset=None, root=None, vel=None):
        entry = all_files[repo][path]
        key = repo + ":" + path
        if key not in selected:
            selected[key] = {"repo": repo, "path": path, "git_blob": entry["sha"], "bytes": entry.get("size", 0), "role": role}
        if preset:
            selected[key].setdefault("zones", []).append({"preset": preset, "root": root, "velocity": vel})

    for repo, prefixes in ONE_SHOT_PREFIXES.items():
        for path in sorted(all_files[repo]):
            if path.lower().endswith(".wav") and path.startswith(prefixes):
                add(repo, path, "one-shot")
    for name, prefix in PERCUSSION.items():
        candidates = [e for p, e in all_files["vcsl"].items() if p.startswith(prefix) and p.lower().endswith(".wav") and e.get("size", 0) < 600000]
        for entry in sorted(candidates, key=lambda e: (e.get("size", 0), e["path"]))[:4]:
            add("vcsl", entry["path"], "percussion:" + name)
    for preset, (repo, prefix, targets, layers) in PRESETS.items():
        candidates = [e for p, e in all_files[repo].items() if p.startswith(prefix) and p.lower().endswith(".wav") and midi(p) is not None and e.get("size", 0) < 10000000]
        if not candidates:
            raise SystemExit(f"no source recordings: {preset} ({prefix})")
        for target in targets:
            roots = sorted({midi(e["path"]) for e in candidates}, key=lambda n: (abs(n - target), n))
            root = roots[0]
            matching = sorted((e for e in candidates if midi(e["path"]) == root), key=lambda e: (velocity(e["path"]), e.get("size", 0), e["path"]))
            levels = sorted({velocity(e["path"]) for e in matching})
            chosen_levels = [levels[0], levels[-1]] if layers > 1 and len(levels) > 1 else [levels[len(levels)//2]]
            for level in chosen_levels:
                entry = next(e for e in matching if velocity(e["path"]) == level)
                add(repo, entry["path"], "zone", preset, root, level)
    result = {"version": 1, "repositories": repositories, "sources": [selected[k] for k in sorted(selected)], "presets": list(PRESETS)}
    OUT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
    print(f"Pinned {len(selected)} source WAVs, {len(PRESETS)} multisample presets -> {OUT}")
    for repo in REPOS:
        count = sum(s["repo"] == repo for s in selected.values())
        size = sum(s["bytes"] for s in selected.values() if s["repo"] == repo)
        print(repo, count, round(size / 1e6, 1), "MB")


if __name__ == "__main__":
    import urllib.parse
    main()
