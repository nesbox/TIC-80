# TIC-80 libretro

Provided is a [TIC-80](https://tic.computer) libretro core to render TIC-80 cartridges through the libretro API and RetroArch.

## Build

To build the core, run the following commands.

```
cmake . -DBUILD_SDL=0 -DBUILD_SOKOL=0
make
```

## Usage

```
retroarch -L lib/tic80_libretro.so demos/sfx.tic
```
