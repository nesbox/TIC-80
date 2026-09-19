#!/bin/sh
# assets.sh — regenerate the demo cartridges the engine embeds.
#
# This replaces assets.bat: the same carts in the same order, with the two
# duplicate luademo lines dropped and the build directory taken as an argument.
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
# Usage: ./assets.sh [build-dir]        (default: build)
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

BUILD_DIR=${1:-build}
BIN="$BUILD_DIR/bin"

for tool in prj2cart wasmp2cart bin2txt; do
    if [ ! -x "$BIN/$tool" ]; then
        echo "assets.sh: no $BIN/$tool — configure with -DBUILD_TOOLS=ON and build first" >&2
        exit 1
    fi
done

# Preflight on one project file: config.js carries binary sections, so a cart
# that comes back starting with CODE (chunk type 5) is the failure above —
# caught here, before a wrong set has overwritten the whole directory.
probe="$BUILD_DIR/.assets-probe.tic"
"$BIN/prj2cart" config.js "$probe"
if [ "$(od -An -tu1 -N1 "$probe" | tr -d ' ')" = "5" ]; then
    rm -f "$probe"
    echo "assets.sh: prj2cart produced a code-only cart — its language scripts are not linked in" >&2
    echo "           (build the tools with -DBUILD_STATIC=ON and the languages on)" >&2
    exit 1
fi
rm -f "$probe"

mkdir -p "$BUILD_DIR/assets"

# <source> <name> — one cartridge per line: <name>.tic is packed from the
# source and <name>.tic.dat from that .tic, both under the build directory.
while read -r src name; do
    [ -z "$src" ] && continue
    "$BIN/prj2cart" "$src" "$BUILD_DIR/$name.tic"
    "$BIN/bin2txt" "$BUILD_DIR/$name.tic" "$BUILD_DIR/assets/$name.tic.dat" -z
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
    "$BIN/wasmp2cart" "$src" "$BUILD_DIR/$name.tic" --binary "$binary"
    "$BIN/bin2txt" "$BUILD_DIR/$name.tic" "$BUILD_DIR/assets/$name.tic.dat" -z
done <<'WASM_CARTS'
demos/wasm/wasmdemo.wasmp               demos/wasm/wasmdemo.wasm               wasmdemo
demos/bunny/wasmmark/wasmmark.wasmp     demos/bunny/wasmmark/wasmmark.wasm     wasmmark
WASM_CARTS

# The new-cart image: no -z, it is a png the studio hands to the editors.
"$BIN/bin2txt" "$BUILD_DIR/cart.png" "$BUILD_DIR/assets/cart.png.dat"
