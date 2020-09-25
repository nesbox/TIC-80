# TIC-80 libretro

Provided is a [TIC-80](https://tic.computer) libretro core to render TIC-80 cartridges through the [libretro](https://www.libretro.com) API and [RetroArch](https://www.retroarch.com).

## Build

To build the core by itself, run the following commands.

```
git clone https://github.com/nesbox/TIC-80.git
cd TIC-80
git submodule update --init --recursive
cd build
cmake .. -DBUILD_PLAYER=OFF -DBUILD_SOKOL=OFF -DBUILD_SDL=OFF -DBUILD_DEMO_CARTS=OFF -DBUILD_LIBRETRO=ON
make
```

## Usage

```
retroarch -L lib/tic80_libretro.so sfx.tic
```
