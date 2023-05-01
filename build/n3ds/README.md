# 3DS build

## Requirements

* devkitARM (tested on release 60, please use latest) + the 3ds-dev meta package
* the following additional packages:
    * devkitpro-pkgbuild-helpers
    * 3ds-libpng
    * 3ds-pkg-config
    * 3ds-zlib

## Building instructions

```
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake -DN3DS=TRUE
make
```

You should now be able to find `tic80.3dsx` in build/bin.
