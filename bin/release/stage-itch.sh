#!/usr/bin/env bash
# Stage the release assets into the layouts butler pushes to itch.io.
#
# One channel, one folder, holding what the itch app installs and launches:
# the executable for Linux, TIC-80.app for macOS, the .exe for Windows, the
# .apk for Android, the web build for HTML. The packages get a channel each
# and are never offered to the app.
#
# Nothing is uploaded as a .zip: butler unpacks one (see its "single-zip
# directories" note), and a single-file upload of an archive is a build the
# app only drops on disk — never installs. The names carry no version either,
# partly because the page shows it in a column of its own, partly because a
# channel holding one file is named after that file. The macOS channels are
# the exception: their build is a single .app bundle, a directory, so itch
# names the download "<project>-<channel>.zip" (tic80-mac.zip on the site).
#
# Usage: stage-itch.sh <asset-dir> <stage-dir> <version>
#   asset-dir  directory with the release assets (tic80-v<short>-*)
#   stage-dir  written from scratch; one subdirectory per channel
#   version    full "1.2.0" (the file-name version is its first two segments)
#
#   → stage/windows/tic80-win.exe, stage/linux/tic80-linux,
#     stage/linux-arm64/tic80-linux-arm64, stage/osx/TIC-80.app,
#     stage/osx-arm64/TIC-80.app, stage/android/tic80-android.apk,
#     stage/html/{index.html,tic80.js,tic80.wasm},
#     stage/deb-amd64/tic80-linux.deb, stage/deb-arm64/tic80-linux-arm64.deb,
#     stage/dmg-intel/tic80-mac.dmg, stage/dmg-arm64/tic80-mac-arm64.dmg

set -euo pipefail

FLAT="${1:?asset dir}"
OUT="${2:?stage dir}"
VER="${3:?version}"
SHORT="${VER%.*}"

# Every asset is required: a release cut before an asset existed would
# otherwise fail somewhere inside unzip, with a message about a path rather
# than about the release.
for asset in win.zip linux.zip linux.deb linux-arm64.zip linux-arm64.deb \
    mac.app.zip mac.dmg mac-arm64.app.zip mac-arm64.dmg android.apk html.zip; do
    [ -f "$FLAT/tic80-v$SHORT-$asset" ] ||
        { echo "missing asset tic80-v$SHORT-$asset in $FLAT" >&2; exit 1; }
done

rm -rf "$OUT"

# --- builds: installed and launched by the itch app ---
# Windows: the bare .exe out of the release zip (the static build is that
# one file; the zip holds nothing else).
mkdir -p "$OUT/windows"
unzip -qo "$FLAT/tic80-v$SHORT-win.zip" tic80.exe -d "$OUT/windows"
mv "$OUT/windows/tic80.exe" "$OUT/windows/tic80-win.exe"

# Linux: the static binary. The zips keep its exec bit, but the unzip is not
# the authority on it — chmod is.
mkdir -p "$OUT/linux" "$OUT/linux-arm64"
unzip -qo "$FLAT/tic80-v$SHORT-linux.zip" -d "$OUT/linux"
mv "$OUT/linux/tic80" "$OUT/linux/tic80-linux"
chmod +x "$OUT/linux/tic80-linux"
unzip -qo "$FLAT/tic80-v$SHORT-linux-arm64.zip" -d "$OUT/linux-arm64"
mv "$OUT/linux-arm64/tic80" "$OUT/linux-arm64/tic80-linux-arm64"
chmod +x "$OUT/linux-arm64/tic80-linux-arm64"

# macOS: the bundle keeps its own name, it is what the user sees after the
# install; the download takes the channel's name instead.
mkdir -p "$OUT/osx" "$OUT/osx-arm64"
unzip -qo "$FLAT/tic80-v$SHORT-mac.app.zip" -d "$OUT/osx"
unzip -qo "$FLAT/tic80-v$SHORT-mac-arm64.app.zip" -d "$OUT/osx-arm64"

mkdir -p "$OUT/android"
cp "$FLAT/tic80-v$SHORT-android.apk" "$OUT/android/tic80-android.apk"

# The web build is played straight out of the channel, so its files go up
# unpacked.
mkdir -p "$OUT/html"
unzip -qo "$FLAT/tic80-v$SHORT-html.zip" -d "$OUT/html"

# --- packages: downloads for the page, never offered to the app ---
mkdir -p "$OUT/deb-amd64" "$OUT/deb-arm64" "$OUT/dmg-intel" "$OUT/dmg-arm64"
cp "$FLAT/tic80-v$SHORT-linux.deb" "$OUT/deb-amd64/tic80-linux.deb"
cp "$FLAT/tic80-v$SHORT-linux-arm64.deb" "$OUT/deb-arm64/tic80-linux-arm64.deb"
cp "$FLAT/tic80-v$SHORT-mac.dmg" "$OUT/dmg-intel/tic80-mac.dmg"
cp "$FLAT/tic80-v$SHORT-mac-arm64.dmg" "$OUT/dmg-arm64/tic80-mac-arm64.dmg"
