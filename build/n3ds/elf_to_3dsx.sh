#!/bin/sh
echo "[3DSX] Building metadata"
${DEVKITPRO}/tools/bin/smdhtool --create "TIC-80 tiny computer" "Fantasy computer for making, playing and sharing tiny games" "Nesbox" "n3ds/icon.png" tic80.smdh
echo "[3DSX] Building binary"
${DEVKITPRO}/tools/bin/3dsxtool bin/tic80_n3ds bin/tic80.3dsx --smdh=tic80.smdh --romfs=n3ds/romfs/
