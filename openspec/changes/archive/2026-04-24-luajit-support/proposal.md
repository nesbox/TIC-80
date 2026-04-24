## Why

Improve script execution performance in TIC-80 by providing an alternative JIT-enabled runtime. This is targeted at performance-intensive games and simulations on desktop platforms (Windows, Linux, macOS) where LuaJIT's JIT compiler can provide significant speedups over the standard Lua 5.3 interpreter.

## What Changes

- Add a new CMake option `BUILD_WITH_LUAJIT` to enable LuaJIT support.
- Implement a build process for LuaJIT within the TIC-80 CMake system for Windows, Linux, and macOS.
- Update the Lua API layer to support switching between standard Lua 5.3 and LuaJIT 2.1 at compile time.
- **BREAKING**: When building with LuaJIT, Lua 5.3 specific features (such as bitwise operators `&`, `|`, `~`, `<<`, `>>` and integer division `//`) will not be supported in scripts.
- **BREAKING**: Enabling LuaJIT will disable all other scripting runtimes (Fennel, Moonscript, Yue, JS, etc.) to ensure a clean performance benchmark environment.

## Capabilities

### New Capabilities
- `luajit-runtime`: Provides the ability to execute Lua scripts using the LuaJIT engine on supported desktop platforms.

### Modified Capabilities
- `lua-runtime`: The standard Lua runtime will now be mutually exclusive with the LuaJIT runtime during the build process.

## Impact

- `cmake/`: New `cmake/luajit.cmake` to handle the LuaJIT build process.
- `src/api/lua.c` & `src/api/luaapi.c`: Conditional compilation to handle Lua 5.1 (LuaJIT) vs Lua 5.3 headers and API differences.
- `src/script.c`: Integration of the LuaJIT script runtime and removal of other runtimes when LuaJIT is enabled.
- `vendor/LuaJIT/`: Integration of the existing vendor source.
- Supported Platforms: Limited to Windows, Linux, and macOS. CI workflows will be updated to only build these platforms, removing console, bare metal, Android, and web targets.
