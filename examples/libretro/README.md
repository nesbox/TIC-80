# TIC-80 libretro

Provided is a [TIC-80](https://tic.computer) libretro core to render TIC-80 cartridges through the libretro API and RetroArch.

## Build

```
cd TIC-80
git submodule update --init --recursive
cmake .
make
```

The core will be built to the `lib` folder.

## Usage

```
retroarch -L tic80_libretro.so cart.tic
```