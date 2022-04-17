# Compiled Language Templates

This directory contains starter projects (and libs) for the compiled languages we support via `script: wasm`.  Currently this list includes:

- Zig (low-level)
- D (low-level)

We could easily support these as well, with contributor help:

- C
- C++
- Go
- Nelua
- Nim
- Odin
- Rust
- Wat

What is needed?  Pull over the example build/template scripts from the [WASM-4](https://wasm4.org) project [templates](https://github.com/aduros/wasm4/tree/main/cli/assets/templates).  These are typically well done and well maintained so they are a great starting point.

- Remove `wasm4.h`.  
- Replace with `tic80.h`
- Implement the core API wrapper for your language
- Implement "nice to haves" to make the API easier to use in your language

If you're interested in helping start by posting on the overarching GitHub topic:

https://github.com/nesbox/TIC-80/issues/1784


