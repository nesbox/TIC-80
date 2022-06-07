# Rust Starter Project Template

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