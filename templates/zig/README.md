# ZIG Starter Project Template

This is a ZIG / TIC-80 starter template. To build it, ensure you have the latest development release (`0.12.0-dev.2727+fad5e7a99` or newer), then run:

```
zig build --release=small

cp zig-out/bin/cart.wasm cart.wasm
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load cart.wasmp & import binary zig-out/bin/cart.wasm & save'
```

Or from the TIC-80 console:

```
tic80 --fs .

load cart.wasmp
import binary cart.wasm
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.

