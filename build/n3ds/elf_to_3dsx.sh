#!/bin/sh
echo "[3DSX] Building metadata"
${DEVKITPRO}/tools/bin/smdhtool --create "TIC-80 tiny computer" "free and open source fantasy computer" "tic.computer" "../src/system/n3ds/icon.png" tic80.smdh
echo "[3DSX] Building binary"
${DEVKITPRO}/tools/bin/3dsxtool bin/tic80_n3ds bin/tic80.3dsx --smdh=tic80.smdh --romfs=../src/system/n3ds/romfs/
