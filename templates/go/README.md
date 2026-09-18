# Go Starter Project Template

This is a Go / TIC-80 starter template using [TinyGo](https://tinygo.org) to
compile Go to WebAssembly.

## Pre-requisites

- [TinyGo](https://tinygo.org/docs/getting-started/) (0.42 or newer)
- Optional: [wasm-opt](https://github.com/WebAssembly/binaryen) for smaller builds

## Files in this template

- ```tic80/``` - the TIC-80 API wrapper library (```cart/tic80```)
- ```main.go``` - the demo game source
- ```target.json``` - TinyGo build target for TIC-80's wasm runtime
- ```cart.wasmp``` - TIC-80 Wasm 'script' file with the game assets
- ```Makefile``` - convenience Makefile that builds the project
- ```run.sh``` - convenience script that builds and runs the game

## Building your game

Edit ```main.go``` to implement your game.  Export the callbacks you need
with ```//go:export``` (```BOOT```, ```TIC```, ```BDR(row)```, ```SCN(row)```,
```MENU(index)```); only ```TIC``` is required.

To build the wasm file, execute ```make```.  This generates
```build/cart.wasm```.  To build and run:

```
% sh run.sh
```

Or manually:

```
% tic80 --fs . --cmd 'load cart.wasmp & import binary build/cart.wasm & save game.tic & run & exit'
```

If your TIC-80 build cannot load text cartridges (non-PRO builds), create the
cartridge from the console instead:

```
new wasm
import binary build/cart.wasm
save game.tic
run
```

## Memory layout

The build reserves TIC-80's first 96 KiB of linear memory by using a
stack-first layout with `96 KiB + 8 KiB` of configured stack, inside the
256 KiB that TIC-80 provides (4 wasm pages).  Free memory for your game
starts at 0x18000 (```tic80.WASM_FREE_RAM```).

## Examples

- ```examples/pix``` - pixel drawing and reading, inspired by the pix()
  examples on the TIC-80 wiki.  Build and run it with:

```
make pix
tic80 --fs . --cmd 'new wasm & import binary build/pix.wasm & run'
```

- ```examples/bdr``` - screen shake glitch via the BDR callback, inspired by
  the BDR() example on the TIC-80 wiki.  Build and run it with:

```
make bdr
tic80 --fs . --cmd 'new wasm & import binary build/bdr.wasm & run'
```

- ```examples/bdr2``` - per-scanline palette gradient (>16 colors) via the
  BDR callback and both video banks, from the TIC-80 wiki.  Build and run:

```
make bdr2
tic80 --fs . --cmd 'new wasm & import binary build/bdr2.wasm & run'
```

- ```examples/ttri2``` - sprite rotation: two ```ttri``` triangles with
  per-vertex z (perspective-correct texture mapping) rotating the 2x2-tile
  sprite at UV (8,0)-(24,16) - with the demo cartridge that is the TIC guy.
  Build and run it with:

```
make ttri2
tic80 --fs . --cmd 'load cart.wasmp & import binary build/ttri2.wasm & save game.tic & run'
```

- ```examples/ttri``` - textured triangles with a UV window scaled by the
  arrow keys, from the TIC-80 wiki.  Build and run it with:

```
make ttri
tic80 --fs . --cmd 'new wasm & import binary build/ttri.wasm & run'
```

## Important notes

- **Call `tic80.Init()` first thing in `BOOT`.**  The TIC-80 wasm runtime
  never calls the module's entry point, so TinyGo's runtime (heap limits,
  GC state) only gets set up when `Init` runs.  Without it the first heap
  allocation kills the game with `[trap] unreachable executed`.
- **The tilesheet is stored tile-major**: tile (x/8, y/8) occupies 32
  consecutive bytes (4 bytes per row, two pixels per byte, low nibble =
  even x).  Writing it as a linear 128x64-byte image will look scrambled
  when sampled by `spr`/`ttri` - see `setSheetPix` in the examples.
- The TinyGo garbage collector is configured as ```leaking``` (it never
  collects).  Avoid allocations in your game loop: hoist slices, strings and
  other composite literals to package-level variables, like
  ```transparent14``` in ```main.go```.
- TIC-80 wants NUL-terminated strings, which Go strings don't provide.
  ```Print```, ```Font``` and ```Trace``` copy your string into a shared
  static buffer (max 255 bytes), so they don't allocate.
- The map remap callback (```tic80.MapOptions{Remap: ...}``) uses wasm
  function-table entries; the wrapper takes care of the low-level details.
  Modify the ```RemapResult``` in place to change the tile that gets drawn.
- You don't need a ```main()``` function; the library provides one.

This template is based on the Go template from the
[WASM-4](https://wasm4.org) project.
