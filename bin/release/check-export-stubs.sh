#!/usr/bin/env bash
# check-export-stubs.sh — drive a real client through every export path and
# check what the site hands back. Runs against a DEPLOYED site (dev by
# default), so it is the step after deploy-client.sh, not a build step: the
# stubs it verifies only exist once they are laid out on a box, and the client
# has to be a PRO one for the alone paths.
#
#   check-export-stubs.sh [--client <path>] [--site <url>] [--tag <vX.Y.Z>] [--keep]
#
#   client  the PRO client to drive; defaults to $TIC80_CLIENT, then to
#           build/bin/tic80 in this checkout
#   site    https://dev.tic80.com by default
#   tag     the directory the site serves stubs from, /export/<tag>/; the last
#           release tag in this checkout by default (a snapshot client asks for
#           that one too — the client's TIC_VERSION_TAG). A 1.x client against
#           the older site layout needs it spelled out (--tag 1.1).
#
# Step by step (dev):
#
#   1. Build the client this run needs: a PRO one, and one that talks to the
#      site being checked. A build talks to production when HEAD sits exactly
#      on a vX.Y.Z tag, so for a dev run build from a commit that is not on
#      one — or put the tag on the commit before it while cmake configures,
#      which is what the 1.2.0 check did:
#          cmake -DBUILD_PRO=On -DBUILD_WITH_ALL=ON -DCMAKE_BUILD_TYPE=Release <repo>
#      then read the build's version.h: TIC_VERSION_IS_RELEASE must be 0, and
#      TIC_VERSION_TAG the directory the site serves ("v1.2.0").
#   2. Deploy the release whose stubs are being checked — the server repo's
#      scripts/deploy-client.sh <version> dev — because the client downloads
#      what the site serves: the check is only as new as the last deploy.
#   3. ./bin/release/check-export-stubs.sh --client <build>/bin/tic80
#      One line per path, then "passed N, failed 0". A stub that came back
#      with its editors, one that differs from the served file, a missing page
#      or cartridge: each prints FAIL with the reason. --keep leaves the
#      downloaded stubs and the client logs in the work dir.
#   4. Against production: --site https://tic80.com, and the tag that client
#      asks for (the 1.1 client's layout is /export/1.1/, so --tag 1.1).
#
# What it checks, per path (console.c exportGame builds the URL, so these are
# the names the shipped client really asks for):
#
#   alone=1 (PRO)  /export/<tag>/<system><lang>  per-language stub: no editors
#   no alone       /export/<tag>/<system>        universal stub: editors stay
#
# Every stub the client received is also compared with what the site serves
# directly, so a pass says the export ended in the deployed file, not a copy,
# and that the page and the cartridge made it into the zip.
set -uo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SELF_DIR/../.." && pwd)"

SITE="https://dev.tic80.com"
CLIENT="${TIC80_CLIENT:-$REPO/build/bin/tic80}"
TAG=""
KEEP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --client) CLIENT="$2"; shift 2 ;;
        --site)   SITE="$2";   shift 2 ;;
        --tag)    TAG="$2";    shift 2 ;;
        --keep)   KEEP=1;      shift ;;
        -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

LANGS="lua js moon yue fennel scheme squirrel wren wasm janet python ruby"

[ -x "$CLIENT" ] || { echo "no client at $CLIENT (--client, or TIC80_CLIENT)" >&2; exit 2; }

if [ -z "$TAG" ]; then
    TAG="$(git -C "$REPO" describe --tags --abbrev=0 2>/dev/null)"
    [ -n "$TAG" ] || { echo "no release tag in $REPO; pass --tag" >&2; exit 2; }
fi

EXPORT="$SITE/export/$TAG"
W="${TMPDIR:-/tmp}/tic80-export-check.$$"
mkdir -p "$W"
[ "$KEEP" = 1 ] && echo "work dir: $W"

# a headless run: no window, no sound card, and the client exits by itself
# once the command queue is done (cli mode)
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy

pass=0; fail=0
ok()  { printf '  %-4s %s\n' ok   "$1"; pass=$((pass+1)); }
bad() { printf '  %-4s %s\n' FAIL "$1"; fail=$((fail+1)); }
size() { wc -c < "$1" | tr -d ' '; }

client() { # <dir> <console command>
    ( cd "$1" && "$CLIENT" --cli --fs="$1" --cmd="$2" > client.log 2>&1 )
}

editors() { grep -qa "SPRITE EDITOR" "$1"; }   # a menu label only they carry

echo "== $EXPORT — web export, alone=1, one language per run"
for lang in $LANGS; do
    d="$W/html-$lang"; mkdir -p "$d"
    client "$d" "new $lang & export html game.zip alone=1"

    zip="$d/game.zip"
    [ -f "$zip" ] || { bad "$lang: no game.zip — $(tail -1 "$d/client.log")"; continue; }
    mkdir -p "$d/x" && unzip -qo "$zip" -d "$d/x"
    wasm="$(cd "$d/x" && ls tic80*.wasm 2>/dev/null | head -1)"
    [ "$wasm" = "tic80$lang.wasm" ] || { bad "$lang: the client asked for '${wasm:-nothing}'"; continue; }
    editors "$d/x/$wasm" && { bad "$lang: the stub carries the editors"; continue; }
    [ -f "$d/x/index.html" ] || { bad "$lang: no page in the zip"; continue; }
    [ -f "$d/x/cart.tic" ] || { bad "$lang: no cartridge in the zip"; continue; }

    curl -fsS -o "$d/served" "$EXPORT/html$lang" || { bad "$lang: $EXPORT/html$lang did not download"; continue; }
    mkdir -p "$d/s" && unzip -qo "$d/served" -d "$d/s"
    cmp -s "$d/x/$wasm" "$d/s/$wasm" || { bad "$lang: the stub differs from the served html$lang"; continue; }

    ok "$lang: $(size "$d/x/$wasm") bytes, no editors, equal to the served html$lang"
done

echo "== $EXPORT — native export, alone=1, one system per run"
for sys in linux linuxarm win mac macintel; do
    d="$W/native-$sys"; mkdir -p "$d"
    out="game-$sys"
    client "$d" "new lua & export $sys $out alone=1"

    f="$d/$out"; [ "$sys" = win ] && f="$d/$out.exe"
    [ -f "$f" ] || { bad "$sys: nothing written — $(tail -1 "$d/client.log")"; continue; }
    editors "$f" && { bad "$sys: the stub carries the editors"; continue; }

    curl -fsS -o "$d/served" "$EXPORT/${sys}lua" || { bad "$sys: $EXPORT/${sys}lua did not download"; continue; }
    # the exported file is the stub with a header and the cartridge zipped onto
    # its end (console.c embedCart), so the stub's own bytes must match it whole
    head -c "$(size "$d/served")" "$f" > "$d/head"
    cmp -s "$d/head" "$d/served" || { bad "$sys: the stub differs from the served ${sys}lua"; continue; }

    ok "$sys: $(size "$d/served") bytes, no editors, cartridge appended (+$(( $(size "$f") - $(size "$d/served") )))"
done

echo "== $EXPORT — free export (no alone): the universal stubs, editors on purpose"
for sys in linux win mac html; do
    d="$W/free-$sys"; mkdir -p "$d"
    if [ "$sys" = html ]; then
        client "$d" 'new lua & export html game.zip'
        [ -f "$d/game.zip" ] || { bad "html: no game.zip — $(tail -1 "$d/client.log")"; continue; }
        mkdir -p "$d/x" && unzip -qo "$d/game.zip" -d "$d/x"
        wasm="$(cd "$d/x" && ls tic80*.wasm 2>/dev/null | head -1)"
        editors "$d/x/$wasm" || { bad "html: the universal stub has no editors"; continue; }
        ok "html: the universal stub ($wasm) carries the editors"
    else
        out="game-$sys"
        client "$d" "new lua & export $sys $out"
        f="$d/$out"; [ "$sys" = win ] && f="$d/$out.exe"
        [ -f "$f" ] || { bad "$sys: nothing written — $(tail -1 "$d/client.log")"; continue; }
        editors "$f" || { bad "$sys: the universal stub has no editors"; continue; }
        ok "$sys: the universal stub ($(size "$f") bytes with the cartridge) carries the editors"
    fi
done

echo
echo "passed $pass, failed $fail"
[ "$KEEP" = 1 ] || rm -rf "$W"
[ "$fail" = 0 ]
