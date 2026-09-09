#!/usr/bin/env bash
# Package the raw build artifacts into the release asset names used on
# github.com and itch.io: tic80-v<shortver>-<platform>.<ext>.
#
# Every artifact is a flat directory named after the workflow job (e.g.
# "tic80-windows") holding the raw binaries. This script turns each into the
# final downloadable asset and writes them into <out>.
#
# Usage: package.sh <artifacts-dir> <out-dir> <version> <shortver>
#   version   full "1.2.0"
#   shortver  "1.2" (first two segments; the file-name version)

set -euo pipefail

ART="${1:?artifacts dir}"
OUT="${2:?out dir}"
VER="${3:?version}"
SHORT="${4:?shortver}"

# Resolve to absolute paths up front: zip_flat changes into each artifact
# dir, so a relative $OUT would resolve there and miss.
mkdir -p "$OUT"
ART="$(cd "$ART" && pwd)"
OUT="$(cd "$OUT" && pwd)"

# zip_flat <src-dir> <out.zip> — archive the *contents* of src-dir (binaries
# at the zip root, not nested under the job name).
zip_flat() {
    local src="$1" dst="$2"
    (cd "$src" && zip -q -r -X "$dst" .)
}

# make_deb <src-dir> <out.deb> <arch> — a proper .deb with desktop integration.
make_deb() {
    local src="$1" dst="$2" arch="$3"
    local pkg
    pkg=$(mktemp -d)
    mkdir -p "$pkg/DEBIAN" "$pkg/usr/bin" "$pkg/usr/lib/tic80" \
             "$pkg/usr/share/applications" \
             "$pkg/usr/share/icons/hicolor/256x256/apps" \
             "$pkg/usr/share/metainfo" "$pkg/usr/share/mime/packages"

    install -m755 "$src/tic80" "$pkg/usr/bin/tic80"
    if compgen -G "$src"/*.so >/dev/null; then
        install -m644 "$src"/*.so "$pkg/usr/lib/tic80/"
    fi

    install -m644 build/linux/tic80.desktop.in "$pkg/usr/share/applications/tic80.desktop"
    install -m644 build/linux/tic80.png "$pkg/usr/share/icons/hicolor/256x256/apps/tic80.png"
    install -m644 build/linux/com.tic80.TIC_80.metainfo.xml "$pkg/usr/share/metainfo/"
    install -m644 build/linux/tic80.xml "$pkg/usr/share/mime/packages/tic80.xml"

    cat >"$pkg/DEBIAN/control" <<EOF
Package: tic80
Version: $VER
Section: education
Priority: optional
Architecture: $arch
Maintainer: Nesbox <grigoruk@gmail.com>
Homepage: https://tic80.com
Depends: libcurl4 | libcurl4t64
Description: Fantasy computer for making, playing and sharing tiny games.
EOF

    dpkg-deb --build --root-owner-group "$pkg" "$dst" >/dev/null
    rm -rf "$pkg"
}

# --- Windows (exe + dlls) ---
[ -d "$ART/tic80-windows" ] && zip_flat "$ART/tic80-windows" "$OUT/tic80-v$SHORT-win.zip"

# --- Linux ---
if [ -d "$ART/tic80-linux-gcc12" ]; then
    zip_flat "$ART/tic80-linux-gcc12" "$OUT/tic80-v$SHORT-linux.zip"
    make_deb "$ART/tic80-linux-gcc12" "$OUT/tic80-v$SHORT-linux.deb" amd64
fi
if [ -d "$ART/tic80-linux-arm64-gcc12" ]; then
    zip_flat "$ART/tic80-linux-arm64-gcc12" "$OUT/tic80-v$SHORT-linux-arm64.zip"
    make_deb "$ART/tic80-linux-arm64-gcc12" "$OUT/tic80-v$SHORT-linux-arm64.deb" arm64
fi

# --- macOS (binaries + dylibs; the .dmg is built separately on macOS) ---
[ -d "$ART/tic80-macos-arm64" ] && zip_flat "$ART/tic80-macos-arm64" "$OUT/tic80-v$SHORT-mac-arm64.zip"
[ -d "$ART/tic80-macos" ]        && zip_flat "$ART/tic80-macos" "$OUT/tic80-v$SHORT-mac.zip"

# --- Android (rename the apk) ---
[ -f "$ART/tic80-android/tic80.apk" ] && cp "$ART/tic80-android/tic80.apk" "$OUT/tic80-v$SHORT-android.apk"

# --- Consoles ---
[ -f "$ART/tic80-nintendo-3ds/tic80.3dsx" ] && zip_flat "$ART/tic80-nintendo-3ds" "$OUT/tic80-v$SHORT-3ds.zip"
[ -f "$ART/tic80-nintendo-switch/tic80.nro" ] && zip_flat "$ART/tic80-nintendo-switch" "$OUT/tic80-v$SHORT-switch.zip"

# --- Web: only the universal build (tic80.js + tic80.wasm + index.html).
# The per-language wasm variants stay out of the release zip; they are
# shipped to /export as web export stubs by the deploy script.
if [ -d "$ART/tic80-html" ]; then
    tmp="$OUT/.html"
    rm -rf "$tmp"
    mkdir -p "$tmp"
    cp "$ART/tic80-html/tic80.js" "$tmp/" 2>/dev/null || true
    cp "$ART/tic80-html/tic80.wasm" "$tmp/" 2>/dev/null || true
    [ -f "$ART/tic80-html/index.html" ] && cp "$ART/tic80-html/index.html" "$tmp/"
    zip_flat "$tmp" "$OUT/tic80-v$SHORT-html.zip"
    rm -rf "$tmp"
fi

# --- export stubs bundle (native) for the server. Not a user download;
# deploy-client.sh pulls it and unpacks into export/<shortver>/. The client
# asks for /export/<shortver>/<platform> as a *file* (e.g. "linux"), so each
# stub binary is renamed to its platform name, not kept in a subdir.
if [ -d "$ART" ]; then
    tmp="$OUT/.stubs"
    rm -rf "$tmp"
    mkdir -p "$tmp"
    for spec in windows:win:tic80.exe linux-gcc12:linux:tic80 linux-arm64:linux-arm64:tic80 macos:mac:tic80 macos-arm64:mac-arm64:tic80; do
        art="${spec%%:*}"; rest="${spec#*:}"; dst="${rest%%:*}"; file="${rest#*:}"
        src="$ART/tic80-$art-export/$file"
        [ -f "$src" ] && cp "$src" "$tmp/$dst"
    done
    # html export stub = the universal web build, as a zip file named "html"
    [ -f "$OUT/tic80-v$SHORT-html.zip" ] && cp "$OUT/tic80-v$SHORT-html.zip" "$tmp/html"
    (cd "$tmp" && tar czf "$OUT/tic80-v$SHORT-stubs.tar.gz" .)
    rm -rf "$tmp"
fi

echo "packaged $(ls -1 "$OUT" | wc -l | tr -d ' ') assets into $OUT"
ls -lh "$OUT"
