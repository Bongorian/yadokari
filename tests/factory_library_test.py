#!/usr/bin/env python3
"""Check provenance, sound files and 16-pad factory arrangements."""
import hashlib
import json
import sys
import wave
import zlib
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parents[1]
library = json.loads((root / "assets/factory/library.json").read_text())
lock = json.loads((root / "tools/content/factory_sources.lock.json").read_text())
sources = {(s["repo"], s["path"]): s for s in lock["sources"]}
assert len(library["samples"]) == len(sources) >= 300
assert len(library["kits"]) == 24
known = {}
for sample in library["samples"]:
    if sample["installed"]:
        path = root / sample["path"]
        assert path.is_file() and path.stat().st_size > 44
        assert hashlib.sha256(path.read_bytes()).hexdigest() == sample["sha256"]
        with wave.open(str(path)) as audio:
            assert audio.getframerate() == 48000 and audio.getnchannels() == 1
            assert audio.getsampwidth() == 2 and audio.getnframes() == sample["frames"]
        known[sample["path"]] = sample
    else:
        assert sample["path"] is None and sample["category"] == "multisample-source"
    assert -60 < sample["rms_dbfs"] < 0
    assert -40 < sample["peak_dbfs"] <= 0
    assert sample["license"] in ("CC0-1.0", "Public-Domain")
    assert sample["license_url"].startswith("https://github.com/")
    assert (sample["source_repo"], sample["source_path"]) in sources
for kit in library["kits"]:
    assert len(kit["pads"]) == 16 and all(p in known for p in kit["pads"])
    assert len(set(kit["pads"])) >= 12
assert all(known[p]["source_repo"] == "octal" for p in library["kits"][1]["pads"][:3])
assert {kit["name"] for kit in library["kits"]} >= {"808 Classic", "909 Classic", "CR-78 Vintage", "LinnDrum LM-2"}
assert len(library["multisample_presets"]) == 16
for preset in library["multisample_presets"]:
    path = root / preset["path"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == preset["sha256"]
    with wave.open(str(path)) as audio:
        assert audio.getnframes() == preset["frames"] and audio.getframerate() == 48000
    zones = preset["zones"]
    assert 1 <= len(zones) <= 16 and zones[-1]["end_fraction"] == 1.0
    assert all(z["key_low"] <= z["root"] <= z["key_high"] for z in zones)
assert len(library["loops"]) == 8
for loop in library["loops"]:
    path = root / loop["path"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == loop["sha256"]
    with wave.open(str(path)) as audio:
        assert audio.getnframes() == loop["frames"] and audio.getframerate() == 48000
        pcm = audio.readframes(audio.getnframes())
        assert abs(int.from_bytes(pcm[:2], "little", signed=True)) < 64
        assert abs(int.from_bytes(pcm[-2:], "little", signed=True)) < 64
    assert loop["bpm"] in range(20, 301) and loop["source_samples"]
for folder in (root / "assets/factory").rglob("*"):
    if folder.is_dir():
        assert len(list(folder.iterdir())) <= 64, folder
# The committed factory masters live in assets/factory-library;
# the assets/presets + assets/kits shelves this test once read
# were uncommitted working fixtures that a clean clone lacks.
assert len(list((root / "assets/factory-library/PRESETS").glob("TB-303 *.mgxp"))) == 2
for project in (list((root / "assets/factory-library/KITS").glob("*.mgxk")) +
                list((root / "assets/factory-library/PRESETS").glob("*.mgxp"))):
    payload = project.read_bytes()
    assert payload[:4] == b"MGXC", project
    assert zlib.crc32(payload[4:-4]) == int.from_bytes(payload[-4:], "little"), project
print("Factory library checked:", len(known), "WAVs,", len(library["kits"]), "kits")
