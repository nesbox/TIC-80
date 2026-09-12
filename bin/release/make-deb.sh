#!/usr/bin/env bash
# Build a .deb from a raw linux artifact (tic80 + optional libs), with the
# desktop entry, the icon and the mime type wired in. Runs on a Linux runner
# (dpkg-deb), from the repository root — the desktop files come from
# build/linux/.
#
# Usage: make-deb.sh <artifact-dir> <out.deb> <arch> <version>
#   arch     dpkg architecture: "amd64" or "arm64"
#   version  full "1.2.0"

set -euo pipefail

SRC="${1:?artifact dir}"
DST="${2:?out deb}"
ARCH="${3:?arch}"
VER="${4:?version}"

pkg=$(mktemp -d)
trap 'rm -rf "$pkg"' EXIT

# dpkg-deb creates the archive itself, not its directory (same as make-dmg.sh).
mkdir -p "$(dirname "$DST")"

mkdir -p "$pkg/DEBIAN" "$pkg/usr/bin" "$pkg/usr/lib/tic80" \
         "$pkg/usr/share/applications" \
         "$pkg/usr/share/icons/hicolor/256x256/apps" \
         "$pkg/usr/share/metainfo" "$pkg/usr/share/mime/packages"

install -m755 "$SRC/tic80" "$pkg/usr/bin/tic80"
if compgen -G "$SRC"/*.so >/dev/null; then
    install -m644 "$SRC"/*.so "$pkg/usr/lib/tic80/"
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
Architecture: $ARCH
Maintainer: Nesbox <grigoruk@gmail.com>
Homepage: https://tic80.com
Depends: libcurl4 | libcurl4t64
Description: Fantasy computer for making, playing and sharing tiny games.
EOF

dpkg-deb --build --root-owner-group "$pkg" "$DST" >/dev/null
