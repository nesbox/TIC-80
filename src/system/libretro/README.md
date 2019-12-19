# TIC-80 libretro

Provided is a [TIC-80](https://tic.computer) libretro core to render TIC-80 cartridges through the libretro API and RetroArch.

## Build

To build the core by itself, run the following commands.

```
git clone https://github.com/nesbox/TIC-80.git
cd TIC-80
git submodule update --init --recursive
cd build
cmake .. -DBUILD_SDL=0 -DBUILD_SOKOL=0 -DBUILD_LIBRETRO=1
make
```

## Usage

```
retroarch -L lib/tic80_libretro.so demos/sfx.tic
```
