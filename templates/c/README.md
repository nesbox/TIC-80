# C Starter Project Template

## Pre-requisites

- [WASI SDK](https://github.com/WebAssembly/wasi-sdk)

## Files in this template

- ```buildcart.sh``` - convenience script to build and run the game cartridge
- ```buildwasm.sh``` - convenience script to build and run the Wasm program
- ```Makefile``` - convenience Makefile that builds the project
- ```wasmdemo.wasmp``` - TIC-80 Wasm 'script' file. Note the embedded game assets data at the end of the file.

## Building your game

Define the environment variable WASI_SDK_PATH; e.g., if you installed WASI
into ```$HOME/wasi-sdk```, then ```export WASI_SDK_PATH=$HOME/wasi-sdk```.

Edit ```src/main.c``` to implement your game. You are of course free to
organize your code in more than one C source file.

If you create sprites, map, music, etc., for your game, remember to
replace the game asset data at the end of ```wasmdemo.wasmp``` with
your creations.

To build the Wasm file, execute ```make```. This generates ```cart.wasm```
in the build directory. To run:

```
% tic80 --fs . --cmd 'load wasmdemo.wasmp & import binary cart.wasm & run & exit'
```

The script ```buildwasm.sh``` contains above steps as a convenience.

To build a TIC-80 cartridge, first build the Wasm file, then build the
cartridge file:

```
% tic80 --fs . --cmd 'load wasmdemo.wasmp & import binary cart.wasm & save game.tic & exit'
```

You can then run your cartridge as follows:

```
% tic80 --fs . --cmd 'load game.tic & run & exit'
```

The script ```buildcart.sh``` does the above steps as a convenience.

## Additional Notes
TIC-80 API functions that merely duplicate standard library functionality are not imported. Please use the `memcpy` and `memset` functions provided by the C standard library instead.