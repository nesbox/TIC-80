# Switch build

## Requirements

* devkitA64 + the switch-dev meta package
* the following additional packages:
    * switch-libpng
    * switch-zlib
    * switch-sdl2
    * switch-curl

## Building instructions

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build
```

You should now be able to find `tic80.nro` in build/bin.
