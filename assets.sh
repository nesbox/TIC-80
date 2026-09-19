#!/bin/sh
# assets.sh — regenerate the demo cartridges the engine embeds.
#
# This replaces assets.bat: the same carts in the same order, with the two
# duplicate luademo lines dropped. The directory argument is where the *tools*
# were built; the carts themselves always land in build/assets/, the one path
# the sources include from — a tool directory anywhere else cannot move them.
#
# Each source in demos/ is packed into a .tic by prj2cart (wasmp2cart for the
# wasm ones, which takes the compiled .wasm alongside the .wasmp), and every
# .tic is then turned into the C array the engine #includes from
# build/assets/*.dat — the path is relative to the *source* file
# ("../build/assets/…" from src/api/), so the .dat files have to land in
# <repo>/build/assets/ whatever build directory the tools come from.
#
# Those .dat files are tracked: run this after changing a demo, check the
# result, and commit it.
#
# Usage: ./assets.sh [tools-dir]        (default: build — where the binaries are)
#
# The tools are not part of a default configure, and they must be built with
# the language runtimes *linked in* — -DBUILD_STATIC=ON with all the
# languages on. prj2cart asks the registered scripts for the language's
# comment prefix (projectComment, src/studio/project.c: the scripts have to be
# in the tool's own address space, a shared build registers none), and from
# that prefix it reads the <TILES>/<SAMPLES>/<TRACKS>/… sections a project
# file carries. Without them the whole file goes into the code chunk: the
# carts come out code-only, missing the banks — silently, with no error, and
# every file in build/assets/ rewritten wrong.
#
#   cmake -DBUILD_TOOLS=ON -DBUILD_WITH_ALL=ON -DBUILD_STATIC=ON .. && cmake --build .
set -eu

cd "$(dirname "$0")"

BUILD_DIR=${1:-build}       # where the tools were built
BIN="$BUILD_DIR/bin"
OUT=build                   # fixed: "../build/assets/…" is what the sources include

for tool in prj2cart wasmp2cart bin2txt; do
    if [ ! -x "$BIN/$tool" ]; then
        echo "assets.sh: no $BIN/$tool — configure with -DBUILD_TOOLS=ON and build first" >&2
        exit 1
    fi
done

mkdir -p "$OUT/assets"

# Preflight on two project files, one javascript and one lua: a tool with no
# language registered finds no comment prefix, misses every binary section and
# writes a cart whose first chunk is CODE (type 5) instead of a bank chunk —
# the failure above, caught before a wrong set has overwritten the directory.
# Two files because the languages register one by one, and a probe that cannot
# be read at all is a failure too, not a pass.
for probe_src in config.js demos/luademo.lua; do
    probe="$OUT/.assets-probe.tic"
    "$BIN/prj2cart" "$probe_src" "$probe"
    first=$(od -An -tu1 -N1 "$probe" 2>/dev/null | tr -d ' \t' || true)
    rm -f "$probe"

    if [ -z "$first" ]; then
        echo "assets.sh: cannot read the cart $probe_src converted to" >&2
        exit 1
    fi

    if [ "$first" = "5" ]; then
        echo "assets.sh: $probe_src came out a code-only cart — the tools have no language scripts in them" >&2
        echo "           (build them with -DBUILD_STATIC=ON and every language on)" >&2
        exit 1
    fi
done

# <source> <name> — one cartridge per line: <name>.tic is packed from the
# source and <name>.tic.dat from that .tic, the intermediate beside the .dat
# it becomes, as assets.bat had it.
while read -r src name; do
    [ -z "$src" ] && continue
    "$BIN/prj2cart" "$src" "$OUT/$name.tic"
    "$BIN/bin2txt" "$OUT/$name.tic" "$OUT/assets/$name.tic.dat" -z
done <<'CARTS'
config.js                               config
demos/luademo.lua                       luademo
demos/benchmark.lua                     benchmark
demos/bpp.lua                           bpp
demos/car.lua                           car
demos/fenneldemo.fnl                    fenneldemo
demos/fire.lua                          fire
demos/font.lua                          font
demos/janetdemo.janet                   janetdemo
demos/jsdemo.js                         jsdemo
demos/miniscriptdemo.ms                 miniscriptdemo
demos/moondemo.moon                     moondemo
demos/music.lua                         music
demos/p3d.lua                           p3d
demos/palette.lua                       palette
demos/pythondemo.py                     pythondemo
demos/quest.lua                         quest
demos/rubydemo.rb                       rubydemo
demos/schemedemo.scm                    schemedemo
demos/sfx.lua                           sfx
demos/squirreldemo.nut                  squirreldemo
demos/tetris.lua                        tetris
demos/forthdemo.fth                     forthdemo
demos/wrendemo.wren                     wrendemo
demos/yuedemo.yue                       yuedemo
demos/bunny/forthmark.fth               forthmark
demos/bunny/janetmark.janet             janetmark
demos/bunny/jsmark.js                   jsmark
demos/bunny/luamark.lua                 luamark
demos/bunny/miniscriptmark.ms           miniscriptmark
demos/bunny/moonmark.moon               moonmark
demos/bunny/pythonmark.py               pythonmark
demos/bunny/rubymark.rb                 rubymark
demos/bunny/schememark.scm              schememark
demos/bunny/squirrelmark.nut            squirrelmark
demos/bunny/wrenmark.wren               wrenmark
demos/bunny/yuemark.yue                 yuemark
CARTS

# <source.wasmp> <binary.wasm> <name> — the wasm carts carry the compiled
# module as a second source, which is why they do not fit the list above.
while read -r src binary name; do
    [ -z "$src" ] && continue
    "$BIN/wasmp2cart" "$src" "$OUT/$name.tic" --binary "$binary"
    "$BIN/bin2txt" "$OUT/$name.tic" "$OUT/assets/$name.tic.dat" -z
done <<'WASM_CARTS'
demos/wasm/wasmdemo.wasmp               demos/wasm/wasmdemo.wasm               wasmdemo
demos/bunny/wasmmark/wasmmark.wasmp     demos/bunny/wasmmark/wasmmark.wasm     wasmmark
WASM_CARTS

# The new-cart image: no -z, it is a png the studio hands to the editors.
"$BIN/bin2txt" "$OUT/cart.png" "$OUT/assets/cart.png.dat"
