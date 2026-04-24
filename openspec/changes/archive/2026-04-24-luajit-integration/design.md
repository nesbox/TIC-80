## Context

TIC-80 currently uses standard Lua 5.3, which can be a performance bottleneck for intensive games. We are integrating LuaJIT as the standard and only Lua-based runtime, effectively replacing the standard interpreter and unifying the build path.

## Goals / Non-Goals

**Goals:**
- Unify Lua runtime as LuaJIT on desktop platforms (Windows, macOS, Linux).
- Integrate `vendor/luajit-cmake` for build orchestration.
- Exclude all other non-Lua scripting runtimes to ensure a clean benchmarking and high-performance environment.
- Maintain full TIC-80 API compatibility via shims.

**Non-Goals:**
- **Lua 5.3 feature compatibility**: Scripts must be 5.1/LuaJIT compatible.
- **Other scripting languages**: Fennel, JS, Moonscript, etc., are explicitly excluded in Lua builds.
- **Support for consoles/web**: Keeping the implementation focused on desktop.

## Decisions

- **Build Orchestration**: Use `add_subdirectory(vendor/luajit-cmake)` to leverage the existing CMake integration for LuaJIT, which handles the complex `minilua` and `buildvm` steps.
- **Conditional Compilation**: Use `TIC_BUILD_WITH_LUA` (already present) to gate runtime selection in `cmake/lua.cmake`.
- **Runtime Exclusivity**: CMake logic will force all other `BUILD_WITH_*` flags to `OFF` when `BUILD_WITH_LUA` is enabled, guaranteeing only the LuaJIT runtime is available.
- **Compatibility Shims**: Implement `luaL_requiref`, `lua_isinteger`, etc., in `src/api/luaapi.c` to interface with LuaJIT.

## Risks / Trade-offs

- **[Risk] Compatibility**: Existing carts relying on 5.3 features will break. 
  - *Mitigation*: This is an intentional breaking change to optimize for high-performance needs.
- **[Risk] Build Complexity**: LuaJIT bootstrapping.
  - *Mitigation*: Rely on the `luajit-cmake` helper as it is well-tested across platforms.
