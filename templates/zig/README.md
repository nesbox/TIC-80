# ZIG Starter Project Template

This is a ZIG / TIC-80 starter template. To build it, ensure you have the latest development release (`0.12.0-dev.1482+e74ced21b` or newer), then run:

```
zig build -Doptimize=ReleaseSmall
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load game.tic & import binary zig-out/bin/cart.wasm & save'
```

Or from the TIC-80 console:

```
load game.tic
import binary zig-out/bin/cart.wasm
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.

