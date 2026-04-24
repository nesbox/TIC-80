## 1. Build System Integration

- [ ] 1.1 Integrate `vendor/luajit-cmake` into the TIC-80 build system by updating `cmake/lua.cmake`.
- [ ] 1.2 Configure `cmake/lua.cmake` to use `luajit::lib` and `luajit::header` when `BUILD_WITH_LUAJIT` is ON.
- [ ] 1.3 Update CMake to automatically set other script options (`BUILD_WITH_MOON`, `BUILD_WITH_JS`, etc.) to OFF when `BUILD_WITH_LUAJIT` is ON.
- [ ] 1.4 Ensure mutual exclusivity between `BUILD_WITH_LUA` (standard 5.3) and `BUILD_WITH_LUAJIT`.

## 2. API Layer Adaptation

- [ ] 2.1 Update `src/api/luaapi.h` to include correct Lua headers based on `TIC_BUILD_WITH_LUAJIT`.
- [ ] 2.2 Modify `src/api/luaapi.c` to handle API differences between Lua 5.1 (LuaJIT) and 5.3 (standard TIC-80).
- [ ] 2.3 Update `getLuaNumber` and other helper functions in `src/api/luaapi.c` to handle 5.1/5.3 type differences.
- [ ] 2.4 Update `src/api/lua.c` to conditionally export the LuaJIT runtime.

## 3. Core & Versioning

- [ ] 3.1 Update `src/script.c` to include the LuaJIT runtime in the static script list and remove other runtimes when `TIC_BUILD_WITH_LUAJIT` is defined.
- [ ] 3.2 Update `cmake/runtime_versions.cmake` to extract and display the LuaJIT version string in the TIC-80 system menu.

## 4. Verification

- [ ] 4.1 Build TIC-80 with `-DBUILD_WITH_LUAJIT=ON` and verify successful compilation on desktop.
- [ ] 4.2 Verify that the `SYSTEM` command in TIC-80 shows the LuaJIT version.
- [ ] 4.3 Verify that no other scripting languages (Fennel, JS, etc.) are available in the LuaJIT build.
- [ ] 4.4 Run a compatible Lua script and verify TIC-80 API calls work correctly.
- [ ] 4.5 Compare performance of `benchmark.lua` manually between Lua and LuaJIT builds by observing the "count" value for each test when the performance bar is stable at 60 FPS.
