# Nim Starter Project Template

This is a Nim / TIC-80 starter template. To build wasm binary and import it into
the 'cart.tic' file, ensure you have a Nim compiler, Wasi-SDK and Tic-80 on your system and run:

The linker settings reserve TIC-80's first 96 KiB of linear memory by using a stack-first layout with `96 KiB + 8 KiB` of configured stack.

```
nimble buildcart
```

Run `nimble tasks` command to see more build options.
