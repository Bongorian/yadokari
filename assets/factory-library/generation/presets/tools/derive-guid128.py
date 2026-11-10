#!/usr/bin/env python3
"""Fill/refresh the `guid128` field of the factory-content identity manifests.

Identity model (firmware `src/preset_identity.h`, MGXC v12): a preset/kit
identity is an opaque 128-bit GUID, printed as 32 lowercase hex chars in
byte order, plus a non-zero u32 revision, an origin (User=0 / Factory=1)
and an optional parent (forks). Factory content has no parent.

Value sources, in priority order:
  1. Repo-pinned: `tools/factory-library-identities.json` (a snapshot of
     the firmware repo's `assets/factory-library/identities.json`, master
     760f8a7f). Those values are a fixed allocation and are adopted
     verbatim; they are never recomputed.
  2. Derived: UUID v5 (RFC 4122/9562, Python `uuid.uuid5`) under a fixed
     project namespace:
       ns_bytes = SHA-256(b"bongorian-mk3-factory-content-namespace")[0:16]
       ns       = UUID(bytes=ns_bytes, version=5)   # sets version/variant
       guid128  = uuid5(ns, "preset:" + slug).hex   # presets
       guid128  = uuid5(ns, "kit:"    + slug).hex   # kits
     where `slug` is the manifest label minus its `bongorian.preset.` /
     `bongorian.kit.` prefix. The text form is the 32-char lowercase hex
     of the UUID bytes, matching `preset_guid_text`.

Usage: derive-guid128.py [--check]
  Without --check the manifests are rewritten in place. With --check
  nothing is written; every derived value is recomputed from scratch and
  compared, and counts/uniqueness/form are verified. Exit code is
  non-zero on any mismatch.
"""
import hashlib
import json
import sys
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
PRESET_MANIFEST = HERE.parent / "preset-guids.json"
KIT_MANIFEST = HERE.parent.parent / "kits" / "kit-guids.json"
PINNED = HERE / "factory-library-identities.json"

NAMESPACE_SEED = b"bongorian-mk3-factory-content-namespace"


def project_namespace() -> uuid.UUID:
    raw = hashlib.sha256(NAMESPACE_SEED).digest()[:16]
    return uuid.UUID(bytes=raw, version=5)


def derive(kind: str, slug: str) -> str:
    return uuid.uuid5(project_namespace(), f"{kind}:{slug}").hex


def pinned_map() -> dict:
    data = json.loads(PINNED.read_text())
    out = {}
    for entry in data["files"]:
        shelf, rest = entry["master"].split("/", 1)
        name = rest.rsplit(".", 1)[0]
        kind = "preset" if shelf == "PRESETS" else "kit"
        out[(kind, name)] = entry["guid"]
    return out


def slug_of(kind: str, label: str) -> str:
    prefix = f"bongorian.{kind}."
    assert label.startswith(prefix), label
    return label[len(prefix):]


def process(path: Path, kind: str, pinned: dict, write: bool) -> list:
    manifest = json.loads(path.read_text())
    problems = []
    for name, entry in manifest.items():
        slug = slug_of(kind, entry["guid"])
        pin = pinned.get((kind, name))
        if pin is not None:
            want, source = pin, "repo:assets/factory-library/identities.json"
        else:
            want, source = derive(kind, slug), "derived:uuid5-project-namespace"
        if entry.get("guid128") != want:
            if write:
                entry["guid128"] = want
                entry["guid128_source"] = source
            else:
                problems.append(f"{kind}:{name}: guid128 mismatch")
        elif not write and entry.get("guid128_source") != source:
            problems.append(f"{kind}:{name}: guid128_source mismatch")
        if write:
            entry["origin"] = "factory"
        elif entry.get("origin") != "factory":
            problems.append(f"{kind}:{name}: origin != factory")
    if write:
        path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    return problems


def main() -> int:
    check = "--check" in sys.argv
    pinned = pinned_map()
    problems = []
    problems += process(PRESET_MANIFEST, "preset", pinned, write=not check)
    problems += process(KIT_MANIFEST, "kit", pinned, write=not check)

    presets = json.loads(PRESET_MANIFEST.read_text())
    kits = json.loads(KIT_MANIFEST.read_text())
    all_guids = [e["guid128"] for e in presets.values()] + \
                [e["guid128"] for e in kits.values()]
    if len(presets) != 91:
        problems.append(f"preset count {len(presets)} != 91")
    if len(kits) != 24:
        problems.append(f"kit count {len(kits)} != 24")
    if len(set(all_guids)) != len(all_guids):
        problems.append("guid128 values are not unique across both manifests")
    for g in all_guids:
        if len(bytes.fromhex(g)) != 16 or g != g.lower() or len(g) != 32:
            problems.append(f"malformed guid128: {g}")
    derived = [n for n, e in presets.items()
               if e["guid128_source"].startswith("derived")]
    derived += [n for n, e in kits.items()
                if e["guid128_source"].startswith("derived")]
    print(f"namespace: {project_namespace()}")
    print(f"presets={len(presets)} kits={len(kits)} "
          f"total_guid128={len(all_guids)} unique={len(set(all_guids))}")
    print(f"repo-pinned: {len(all_guids) - len(derived)}, derived: {len(derived)}")
    if derived:
        print("derived entries:", ", ".join(sorted(derived)))
    if problems:
        print("PROBLEMS:")
        for p in problems:
            print(" -", p)
        return 1
    print("OK" if check else "written + verified OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
