#!/usr/bin/env bash
#
# release.sh -- build gLabels in Release mode and package it for the current OS.
#
#   Linux   : .AppImage, .deb, .rpm (if rpmbuild is installed), .tar.gz
#             Qt is bundled in all of them, so they run on machines without Qt.
#   Windows : .exe installer (NSIS) and portable .zip     (run from Git Bash, MSVC + Qt for MSVC)
#   macOS   : .tar.gz and .dmg                            (need Qt from Homebrew at runtime)
#
# Packages for an OS can only be built on that OS; run this script on each
# platform (or let .github/workflows/release-packages.yml do it on GitHub).
#
# Usage: ./release.sh [options]
#
#   -q, --qt PATH        Qt 6 prefix (default: auto-detect, see build.sh)
#   -o, --out DIR        Output directory (default: dist)
#   -f, --formats LIST   Comma-separated subset, e.g. appimage,deb  (default: all for this OS)
#       --version VER    Package version (default: from the build, e.g. 5.0)
#   -b, --build-dir DIR  Build directory (default: build-release)
#       --skip-build     Reuse an existing build directory
#   -h, --help           Show this help
#
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SRC_DIR/build-release"
TOOLS_DIR="$SRC_DIR/.tools"
OUT_DIR="$SRC_DIR/dist"
QT_ARG=()
FORMATS=""
VERSION=""
SKIP_BUILD=0

PKG_NAME=glabels-qt
SUMMARY="gLabels label and business card designer"
MAINTAINER="gLabels Project <bugs@glabels.org>"
HOMEPAGE="https://glabels.org"

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

usage() { sed -n '3,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -q|--qt)       QT_ARG=(--qt "$2"); shift 2 ;;
        -o|--out)      OUT_DIR="$2"; shift 2 ;;
        -f|--formats)  FORMATS="$2"; shift 2 ;;
        -b|--build-dir) BUILD_DIR="$2"; shift 2 ;;
        --version)     VERSION="$2"; shift 2 ;;
        --skip-build)  SKIP_BUILD=1; shift ;;
        -h|--help)     usage ;;
        *)             die "unknown option: $1 (see --help)" ;;
    esac
done

case "$(uname -s)" in
    Linux*)               OS=linux;   ALL_FORMATS="appimage,deb,rpm,tar" ;;
    Darwin*)              OS=macos;   ALL_FORMATS="tar,dmg" ;;
    MINGW*|MSYS*|CYGWIN*) OS=windows; ALL_FORMATS="exe,zip" ;;
    *)                    die "unsupported OS: $(uname -s)" ;;
esac
FORMATS="${FORMATS:-$ALL_FORMATS}"
want() { [[ ",$FORMATS," == *",$1,"* ]]; }

ARCH="$(uname -m)"
[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$SRC_DIR/$BUILD_DIR"
[[ "$OUT_DIR" = /* ]] || OUT_DIR="$SRC_DIR/$OUT_DIR"
mkdir -p "$OUT_DIR"

#------------------------------------------------------------------------------
# Build
#------------------------------------------------------------------------------
if [[ $SKIP_BUILD = 0 ]]; then
    # Keep the Qt library path in installed binaries so deploy tools can find Qt.
    "$SRC_DIR/build.sh" --type Release --build-dir "$BUILD_DIR" ${QT_ARG[@]+"${QT_ARG[@]}"} \
        -- -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON
fi
[[ -f "$BUILD_DIR/CMakeCache.txt" ]] || die "no build found in $BUILD_DIR"

if [[ -z "$VERSION" ]]; then
    VERSION="$(tr -d '\r\n' < "$BUILD_DIR/VERSION.txt")"
fi
log "Packaging $PKG_NAME $VERSION for $OS/$ARCH"

# Qt prefix actually used by the build (from Qt6_DIR = <prefix>/lib/cmake/Qt6)
QT6_DIR="$(sed -n 's/^Qt6_DIR:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt")"
QT_PREFIX="$(cd "$QT6_DIR/../../.." && pwd)"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/glabels-release.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

stage_install() {   # stage_install <prefix>
    cmake --install "$BUILD_DIR" --config Release --prefix "$1" >/dev/null
}

#------------------------------------------------------------------------------
# Linux
#------------------------------------------------------------------------------
fetch_tool() {   # fetch_tool <file> <url>
    mkdir -p "$TOOLS_DIR"
    if [[ ! -x "$TOOLS_DIR/$1" ]]; then
        log "Downloading $1"
        curl -fL --retry 3 -o "$TOOLS_DIR/$1" "$2"
        chmod +x "$TOOLS_DIR/$1"
    fi
}

linux_bundle() {
    # Build a self-contained AppDir (app + Qt libs + plugins) with linuxdeploy.
    fetch_tool "linuxdeploy-$ARCH.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
    fetch_tool "linuxdeploy-plugin-qt-$ARCH.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage"

    APPDIR="$WORK/AppDir"
    stage_install "$APPDIR/usr"

    export APPIMAGE_EXTRACT_AND_RUN=1          # works without FUSE (containers, CI)
    export QMAKE="$QT_PREFIX/bin/qmake"
    export LD_LIBRARY_PATH="$QT_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export EXTRA_QT_MODULES="svg" EXTRA_QT_PLUGINS="svg"
    export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
    export PATH="$TOOLS_DIR:$PATH"

    local out=()
    if want appimage; then out=(--output appimage); fi
    log "Bundling Qt with linuxdeploy"
    ( cd "$WORK" && "$TOOLS_DIR/linuxdeploy-$ARCH.AppImage" \
        --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/glabels-qt" \
        --executable "$APPDIR/usr/bin/glabels-batch-qt" \
        --desktop-file "$APPDIR/usr/share/applications/org.glabels.glabels-qt.desktop" \
        --plugin qt ${out[@]+"${out[@]}"} )

    if want appimage; then
        mv "$WORK"/*.AppImage "$OUT_DIR/$PKG_NAME-$VERSION-$ARCH.AppImage"
        log "Created $OUT_DIR/$PKG_NAME-$VERSION-$ARCH.AppImage"
    fi
}

linux_pkgroot() {
    # Filesystem layout shared by .deb and .rpm:
    #   /opt/glabels-qt/{bin,lib,plugins,share}  bundled app
    #   /usr/bin/glabels-qt -> /opt/...          launchers
    #   /usr/share/{applications,icons,mime,metainfo}  desktop integration
    PKGROOT="$WORK/pkgroot"
    mkdir -p "$PKGROOT/opt" "$PKGROOT/usr/bin" "$PKGROOT/usr/share"
    cp -a "$APPDIR/usr" "$PKGROOT/opt/$PKG_NAME"
    ln -s "/opt/$PKG_NAME/bin/glabels-qt"       "$PKGROOT/usr/bin/glabels-qt"
    ln -s "/opt/$PKG_NAME/bin/glabels-batch-qt" "$PKGROOT/usr/bin/glabels-batch-qt"
    local d
    for d in applications icons mime metainfo; do
        if [[ -d "$APPDIR/usr/share/$d" ]]; then cp -a "$APPDIR/usr/share/$d" "$PKGROOT/usr/share/"; fi
    done
}

linux_deb() {
    have dpkg-deb || { warn "dpkg-deb not found, skipping .deb"; return; }
    local root="$WORK/deb" debarch debver
    debarch="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
    debver="${VERSION/-/\~}-1"                   # 5.0-master660 -> 5.0~master660-1
    cp -a "$PKGROOT" "$root"
    mkdir -p "$root/DEBIAN"
    cat > "$root/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $debver
Section: graphics
Priority: optional
Architecture: $debarch
Maintainer: $MAINTAINER
Homepage: $HOMEPAGE
Installed-Size: $(du -sk "$PKGROOT" | cut -f1)
Depends: libc6 (>= 2.31), libstdc++6, libgl1, libegl1, libxcb-cursor0, libfontconfig1, libfreetype6, libx11-6, libx11-xcb1, libxcb1, libxkbcommon0, libxkbcommon-x11-0, libdbus-1-3, libglib2.0-0
Description: $SUMMARY
 gLabels is a program for creating labels and business cards.
 This package bundles its own copy of Qt.
EOF
    local deb="$OUT_DIR/${PKG_NAME}_${debver}_${debarch}.deb"
    dpkg-deb --root-owner-group --build "$root" "$deb" >/dev/null 2>&1 \
        || fakeroot dpkg-deb --build "$root" "$deb" >/dev/null
    log "Created $deb"
}

linux_rpm() {
    have rpmbuild || { warn "rpmbuild not found, skipping .rpm (install rpm / rpm-build)"; return; }
    local top="$WORK/rpm" rpmver="${VERSION//-/\~}"
    mkdir -p "$top"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    cat > "$top/SPECS/$PKG_NAME.spec" <<EOF
%global debug_package %{nil}
%global __os_install_post %{nil}
%define _build_id_links none
Name:      $PKG_NAME
Version:   $rpmver
Release:   1
Summary:   $SUMMARY
License:   GPL-3.0-or-later
URL:       $HOMEPAGE
AutoReqProv: no
Requires:  libglvnd-glx, libglvnd-egl, fontconfig, freetype, libX11, libX11-xcb, libxcb, libxkbcommon, libxkbcommon-x11, dbus-libs, glib2

%description
gLabels is a program for creating labels and business cards.
This package bundles its own copy of Qt.

%install
mkdir -p %{buildroot}
cp -a "$PKGROOT"/. %{buildroot}/

%files
/opt/$PKG_NAME
/usr/bin/glabels-qt
/usr/bin/glabels-batch-qt
/usr/share/applications/*
/usr/share/icons/hicolor/*/*/*
/usr/share/mime/packages/*
/usr/share/metainfo/*
EOF
    rpmbuild --quiet --define "_topdir $top" --target "$ARCH" -bb "$top/SPECS/$PKG_NAME.spec"
    local rpm
    for rpm in "$top"/RPMS/*/*.rpm; do
        mv "$rpm" "$OUT_DIR/"
        log "Created $OUT_DIR/$(basename "$rpm")"
    done
}

linux_tar() {
    local name="$PKG_NAME-$VERSION-linux-$ARCH"
    cp -a "$APPDIR/usr" "$WORK/$name"
    tar -C "$WORK" -czf "$OUT_DIR/$name.tar.gz" "$name"
    log "Created $OUT_DIR/$name.tar.gz  (run $name/bin/glabels-qt)"
}

#------------------------------------------------------------------------------
# Windows (Git Bash + MSVC): CPack uses the windeployqt output from the build
#------------------------------------------------------------------------------
windows_packages() {
    local gens=()
    if want exe; then
        if ! have makensis; then
            local nsis
            for nsis in "/c/Program Files (x86)/NSIS" "/c/Program Files/NSIS"; do
                if [[ -x "$nsis/makensis.exe" ]]; then export PATH="$nsis:$PATH"; fi
            done
        fi
        if have makensis; then gens+=(NSIS); else warn "NSIS not found, skipping .exe (https://nsis.sourceforge.io)"; fi
    fi
    if want zip; then gens+=(ZIP); fi
    [[ ${#gens[@]} -gt 0 ]] || return
    local IFS=';'
    ( cd "$WORK" && cpack --config "$BUILD_DIR/CPackConfig.cmake" -C Release -G "${gens[*]}" \
        -D CPACK_PACKAGE_FILE_NAME="$PKG_NAME-$VERSION-windows-x64" -B "$WORK/cpack" )
    local f
    for f in "$WORK"/cpack/*.exe "$WORK"/cpack/*.zip; do
        [[ -f "$f" ]] || continue
        mv "$f" "$OUT_DIR/"
        log "Created $OUT_DIR/$(basename "$f")"
    done
}

#------------------------------------------------------------------------------
# macOS: the project has no .app bundle target yet, so Qt is not bundled.
#------------------------------------------------------------------------------
macos_packages() {
    local name="$PKG_NAME-$VERSION-macos-$ARCH"
    stage_install "$WORK/$name"
    if want tar; then
        tar -C "$WORK" -czf "$OUT_DIR/$name.tar.gz" "$name"
        log "Created $OUT_DIR/$name.tar.gz"
    fi
    if want dmg; then
        hdiutil create -quiet -volname "gLabels $VERSION" -srcfolder "$WORK/$name" \
            -ov -format UDZO "$OUT_DIR/$name.dmg"
        log "Created $OUT_DIR/$name.dmg"
    fi
    warn "macOS packages need Qt on the target machine (brew install qt)"
}

#------------------------------------------------------------------------------
case "$OS" in
    linux)
        linux_bundle
        if want deb || want rpm; then linux_pkgroot; fi
        if want deb; then linux_deb; fi
        if want rpm; then linux_rpm; fi
        if want tar; then linux_tar; fi
        ;;
    windows) windows_packages ;;
    macos)   macos_packages ;;
esac

log "Packages in $OUT_DIR:"
ls -lh "$OUT_DIR"
