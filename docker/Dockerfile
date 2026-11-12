# YADOKARI dev container — the locked build environment, anywhere.
#
# Reproduces tools/content/catalog_environment.lock.json
# (Ubuntu 24.04 / GCC 13.3) plus the pinned Arm GNU Toolchain
# 15.2.rel1 used for firmware builds. Works on Linux and Mac
# (Docker Desktop); the toolchain tarball is selected by host
# architecture and verified by SHA-256.
#
#   docker build -f docker/Dockerfile -t yadokari-dev .
#   docker run --rm -it -v "$PWD":/src -w /src yadokari-dev bash
#
FROM ubuntu:24.04

# Pin the apt universe to the dated snapshot.ubuntu.com snapshot
# whose package versions match tools/content/catalog_environment
# .lock.json (glibc 2.39-0ubuntu8.9, GCC 13.3.0-6ubuntu2~24.04.1).
# The lock is the source of truth (CI inventory D2): a floating
# archive silently drifted glibc to 2.39-0ubuntu8.12 and broke the
# catalog-environment gate. When the lock is deliberately
# re-baselined, move this date to the matching snapshot.
#
# Coupling check: scripts/ci-check-pins.py runs in the CI
# preflight job and asserts that this snapshot serves every
# package in tools/content/catalog_environment.lock.json at
# exactly the locked version. If that lock is re-baselined, update
# UBUNTU_SNAPSHOT below and let the preflight check prove the new
# pairing before any expensive job runs. Do not hand-edit the
# generated lock to match a snapshot chosen by convenience.
ARG UBUNTU_SNAPSHOT=20261111T000000Z
# Bootstrap a CA store first: the base image ships none, and
# snapshot.ubuntu.com is https-only. This one package comes from
# the stock (http, archive-key-signed) sources; everything else
# installs from the pinned snapshot below. ca-certificates is a
# trust store only — it is not part of the catalog lock and does
# not affect the environment fingerprint.
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates \
 && rm -rf /var/lib/apt/lists/*
# Apply UBUNTU_SNAPSHOT to every configured Ubuntu source. The
# pairing between this substitution and the catalog lock is the
# invariant enforced by scripts/ci-check-pins.py.
RUN set -e; \
    for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.sources /etc/apt/sources.list.d/*.list; do \
      [ -f "$f" ] || continue; \
      sed -i -E "s#https?://(archive|security|ports)\.ubuntu\.com/(ubuntu|ubuntu-ports)#https://snapshot.ubuntu.com/ubuntu/${UBUNTU_SNAPSHOT}#g" "$f"; \
    done; \
    printf 'Acquire::Check-Valid-Until "false";\n' > /etc/apt/apt.conf.d/99-snapshot

ARG ARM_TC_VERSION=15.2.rel1
ARG ARM_TC_SHA256_X86_64=66f7ce7c1bf662f589a4caf440812375f3cd8000a033ccf0971127a0726d6921
ARG ARM_TC_SHA256_AARCH64=46195685b6aec1077e3f1b7706b43a6aa1fef4d8d3bff3a411b7dad1c5b1196b

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      g++-13 gcc-13 make python3 python3-pil python3-usb git xz-utils curl ca-certificates libsdl2-dev nodejs \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100 \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-13 100 \
 && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-13 100 \
 && rm -rf /var/lib/apt/lists/*

RUN set -e; \
    arch="$(uname -m)"; \
    case "$arch" in \
      x86_64)  sha="$ARM_TC_SHA256_X86_64" ;; \
      aarch64) sha="$ARM_TC_SHA256_AARCH64" ;; \
      *) echo "unsupported host arch: $arch" >&2; exit 1 ;; \
    esac; \
    curl -fL "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TC_VERSION}/binrel/arm-gnu-toolchain-${ARM_TC_VERSION}-${arch}-aarch64-none-elf.tar.xz" -o /tmp/arm.tar.xz; \
    echo "$sha  /tmp/arm.tar.xz" | sha256sum -c -; \
    mkdir -p /opt/arm-gnu-toolchain; \
    tar -xJf /tmp/arm.tar.xz -C /opt/arm-gnu-toolchain --strip-components=1; \
    rm /tmp/arm.tar.xz

ENV PATH=/opt/arm-gnu-toolchain/bin:$PATH
WORKDIR /src
