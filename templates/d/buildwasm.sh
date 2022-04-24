#!/bin/sh
make clean
make
tic80 --fs . --cmd 'load wasmdemo.wasmp & import binary cart.wasm & run & exit'
