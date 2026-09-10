#!/usr/bin/env bash
# Stage release assets into the layouts butler pushes to itch.io.
#
# The itch app launches what it finds in an upload, so every platform's
# upload has to be runnable: Windows and Android take the .exe and the .apk
# as they are, HTML takes the web build's folder, and Linux and macOS get a
# folder with the executable and a .app bundle. The .deb and the .dmg ride
# along in the same upload — they have nothing to launch, but the app
# ignores them and the download page can still offer a package.
#
# Usage: stage-itch.sh <asset-dir> <stage-dir> <version>
#   asset-dir  directory with the release assets (tic80-v<short>-*)
#   stage-dir  written from scratch; every push points inside it
#   version    full "1.2.0" (the file-name version is its first two segments)
#
#   → stage/tic80-v1.2-win.exe, tic80-v1.2-android.apk,
#     stage/linux/{tic80, tic80-v1.2-linux.deb},
#     stage/linux-arm64/{tic80, tic80-v1.2-linux-arm64.deb},
#     stage/osx/{TIC-80.app, tic80-v1.2-mac.dmg},
#     stage/osx-arm64/{TIC-80.app, tic80-v1.2-mac-arm64.dmg},
#     stage/html/index.html

set -euo pipefail

FLAT="${1:?asset dir}"
OUT="${2:?stage dir}"
VER="${3:?version}"
SHORT="${VER%.*}"

# Every asset is required: a release cut before the .app.zip step existed
# would otherwise fail somewhere inside unzip, with a message about a path
# rather than about the release.
for asset in win.zip linux.zip linux.deb linux-arm64.zip linux-arm64.deb \
    mac.app.zip mac.dmg mac-arm64.app.zip mac-arm64.dmg android.apk html.zip; do
    [ -f "$FLAT/tic80-v$SHORT-$asset" ] ||
        { echo "missing asset tic80-v$SHORT-$asset in $FLAT" >&2; exit 1; }
done

rm -rf "$OUT"
mkdir -p "$OUT/linux" "$OUT/linux-arm64" "$OUT/osx" "$OUT/osx-arm64" "$OUT/html"

# Windows: the single .exe keeps its versioned name (butler would drop it
# from an unpacked zip).
unzip -qo "$FLAT/tic80-v$SHORT-win.zip" tic80.exe -d "$OUT"
mv "$OUT/tic80.exe" "$OUT/tic80-v$SHORT-win.exe"

# Linux: the static binary in a folder of its own, the package beside it.
# The zips keep the exec bit, but the unzip is not the authority on it —
# chmod is.
unzip -qo "$FLAT/tic80-v$SHORT-linux.zip" -d "$OUT/linux"
chmod +x "$OUT/linux/tic80"
cp "$FLAT/tic80-v$SHORT-linux.deb" "$OUT/linux/"
unzip -qo "$FLAT/tic80-v$SHORT-linux-arm64.zip" -d "$OUT/linux-arm64"
chmod +x "$OUT/linux-arm64/tic80"
cp "$FLAT/tic80-v$SHORT-linux-arm64.deb" "$OUT/linux-arm64/"

# macOS: the .app bundle (zipped by the release's dmg job, symlinks and all)
# with the dmg beside it.
unzip -qo "$FLAT/tic80-v$SHORT-mac.app.zip" -d "$OUT/osx"
cp "$FLAT/tic80-v$SHORT-mac.dmg" "$OUT/osx/"
unzip -qo "$FLAT/tic80-v$SHORT-mac-arm64.app.zip" -d "$OUT/osx-arm64"
cp "$FLAT/tic80-v$SHORT-mac-arm64.dmg" "$OUT/osx-arm64/"

# Android and HTML go up as they came.
cp "$FLAT/tic80-v$SHORT-android.apk" "$OUT/"
unzip -qo "$FLAT/tic80-v$SHORT-html.zip" -d "$OUT/html"
