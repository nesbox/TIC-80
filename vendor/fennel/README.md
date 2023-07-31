# Fennel Integration

The code here is a full implementation of [Fennel](https://fennel-lang.org/),
provided as a library. Each time a new Fennel release is made, these files must
be regenerated.

### Requirements

- Lua
- `xxd` (provided by `vim`)

### Steps

1. Clone a copy of Fennel into the same directory you cloned `TIC-80` into, making them "siblings".

``` sh
git clone https://git.sr.ht/~technomancy/fennel
```

`ls` here should list both `fennel` and `TIC-80`.

2. Make a full Fennel build.

``` sh
cd fennel
make
```

3. From within `TIC-80/vendor/fennel/`, delete the previous files and regenerate them.

``` sh
rm loadfennel.lua fennel.h fennel.lua
make
```

4. About 20,000 lines will have changed. Commit the update and open a PR.
