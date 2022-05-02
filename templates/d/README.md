# D Starter Project Template

## Pre-requisites

- [LDC](https://wiki.dlang.org/LDC)
- [WASI libc](https://github.com/WebAssembly/wasi-libc)

### WASI libc

WASI libc's [README](https://github.com/WebAssembly/wasi-libc#usage) states
that "the easiest way to get started with this is to use wask-sdk, which
includes a build of WASI libc in its sysroot."

Just [building WASI libc from source](https://github.com/WebAssembly/wasi-libc#building-from-source)
works too, for the purpose of programming TIC-80 games in D. Let's say you
install into ```$HOME/wasi-sdk/share/wasi-sysroot```, which then look like this:

```
% ls -l
total 12
drwxr-xr-x 10 pierce pierce 4096 Apr 24 16:19 include/
drwxr-xr-x  3 pierce pierce 4096 Apr 24 16:19 lib/
drwxr-xr-x  3 pierce pierce 4096 Apr 24 16:19 share/
```

## Files in this template

- ```buildcart.sh``` - convenience script to build and run the game cartridge
- ```buildwasm.sh``` - convenience script to build and run the Wasm program
- ```dub.json``` - D's dub package description file
- ```Makefile``` - convenience Makefile that invokes ```dub```
- ```wasmdemo.wasmp``` - TIC-80 Wasm 'script' file. Note the embedded game assets data at the end of the file.

## Building your game

Define the environment variable WASI_SDK_PATH; e.g., if you installed WASI
libc into ```$HOME/wasi-sdk/share/wasi-sysroot```, then ```export WASI_SDK_PATH=$HOME/wasi-sdk```.

Edit ```src/main.d``` to implement your game. You are of course free to
organize your code in more than one D source file.

If you create sprites, map, music, etc., for your game, remember to
replace the game asset data at the end of ```wasmdemo.wasmp``` with
your creations.

To build the Wasm file, execute ```make```. This generates ```cart.wasm```
in the current directory. To run:

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

