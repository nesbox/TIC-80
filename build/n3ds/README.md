# 3DS build

## Requirements

* devkitARM (tested on release 54, please use latest) + the 3ds-dev meta package
* the following additional packages:
    * devkitpro-pkgbuild-helpers
    * 3ds-libpng
    * 3ds-pkg-config
    * 3ds-zlib

If you'd like to build a .CIA archive in addition to .3DSX, the following tools must be added to your PATH:

* bannertool
* makerom

## Building instructions

```
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/3ds.cmake -DN3DS=TRUE -DN3DS_BUILD_CIA=?
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/3ds.cmake -DN3DS=TRUE -DN3DS_BUILD_CIA=? # must be run twice due to bug
make
```

You should now be able to find `tic80.3dsx` (and `tic80.cia`, if you enabled the CIA build option) in build/bin.
