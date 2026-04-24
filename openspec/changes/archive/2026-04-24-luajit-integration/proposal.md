## Why

TIC-80 currently uses standard Lua 5.3, which can be a bottleneck for computationally expensive games. By integrating LuaJIT, we gain JIT-compilation for significantly higher performance on desktop platforms (Windows, Linux, macOS).

## What Changes

- **Consolidated Lua Build**: Standardized the build system to use LuaJIT as the mandatory Lua runtime when `BUILD_WITH_LUA` is enabled.
- **Removed Alternative Runtimes**: To provide a clean, high-performance environment, all other scripting languages (Fennel, Moonscript, Yue, JS, etc.) are now disabled when Lua is enabled.
- **Compatibility**: Integrated necessary shim functions (`luaL_requiref`, `lua_isinteger`) for full TIC-80 API exposure.
- **Breaking**: Lua 5.3 specific features (bitwise operators `&`, `|`, `//`) are not supported. Scripts must use Lua 5.1/LuaJIT syntax.

## Capabilities

### New Capabilities
- `luajit-runtime`: Provides high-performance script execution via LuaJIT on Windows, Linux, and macOS.

### Modified Capabilities
- `lua-runtime`: Now exclusively maps to LuaJIT, with legacy Lua 5.3 runtime support removed.

## Impact

- `cmake/`: Consolidated runtime selection logic in `cmake/lua.cmake`.
- `src/api/`: Added compatibility shims in `luaapi.c` for LuaJIT support.
- `src/script.c`: Hardened runtime exclusivity logic to ensure only Lua/LuaJIT is available.
- Supported Platforms: Desktop only (Windows, macOS, Linux).
