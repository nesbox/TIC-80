# TIC-80 v1.2.0

TIC-80 v1.2.0 brings two years of fixes and improvements since v1.1 — a new
scripting language, a Nintendo Switch port, arm64 builds, and a long list of
bug fixes. In all, 215 pull requests from 64 contributors.

## What's new

- **YueScript** — a new scripting language.
- **Nintendo Switch** port.
- **FFT API** (`fft` / `ffts`) exposed.
- **`resume` / `reload`** console command.
- **arm64 builds** for Linux and macOS.

## Platform

- **Android** — external keyboard arrow keys, Ctrl shortcuts no longer insert
  stray characters, `.tic` / `.png` file association, updated SDL and Gradle.
- **Web (WASM)** — config saving, `btnp` / `fget` return types, Safari arrow
  keys in exports, template fixes.
- **macOS** — horizontal scroll direction, Apple Silicon and Intel build images.

## Bug fixes

- Ruby memory leak in tick/boot/bdr callbacks.
- Music editor hex input with Caps Lock.
- Mouse cursor mapping vs tab width.
- Squirrel out-of-bounds indexing; Janet and Scheme keyword arrays.
- Language-binding corrections across Fennel, MoonScript, and Python.

## Build & infrastructure

- Monolithic static **libretro** build.
- `msf_gif` inlined (drops ~700 MB of submodule).
- Modernized GitHub Actions (macOS 15 Intel, arm64 runners).

---

**Statistics since v1.1.2837:** 301 commits, 215 merged pull requests,
64 contributors.
