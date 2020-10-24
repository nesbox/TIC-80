# 3DS build

## Requirements

* devkitARM (tested on release 54, please use latest) + the 3ds-dev meta package
* the following additional packages:
    * devkitpro-pkgbuild-helpers
    * 3ds-libpng
    * 3ds-pkg-config
    * 3ds-zlib

## Building instructions

```
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/3ds.cmake -DN3DS=TRUE
make
```

You should now be able to find `tic80.3dsx` in build/bin.
