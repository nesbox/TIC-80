## Why

TIC-80 needs a high-performance scripting runtime for computationally intensive projects. By integrating LuaJIT, we enable near-native performance for Lua scripts on desktop platforms (Windows, macOS, Linux). This implementation consolidates the build system by making LuaJIT the mandatory Lua runtime when Lua is enabled, ensuring a consistent and high-performance environment.

## What Changes

- **Consolidated Build System**: Simplified Lua configuration to use `BUILD_WITH_LUA` exclusively.
- **LuaJIT Integration**: Integrated `vendor/luajit-cmake` as the standard build method for Lua when `BUILD_WITH_LUA` is enabled.
- **Runtime Exclusivity**: Automatic disabling of all other scripting runtimes (Fennel, JS, Moonscript, etc.) when Lua is enabled to focus benchmarking and performance testing.
- **Platform Scope**: Limited support to Windows, Linux, and macOS. CI workflows will be updated to only build these platforms, removing console (3DS, Switch), bare metal, Android, and web targets.
- **Compatibility**: Added necessary shim functions (`luaL_requiref`, `lua_isinteger`) for full TIC-80 API exposure on Lua 5.1/LuaJIT.

## Capabilities

### New Capabilities
- `luajit-runtime`: Provides the ability to execute Lua scripts using the LuaJIT engine on supported desktop platforms, replacing the standard Lua 5.3 interpreter.

## Impact

- `cmake/`: Refactored `lua.cmake` and `core.cmake` to consolidate Lua runtime selection and simplify flags.
- `src/api/`: Added compatibility shims in `luaapi.c` for LuaJIT support.
- `src/script.c`: Updated to exclude non-Lua runtimes when Lua is enabled.
- `CI/CD`: GitHub Actions workflows constrained to supported desktop platforms.
- Supported Platforms: Desktop only (Windows, macOS, Linux).
