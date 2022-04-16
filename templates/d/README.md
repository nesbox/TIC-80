# D Starter Project Template

This is a D TIC-80 starter template.  To build the D source files:

```
% ./build.sh
```

To import the resulting WASM file into a cartridge:

```
tic80 --fs .
tic80 prompt> load wasmdemo.wasmp
tic80 prompt> import binary cart.wasm
tic80 prompt> save game.tic
tic80 prompt> exit
```

Or, on the command line:

```
% tic80 --fs . --cmd 'load wasmdemo.wasmp & import binary cart.wasm & save game.tic & exit'
```

Now, run ```game.tic```:

```
% tic80 --fs . --cmd 'load game.tic & run & exit'
```

