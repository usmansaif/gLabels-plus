# Build environment for Linux packages that run on Ubuntu 20.04 LTS and newer.
#
# Packages built on the oldest supported system (glibc 2.31) run on every newer
# one, so the release packages are built here rather than on the build host.
# Qt 6.5 LTS is installed with aqtinstall (its binaries need glibc 2.28+) and is
# bundled into the packages, so the target system needs no Qt.
#
# Used by ../../release-ubuntu2004.sh
#
FROM ubuntu:20.04

ARG QT_VERSION=6.5.3
ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++-10 gcc-10 ninja-build pkg-config git curl ca-certificates file \
        python3 python3-dev python3-pip python3-setuptools \
        dpkg-dev fakeroot rpm \
        zlib1g-dev libqrencode-dev \
        libgl1-mesa-dev libegl1-mesa-dev libglib2.0-0 libdbus-1-3 \
        libfontconfig1-dev libfreetype6-dev libcups2 \
        libx11-dev libx11-xcb-dev libxext-dev libxrender-dev libxkbcommon-dev libxkbcommon-x11-0 \
        libxcb1-dev libxcb-cursor0 libxcb-glx0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 \
        libxcb-xinerama0 libxcb-xkb1 libxcb-util1 \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 20.04 ships CMake 3.16, GCC 9 and Python 3.8: use pip's CMake, GCC 10
# (better C++20) and the last aqtinstall release for Python 3.8
RUN python3 -m pip install --no-cache-dir --upgrade "pip<24" \
    && python3 -m pip install --no-cache-dir "cmake>=3.22,<3.31" "aqtinstall==3.1.18"

RUN python3 -m aqt install-qt linux desktop ${QT_VERSION} gcc_64 --outputdir /opt/Qt \
    && rm -f aqtinstall.log

ENV CC=gcc-10 \
    CXX=g++-10 \
    QT_DIR=/opt/Qt/${QT_VERSION}/gcc_64 \
    PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:${PATH}

WORKDIR /src
