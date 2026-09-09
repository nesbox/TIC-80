#!/usr/bin/env bash
# deploy-client.sh — push the client's web player + export stubs to a server
# (dev by default) from a finished release run, without touching the server
# binary.
#
#   deploy-client.sh <version> <run-id> [dev|prod]
#
# <run-id> is the GitHub Actions run id of the release workflow. It downloads
# the html + export-stub artifacts and rsync's them into place:
#   - universal wasm -> <root>/js/<version>/        (TIC80_JS_DIR)
#   - native stubs   -> <root>/export/<major.minor>/<platform>/
#
# The server serves /js/<version>/ straight from JSDir, so a client release
# is files-only: no go build, no restart.
#
#   version   full "1.2.0"
#   shortver  "1.2" (derived)

set -euo pipefail

VERSION="${1:?usage: deploy-client.sh <version> <run-id> [dev|prod]}"
RUN_ID="${2:?usage: deploy-client.sh <version> <run-id> [dev|prod]}"
TARGET="${3:-dev}"

SHORT="${VERSION%.*}"

case "$TARGET" in
    dev)  HOST="ubuntu@162.19.228.166"; ROOT="/srv/tic80-dev"; UNIT="tic80-dev" ;;
    prod) HOST="ubuntu@162.19.228.166"; ROOT="/srv/tic80";     UNIT="tic80" ;;
    *) echo "unknown target: $TARGET" >&2; exit 1 ;;
esac

JS_DIR="$ROOT/js"
EXPORT="$ROOT/export"
ENV="$ROOT/tic80.env"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> downloading artifacts from run $RUN_ID"
gh run download "$RUN_ID" --repo nesbox/TIC-80 --dir "$WORK"

echo "==> staging web player -> $JS_DIR/$VERSION"
ssh "$HOST" "mkdir -p '$JS_DIR/$VERSION'"
rsync -a \
    "$WORK/tic80-html/tic80.js" \
    "$WORK/tic80-html/tic80.wasm" \
    "$WORK/tic80-html/index.html" \
    "$HOST:$JS_DIR/$VERSION/"

echo "==> staging export stubs -> $EXPORT/$SHORT/<platform>"
ssh "$HOST" "mkdir -p '$EXPORT/$SHORT/win' '$EXPORT/$SHORT/linux' '$EXPORT/$SHORT/linux-arm64' '$EXPORT/$SHORT/mac' '$EXPORT/$SHORT/mac-arm64'"
rsync -a "$WORK/tic80-windows-export/"       "$HOST:$EXPORT/$SHORT/win/"
rsync -a "$WORK/tic80-linux-gcc12-export/"   "$HOST:$EXPORT/$SHORT/linux/"
rsync -a "$WORK/tic80-linux-arm64-gcc12-export/" "$HOST:$EXPORT/$SHORT/linux-arm64/"
rsync -a "$WORK/tic80-macos-export/"         "$HOST:$EXPORT/$SHORT/mac/"
rsync -a "$WORK/tic80-macos-arm64-export/"   "$HOST:$EXPORT/$SHORT/mac-arm64/"

echo "==> updating TIC80_VERSION=$VERSION and restarting $UNIT"
ssh "$HOST" "sed -i 's/^TIC80_VERSION=.*/TIC80_VERSION=$VERSION/' '$ENV'"
ssh "$HOST" "sudo systemctl restart $UNIT"

echo "==> done — $VERSION player + stubs deployed to $TARGET"
