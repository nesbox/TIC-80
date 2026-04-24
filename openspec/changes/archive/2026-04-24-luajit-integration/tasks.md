## 1. Build System Setup

- [x] 1.1 Update `cmake/lua.cmake` to make LuaJIT mandatory when `BUILD_WITH_LUA` is ON.
- [x] 1.2 Implement automatic disabling of all non-Lua scripting runtimes (JS, Fennel, etc.) in `cmake/lua.cmake`.
- [x] 1.3 Link LuaJIT targets (`luajit::lib`, `luajit::header`) into TIC-80 core.

## 2. API & Compatibility

- [x] 2.1 Update `src/api/luaapi.c` to use `LUA_JIT` macro and implement necessary compatibility shims.
- [x] 2.2 Update `src/script.c` to conditionally exclude non-Lua runtimes using `!defined(LUA_JIT)`.

## 3. Core Updates

- [x] 3.1 Update `cmake/runtime_versions.cmake` to detect and display LuaJIT version.
- [x] 3.2 Update `src/script.c` to ensure `Lua` is registered.

## 4. Verification & Finalization

- [x] 4.1 Verify successful build with LuaJIT.
- [x] 4.2 Verify that `new` command initializes a Lua cart correctly.
- [x] 4.3 Clean up remaining `TIC_BUILD_WITH_LUAJIT` and `BUILD_WITH_LUAJIT` symbols.
- [x] 4.4 Final verification of runtime exclusivity.
