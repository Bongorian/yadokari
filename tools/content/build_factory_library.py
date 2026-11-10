#!/usr/bin/env python3
"""Build the reproducible, separately installable MK3 factory audio library."""
import concurrent.futures
import hashlib
import io
import json
import re
import subprocess
import urllib.parse
import urllib.request
import wave
import warnings
from pathlib import Path

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads((ROOT / "tools/content/factory_sources.lock.json").read_text())
CACHE = ROOT / ".deps/factory-sources"
OUT = ROOT / "assets/factory"
KITS = ROOT / "build/factory-kits.tsv"
MULTIS = ROOT / "build/factory-multis.tsv"
KINDS = ("kick", "snare", "hat_closed", "hat_open", "clap", "rim", "tom",
         "tom", "perc", "perc", "perc", "perc", "cymbal", "fx", "kick", "snare")
KIT_FAMILIES = [
    ("808 Classic", "fischer808", ""), ("909 Classic", "octal", ""),
    ("606 Acid", "fischer808", ""), ("707 House", "octal", ""),
    ("727 Latin", "vcsl", ""), ("CR-78 Vintage", "oramics", "CR-78"),
    ("LinnDrum LM-2", "oramics", "LM-2"), ("DMX Drumtraks", "oramics", "LM-2"),
    ("Acoustic Studio", "vcsl", ""), ("Acoustic Jazz", "vcsl", ""),
    ("Boom Bap", "oramics", "LM-2"), ("Trap Drill", "octal", ""),
    ("Deep House", "octal", ""), ("Industrial Techno", "octal", ""),
    ("Breakbeat Jungle", "oramics", "LM-2"),
    ("Glitch Experimental", "octal", ""),
    ("Minimal Techno", "octal", ""), ("Hard Techno", "octal", ""),
    ("Psytrance", "octal", ""), ("Melodic Techno", "octal", ""),
    ("Electro", "fischer808", ""), ("Lo-fi Hip-Hop", "oramics", "CR-78"),
    ("Disco Funk", "oramics", "LM-2"), ("Ambient Percussion", "vcsl", ""),
]


def classify(source):
    path = source["path"].lower()
    repo = source["repo"]
    if repo == "fischer808":
        prefix = path.split("/")[0]
        return {"bd8": "kick", "sd8": "snare", "ch8": "hat_closed",
                "oh8": "hat_open", "cp8": "clap", "rs8": "rim",
                "lc8": "cymbal", "cy8": "cymbal", "hc8": "conga",
                "cb8": "cowbell", "ma8": "perc", "mc8": "perc",
                "ht8": "tom", "lt8": "tom", "mt8": "tom",
                "cl8": "perc"}.get(prefix, "perc")
    if repo == "vcsl":
        role = source["role"].split(":")[-1].lower()
        return {"studiokick": "kick", "studiosnare": "snare",
                "hihat": "hat_closed", "claps": "clap"}.get(role, role)
    if "kicks/" in path or "kick" in path or "bassdrum" in path:
        return "kick"
    if "snare" in path: return "snare"
    if "hats/closed" in path or "hihat-closed" in path or "hihat.wav" in path: return "hat_closed"
    if "hats/open" in path or "hihat-open" in path or "hihat-metal" in path: return "hat_open"
    if "clap" in path: return "clap"
    if "rim" in path: return "rim"
    if "tom" in path: return "tom"
    if "cymbal" in path or "crash" in path or "ride" in path: return "cymbal"
    if "fx/" in path or "riser" in path or "noise" in path: return "fx"
    if "conga" in path: return "conga"
    if "bongo" in path: return "bongo"
    if "cow" in path: return "cowbell"
    return "perc"


def download(source):
    repo = LOCK["repositories"][source["repo"]]
    path = source["path"]
    name = hashlib.sha256((source["repo"] + ":" + path).encode()).hexdigest() + ".wav"
    cached = CACHE / name
    if cached.exists():
        data = cached.read_bytes()
    else:
        url = (f"https://raw.githubusercontent.com/{repo['repo']}/{repo['commit']}/"
               + urllib.parse.quote(path, safe="/"))
        request = urllib.request.Request(url, headers={"User-Agent": "mk3-factory-builder"})
        with urllib.request.urlopen(request, timeout=90) as response:
            data = response.read()
    digest = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
    if digest != source["git_blob"]:
        raise ValueError(f"source checksum mismatch: {source['repo']}/{path}")
    if not cached.exists():
        cached.write_bytes(data)
    return data


def verify_licenses():
    for short, repository in LOCK["repositories"].items():
        for evidence in repository["license_evidence"]:
            path = evidence["path"]
            name = hashlib.sha256((short + ":license:" + path).encode()).hexdigest()
            cached = CACHE / name
            if cached.exists():
                data = cached.read_bytes()
            else:
                url = (f"https://raw.githubusercontent.com/{repository['repo']}/"
                       f"{repository['commit']}/" + urllib.parse.quote(path, safe="/"))
                request = urllib.request.Request(url, headers={"User-Agent": "mk3-factory-builder"})
                with urllib.request.urlopen(request, timeout=30) as response:
                    data = response.read()
            if hashlib.sha256(data).hexdigest() != evidence["sha256"]:
                raise ValueError(f"license evidence changed: {short}/{path}")
            if not cached.exists():
                cached.write_bytes(data)


def license_for(source):
    if source["repo"] == "oramics":
        instrument = source["path"].split("/")[1]
        evidence = f"DM/{instrument}/README.md"
        license_id = "Public-Domain"
    else:
        evidence = "LICENSE"
        license_id = "CC0-1.0"
    repository = LOCK["repositories"][source["repo"]]
    assert evidence in [e["path"] for e in repository["license_evidence"]]
    return license_id, (f"https://github.com/{repository['repo']}/blob/"
                        f"{repository['commit']}/{urllib.parse.quote(evidence, safe='/')}")


def decode(data):
    command = ["ffmpeg", "-v", "error", "-i", "pipe:0", "-f", "wav",
               "-acodec", "pcm_s16le", "-ac", "1", "-ar", "48000", "pipe:1"]
    result = subprocess.run(command, input=data, capture_output=True, check=True)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", wavfile.WavFileWarning)
        rate, pcm = wavfile.read(io.BytesIO(result.stdout))
    if rate != 48000: raise ValueError("resample failed")
    audio = pcm.astype(np.float32) / 32768.0
    if not len(audio) or not np.isfinite(audio).all():
        raise ValueError("empty or nonfinite decoded audio")
    return audio


def finish(audio, maximum):
    audio = audio[:int(48000 * maximum)]
    active = np.flatnonzero(np.abs(audio) > 0.0005)
    if len(active):
        begin = max(0, int(active[0]) - 240)
        end = min(len(audio), int(active[-1]) + 2400)
        audio = audio[begin:end]
    else:
        raise ValueError("source is silent")
    peak = float(np.max(np.abs(audio)))
    gain = min(8.0, 0.72 / peak)
    audio = audio * gain
    fade = min(480, len(audio) // 4)
    audio[:fade] *= np.linspace(0.0, 1.0, fade, dtype=np.float32)
    audio[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)
    return audio, gain


def save_wav(path, audio):
    data = np.asarray(np.clip(audio * 32767, -32768, 32767), dtype="<i2")
    with wave.open(str(path), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(48000)
        file.writeframes(data.tobytes())
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_one(item):
    index, source = item
    data = download(source)
    try:
        audio, gain = finish(decode(data), 5.0 if source.get("zones") else 3.0)
    except (ValueError, subprocess.CalledProcessError) as error:
        raise ValueError(f"cannot convert {source['repo']}/{source['path']}: {error}") from error
    category = classify(source) if source["role"] != "zone" else "multisample-source"
    origin = Path(source["path"]).stem
    origin = re.sub(r"[^A-Za-z0-9]+", "-", origin).strip("-")[:24] or "Sound"
    family = {"fischer808": "808", "octal": "909", "oramics": "Vintage",
              "vcsl": "VCSL", "vsco": "VSCO"}.get(source["repo"], source["repo"])
    name = f"F-{origin}-{family}-{category}-{index:04d}.wav"
    group = source.get("zones", [{}])[0].get("preset", category)
    installed = source["role"] != "zone"
    folder = OUT / source["repo"] / group if installed else CACHE / "converted"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / name
    digest = save_wav(path, audio)
    license_id, license_url = license_for(source)
    peak = float(np.max(np.abs(audio)))
    rms = float(np.sqrt(np.mean(np.square(audio, dtype=np.float64))))
    if peak > 0.9 or peak < 0.001 or abs(float(audio[0])) > 0.001 or abs(float(audio[-1])) > 0.001:
        raise ValueError(f"factory audio quality check failed: {source['path']}")
    return {"id": name[:-4], "path": str(path.relative_to(ROOT)),
            "source_repo": source["repo"], "source_path": source["path"],
            "source_git_blob": source["git_blob"], "sha256": digest,
            "frames": len(audio), "rate": 48000, "channels": 1,
            "root": source.get("zones", [{}])[0].get("root", 60),
            "category": category,
            "installed": installed,
            "tags": [source["repo"], category], "bpm": None,
            "license": license_id, "license_url": license_url,
            "peak_dbfs": round(20 * np.log10(peak), 2),
            "rms_dbfs": round(20 * np.log10(rms), 2),
            "gain": round(gain, 4), "zones": source.get("zones", [])}


def choose_kit(index, family, subset, entries):
    selected = []
    for pad, kind in enumerate(KINDS):
        def rank(e):
            path = e["source_path"].lower()
            preferred = e["source_repo"] == family and (not subset or subset.lower() in path)
            category = e["category"]
            match = category == kind or (kind == "perc" and category in
                    ("perc", "bongo", "conga", "cowbell", "shaker", "tambourine"))
            near = kind == "perc" and category not in ("kick", "snare", "fx")
            uniqueness = e["id"] not in selected
            return (not match, not preferred, not near, not uniqueness,
                    (index * 7 + pad * 11 + int(e["id"].rsplit("-", 1)[-1])) % 53, e["id"])
        candidate = min(entries, key=rank)
        selected.append(candidate["id"])
    return selected


def build_multis(entries):
    grouped = {name: [] for name in LOCK["presets"]}
    for entry in entries:
        for zone in entry["zones"]:
            grouped[zone["preset"]].append((int(zone["root"]), int(zone["velocity"]), entry))
    output = []
    lines = []
    folder = OUT / "multis"
    folder.mkdir(exist_ok=True)
    for name, candidates in grouped.items():
        unique = {(root, velocity, e["id"]): (root, velocity, e)
                  for root, velocity, e in candidates}
        selected = sorted(unique.values(), key=lambda x: (x[0], x[1], x[2]["id"]))
        assert 1 <= len(selected) <= 16, name
        audio = []
        zones = []
        roots = sorted({r for r, _, _ in selected})
        frames = 0
        for root, velocity, entry in selected:
            with wave.open(str(ROOT / entry["path"])) as file:
                pcm = (np.frombuffer(file.readframes(file.getnframes()), dtype="<i2")
                       .astype(np.float32) / 32768 / entry["gain"])
            audio.append(pcm)
            frames += len(pcm)
            at = roots.index(root)
            low = 0 if at == 0 else (roots[at-1] + root)//2 + 1
            high = 127 if at == len(roots)-1 else (root + roots[at+1])//2
            same = sorted({v for r, v, _ in selected if r == root})
            level = same.index(velocity)
            vel_low = 1 if level == 0 else 1 + level*127//len(same)
            vel_high = (level+1)*127//len(same)
            zones.append({"root": root, "key_low": low, "key_high": high,
                          "velocity_low": vel_low, "velocity_high": vel_high,
                          "end_frame": frames, "source": entry["id"]})
        packed = np.concatenate(audio)
        packed *= 0.72 / max(0.001, float(np.max(np.abs(packed))))
        path = folder / f"M-{name}.wav"
        # Restore each source's original dynamics, then set one common preset gain.
        save_wav(path, packed)
        for zone in zones:
            zone["end_fraction"] = zone["end_frame"] / frames
        relative = str(path.relative_to(ROOT))
        output.append({"name": name, "path": relative, "zones": zones,
                       "frames": frames, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        lines.append("\t".join([name, relative, str(len(zones))] +
                     [",".join(map(str, (z["root"], z["key_low"], z["key_high"],
                                          z["velocity_low"], z["velocity_high"],
                                          z["end_fraction"]))) for z in zones]))
    MULTIS.write_text("\n".join(lines) + "\n")
    return output


def build_loops(kit_lines, entries):
    by_path = {entry["path"]: entry for entry in entries}
    recipes = [
        ("Funk-104", 104, 22), ("Breakbeat-132", 132, 14),
        ("Jungle-172", 172, 14), ("House-124", 124, 12),
        ("Techno-138", 138, 13), ("Trap-140", 140, 11),
        ("Electro-118", 118, 20), ("Ambient-90", 90, 23),
    ]
    folder = OUT / "loops"
    folder.mkdir(exist_ok=True)
    output = []
    for name, bpm, kit_index in recipes:
        pads = kit_lines[kit_index].split("\t")[1:]
        beat = 48000 * 60 / bpm
        length = int(round(beat * 16))
        mix = np.zeros(length, dtype=np.float32)
        cache = {}
        events = []
        for bar in range(4):
            for step in range(16):
                position = int(round((bar * 4 + step / 4) * beat))
                if step in (0, 8) or (name.startswith("Jungle") and step == 11):
                    events.append((position, pads[0], 0.64))
                if step in (4, 12) or (name.startswith("Breakbeat") and step == 14):
                    events.append((position, pads[1], 0.45))
                if step % 2 == 0:
                    events.append((position, pads[2], 0.18))
                if step in (3, 7, 11, 15):
                    events.append((position, pads[8 + (step // 4) % 4], 0.13))
        for position, path, level in events:
            if path not in cache:
                with wave.open(str(ROOT / path)) as file:
                    cache[path] = np.frombuffer(file.readframes(file.getnframes()),
                                                 dtype="<i2").astype(np.float32) / 32768
            sample = cache[path]
            count = min(len(sample), length-position)
            mix[position:position+count] += sample[:count] * level
        peak = float(np.max(np.abs(mix)))
        if peak > 0.82: mix *= 0.82 / peak
        fade = min(240, length//8)
        mix[:fade] *= np.linspace(0, 1, fade, dtype=np.float32)
        mix[-fade:] *= np.linspace(1, 0, fade, dtype=np.float32)
        path = folder / f"L-{name}.wav"
        digest = save_wav(path, mix)
        output.append({"name": name, "path": str(path.relative_to(ROOT)),
                       "bpm": bpm, "bars": 4, "root": 60, "category": "loop",
                       "tags": [name.split("-")[0].lower(), "drum-loop"],
                       "license": "CC0-1.0", "source_samples":
                       [by_path[p]["id"] for p in sorted(cache)],
                       "sha256": digest, "frames": length})
    return output


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    verify_licenses()
    OUT.mkdir(parents=True, exist_ok=True)
    for pattern in ("F-*.wav", "M-*.wav", "L-*.wav"):
        for old in OUT.rglob(pattern):
            old.unlink()
    KITS.parent.mkdir(parents=True, exist_ok=True)
    sources = LOCK["sources"]
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as workers:
        entries = list(workers.map(build_one, enumerate(sources, 1)))
    by_id = {e["id"]: e for e in entries}
    multis = build_multis(entries)
    lines = []
    for index, (name, family, subset) in enumerate(KIT_FAMILIES):
        chosen = choose_kit(index, family, subset,
                            [e for e in entries if e["category"] != "multisample-source"])
        lines.append("\t".join([name] + [by_id[x]["path"] for x in chosen]))
    KITS.write_text("\n".join(lines) + "\n")
    loops = build_loops(lines, entries)
    library = {
        "version": 1, "format": "PCM16 mono 48000 Hz",
        "source_lock": "tools/content/factory_sources.lock.json",
        "kits": [{"name": n, "style_inspired": n not in
                  ("808 Classic", "CR-78 Vintage", "LinnDrum LM-2"),
                  "pads": line.split("\t")[1:]} for (n, _, _), line in zip(KIT_FAMILIES, lines)],
        "samples": [{**e, "path": e["path"] if e["installed"] else None}
                    for e in entries],
        "multisample_presets": multis, "loops": loops,
        "repositories": LOCK["repositories"],
        "note": "Machine names are arrangement/style labels; 909 is generated CC0 "
                "909-style synthesis. This is not a vendor ROM collection.",
    }
    (OUT / "library.json").write_text(json.dumps(library, indent=2) + "\n")
    assert len(library["kits"]) == 24 and len(entries) >= 300
    assert all(len(k["pads"]) == 16 for k in library["kits"])
    print(f"Factory library: {len(entries)} checked WAVs, {len(lines)} kits, "
          f"{len(multis)} multisample instruments, {len(loops)} loops, "
          f"{sum(p.stat().st_size for p in OUT.rglob('*.wav'))//1000000} MB installed")


if __name__ == "__main__":
    main()
