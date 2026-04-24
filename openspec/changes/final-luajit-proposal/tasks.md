## 1. Build System Setup

- [x] 1.1 Update `cmake/lua.cmake` to use `vendor/luajit-cmake` whenever `BUILD_WITH_LUA` is ON.
- [x] 1.2 Implement automatic disabling of all non-Lua scripting runtimes in `cmake/lua.cmake`.
- [ ] 1.3 Link LuaJIT targets into the TIC-80 core.

## 2. API & Compatibility

- [x] 2.1 Update `src/api/luaapi.c` with unconditional compatibility shims (`luaL_requiref`, `lua_isinteger`, `luaopen_coroutine`).
- [x] 2.2 Update `src/script.c` to conditionally exclude non-Lua runtimes.

## 3. Core & Versioning

- [x] 3.1 Update `cmake/runtime_versions.cmake` to detect and display LuaJIT version.

## 4. Verification & Finalization

- [x] 4.1 Verify successful build with `-DBUILD_WITH_LUA=ON`.
- [x] 4.2 Verify that no other scripting languages are available.
- [x] 4.3 Run benchmark carts to verify LuaJIT execution.
- [x] 4.4 Update `.github/workflows/build.yml` to remove console, bare metal, Android, and web build targets.

