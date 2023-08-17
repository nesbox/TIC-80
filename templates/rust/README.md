# Rust Starter Project Template

## Important Note
Don't access TIC-80's I/O memory by dereferencing raw pointers. The optimiser will ruin attempts to do this, because Rust has no equivalent to C's `volatile` for direct access. Instead, use [`std::ptr::read_volatile`](https://doc.rust-lang.org/std/ptr/fn.read_volatile.html) and [`std::ptr::write_volatile`](https://doc.rust-lang.org/std/ptr/fn.write_volatile.html), or just use the standard TIC-80 `peek`/`poke`.

This is a Rust / TIC-80 starter template. Before using it, make sure you have installed the `wasm32-unknown-unknown` target using rustup:
```
rustup target add wasm32-unknown-unknown
```

Then, to build a cart.wasm file, run:

```
cargo build --release
```

To import the resulting WASM to a cartridge named `game.tic`:

```
tic80 --fs . --cmd 'new wasm & import binary target/wasm32-unknown-unknown/release/cart.wasm & save game'
```

Or from the TIC-80 console:

```
new wasm
import binary target/wasm32-unknown-unknown/release/cart.wasm
save game
```

This is assuming you've run TIC-80 with `--fs .` inside your project directory.


## wasm-opt
It is highly recommended that you run `wasm-opt` on the output `cart.wasm` file, especially if using the usual unoptimised builds. To do so, make sure `wasm-opt` is installed, then run:
```
wasm-opt -Os target/wasm32-unknown-unknown/release/cart.wasm -o cart.wasm
```
This will create a new, smaller `cart.wasm` file in the working directory.
