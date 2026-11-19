#!/usr/bin/env bash
#
# release-ubuntu2004.sh -- build the Linux packages (.AppImage, .deb, .rpm, .tar.gz)
# inside an Ubuntu 20.04 container, so they run on Ubuntu 20.04 LTS and every newer
# release (and other distributions with glibc 2.31 or newer).
#
# Usage: ./release-ubuntu2004.sh [release.sh options, e.g. --formats appimage,deb]
#
# Needs Docker.  Packages are written to dist/.
#
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE=glabels-plus-build:ubuntu20.04

command -v docker >/dev/null 2>&1 || { echo "error: docker not found" >&2; exit 1; }

echo "==> Building the Ubuntu 20.04 build image ($IMAGE)"
docker build -t "$IMAGE" "$SRC_DIR/packaging/ubuntu-20.04"

echo "==> Building packages in the container"
docker run --rm \
    -u "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -v "$SRC_DIR:/src" \
    "$IMAGE" \
    bash -c 'git config --global --add safe.directory /src 2>/dev/null || true; \
             ./release.sh --qt "$QT_DIR" --build-dir /tmp/glabels-build-release "$@"' _ "$@"

echo "==> Packages:"
ls -1 "$SRC_DIR/dist"
