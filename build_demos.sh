#!/usr/bin/env bash

cmake -DBUILD_SDL=Off -DBUILD_SOKOL=Off .
make generate_demo_carts
git clean -xdf -e *.tic.dat
git submodule foreach --recursive git clean -xfd
