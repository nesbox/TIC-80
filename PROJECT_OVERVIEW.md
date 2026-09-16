# TIC-80 — Project Overview (Agent Reference)

> Personal reference doc: read this before doing any deep-dive into the repo so you can skip
> broad recon. It was compiled from a full first-pass study of the codebase (see "Sources" at
> the end). Paths are relative to the repo root. Line numbers drift — prefer symbol names.

## 1. TL;DR

TIC-80 is an MIT-licensed **fantasy console** (made by Vadim Grigoruk @nesbox, [tic80.com](https://tic80.com)):
a self-contained environment for making/playing/sharing tiny retro games with fixed limits —
240×136 @ 60 fps, 16-color palette, 256 8×8 sprites, 4-channel audio, 96 KB addressable RAM —
plus built-in editors (code/sprites/map/world/sfx/music), a typed console shell, and a portable
cartridge format (`.tic` binary, PNG-embedded, or PRO text format).

Architecture is layered:

```
include/ (public C API of the runtime lib)
  └─ src/ "core" (tic_core: memory model + all game APIs + frame loop)
       └─ src/api/*.c (per-language bindings; 12 languages)
            └─ src/studio/ (the IDE: console, editors, screens, fs/net/config)
                 └─ src/system/ (platform frontends: sdl, libretro, n3ds, nswitch, baremetalpi)
```

## 2. Repository & workspace state (as of the study)

- Branch `main`, clean, synced with `origin/main` = https://github.com/nesbox/TIC-80.git.
  HEAD `4aba09c9` (2026-07-06). Extra remotes: `MikanHako1024`, `borbware` (Steamworks forks).
- Many local/remote branches: experiments (`sokol`, `clay`, `luajit`, `stack-overflow-fix`,
  `main-sokol-sync`, ...), lots of `origin/pr/*` refs, `stable`.
- 23 submodules in `vendor/` (embedded VMs + libs), all initialized.
- **`vendor/pocketpy` is checked out but dirty** (` M vendor/pocketpy` in git status): the submodule
  worktree has an untracked `3rd/libhv/` dir and headers report PocketPy 2.1.7 while the recorded
  commit is v2.1.6-39-g979addec. Cosmetic; don't "fix" by cleaning blindly.
- **`build/` is partially git-tracked** (112 files: platform scaffold — `assets/*.tic.dat`,
  `cart.png`, `macosx/tic80.plist.in` + icns, `html/prejs.js`, `tools/*`, `android/**`, n3ds/switch
  icons, `janet/janetconf.h`, ...). It doubles as CMake binary dir and source of packaging assets.
  `cmake/install.cmake` and `cmake/studio.cmake` require tracked scaffold files to exist at
  configure time.
- **Clean-rebuild procedure (learned the hard way): do NOT `rm -rf build`** — that deletes tracked
  scaffold and configure fails (`install.cmake: configure_file macosx/tic80.plist.in`). Instead:
  `git restore -- build && git clean -fdx build` (removes only untracked/ignored generated files),
  then configure.
- Current (fresh, verified) configure from `build/CMakeCache.txt`: **Ninja / `Release`** /
  `BUILD_PRO=ON` / `BUILD_SDL=ON` / `BUILD_SDLGPU=OFF` / `BUILD_EDITORS=ON` / `BUILD_STATIC=OFF` /
  **Lua only** (`BUILD_WITH_LUA=ON`, all other `BUILD_WITH_*` off), macOS arm64, clang.
- Fresh build at HEAD `4aba09c9` (Jul 2026): `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DBUILD_PRO=ON -DBUILD_WITH_LUA=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build`
  → 595/595 targets, artifacts in `build/bin/`: `tic80` (arm64 Mach-O), `lua.dylib`, SDL2 dylibs.
  Smoke: `build/bin/tic80 --version` → `1.2.3083-dev Pro (4aba09c)` (exits cleanly, no window).
  Rebuild anytime with `cmake --build build --parallel`, or use `./debug-macos.sh` for flag-driven
  fresh runs (`-p` pro, `-a` asan, `-s` static, `-d` debug, `-f` fresh, ...).

## 3. Source layout (what lives where)

| Area | Path | Contents |
|---|---|---|
| Public API headers | `include/` | `tic80.h` (5-fn host API), `tic80_types.h` (u8..u64/s8..s64), `tic80_config.h` (platform defs, `TIC_MODULE_EXT` = .dylib/.so/.dll) |
| Core glue | `src/tic.c`, `src/tic.h`, `src/cart.{c,h}`, `src/tools.{c,h}`, `src/zip.c`, `src/tilesheet.c`, `src/script.{c,h}`, `src/api.h`, `src/defines.h` | runtime entry, memory/cart structs, cart (de)serialization, helpers, script registry, API macro table |
| Core machine | `src/core/` | `core.c` (lifecycle, sync/vbank/reset/blit), `draw.c` (all drawing APIs), `draw_dep.c` (deprecated `textri`, see gotchas), `io.c` (btn/key/mouse), `sound.c` (sfx/music synth), `font.inl`/`altfont.inl` fonts |
| Ext libs | `src/ext/` | miniaudio.h (95k lines), fft.c/kiss_fft (spectrum), png.c/gif.c/msf_gif.h, json.c, md5.c, history.c (undo snapshots) |
| Bindings | `src/api/` | lua.c, moonscript.c, fennel.c, yue.cpp, wren.c, mruby.c, js.c, python.c, scheme.c, squirrel.c, janet.c, wasm.c, luaapi.c, parse_note.c |
| Studio | `src/studio/` | studio.c/h (app), screens/, editors/, fs.c, net.c, project.c, config.c, system.h, anim.h |
| Frontends | `src/system/` | sdl/ (main.c + player.c), libretro/, n3ds/, nswitch/, baremetalpi/ |
| Build | `CMakeLists.txt`, `cmake/*.cmake`, `version.h.in` | per-component cmake; runtime version pins in `cmake/runtime_versions.cmake` |
| Other | `demos/` (per-language demo carts), `templates/{c,d,nim,rust,zig}` (WASM starters), `assets.bat`, `config.js` (see §12), `tools/zig_sync`, `.github/workflows/{build,webapp}.yml`, `debug-{macos.sh,linux.sh,windows.ps1}` | CI, tooling |

## 4. Machine / memory model (`src/tic.h`)

- `tic_ram` — 96 KB single union over `u8 data[TIC_RAM_SIZE]` containing: `vram` (2×16 KB banks
  = screen + palette + 4-bpp mapping + vars), `tiles`/`sprites`, `map` (240×136 cells), input,
  sfx (64 samples × 30 ticks, 16 waveforms), music (60 patterns/tracks), sound registers,
  `persistent` (256 × 32-bit pmem slots), flags, font, key mapping, pcm. Everything is `peek/poke`-able.
- Two **video banks** (`vbank(0/1)`), each 16 KB: screen 240×136 at 4 bpp, palette, blit segment,
  screen-xy offset vars, border/clear colors, cursor. `tic_core_blit_ex` composites them: a vbank-1
  pixel equal to vbank-1's "clear" color shows through to the vbank-0 pixel (chroma-key overlay —
  **this is how the studio UI overlays the running game**).
- `tic_cartridge` = `tic_bank banks[TIC_BANKS=8]` (each: tiles/sprites/map/sfx/music/flags/
  palette×2/screen) + `tic_code` (512 KB = 8×64 KB banks) + `tic_binary` (4×64 KB, for WASM carts)
  + `u8 lang`. Free (non-PRO) builds use only bank 0.
- Screen/audio/timing constants: 240×136 (full frame 256×144 with 8px borders), 60 fps, 44.1 kHz
  stereo s16, 4 channels; sound synthesized on a `CLOCKRATE = 255<<13` clock via **blip-buf**
  delta resampling (`core/core.c`, `vendor/blip-buf`).

## 5. Cartridge formats (`src/cart.c`, `src/studio/project.c`)

- **Binary**: sequence of 4-byte `Chunk {type:5, bank:3, size:16, temp:8}` headers + payload.
  ~21 chunk types (TILES=1, SPRITES=2, MAP=4, CODE=5, FLAGS=6, SAMPLES=9, WAVEFORM=10,
  PALETTE=12, MUSIC=14, PATTERNS=15, CODE_ZIP=16, DEFAULT=17, SCREEN=18, BINARY=19, LANG=20, ...).
  `DEFAULT` chunk = Sweetie16 palette + default waveforms shortcut. `tic_cart_save` writes one or
  more code chunks if code > 64 KB; deprecated chunks (cover GIF, old patterns) are read for
  ancient-cart compat under `BUILD_DEPRECATED`.
- **PNG**: a `.png` whose payload is a zlib-zipped raw chunked cart (`getRawCartFromPng`).
  `.png` saves also embed a 256×256 cover image generated from meta-tags + screen (console.c).
- **PRO text format** (`project.c`, only with `BUILD_PRO`): human-readable cart files per
  language (`// title/author/desc`, `-- script:` etc.), then hex sections `-- <TILES>`,
  `-- <SPRITES>`, `<MAP>`, `<WAVES>`, `<SFX>`, `<PATTERNS>`, `<TRACKS>`, `<FLAGS>`, `<SCREEN>`,
  `<PALETTE>` with per-bank suffixed tags (`TAG0..3`) in PRO. Code is plain text until the first
  `-- <` section. Detection by language `fileExtension`/`projectComment`.

## 6. Game API system (`src/api.h`)

- Single master macro `TIC_API_LIST(macro)` declares **49 game functions** (name, doc, arity,
  required-args, return type, full C signature):
  graphics `print cls pix line rect rectb circ circb elli ellib tri trib ttri paint spr map mget
  mset clip vbank font`; input `btn btnp key keyp mouse`; memory `peek poke peek1 poke1 peek2
  poke2 peek4 poke4 memcpy memset`; audio `sfx music fft ffts`; persistence/data `pmem sync fget
  fset`; misc `time tstamp trace exit reset` (deprecated `textri` extra under `BUILD_DEPRECATED`).
- It drives: `tic_api_*` prototypes; the runtime `core->api` function-pointer table (filled in
  `tic_core_create`); the docs; per-language metadata (see §9).
- Implementations: core.c (peek/poke family, memcpy/memset, trace, pmem, exit, time, tstamp,
  sync, vbank, reset, clip, blit), core/draw.c (all shape/sprite/map/print/font APIs), core/io.c
  (btn/btnp/key/keyp/mouse + hold tracking), core/sound.c (sfx/music), ext/fft.c (fft/ffts,
  spectrum of live audio captured via miniaudio).

## 7. Core loop & lifecycle (symbols in `src/core/core.c`, `src/tic.c`)

- `tic_core_create(samplerate, format)` → mallocs `tic_core` (its `memory` is the `tic_mem`),
  allocates `ram` (96 KB) + `base_ram`, host screen buffer (256×144 u32 RGBA), samples buffer,
  blip-buf resamplers, fills the api table, calls `tic_api_reset`.
- `tic80_tick` = `tic_core_tick_start` (sound tick + snap system keyboard/gamepad state) →
  `tic_core_tick` (first tick lazily: pick script by `cart.lang`, `cart2ram` copies cart bank 0 →
  RAM via `tic_api_sync(all, bank=0, toCart=false)`, then language `init → BOOT → TIC`) →
  `tic_core_tick_end` (edge/hold bookkeeping) → `tic_core_blit` (composite 2 vbanks to RGBA with
  per-scanline `SCN`/`BDR` callback hooks).
- Security notes in code: cartridge code cannot corrupt keyboard/gamepad input state (`tick_start`
  saves it, `tick_end` restores previous from the trusted copy) — see issue #1785 comments.
- `tic_api_sync(mask,bank,toCart)` copies section subsets RAM↔cart banks (offsetof pairs from
  `TIC_SYNC_LIST` in api.h: tiles/sprites/map/sfx/music/palette/flags/screen; palette special-cases
  both vbanks). `core->state.synced` allows sync once per frame (callable once).
- `tic_core_pause/resume` snapshot the **entire** `tic_ram` + core state (used by the in-game
  menu → RESUME). `tic_api_reset` re-inits vbanks, fonts (font.inl), sound, saveid; used on
  mode switches and cart loads.
- Host callbacks in `tic_tick_data`: error/trace/exit/counter/freq (time() = counter-freq delta).

## 8. Scripting languages (`src/api/*`, `src/script.{h,c}`, `cmake/*.cmake`)

| Lang | File | VM / note |
|---|---|---|
| Lua | lua.c + luaapi.c | Lua 5.3.6 (vendored) |
| Moonscript | moonscript.c | compiles → Lua, reuses luaapi |
| Fennel | fennel.c | compiles → Lua |
| Yuescript | yue.cpp | → Lua, id 21 |
| Ruby | mruby.c | mruby (id 11) |
| JS | js.c | QuickJS (id 12) |
| Python | python.c | PocketPy (id 20) |
| Scheme | scheme.c | s7 (id 19) |
| Squirrel | squirrel.c | id 15 |
| Janet | janet.c | id 18 |
| WASM | wasm.c | wasm3, `.wasmp` carts, uses `cart.binary` section |
| Wren | wren.c | id via signature dispatch on embedded `TIC` class |

- Each file ends `TIC_EXPORT const tic_script EXPORT_SCRIPT(Lang) = {...}`. `EXPORT_SCRIPT`
  prefixes `Lang` only under `TIC_RUNTIME_STATIC`; otherwise the exported symbol is plain
  `ScriptConfig` for `dlopen`/`dlsym` (`.dylib`/`.so`/`.dll` per `TIC_MODULE_EXT`).
- `tic_script` struct (script.h): id/name/fileExtension/projectComment, `init/close/tick/boot/
  callback(scanline,border,menu)`, `getOutline` (code outline), `eval`, comment/string syntax for
  the code editor, keywords, `api_keywords`, embedded `demo/mark/demos` carts.
- Registry: `Scripts[16]` in script.c, sorted by id; `tic_add_script` (dlopen'd modules register
  here), `tic_get_script` matches `cart.lang` byte **or** `-- script:` meta-tag, falls back to the
  first registered script. Max 16 languages.
- Dynamic loading: non-static builds produce one shared lib per enabled language (e.g. the
  workspace has `lua.dylib`); studio dlopens every `*.<ext>` module next to the binary at startup
  (studio.c) and `tic80_load` also tries `<script-tag><TIC_MODULE_EXT>` for unknown carts.
- Demos: `demos/<lang>demo.*`, `demos/bunny/<lang>mark.*`; each embedded in the binary as
  zipped `.tic.dat` arrays regenerated by `assets.bat` via `prj2cart`+`bin2txt` tools.

## 9. Studio / IDE (`src/studio/`)

- Modes (`EditorMode` in studio.h): START → CONSOLE → CODE / SPRITE / MAP / WORLD / SFX / MUSIC
  editors → RUN (game) → MENU, plus SURF (cart browser). `Studio` struct is opaque (studio.c).
- Flow: `studio_create` (parse args w/ argparse, dlopen language modules, create core `tic_mem`,
  fs, all screens/editors, config; `--skip/--cli` jumps straight to CONSOLE) →
  per-frame `studio_tick(studio, input)` (render current mode, process animations/mouse edge
  states/shortcuts, `tic_core_blit_ex`) + `studio_sound` from the audio callback.
- `setStudioMode` is the single transition point (pause core when leaving RUN, `tic_api_reset`
  otherwise, per-mode init hooks). Hotkeys in `processShortcuts`: ctrl+R/ctrl+Enter run,
  ctrl+S save, F1..F5/alt+1..5 editor modes, alt+` console, esc back, F7 cover, F8 screenshot
  (a 1-frame GIF!), F9 gif record, F10/F12 byte-battle, ctrl+0..3 PRO bank switch, alt+Enter/F11
  fullscreen, F6 CRT.
- Runtime hosting: studio owns ONE `tic_mem`. **Editors mutate `tic->cart` in place** (code
  editor src == `cart.code.data`; others get `&cart.banks[i].{tiles,map,sfx,music}`); RAM is the
  runtime scratch, mirrored around edits (`tiles2ram/map2ram`, then `ram2map` after `tic_api_mset`).
  Cart content reaches game RAM only at run/reset (`cart2ram`) or explicit game `sync()`.
- Screens (`screens/`): `console.c` (4.6k lines — the typed command shell: load/save/run/edit/
  new/demo/export/import/dir/folder/help/config/surf/menu + tab completion + API/RAM help tables;
  text buffer 30×17 chars ×64), `console_minimal.c` (headless stub when SURF w/o editors),
  `start.c` (splash + **embedded-cart scan**: searches own executable for `TIC.CART` signature),
  `run.c` (per-frame `tic_core_tick` + pmem persistence to `.local/<md5|saveid>`),
  `menu.c` (one reusable animated Menu widget — used for main menu, options AND Yes/No confirm
  dialogs), `mainmenu.c` (menu contents incl. `-- menu:` custom game menus, options, gamepad
  key-assignment), `surf.c` (web cart browser; SURF builds only).
- Editors (`editors/`): `code.c` (full editor: syntax highlighting per lang, vi/emacs/standard
  keybinding modes, find/goto/bookmark/outline/replace/drag submodes, clipboard), `sprite.c`
  (pixel editor, palette+flags editors, tilesheet ops), `map.c` (tilemap; uses `tic_api_mset`),
  `world.c` (only ~159 lines — a mini-map launcher into the map editor), `sfx.c`, `music.c`
  (tracker + piano-roll tabs). Undo = region snapshot history (`src/ext/history.c`).
- UI approach: **hand-rolled immediate-mode UI drawn into vbank-1** of the emulated framebuffer,
  composited over the game by `tic_core_blit_ex`; manual `tic_rect` hit-testing; 6×8 font from the
  config-cart sprite sheet. **No external GUI toolkit**; the `clay` UI experiment lives only on
  the `clay` branch (not merged).
- `fs.c` virtual filesystem (cwd + roots, dirs/files, save/load, hash-based net loads for public
  dir, mtime checks; emscripten syncfs). `net.c` `tic_net` async GET with 3 backends: Emscripten
  fetch / naett (desktop, `USE_NAETT`) / stub. Host = tic80.com.
- `config.c`: user config stored as an **editable cart** `config.tic` (theme/UI settings;
  defaults embedded from `config.tic.dat`) + `options.json` (StudioOptions incl. gamepad mapping).
- PRO extras gated by `TIC80_PRO`: 4 cart banks end-to-end, project.c text carts, more exports.

## 10. System frontends (`src/system/`)

- `sdl/main.c` (2.2k lines, one file for **desktop SDL2 AND Emscripten**): window, 60 fps pacing,
  `gpuTick` → pollEvents (keyboard/gamepad/mouse, touch overlays when `BUILD_TOUCH_INPUT`) →
  `studio_tick`, render texture (SDL_Renderer, or sdl-gpu CRT path when `BUILD_SDLGPU` →
  `CRT_SHADER_SUPPORT`); audio via **SDL audio callback → `studio_sound` → `tic_core_synth_sound`**
  (miniaudio is NOT the output path; it is used by `src/ext/fft.c` only). Exposes `tic_sys_*`
  platform hooks (clipboard, fullscreen, message, open_url/path, preseed, default_mapping...).
  `player.c` = editor-less playback binary.
- `libretro/` (`tic80_libretro.c` + libretro-common): retro_run → update/draw/audio batches.
  `cmake/libretro.cmake`: optional monolithic static build that merges core + all languages.
- `n3ds/` ctrulib/Citro3D; `nswitch/` libnx (nxlink/romfs) + SDL; `baremetalpi/` Circle C++
  kernel. Each drives the same studio/core API.

## 11. Build system, CI, tools

- Root `CMakeLists.txt` + per-component `cmake/*.cmake`; `cmake/runtime_versions.cmake` pins
  vendored runtime versions; `cmake/version.cmake` → `build/version.h`.
- Options: `BUILD_WITH_ALL` / `BUILD_WITH_<LANG>`, `BUILD_SDL`, `BUILD_SDLGPU`, `BUILD_EDITORS`,
  `BUILD_SURF` (auto-ON with editors; standalone web-player builds use it without editors),
  `BUILD_PRO`, `BUILD_PLAYER`, `BUILD_STATIC` (→ `TIC_RUNTIME_STATIC`, languages linked in),
  `BUILD_LIBRETRO`, `BUILD_TOOLS` (bin2txt, prj2cart CLIs), `BUILD_TOUCH_INPUT`, `BUILD_NO_OPTIMIZATION`,
  `BUILD_ASAN_DEBUG`, `BUILD_WITH_ZLIB`, `PREFER_SYSTEM_LIBRARIES`, `TIC80_TARGET` suffix.
- Targets: `runtime` (INTERFACE; carries `BUILD_DEPRECATED` define), `tic80core` (STATIC core;
  links language libs only when BUILD_STATIC), per-language modules/libs, `tic80studio` (STATIC;
  console/editors/surf/project files selected by flags; links naett if `USE_NAETT`), `tic80` exe.
- `cmake/studio.cmake` compiles `build/assets/cart.png.dat` into the studio (default cart icon);
  `assets.bat` regenerates embedded `.dat` assets from `config.js`/PNG via the tools.
- CI: `.github/workflows/build.yml` (matrix: windows(+mingw), linux gcc12/14, arm64, rpi, 3ds,
  switch, macos-arm64, android, html) all with SDLGPU=ON + BUILD_WITH_ALL=ON (+Pro); `webapp.yml`
  publishes web build to the `webapp` branch. Emscripten link flags live in `cmake/sdl.cmake`.
- Dev scripts: `debug-macos.sh` / `debug-linux.sh` / `debug-windows.ps1` — cmake wrappers with
  flags: fresh/pro/asan/debug/static/macports/homebrew/warnings etc. (`./debug-macos.sh --help`).
- `tools/zig_sync`: copies `templates/zig/{src/tic80.zig,build.zig}` into each demos zig project.
- `templates/{c,d,nim,rust,zig}`: starter projects/libs for `script: wasm` carts.

## 12. Misc / oddities worth remembering

- Root `config.js` has **no runtime consumer** — it is the text "project" form of the default
  config cartridge (JS config + `// <TILES>/<SPRITES>/<MAP>/<WAVES>/<SFX>/<PALETTE>` sections);
  `assets.bat` converts it into embedded `build/assets/config.tic.dat` used by `studio/config.c`.
- `tic_core_blit_ex` composite: vbank-1 "clear color" pixels show vbank-0 through; two video
  banks give the studio its overlay UI; `vbank` API swaps whole 16 KB banks (pointer swap, no copy).
- Deprecated `textri()` exists under `BUILD_DEPRECATED` (define propagated via the `runtime`
  INTERFACE target). `src/core/draw_dep.c` implements it; in the current shared desktop build the
  define is NOT set for core files (only language modules compile with it), so `draw_dep.c` may
  not be compiled at all — don't rely on textri for new work; use `ttri`.
- Screenshot (F8) is a 1-frame GIF (msf_gif), not PNG. Gif recording is 33 fps.
- Console `config` command loads `config.tic` as an editable cartridge; "the IDE's config is a cart".
- Cart change detection = MD5 of cart + file mtime; external edits prompt reload mid-session.
- Byte-battle (`--codeexport/--codeimport [--delay --lowerlimit --upperlimit --battletime]`) =
  pair-programming between two processes by swapping code through files.
- Memory addresses: RAM 96 KB total; `0x00000`-ish VRAM/screen; tiles/sprites/map follow;
  map writes at 0x08000+ (240-byte rows) per the map() docs; persistent pmem is 1 KB of u32 slots.
- Free build = 1 cart bank + no text projects; PRO = 8 banks (`TIC_BANKS=8`), text format, exports.

## 13. Task → file quick matrix

| Task | Start here |
|---|---|
| Add/change a game API | `src/api.h` (`TIC_API_LIST`), impl in `src/core/*.c`; per-lang glue if needed |
| Runtime/game loop bugs | `src/core/core.c`, `core/io.c`, `core/sound.c`, `core/draw.c` |
| Memory map / bank logic | `src/tic.h` (structs), `core/core.c` (`tic_api_sync`, `cart2ram`, `vbank`) |
| Cart file format/compat | `src/cart.c`, `src/studio/project.c` |
| New language binding | copy a simple binding (`src/api/*.c`), register in `script.c` + `cmake/*.cmake` |
| IDE/editor features | `src/studio/studio.c` (modes/shortcuts), `screens/console.c`, `editors/*.c` |
| Input/gamepad/keyboard | `src/system/sdl/main.c` + `core/io.c` (`tic_core_tick_io`) |
| Audio/music | `core/sound.c`, `blip-buf`, `src/ext/fft.c` (capture) |
| Web/Emscripten build | `cmake/sdl.cmake`, `.github/workflows/{build,webapp}.yml` |
| Build flags / packaging | root `CMakeLists.txt`, `cmake/*.cmake`, `debug-*.sh` |

## Sources

Compiled from a direct study of this checkout (branch `main` @ `4aba09c9`): README.md, all key
headers under `include/` + `src/` (`tic.h`, `api.h`, `tic.c`, `cart.c`, `script.{h,c}`,
`core/*.{c,h}`, `tools.h`, `studio.h`, `system.h`), `CMakeLists.txt`, `cmake/*.cmake`,
`.gitmodules`, `.github/workflows/`, `debug-*.sh`, `config.js`, plus two deep-dive research passes
covering `src/studio/**` and the languages/platforms/CI matrix.
