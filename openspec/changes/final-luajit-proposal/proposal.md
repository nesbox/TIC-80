## Why

TIC-80 needs a high-performance scripting runtime. By integrating LuaJIT as the mandatory Lua runtime, we gain JIT-compilation performance on all desktop platforms.

## What Changes

- **Consolidated Lua Build**: Standardized the build system to use LuaJIT as the only Lua runtime when `BUILD_WITH_LUA` is enabled.
- **Runtime Exclusivity**: Automatic disabling of all other scripting runtimes (Fennel, Moonscript, JS, etc.) when Lua is enabled.
- **Compatibility**: Integrated required shim functions (`luaL_requiref`, `lua_isinteger`) directly into `luaapi.c` for Lua 5.1/LuaJIT compatibility.
- Platform Scope: Supported platforms are Windows, macOS, and Linux. CI workflows in `.github/workflows/build.yml` will be updated to only support these platforms, removing console, bare metal, Android, and web targets.


## Capabilities

### New Capabilities
- `luajit-runtime`: Provides high-performance script execution via LuaJIT, replacing standard Lua 5.3.

## Impact

- `cmake/`: Consolidated runtime selection logic in `cmake/lua.cmake`.
- `src/api/`: Added compatibility shims in `luaapi.c` for LuaJIT support.
- `src/script.c`: Updated to exclude non-Lua runtimes when Lua is enabled.
- Supported Platforms: Desktop only (Windows, macOS, Linux).
