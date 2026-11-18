#!/usr/bin/env bash
#
# build.sh -- configure and build gLabels on Linux, macOS or Windows (Git Bash / MSYS2).
#
# Usage: ./build.sh [options] [-- extra cmake args]
#
#   -t, --type TYPE        Build type: Release (default) or Debug
#   -b, --build-dir DIR    Build directory (default: build)
#   -q, --qt PATH          Qt 6 prefix, e.g. ~/Qt/6.5.3/gcc_64 (default: auto-detect)
#   -j, --jobs N           Parallel jobs (default: number of CPUs)
#   -d, --deps             Install build dependencies with the system package manager
#       --install-qt VER   Download Qt VER (e.g. 6.5.3) into ~/Qt with aqtinstall and use it
#       --test             Run the unit tests after building
#       --clean            Delete the build directory first
#   -h, --help             Show this help
#
# Environment: QT_DIR may also be used to point at the Qt prefix.
#
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_TYPE=Release
BUILD_DIR=build
QT_PREFIX="${QT_DIR:-}"
JOBS=""
INSTALL_DEPS=0
INSTALL_QT=""
RUN_TESTS=0
CLEAN=0
EXTRA_CMAKE_ARGS=()

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

usage() { sed -n '3,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--type)       BUILD_TYPE="$2"; shift 2 ;;
        -b|--build-dir)  BUILD_DIR="$2"; shift 2 ;;
        -q|--qt)         QT_PREFIX="$2"; shift 2 ;;
        -j|--jobs)       JOBS="$2"; shift 2 ;;
        -d|--deps)       INSTALL_DEPS=1; shift ;;
        --install-qt)    INSTALL_QT="$2"; shift 2 ;;
        --test)          RUN_TESTS=1; shift ;;
        --clean)         CLEAN=1; shift ;;
        -h|--help)       usage ;;
        --)              shift; EXTRA_CMAKE_ARGS=("$@"); break ;;
        *)               die "unknown option: $1 (see --help)" ;;
    esac
done

case "$BUILD_TYPE" in
    Release|Debug|RelWithDebInfo|MinSizeRel) ;;
    release) BUILD_TYPE=Release ;;
    debug)   BUILD_TYPE=Debug ;;
    *) die "invalid build type: $BUILD_TYPE" ;;
esac

[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$SRC_DIR/$BUILD_DIR"

#------------------------------------------------------------------------------
# Platform detection
#------------------------------------------------------------------------------
case "$(uname -s)" in
    Linux*)               OS=linux ;;
    Darwin*)              OS=macos ;;
    MINGW*|MSYS*|CYGWIN*) OS=windows ;;
    *)                    die "unsupported OS: $(uname -s)" ;;
esac

if [[ -z "$JOBS" ]]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

SUDO=""
if [[ $OS = linux && $(id -u) -ne 0 ]] && have sudo; then SUDO=sudo; fi

#------------------------------------------------------------------------------
# Dependencies
#------------------------------------------------------------------------------
install_deps() {
    log "Installing build dependencies"
    if [[ $OS = linux ]]; then
        if have apt-get; then
            $SUDO apt-get update
            $SUDO apt-get install -y build-essential cmake ninja-build pkg-config git \
                zlib1g-dev libqrencode-dev libgl1-mesa-dev libxkbcommon-dev \
                libfontconfig1-dev libfreetype6-dev libxcb-cursor0 file
            # Qt 6 packages only exist on Debian 12+/Ubuntu 22.04+
            if apt-cache show qt6-base-dev >/dev/null 2>&1; then
                $SUDO apt-get install -y qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
                    qt6-l10n-tools libqt6svg6-dev
            else
                warn "this distro has no Qt 6 packages; use --install-qt 6.5.3"
            fi
        elif have dnf; then
            $SUDO dnf install -y gcc-c++ cmake ninja-build git zlib-devel qrencode-devel \
                qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel qt6-linguist rpm-build
        elif have pacman; then
            $SUDO pacman -S --needed --noconfirm base-devel cmake ninja git zlib qrencode \
                qt6-base qt6-svg qt6-tools
        elif have zypper; then
            $SUDO zypper install -y gcc-c++ cmake ninja git zlib-devel qrencode-devel \
                qt6-base-devel qt6-svg-devel qt6-tools-devel qt6-linguist-devel rpm-build
        else
            die "no supported package manager found (apt, dnf, pacman, zypper)"
        fi
    elif [[ $OS = macos ]]; then
        have brew || die "Homebrew is required: https://brew.sh"
        brew install cmake ninja qt zlib qrencode
    else
        warn "On Windows install manually:"
        warn "  - Visual Studio 2022 Build Tools (Desktop development with C++)"
        warn "  - CMake (https://cmake.org) and NSIS (https://nsis.sourceforge.io) for installers"
        warn "  - Qt 6 for MSVC (or run this script with --install-qt 6.5.3)"
    fi
}

#------------------------------------------------------------------------------
# Qt
#------------------------------------------------------------------------------
install_qt() {
    local ver="$1" host arch dir
    case "$OS" in
        linux)   host=linux;   arch=gcc_64; dir=gcc_64
                 # Qt >= 6.7 renamed the Linux desktop arch
                 if [[ "$(printf '%s\n' 6.7 "$ver" | sort -V | head -1)" = 6.7 ]]; then arch=linux_gcc_64; fi ;;
        macos)   host=mac;     arch=clang_64; dir=macos ;;
        windows) host=windows; arch=win64_msvc2019_64; dir=msvc2019_64 ;;
    esac
    QT_PREFIX="$HOME/Qt/$ver/$dir"
    if [[ -d "$QT_PREFIX" ]]; then
        log "Qt $ver already present at $QT_PREFIX"
        return
    fi
    local py
    py="$(command -v python3 || command -v python || true)"
    [[ -n "$py" ]] || die "python3 is required for --install-qt"
    log "Installing Qt $ver ($arch) into ~/Qt with aqtinstall"
    "$py" -m pip install --user --upgrade aqtinstall
    "$py" -m aqt install-qt "$host" desktop "$ver" "$arch" --outputdir "$HOME/Qt"
}

detect_qt() {
    if [[ -n "$QT_PREFIX" ]]; then return; fi
    local tool
    # 1. Qt tools on PATH
    for tool in qtpaths6 qtpaths qmake6 qmake; do
        if have "$tool"; then
            local v p
            v="$("$tool" -query QT_VERSION 2>/dev/null || true)"
            p="$("$tool" -query QT_INSTALL_PREFIX 2>/dev/null || true)"
            if [[ "$v" == 6.* && -n "$p" ]]; then QT_PREFIX="$p"; return; fi
        fi
    done
    # 2. Qt online installer / aqtinstall layout: ~/Qt/<version>/<arch>
    local roots=("$HOME/Qt" "/opt/Qt" "/c/Qt" "C:/Qt") root cand
    for root in "${roots[@]}"; do
        [[ -d "$root" ]] || continue
        for cand in $(ls -d "$root"/6.*/* 2>/dev/null | sort -rV); do
            if [[ -f "$cand/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
                case "$OS:$(basename "$cand")" in
                    linux:gcc_64|linux:linux_gcc_64|macos:macos|macos:clang_64|windows:msvc*)
                        QT_PREFIX="$cand"; return ;;
                esac
            fi
        done
    done
    # 3. Homebrew
    if [[ $OS = macos ]] && have brew && brew --prefix qt >/dev/null 2>&1; then
        QT_PREFIX="$(brew --prefix qt)"; return
    fi
    # 4. Otherwise rely on CMake finding a system-wide Qt (e.g. /usr)
}

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------
if [[ $INSTALL_DEPS = 1 ]]; then install_deps; fi
if [[ -n "$INSTALL_QT" ]]; then install_qt "$INSTALL_QT"; fi
detect_qt

have cmake || die "cmake not found (run with --deps)"

CMAKE_ARGS=(-S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE")
if [[ -n "$QT_PREFIX" ]]; then
    QT_PREFIX="$(cd "$QT_PREFIX" && pwd)"
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
    export PATH="$QT_PREFIX/bin:$PATH"   # qtpaths/windeployqt/macdeployqt
    log "Using Qt at $QT_PREFIX"
else
    log "Using system Qt"
fi

# Ninja everywhere except Windows, where the Visual Studio generator finds MSVC by itself.
if [[ $OS != windows ]] && have ninja && [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    CMAKE_ARGS+=(-G Ninja)
fi

if [[ $CLEAN = 1 && -d "$BUILD_DIR" ]]; then
    log "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

log "Configuring ($BUILD_TYPE) in $BUILD_DIR"
cmake "${CMAKE_ARGS[@]}" ${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}

log "Building with $JOBS jobs"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS"

# The build regenerates translations/glabels_C.ts; keep the source tree clean.
if have git && git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$SRC_DIR" checkout -- translations/glabels_C.ts 2>/dev/null || true
fi

if [[ $RUN_TESTS = 1 ]]; then
    log "Running tests"
    ( cd "$BUILD_DIR" && QT_QPA_PLATFORM=offscreen ctest --build-config "$BUILD_TYPE" --output-on-failure )
fi

log "Done. Binaries are in $BUILD_DIR/glabels and $BUILD_DIR/glabels-batch"
