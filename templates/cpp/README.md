# C++ Starter Project Template (using Zig Build System)

## Pre-requisites

- [Zig Compiler](https://ziglang.org/) 0.17 or above

This is a C++ / TIC-80 starter template.

The build reserves TIC-80's first 96 KiB of linear memory by folding that space into the configured stack size and leaving 8 KiB of actual stack above it.

```
zig build
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load cart.wasmp & import binary zig-out/bin/cart.wasm & save'
```

Or from the TIC-80 console:

```
tic80 --fs .

load cart.wasmp
import binary zig-out/bin/cart.wasmp
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.

Or easy call it :)
```zsh
zig build run
```
