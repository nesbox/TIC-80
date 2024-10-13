# 3DS build

## Requirements

* devkitARM (tested on release 65, please use latest) + the 3ds-dev meta package
* the following additional packages:
    * 3ds-libpng
    * 3ds-zlib

## Building instructions

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake
cmake --build build
```

You should now be able to find `tic80.3dsx` in build/bin.
