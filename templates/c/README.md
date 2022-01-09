# C Starter Project Template 

This is a C / TIC-80 starter template.  To build it:

```
export WASI_SDK_PATH=
make
```

To import the resulting WASM to a cartridge:

```
tic80 --fs . --cmd 'new wasm & import binary build/cart.wasm & save'
```

Or from the TIC-80 console:

```
new wasm
import binary build/cart.wasm
save
```

Alternatively, you can change the script line to `script: wasm` of an
exisiting cartridge and then import.

This is assuming you've run TIC-80 with `--fs .` inside your project directory.
