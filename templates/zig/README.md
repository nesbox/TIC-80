# ZIG Starter Project Template

This is a ZIG / TIC-80 starter template. To build it, ensure you have the latest stable Zig release (0.11) or the development release (0.12), then run:

```
zig build -Doptimize=ReleaseSmall
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load game.tic & import binary zig-out/lib/cart.wasm & save'
```

Or from the TIC-80 console:

```
load game.tic
import binary zig-out/lib/cart.wasm
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.

