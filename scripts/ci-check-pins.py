#!/usr/bin/env python3
"""Fail-fast CI invariants for external inputs whose state outlives a run.

Coupling guarded here (keep these references in sync):

* ``docker/Dockerfile`` pins its apt universe with
  ``ARG UBUNTU_SNAPSHOT``.  The snapshot is not an arbitrary date:
  it must serve every package at exactly the version recorded in
  ``tools/content/catalog_environment.lock.json``.  The lock is
  the source of truth, and this script is the machine check that
  prevents "re-baseline the lock, forget the snapshot" (or the
  reverse) from being discovered only by the slow container job.
* ``.github/workflows/ci.yml`` caches content that already has
  repository inputs applied to it.  A cache key must therefore
  cover those inputs: the Circle checkout covers the pinned
  Circle commit and every patch script/patch file, and the Arm
  toolchain cache references ``ARM_TC_VERSION`` and the runner
  architecture.  An unregistered cache fails here instead of
  silently restoring stale state in a later run.

Run this before the expensive CI jobs.  Every failure names the
invariant and the files/packages/caches involved.
"""

from __future__ import annotations

import argparse
import fnmatch
import gzip
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterable

try:
    import yaml
except ImportError:  # pragma: no cover - exercised by the workflow guard
    raise SystemExit(
        "CI preflight invariant [python-dependency] failed: PyYAML is "
        "required to parse .github/workflows/ci.yml; install python3-yaml."
    )


INVARIANT = "CI preflight invariant"
UBUNTU_CODENAMES = {"24.04": "noble"}
SNAPSHOT_ROOT = "https://snapshot.ubuntu.com/ubuntu"
SUITE_SUFFIXES = ("", "-updates", "-security", "-backports")
# Search main first: on the happy path all lock packages resolve
# there, so preflight downloads only a few MB.  The other
# components are searched before declaring a package absent, so
# the verdict remains about the whole configured apt universe.
COMPONENT_SEARCH_ORDER = ("main", "universe", "restricted", "multiverse")


class InvariantFailure(Exception):
    """A named, actionable invariant failure."""


def fail(invariant: str, message: str) -> InvariantFailure:
    return InvariantFailure(f"[{invariant}] {message}")


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument(
        "--workflow",
        type=Path,
        default=Path(".github/workflows/ci.yml"),
        help="workflow path, absolute or relative to --root",
    )
    parser.add_argument(
        "--dockerfile",
        type=Path,
        default=Path("docker/Dockerfile"),
        help="Dockerfile path, absolute or relative to --root",
    )
    parser.add_argument(
        "--lock",
        type=Path,
        default=Path("tools/content/catalog_environment.lock.json"),
        help="catalog lock path, absolute or relative to --root",
    )
    parser.add_argument(
        "--check",
        choices=("all", "pins", "cache"),
        default="all",
        help="run only one invariant family (used by negative tests)",
    )
    return parser.parse_args()


def resolve(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def read_text(path: Path, invariant: str) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise fail(invariant, f"cannot read {path}: {exc}") from exc


def parse_lock(path: Path) -> tuple[list[tuple[str, str, str]], str]:
    invariant = "catalog-lock"
    try:
        data = json.loads(read_text(path, invariant))
        raw_packages = data["packages"]
        fingerprint = data["fingerprint"]
    except (KeyError, TypeError, ValueError) as exc:
        raise fail(invariant, f"{path} is not a readable catalog lock: {exc}") from exc

    packages: list[tuple[str, str, str]] = []
    for entry in raw_packages:
        try:
            display_name, version = str(entry).split("\t", 1)
        except ValueError as exc:
            raise fail(
                invariant,
                f"{path} has a malformed package entry {entry!r}; "
                "expected '<package>[:<arch>]\t<version>'.",
            ) from exc
        base_name = display_name.split(":", 1)[0]
        packages.append((display_name, base_name, version))

    machine = str(fingerprint.get("machine", ""))
    debian_arch = {"aarch64": "arm64", "x86_64": "amd64"}.get(machine)
    if debian_arch is None:
        # An explicit architecture qualifier in the package list is
        # the other authoritative source in the lock.
        for display_name, _, _ in packages:
            if ":" in display_name:
                debian_arch = display_name.rsplit(":", 1)[1]
                break
    if debian_arch is None:
        raise fail(
            invariant,
            f"{path} does not identify its Debian architecture "
            "(fingerprint.machine or a package ':<arch>' qualifier).",
        )
    return packages, debian_arch


def parse_dockerfile(path: Path) -> tuple[str, str]:
    invariant = "snapshot-pin"
    text = read_text(path, invariant)

    snapshot_match = re.search(
        r"(?m)^ARG\s+UBUNTU_SNAPSHOT=([0-9]{8}T[0-9]{6}Z)\s*$", text
    )
    if snapshot_match is None:
        raise fail(
            invariant,
            f"{path} does not define 'ARG UBUNTU_SNAPSHOT=<yyyymmddThhmmssZ>'; "
            "the apt pin cannot be checked against "
            "tools/content/catalog_environment.lock.json.",
        )
    if "snapshot.ubuntu.com" not in text or "${UBUNTU_SNAPSHOT}" not in text:
        raise fail(
            invariant,
            f"{path} defines UBUNTU_SNAPSHOT but does not use it to build "
            "the snapshot.ubuntu.com apt source; the checked date would "
            "not be the installed date.",
        )

    from_match = re.search(r"(?mi)^FROM\s+ubuntu:([0-9]+\.[0-9]+)\b", text)
    if from_match is None:
        raise fail(
            invariant,
            f"{path} does not declare an ubuntu:<version> base image; "
            "the snapshot suite cannot be derived.",
        )
    ubuntu_version = from_match.group(1)
    if ubuntu_version not in UBUNTU_CODENAMES:
        raise fail(
            invariant,
            f"Ubuntu {ubuntu_version} in {path} has no codename mapping in "
            "scripts/ci-check-pins.py; add the mapping when the base image "
            "and catalog lock move to a new Ubuntu release.",
        )
    return snapshot_match.group(1), UBUNTU_CODENAMES[ubuntu_version]


def iter_package_stanzas(lines: Iterable[bytes]):
    stanza: dict[str, str] = {}
    last_key: str | None = None
    for raw_line in lines:
        line = raw_line.decode("utf-8", "replace").rstrip("\n")
        if not line:
            if stanza:
                yield stanza
            stanza = {}
            last_key = None
            continue
        if line[0] in " \t" and last_key is not None:
            stanza[last_key] += "\n" + line.strip()
            continue
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        last_key = key
        stanza[key] = value.strip()
    if stanza:
        yield stanza


def scan_index(
    snapshot: str,
    suite: str,
    component: str,
    debian_arch: str,
    wanted: dict[str, set[str]],
    found: dict[str, set[str]],
) -> str | None:
    """Scan one Packages.gz; return an error string or None."""
    url = (
        f"{SNAPSHOT_ROOT}/{snapshot}/dists/{suite}/{component}/"
        f"binary-{debian_arch}/Packages.gz"
    )
    request = urllib.request.Request(
        url, headers={"User-Agent": "yadokari-ci-check-pins/1"}
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            with gzip.GzipFile(fileobj=response) as stream:
                for stanza in iter_package_stanzas(stream):
                    package = stanza.get("Package")
                    version = stanza.get("Version")
                    architecture = stanza.get("Architecture")
                    if package is None or version is None:
                        continue
                    if architecture not in (debian_arch, "all"):
                        continue
                    if version in wanted.get(package, set()):
                        found.setdefault(package, set()).add(version)
    except (OSError, urllib.error.URLError) as exc:
        return f"{url}: {exc}"
    return None


def check_pins(root: Path, dockerfile_path: Path, lock_path: Path) -> str:
    snapshot, codename = parse_dockerfile(dockerfile_path)
    packages, debian_arch = parse_lock(lock_path)
    wanted: dict[str, set[str]] = {}
    display_by_base: dict[str, str] = {}
    for display_name, base_name, version in packages:
        wanted.setdefault(base_name, set()).add(version)
        display_by_base[base_name] = display_name

    found: dict[str, set[str]] = {}
    fetch_errors: list[str] = []
    suites = [f"{codename}{suffix}" for suffix in SUITE_SUFFIXES]
    # Prefer the pockets where the locked toolchain versions live.
    suite_search_order = [suites[1], suites[2], suites[0], suites[3]]

    def unresolved() -> list[str]:
        return [
            base
            for base, versions in wanted.items()
            if not versions.issubset(found.get(base, set()))
        ]

    for suite in suite_search_order:
        error = scan_index(
            snapshot, suite, "main", debian_arch, wanted, found
        )
        if error:
            fetch_errors.append(error)
        if not unresolved():
            break

    if unresolved():
        for suite in suite_search_order:
            for component in COMPONENT_SEARCH_ORDER[1:]:
                error = scan_index(
                    snapshot, suite, component, debian_arch, wanted, found
                )
                if error:
                    fetch_errors.append(error)
                if not unresolved():
                    break
            if not unresolved():
                break

    missing = unresolved()
    if missing:
        details = "; ".join(
            f"{display_by_base[base]}={version} not in snapshot {snapshot}"
            for base in missing
            for version in sorted(wanted[base] - found.get(base, set()))
        )
        message = (
            f"Dockerfile snapshot {snapshot} "
            f"({dockerfile_path}) does not serve every locked package "
            f"from {lock_path}: {details}. Move UBUNTU_SNAPSHOT to a "
            "snapshot that serves the lock exactly, or deliberately "
            "re-baseline the lock and the snapshot together."
        )
        if fetch_errors:
            message += " Index fetch errors included: " + "; ".join(fetch_errors[:4])
        raise fail("snapshot-pin", message)

    return (
        f"snapshot-pin: {len(packages)} locked packages from {lock_path} "
        f"are present at the locked versions in snapshot {snapshot} "
        f"({codename}, {debian_arch}) used by {dockerfile_path}."
    )


def normalise_expression(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def hash_file_patterns(key: str) -> list[str]:
    patterns: list[str] = []
    for call in re.findall(r"hashFiles\((.*?)\)", key, flags=re.DOTALL):
        for token in re.findall(r"'([^']+)'|\"([^\"]+)\"", call):
            patterns.append(token[0] or token[1])
    return patterns


def covers(patterns: list[str], path: str) -> bool:
    return any(
        pattern == path or fnmatch.fnmatchcase(path, pattern)
        for pattern in patterns
    )


def circle_pin(root: Path) -> str:
    invariant = "cache-key:circle"
    pins: set[str] = set()
    for relative in ("scripts/build-firmware.sh", "scripts/build-firmware-pi5.sh"):
        text = read_text(root / relative, invariant)
        match = re.search(r"checkout\s+([0-9a-f]{40})\b", text)
        if match:
            pins.add(match.group(1))
    if len(pins) != 1:
        raise fail(
            invariant,
            "scripts/build-firmware.sh and scripts/build-firmware-pi5.sh "
            f"do not agree on one pinned Circle commit (found: {sorted(pins)}); "
            "the Circle cache invariant has no single content input to check.",
        )
    return pins.pop()


def cache_templates(step: dict) -> list[tuple[str, str]]:
    with_block = step.get("with") or {}
    templates: list[tuple[str, str]] = []
    if with_block.get("key") is not None:
        templates.append(("key", str(with_block["key"])))
    if with_block.get("restore-keys") is not None:
        restore = str(with_block["restore-keys"])
        for line in restore.splitlines():
            if line.strip():
                templates.append(("restore-keys", line.strip()))
    return templates


def classify_cache(step: dict) -> str | None:
    with_block = step.get("with") or {}
    path = str(with_block.get("path", ""))
    name = str(step.get("name", ""))
    haystack = f"{name}\n{path}"
    if ".deps" in haystack or "circle" in haystack.lower():
        return "circle"
    if "arm-gnu-toolchain" in haystack or "toolchain" in haystack.lower():
        return "toolchain"
    return None


def check_one_cache(
    root: Path,
    workflow_path: Path,
    job_id: str,
    step: dict,
    pin: str,
) -> None:
    with_block = step.get("with") or {}
    path = str(with_block.get("path", ""))
    display_name = str(step.get("name") or step.get("uses") or "<unnamed cache>")
    label = f"{display_name} (job {job_id}, path {path or '<missing>'})"
    kind = classify_cache(step)
    if kind is None:
        raise fail(
            "cache-key:unregistered-cache",
            f"{label} in {workflow_path} is not covered by a cache-key "
            "invariant in scripts/ci-check-pins.py. Register the cache's "
            "content inputs there before adding the cache; an unregistered "
            "cache can silently restore state built from different inputs.",
        )

    templates = cache_templates(step)
    if not templates:
        raise fail(
            f"cache-key:{kind}",
            f"{label} in {workflow_path} has no cache key to check.",
        )

    if kind == "toolchain":
        for field, template in templates:
            normalised = normalise_expression(template)
            missing = []
            if "${{ env.ARM_TC_VERSION }}" not in normalised:
                missing.append("${{ env.ARM_TC_VERSION }}")
            if "${{ runner.arch }}" not in normalised:
                missing.append("${{ runner.arch }}")
            if missing:
                raise fail(
                    "cache-key:toolchain",
                    f"{label} in {workflow_path} has a {field} that does "
                    f"not cover its content inputs: missing {', '.join(missing)}. "
                    "The toolchain cache must reference ARM_TC_VERSION and "
                    "runner.arch so a version or architecture change cannot "
                    "restore the previous toolchain.",
                )
        return

    # Circle: the restored tree is the pinned upstream checkout with
    # this repository's patch series already applied.
    patch_scripts = sorted(
        str(path.relative_to(root)) for path in (root / "scripts").glob("patch-*.py")
    )
    patch_files = sorted(
        str(path.relative_to(root)) for path in (root / "scripts").glob("*.patch")
    )
    required_files = patch_scripts + patch_files
    for field, template in templates:
        normalised = normalise_expression(template)
        if pin not in normalised:
            raise fail(
                "cache-key:circle",
                f"{label} in {workflow_path} has a {field} that does not "
                f"cover the pinned Circle commit {pin} from "
                "scripts/build-firmware.sh / scripts/build-firmware-pi5.sh.",
            )
        patterns = hash_file_patterns(template)
        missing_files = [
            required for required in required_files if not covers(patterns, required)
        ]
        if missing_files:
            raise fail(
                "cache-key:circle",
                f"{label} in {workflow_path} has a {field} whose hashFiles "
                "inputs do not cover every Circle patch input: missing "
                f"{', '.join(missing_files)}. The restored .deps tree carries "
                "the applied patch state, so every scripts/patch-*.py and "
                "scripts/*.patch input must be in the cache key.",
            )


def check_cache_keys(root: Path, workflow_path: Path) -> str:
    invariant = "cache-key"
    text = read_text(workflow_path, invariant)
    try:
        workflow = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        raise fail(invariant, f"{workflow_path} is not valid YAML: {exc}") from exc

    jobs = (workflow or {}).get("jobs")
    if not isinstance(jobs, dict):
        raise fail(invariant, f"{workflow_path} has no jobs mapping to inspect.")

    env = (workflow or {}).get("env") or {}
    if "ARM_TC_VERSION" not in env:
        raise fail(
            "cache-key:toolchain",
            f"{workflow_path} does not define env.ARM_TC_VERSION; the "
            "toolchain cache invariant cannot reference the version input.",
        )

    pin = circle_pin(root)
    cache_count = 0
    for job_id, job in jobs.items():
        if not isinstance(job, dict):
            continue
        for step in job.get("steps") or []:
            if not isinstance(step, dict):
                continue
            uses = str(step.get("uses", ""))
            if uses.startswith("actions/cache@"):
                cache_count += 1
                check_one_cache(root, workflow_path, str(job_id), step, pin)

    if cache_count == 0:
        raise fail(
            invariant,
            f"{workflow_path} contains no actions/cache steps; expected the "
            "registered toolchain and Circle caches to be present and checked.",
        )
    return (
        f"cache-key: all {cache_count} actions/cache keys in {workflow_path} "
        "cover their registered content inputs (Circle pin + patch inputs; "
        "toolchain version + runner architecture)."
    )


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    workflow_path = resolve(root, args.workflow)
    dockerfile_path = resolve(root, args.dockerfile)
    lock_path = resolve(root, args.lock)

    try:
        if args.check in ("all", "pins"):
            print(check_pins(root, dockerfile_path, lock_path))
        if args.check in ("all", "cache"):
            print(check_cache_keys(root, workflow_path))
    except InvariantFailure as exc:
        print(f"::error title={INVARIANT}::{exc}", file=sys.stderr)
        print(f"{INVARIANT} failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
