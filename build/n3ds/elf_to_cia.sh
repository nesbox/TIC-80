#!/bin/sh
${DEVKITARM}/bin/arm-none-eabi-strip -o tic80_cia.elf bin/tic80_n3ds
bannertool makebanner -i ../src/system/n3ds/cia/banner.png -a ../src/system/n3ds/cia/sound.wav -o tic80_cia.bnr
bannertool makesmdh -s "TIC-80" -l "TIC-80 tiny computer" -p "nesbox" -i ../src/system/n3ds/icon.png -o tic80_cia.smdh -r regionfree
echo "[CIA] Building binary"
makerom -f cia -elf tic80_cia.elf -icon tic80_cia.smdh -banner tic80_cia.bnr -desc app:4 -v -o bin/tic80.cia -target t -exefslogo -rsf ../src/system/n3ds/cia/build.rsf
