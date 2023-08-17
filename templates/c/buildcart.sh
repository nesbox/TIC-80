#!/bin/sh
rm -f game.tic
make clean
make
tic80 --fs . --cmd 'load wasmdemo.wasmp & import binary build/cart.wasm & save game.tic & exit'
tic80 --fs . --cmd 'load game.tic & run & exit'
