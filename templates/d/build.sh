#!/bin/sh
(cd src; ldc2 -mtriple=wasm32-unknown-unknown-wasm -L-allow-undefined -L-no-entry -of=../cart.wasm main.d)
