# Lugforth for TIC-80

The Forth of TIC-80 when it is built with `-DBUILD_WITH_LUGFORTH=ON` (on in a Build all tree):
[Lugforth](https://github.com/luginf/lugforth) compiled to a WebAssembly module, then translated to C by
`wasm2c` (from [WABT](https://github.com/WebAssembly/wabt)). The Forth runs natively, but inside the module's own
memory: every address a cartridge makes is an offset in it, each access is checked against its bounds and traps
past them, and the module's only way out is its imports, which `src/api/lugforth.c` provides and which check
every address they are handed. A trap stops the cartridge with an error; the player goes on.

| file | what | from |
| --- | --- | --- |
| `lugforth_w2c.c`, `lugforth_w2c.h` | the module, as C | `make -C ports/tic80 c` in Lugforth |
| `tic80_api.h` | the API words: ids, names, stack effects | `ports/tic80/api.h` in Lugforth |
| `wasm-rt.h`, `wasm-rt-impl.c`, `wasm-rt-impl.h`, `wasm-rt-exceptions.h`, `wasm-rt-exceptions-impl.c` | the wasm2c runtime | WABT 1.0.34 |
| `VERSION` | the Lugforth version the module was built from | |
| `LICENSE` | Apache 2.0: Lugforth (a fork of uEforth) and the WABT runtime | |

TIC-80 builds them with `WASM_RT_USE_MMAP=0` and `WASM_RT_MEMCHECK_BOUNDS_CHECK=1` (`cmake/lugforth.cmake`): memory
from malloc and an explicit bounds check, portable to every platform, where guard pages need mmap and a signal
handler. The one change to the runtime is in `wasm-rt-impl.c`: its `os_` functions (mmap, signals) are compiled
only when those modes use them, so that this build needs neither `sys/mman.h` nor `windows.h`.

To update, in the Lugforth repository (clang with the wasm32 target, and wasm2c from the package wabt):

    make -C ports/tic80 test c
    cp out/tic80/lugforth_w2c.c out/tic80/lugforth_w2c.h <TIC-80>/vendor/lugforth/
    cp ports/tic80/api.h <TIC-80>/vendor/lugforth/tic80_api.h

and write the version of Lugforth's `BUILD` in `VERSION`.
