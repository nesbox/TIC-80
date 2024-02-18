# Bunnymark in Zig / WASM

This is a Zig project.  To build it you'll need Zig 0.12 or newer.

### Building

```
zig build --release=small
```

### Replacing the assets used to build TIC-80

Copy the build over the canonical WASM file:

```
cp zig-out/bin/cart.wasm wasmmark.wasm
```

During a TIC-80 build the `wasm` and `wasmp` file will be merged into a single demo cartridge and embedded into the TIC-80 binary.
