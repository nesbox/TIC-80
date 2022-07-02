# ZIG Starter Project Template

This is a ZIG / TIC-80 starter template.  To build it, ensure you have a recent development release of Zig 0.10, then run:

```
zig build
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