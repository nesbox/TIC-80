# Rust Starter Project Template

## Important Note
This template currently produces unoptimised builds. This is required due to TIC-80's memory layout placing data at the address 0, which Rust does not understand.
If you aren't using direct framebuffer access, you should be able to use another level by changing the  `Cargo.toml`.

This is a Rust / TIC-80 starter template. To build it:

```
cargo build --release
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'load game.tic & import binary target/wasm32-unknown-unknown/release/cart.wasm & save'
```

Or from the TIC-80 console:

```
load game.tic
import binary target/wasm32-unknown-unknown/release/cart.wasm
save
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.


## wasm-opt
It is highly recommended that you run `wasm-opt` on the output `cart.wasm` file, especially if using the usual unoptimised builds. To do so, make sure `wasm-opt` is installed, then run:
```
wasm-opt -Os target/wasm32-unknown-unknown/release/cart.wasm -o cart.wasm
```
This will create a new, smaller `cart.wasm` file in the working directory.