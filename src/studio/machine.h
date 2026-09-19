// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "defines.h"
#include "tic80_types.h"

// The studio's modes, and the rules that move between them, live in
// machine.c — the one place that answers "which screen is on". The transition
// function there is pure: given a state and a fact about the world it returns
// the effects the studio has to run, and studio.c runs them. Nothing here may
// include studio.h, SDL or the core: the module is unit-tested on its own
// (tests/studio_machine.c), which is the point of the split.

typedef enum
{
    TIC_START_MODE,
    TIC_CONSOLE_MODE,
    TIC_RUN_MODE,
    TIC_CODE_MODE,
    TIC_SPRITE_MODE,
    TIC_MAP_MODE,
    TIC_WORLD_MODE,
    TIC_SFX_MODE,
    TIC_MUSIC_MODE,
    TIC_MENU_MODE,
    TIC_SURF_MODE,

    TIC_MODES_COUNT
} EditorMode;

// Who asked for the run decides what ESC does in it (#2937): the studio's own
// runs — Ctrl+R, the console's `run` — step back out to the editor, a cart
// opened to play — a file argument, a dropped file, SURF, the web player —
// gets the pause menu. What the cart declares (a `menu:` tag) is content, not
// a role: it adds the game's own items to that menu, nothing more.
typedef enum
{
    RUN_FROM_STUDIO,
    RUN_FROM_PLAYER,
} RunOrigin;

// What the build carries. These macros are the only place a build flag is read
// for the mode machinery; everything downstream takes the value
// (sm_build_features), so the rules themselves are data a test can vary.
#if defined(BUILD_EDITORS)
#define SM_HAS_EDITORS 1
#define SM_HAS_SURF    1
#elif defined(BUILD_SURF)
#define SM_HAS_EDITORS 0
#define SM_HAS_SURF    1
#else
#define SM_HAS_EDITORS 0
#define SM_HAS_SURF    0
#endif

typedef struct
{
    bool has_editors;   // BUILD_EDITORS: the console and the editors exist
    bool has_surf;      // BUILD_SURF: the browser is compiled in
} SmFeatures;

SmFeatures sm_build_features(void);

// The editor tabs, in the order the toolbar draws them and Ctrl+PgUp/PgDn walks
// them. The count is a compile-time constant so the toolbar's layout arithmetic
// stays constant expressions; machine.c pins it to the ring with a
// static_assert, so the two cannot drift.
enum { SM_EDITOR_RING_COUNT = 5 };
const EditorMode* sm_editor_ring(void);

// A mode's name for the transition trace (TIC80_TRACE_MODES) and for test
// failure messages. "?" is not a mode: it is what an out-of-range value gets.
const char* sm_mode_name(EditorMode mode);
