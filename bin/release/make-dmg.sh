#!/usr/bin/env bash
# Build an unsigned macOS .dmg from a raw mac artifact (tic80 + dylibs).
# Runs on a macOS runner (hdiutil). The .app bundle is assembled from the
# build/macosx template resources.
#
# Usage: make-dmg.sh <artifact-dir> <out-dir> <version> <name>
#   version  full "1.2.0"
#   name     file-name suffix without "mac", e.g. "" for Intel or "-arm64"
#            → out/tic80-v<shortver>-mac<name>.dmg

set -euo pipefail

ART="${1:?artifact dir}"
OUT="${2:?out dir}"
VER="${3:?version}"
NAME="${4:-}" # "" or "-arm64"

SHORT="$(echo "$VER" | sed -E 's/\.[0-9]+$//')" # 1.2.0 -> 1.2

mkdir -p "$OUT"
APP="TIC-80.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

MAJOR="$(echo "$VER" | cut -d. -f1)"
MINOR="$(echo "$VER" | cut -d. -f2)"
PATCH="$(echo "$VER" | cut -d. -f3)"

sed \
    -e "s/@VERSION_MAJOR@/$MAJOR/" \
    -e "s/@VERSION_MINOR@/$MINOR/" \
    -e "s/@VERSION_REVISION@/$PATCH/" \
    -e "s/@VERSION_YEAR@/$(date +%Y)/" \
    build/macosx/tic80.plist.in >"$APP/Contents/Info.plist"

cp build/macosx/tic80.icns "$APP/Contents/Resources/"
install -m755 "$ART/tic80" "$APP/Contents/MacOS/tic80"
if compgen -G "$ART"/*.dylib >/dev/null; then
    cp "$ART"/*.dylib "$APP/Contents/MacOS/"
fi

hdiutil create -volname TIC-80 -srcfolder "$APP" -ov -format UDZO \
    "$OUT/tic80-v$SHORT-mac$NAME.dmg" >/dev/null

rm -rf "$APP"
ls -lh "$OUT"
