#!/usr/bin/env bash
# Build an ad-hoc signed macOS .dmg from a raw mac artifact (tic80 + dylibs).
# Runs on a macOS runner (hdiutil). The .app bundle is assembled from the
# build/macosx template resources, ad-hoc signed (no Developer ID), and staged
# with an /Applications shortcut for drag-and-drop install.
#
# Usage: make-dmg.sh <artifact-dir> <out-dir> <version> <shortver> <name>
#   version  full "1.2.0"
#   shortver "1.2" (first two segments; the file-name version)
#   name     suffix without "mac", e.g. "" for Intel or "-arm64"
#            → out/tic80-v<shortver>-mac<name>.dmg
#            → out/tic80-v<shortver>-mac<name>.app.zip (the same bundle, for
#              itch.io, whose launcher takes an .app rather than a dmg)

set -euo pipefail

ART="${1:?artifact dir}"
OUT="${2:?out dir}"
VER="${3:?version}"
SHORT="${4:?shortver}"
NAME="${5:-}" # "" or "-arm64"

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

# Ad-hoc sign so Gatekeeper reports "unidentified developer" (right-click →
# Open) rather than "damaged". No Developer ID — that's notarization.
codesign --force --deep -s - "$APP" >/dev/null 2>&1 || true

# Stage the .app with an /Applications shortcut for drag-and-drop install.
STAGE=".dmg-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"

# hdiutil intermittently fails with "Resource busy" on CI runners (xattrs /
# Spotlight grabbing the .app). Clear xattrs and retry a couple of times.
xattr -cr "$STAGE" 2>/dev/null || true

ok=0
for attempt in 1 2 3; do
    if hdiutil create -volname TIC-80 -srcfolder "$STAGE" -ov -format UDZO \
        "$OUT/tic80-v$SHORT-mac$NAME.dmg" >/dev/null 2>&1; then
        ok=1
        break
    fi
    sleep 3
done

if [ "$ok" != 1 ]; then
    echo "hdiutil create failed after retries" >&2
    exit 1
fi

# A zipped copy of the bundle, taken before the cleanup below: the itch.io
# jobs unpack it and push the .app itself (its launcher runs a bundle, not a
# dmg), and it rides along as a release asset.
zip -qry "$OUT/tic80-v$SHORT-mac$NAME.app.zip" "$APP"

rm -rf "$APP" "$STAGE"
ls -lh "$OUT"
