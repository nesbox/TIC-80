# ZIG Starter Project Template

This is a ZIG / TIC-80 starter template. To build it, ensure you have the latest development release (`0.14.0-dev.1421+f87dd43c1` or newer), then run:

The build reserves TIC-80's first 96 KiB of linear memory by folding that space into the configured stack size and leaving 8 KiB of actual stack above it.

```
zig build --release=small
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load cart.wasmp & import binary zig-out/bin/cart.wasm & save'
```

Or from the TIC-80 console:

```
tic80 --fs .

load zig-out/bin/cart.wasmp
import binary cart.wasm
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.

Or easy call it :)
```zsh
sh run.sh
```
