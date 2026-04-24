## Context

TIC-80 currently uses Lua 5.3 as its primary scripting engine. While functional, performance can be a bottleneck for complex computations. LuaJIT is a highly optimized JIT compiler for Lua 5.1 that can significantly improve performance. The LuaJIT source is already present in `vendor/LuaJIT`, and a helper project `vendor/luajit-cmake` is available to facilitate its build via CMake.

## Goals / Non-Goals

**Goals:**
- Provide a `BUILD_WITH_LUAJIT` CMake option for desktop platforms (Windows, Linux, macOS).
- Integrate the LuaJIT build process by utilizing the `vendor/luajit-cmake` project.
- Ensure the full TIC-80 API is available to scripts running in LuaJIT.
- Disable all other scripting runtimes (Fennel, Moonscript, Yue, JS, etc.) when building with LuaJIT.

**Non-Goals:**
- **Lua 5.3 Feature Parity:** We will not implement or polyfill Lua 5.3 features (like bitwise operators `&`, `|` or integer division `//`) in LuaJIT.
- **Support for other runtimes:** No support for Fennel, Moonscript, Yue, JS, Wren, etc., when LuaJIT is enabled.
- **Non-Desktop Platforms:** Support for consoles (3DS, Switch), mobile (Android, iOS), or Web (WASM) is out of scope.

## Decisions

- **Build Method:** Use `add_subdirectory(vendor/luajit-cmake)` to build LuaJIT. This project handles the complex `minilua` and `buildvm` bootstrap process across all target desktop platforms.
- **API Mapping:** Update `src/api/lua.c` and `src/api/luaapi.c` to use conditional compilation (`#if defined(TIC_BUILD_WITH_LUAJIT)`).
- **Exclusion of Other Runtimes:** When `BUILD_WITH_LUAJIT` is ON, CMake will automatically set other script options (`BUILD_WITH_MOON`, `BUILD_WITH_JS`, etc.) to OFF.
- **Mutual Exclusion:** `BUILD_WITH_LUA` and `BUILD_WITH_LUAJIT` will be mutually exclusive.

## Risks / Trade-offs

- **[Risk] Build Incompatibility** → `luajit-cmake` might have issues with some toolchains.
  - *Mitigation*: The project is specifically designed for cross-platform CMake builds and supports MSVC, GCC, and Clang.
- **[Risk] Script Incompatibility** → Existing 5.3-based carts and carts using other languages will fail on LuaJIT builds.
  - *Mitigation*: This is an opt-in build flag for developers/benchmarking.
