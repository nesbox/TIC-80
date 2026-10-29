#!/bin/sh
# Assemble the sokol Android APK from a CMake build: aapt2, zipalign and
# apksigner. One APK per call; no gradle and no dex.

set -eu

usage() {
    echo "usage: $0 <libtic80.so> <out.apk> [application id]" >&2
    echo "  UNSIGNED=1 builds without a signature; otherwise" >&2
    echo "  RELEASE_STORE_PASSWORD and RELEASE_KEY_PASSWORD are required." >&2
    exit 2
}

[ $# -ge 2 ] && [ $# -le 3 ] || usage

LIB=$1
OUT=$2
APP=${3:-com.nesbox.tic}

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
HERE=$ROOT/build/android-sokol
SDK=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}

[ -n "$SDK" ] || { echo "apk.sh: ANDROID_HOME (or ANDROID_SDK_ROOT) is not set" >&2; exit 1; }

BUILD_TOOLS=${BUILD_TOOLS:-$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -n 1)}
[ -n "${BUILD_TOOLS:-}" ] && [ -d "$BUILD_TOOLS" ] || { echo "apk.sh: no build-tools under $SDK" >&2; exit 1; }

ANDROID_JAR=$SDK/platforms/android-31/android.jar
[ -f "$ANDROID_JAR" ] || { echo "apk.sh: $ANDROID_JAR is missing (install platforms;android-31)" >&2; exit 1; }

AAPT2=$BUILD_TOOLS/aapt2
ZIPALIGN=$BUILD_TOOLS/zipalign
APKSIGNER=$BUILD_TOOLS/apksigner

for tool in "$AAPT2" "$ZIPALIGN" "$APKSIGNER"; do
    [ -x "$tool" ] || { echo "apk.sh: $tool is missing" >&2; exit 1; }
done

# The build's library carries symbols and DWARF; gradle-stripped APKs do not.
NDK=${ANDROID_NDK:-$(ls -d "$SDK"/ndk/* 2>/dev/null | sort -V | tail -n 1)}
[ -n "${NDK:-}" ] && [ -d "$NDK" ] || { echo "apk.sh: ANDROID_NDK is not set (needed for llvm-strip)" >&2; exit 1; }

STRIP=$(ls -d "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip 2>/dev/null | head -n 1)
[ -n "${STRIP:-}" ] && [ -x "$STRIP" ] || { echo "apk.sh: llvm-strip is missing under $NDK" >&2; exit 1; }

[ -s "$LIB" ] || { echo "apk.sh: the library $LIB is missing or empty" >&2; exit 1; }

# The version rule build/android/app/build.gradle carries: an exact vX.Y.Z tag
# is that release, anything else takes the line and the commit count, and the
# code follows the last release's patch so a snapshot installs over it.
tag=$(git -C "$ROOT" describe --tags --exact-match HEAD 2>/dev/null || true)

if echo "$tag" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+$'; then
    major=$(echo "$tag" | cut -d. -f1 | tr -d v)
    minor=$(echo "$tag" | cut -d. -f2)
    patch=$(echo "$tag" | cut -d. -f3)
    code_patch=$patch
    [ "$code_patch" -lt 100 ] || code_patch=0
    VERSION_NAME=$major.$minor.$patch
else
    last=$(git -C "$ROOT" describe --tags --abbrev=0 HEAD 2>/dev/null | tr -d v || true)

    echo "$last" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' ||
        { echo "apk.sh: no vX.Y.Z tag on HEAD or behind it; git fetch --tags" >&2; exit 1; }

    major=$(echo "$last" | cut -d. -f1)
    minor=$(echo "$last" | cut -d. -f2)
    patch=$(echo "$last" | cut -d. -f3)
    # ticCodePatch() runs before the +1 in build.gradle, not after it.
    code_patch=$patch
    [ "$code_patch" -lt 100 ] || code_patch=0
    code_patch=$((code_patch + 1))
    VERSION_NAME=$major.$minor.$(git -C "$ROOT" rev-list HEAD --count)-dev
fi

VERSION_CODE=$((major * 10000 + minor * 100 + code_patch))

WORK=$(mktemp -d "${TMPDIR:-/tmp}/tic80-apk.XXXXXX")
trap 'rm -rf "$WORK"' EXIT INT TERM

"$AAPT2" compile --dir "$HERE/res" -o "$WORK/res.zip"

"$AAPT2" link \
    -o "$WORK/base.apk" \
    -I "$ANDROID_JAR" \
    --manifest "$HERE/AndroidManifest.xml" \
    --min-sdk-version 26 \
    --target-sdk-version 31 \
    --version-code "$VERSION_CODE" \
    --version-name "$VERSION_NAME" \
    --rename-manifest-package "$APP" \
    "$WORK/res.zip"

mkdir -p "$WORK/apk/lib/arm64-v8a"
cp "$LIB" "$WORK/apk/lib/arm64-v8a/libtic80.so"
"$STRIP" --strip-unneeded "$WORK/apk/lib/arm64-v8a/libtic80.so"
(cd "$WORK/apk" && zip -q -0 "$WORK/base.apk" lib/arm64-v8a/libtic80.so)

# extractNativeLibs is off, so the library is stored and aligned to a 16 KB page.
"$ZIPALIGN" -f -P 16 4 "$WORK/base.apk" "$WORK/aligned.apk"
"$ZIPALIGN" -c -P 16 4 "$WORK/aligned.apk"

if [ "${UNSIGNED:-0}" = 1 ]; then
    cp "$WORK/aligned.apk" "$OUT"
else
    : "${RELEASE_STORE_PASSWORD:?RELEASE_STORE_PASSWORD is not set}"
    : "${RELEASE_KEY_PASSWORD:?RELEASE_KEY_PASSWORD is not set}"

    STORE=${RELEASE_STORE_FILE:-$ROOT/build/android/my-release-key.keystore}
    ALIAS=${RELEASE_KEY_ALIAS:-alias_name}

    [ -f "$STORE" ] || { echo "apk.sh: the keystore $STORE is missing" >&2; exit 1; }

    "$APKSIGNER" sign --ks "$STORE" --ks-key-alias "$ALIAS" \
        --ks-pass "pass:$RELEASE_STORE_PASSWORD" \
        --key-pass "pass:$RELEASE_KEY_PASSWORD" \
        --out "$OUT" "$WORK/aligned.apk"
    "$APKSIGNER" verify "$OUT"
fi

echo "$OUT: $APP $VERSION_NAME ($VERSION_CODE)"
