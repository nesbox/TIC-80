#!/usr/bin/env bash
# Build a macOS .dmg and a zipped .app from a raw mac artifact (tic80 +
# dylibs). Runs on a macOS runner (hdiutil, notarytool). The bundle comes from
# the build/macosx template resources and is staged with an /Applications
# shortcut for drag-and-drop install.
#
# Signing credentials come from the environment, all repository secrets:
#   MACOS_CERT_P12       base64 Developer ID Application .p12, with its key
#   MACOS_CERT_PASSWORD  password of that .p12
#   APPLE_ID             Apple ID of the developer account
#   APPLE_APP_PASSWORD   app-specific password for that Apple ID
#   APPLE_TEAM_ID        Team ID the Apple ID belongs to
# Signing is opt-in: only MACOS_SIGNING=1 enables it, and then every missing
# credential is fatal. Without the switch the bundle is ad-hoc signed, even if
# credentials happen to be in the environment — a pull request never signs.
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

BUNDLE="TIC-80.app"
STAGE=".dmg-stage"
DMG="$OUT/tic80-v$SHORT-mac$NAME.dmg"
APP_ZIP="$OUT/tic80-v$SHORT-mac$NAME.app.zip"
WORK=""
KEYCHAIN=""
old_keychains=()

cleanup() {
    rm -rf "$BUNDLE" "$STAGE"
    if [ -n "$KEYCHAIN" ]; then
        if [ "${#old_keychains[@]}" -gt 0 ]; then
            security list-keychains -d user -s "${old_keychains[@]}" 2>/dev/null || true
        fi
        security delete-keychain "$KEYCHAIN" 2>/dev/null || true
    fi
    if [ -n "$WORK" ]; then
        rm -rf "$WORK"
    fi
}
trap cleanup EXIT

mkdir -p "$OUT"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"

MAJOR="$(echo "$VER" | cut -d. -f1)"
MINOR="$(echo "$VER" | cut -d. -f2)"
PATCH="$(echo "$VER" | cut -d. -f3)"

sed \
    -e "s/@VERSION_MAJOR@/$MAJOR/" \
    -e "s/@VERSION_MINOR@/$MINOR/" \
    -e "s/@VERSION_REVISION@/$PATCH/" \
    -e "s/@VERSION_YEAR@/$(date +%Y)/" \
    build/macosx/tic80.plist.in >"$BUNDLE/Contents/Info.plist"

cp build/macosx/tic80.icns "$BUNDLE/Contents/Resources/"
install -m755 "$ART/tic80" "$BUNDLE/Contents/MacOS/tic80"
if compgen -G "$ART"/*.dylib >/dev/null; then
    cp "$ART"/*.dylib "$BUNDLE/Contents/MacOS/"
fi

SIGN=0
if [ "${MACOS_SIGNING:-0}" = 1 ]; then
    SIGN=1
fi

if [ "$SIGN" = 1 ]; then
    if [ -z "${MACOS_CERT_P12:-}" ] || [ -z "${MACOS_CERT_PASSWORD:-}" ] ||
        [ -z "${APPLE_ID:-}" ] || [ -z "${APPLE_APP_PASSWORD:-}" ] ||
        [ -z "${APPLE_TEAM_ID:-}" ]; then
        echo "MACOS_SIGNING=1 needs MACOS_CERT_P12, MACOS_CERT_PASSWORD, APPLE_ID, APPLE_APP_PASSWORD and APPLE_TEAM_ID" >&2
        exit 1
    fi

    WORK="$(mktemp -d "${TMPDIR:-/tmp}/tic80-notarize.XXXXXX")"
    KEYCHAIN="$WORK/signing.keychain-db"
    CERT_FILE="$WORK/cert.p12"
    KEYCHAIN_PASSWORD="$(uuidgen)"

    while IFS= read -r line; do
        old_keychains+=("$(printf '%s' "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/"//g')")
    done < <(security list-keychains -d user)

    security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
    security set-keychain-settings -lut 21600 "$KEYCHAIN"
    security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
    printf '%s' "$MACOS_CERT_P12" | base64 -d >"$CERT_FILE"
    security import "$CERT_FILE" -k "$KEYCHAIN" -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign >/dev/null
    # The portal ships the leaf only; the intermediate is Apple's public CA.
    security import "$(dirname "$0")/DeveloperIDG2CA.cer" -k "$KEYCHAIN" -T /usr/bin/codesign >/dev/null
    security set-key-partition-list -S apple-tool:,apple: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN" >/dev/null
    if [ "${#old_keychains[@]}" -gt 0 ]; then
        security list-keychains -d user -s "$KEYCHAIN" "${old_keychains[@]}"
    else
        security list-keychains -d user -s "$KEYCHAIN"
    fi

    IDENTITIES="$(security find-identity -v -p codesigning "$KEYCHAIN" | sed -n 's/.*"\(Developer ID Application: .*\)"$/\1/p')"
    IDENTITY="${IDENTITIES%%$'\n'*}"
    if [ -z "$IDENTITY" ]; then
        echo "no Developer ID Application identity: is the .p12 exported with its private key?" >&2
        exit 1
    fi
    echo "signing with $IDENTITY"

    for lib in "$BUNDLE"/Contents/MacOS/*.dylib; do
        if [ -e "$lib" ]; then
            codesign --force --options runtime --timestamp --sign "$IDENTITY" "$lib" >/dev/null
        fi
    done
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$BUNDLE/Contents/MacOS/tic80" >/dev/null
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$BUNDLE" >/dev/null

    notarize() { # notarize <file> <label>
        local file="$1" label="$2" out
        echo "notarizing $label"
        if ! out="$(xcrun notarytool submit "$file" \
            --apple-id "$APPLE_ID" \
            --password "$APPLE_APP_PASSWORD" \
            --team-id "$APPLE_TEAM_ID" \
            --wait --timeout 30m 2>&1)"; then
            printf '%s\n' "$out" >&2
            echo "notary service rejected $label" >&2
            exit 1
        fi
        if ! printf '%s' "$out" | grep -q "status: Accepted"; then
            printf '%s\n' "$out" >&2
            echo "notarization of $label did not reach Accepted" >&2
            exit 1
        fi
    }

    ditto -c -k --keepParent "$BUNDLE" "$WORK/TIC-80.zip"
    notarize "$WORK/TIC-80.zip" "app"
    xcrun stapler staple "$BUNDLE" >/dev/null
else
    # Ad-hoc so Gatekeeper reports "unidentified developer" (right-click → Open)
    # rather than "damaged". Every pull request and local run lands here.
    codesign --force --deep -s - "$BUNDLE" >/dev/null
fi

# Stage the .app with an /Applications shortcut for drag-and-drop install.
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$BUNDLE" "$STAGE/"
ln -s /Applications "$STAGE/Applications"

# hdiutil intermittently fails with "Resource busy" on CI runners (xattrs /
# Spotlight grabbing the .app). Clear xattrs and retry a couple of times.
xattr -cr "$STAGE" 2>/dev/null || true

ok=0
for attempt in 1 2 3; do
    if hdiutil create -volname TIC-80 -srcfolder "$STAGE" -ov -format UDZO \
        "$DMG" >/dev/null 2>&1; then
        ok=1
        break
    fi
    sleep 3
done

if [ "$ok" != 1 ]; then
    echo "hdiutil create failed after retries" >&2
    exit 1
fi

if [ "$SIGN" = 1 ]; then
    codesign --force --timestamp --sign "$IDENTITY" "$DMG" >/dev/null
    notarize "$DMG" "dmg"
    xcrun stapler staple "$DMG" >/dev/null

    codesign --verify --deep --strict "$BUNDLE"
    if ! spctl_out="$(spctl -a -vvv -t exec "$BUNDLE" 2>&1)"; then
        printf '%s\n' "$spctl_out" >&2
        echo "Gatekeeper rejected $BUNDLE" >&2
        exit 1
    fi
    case "$spctl_out" in
        *"source=Notarized Developer ID"*) ;;
        *)
            printf '%s\n' "$spctl_out" >&2
            echo "Gatekeeper did not report a Notarized Developer ID source" >&2
            exit 1
            ;;
    esac
    xcrun stapler validate "$BUNDLE" >/dev/null
    xcrun stapler validate "$DMG" >/dev/null
fi

# A zipped copy of the bundle, taken after stapling: the itch.io jobs unpack
# it and push the .app itself (its launcher runs a bundle, not a dmg).
ditto -c -k --keepParent "$BUNDLE" "$APP_ZIP"

ls -lh "$OUT"
