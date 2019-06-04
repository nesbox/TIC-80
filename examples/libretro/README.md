# TIC-80 libretro

Provided is a [TIC-80](https://tic.computer) libretro core to render TIC-80 cartridges through the libretro API and RetroArch.

## Build

To build the core, run the following commands.

```
cd examples/libretro
make
```

## Usage

```
retroarch -L tic80_libretro.so ../../demos/sfx.tic
```