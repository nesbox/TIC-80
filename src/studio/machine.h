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

// Who owns ESC this frame. The machine cannot know what is open inside the code
// editor — a find box, a vi mode, a drag — and should not: the executor reports
// the fact, the machine decides what it means.
typedef enum
{
    SM_ESC_GATE_OPEN,       // nothing is holding ESC
    SM_ESC_GATE_VI,         // the code editor is in vi mode, out of normal
    SM_ESC_GATE_CODE,       // the code editor has a popup open
} SmEscapeGate;

// What the world looks like from outside the machine, gathered by the executor
// once per dispatch (studioEnv in studio.c) — the machine never reaches for it
// itself, which is what keeps it a pure function of its arguments.
typedef struct
{
    bool cart_loaded;       // studio_is_cart_loaded: the variant's own answer
    bool has_game_menu;     // the loaded cart declares a `menu:` tag
    bool menu_has_back;     // the MENU on screen has somewhere to go back to
    bool keepcmd_replay;    // a `run` with --keepcmd and commands still queued
    SmEscapeGate escape_gate;
} SmEnv;

// SM_MODE_NONE is "no mode": an empty mailbox, a dialog that has not been
// raised. It is not a value any transition may land in, and the tests sweep
// for that.
enum { SM_MODE_NONE = -1 };

typedef struct
{
    EditorMode mode;

    // "The last mode that is not START, CONSOLE or MENU" — history, not a
    // return target: it is what a menu with nothing to go back to falls back
    // to. Never holds one of those three.
    EditorMode prev_mode;

    // Where leaving a run lands when the studio asked for it (#2937), and
    // where CLOSE GAME lands when the player did (#3015). Set when a run
    // starts, preserved while a menu sits over it.
    EditorMode run_from;

    // The toolbar's click lands one frame later than the click: drawToolbar
    // writes the mode here and the next frame's SM_EV_FRAME picks it up.
    EditorMode pending_mode;

    // Where an answered confirm dialog goes back to. RUN is not a target but
    // an instruction: resume the run the dialog was raised over, because a
    // mode switch cannot un-pause the core.
    EditorMode dialog_return;

    // Whether the MENU on screen is a confirm dialog rather than a menu the
    // player navigates. A dialog raised in the menu names the menu as its
    // return target, a dialog raised over another dialog keeps the target of
    // the one underneath, and this is what tells those two apart.
    bool dialog;

    bool player_run;        // the run belongs to the player, not the studio
    bool menu_over_run;     // the menu on screen was opened over a run
    bool code_focus;        // F1/Alt+1 asked for the code editor, not the mode
} SmState;

typedef enum
{
    SM_EV_FRAME,            // top of the frame: pick up pending_mode
    SM_EV_REQUEST_MODE,     // setStudioMode(studio, mode)                  .mode
    SM_EV_REQUEST_CODE_FOCUS, // F1 / Alt+1: go to CODE unless already there
    SM_EV_CYCLE_EDITOR,     // Ctrl+PgUp/PgDn over the editor ring          .dir
    SM_EV_ESCAPE,           // the whole ESC ladder
    SM_EV_OPEN_MENU,        // gotoMenu: ESC in a run, the `menu` command
    SM_EV_OPEN_SURF,        // gotoSurf: the console's `surf`, the gamepad button
    SM_EV_LEAVE_RUN,        // leaveRun: a studio run steps back out
    SM_EV_CLOSE_GAME,       // exitGame: the CLOSE GAME row
    SM_EV_RESUME_GAME,      // resumeGame: un-pause, do not re-initialise
    SM_EV_RUN_GAME,         // runGame, which resets the core itself       .origin
    SM_EV_RUN_EXITED,       // the cart called exit()
    SM_EV_RUN_ERRORED,      // the cart's script raised
    SM_EV_CONFIRM_OPENED,   // confirmDialog
    SM_EV_CONFIRM_ANSWERED, // a dialog's yes/no, ESC included            .yes
    SM_EV_MENU_BACK,        // the main menu's back handler
    SM_EV_CART_LOADED,      // a cart became the loaded one: no transition,
    SM_EV_CART_LOAD_FAILED, // but the next env reads cart_loaded from it
    SM_EV_STARTUP,          // --skip and the bytebattle start             .mode

    SM_EV_COUNT
} SmEventKind;

typedef struct
{
    SmEventKind kind;

    union
    {
        EditorMode mode;    // REQUEST_MODE, STARTUP (SM_MODE_NONE = nothing)
        RunOrigin  origin;  // RUN_GAME
        s32        dir;     // CYCLE_EDITOR: -1 or +1
        bool       yes;     // CONFIRM_ANSWERED
    };
} SmEvent;

// What the studio has to do about a transition. An ordered list rather than a
// bitmask: two of these are order-sensitive (the core is paused before it is
// reset, surf is initialised before the switch's own effects) and one has to
// arrive last (the dialog's callback runs once the screen it answers to is
// back, and that rebuild replaces what the dialog was raised with).
typedef enum
{
    SM_EFF_PAUSE_CORE,       // tic_core_pause: leaving RUN snapshots the core
    SM_EFF_RESET_CORE,       // tic_api_reset
    SM_EFF_RESUME_CORE,      // tic_core_resume: the only way out of a pause
    SM_EFF_INIT_RUN,         // initRunMode
    SM_EFF_INIT_SURF,        // initSurfMode, before the switch to SURF
    SM_EFF_SURF_RESUME,      // surf->resume: back to a browser that is already built
    SM_EFF_INIT_WORLD,       // initWorldMap
    SM_EFF_REBUILD_MAINMENU, // free + studio_mainmenu_init
    SM_EFF_CONSOLE_DONE,     // console->done, only when CONSOLE follows SURF
    SM_EFF_MUSIC_NEXT_TAB,   // MUSIC asked for while already in MUSIC
    SM_EFF_VI_MODE_RESET,    // studio->viMode = 0, emitted even when nothing moved
    SM_EFF_EDITOR_ESCAPE,    // code->escape: the editor's own popup consumes ESC
    SM_EFF_MENU_BACK_ANIM,   // a menu that has a back: play it
    SM_EFF_DIALOG_CALLBACK,  // call the stored confirm callback with `yes`
    SM_EFF_EXIT_STUDIO,      // studio_exit: an editorless run that ended itself

    SM_EFF_COUNT
} SmEffect;

enum { SM_EFFECTS_MAX = 8 };

typedef struct
{
    u8 count;
    SmEffect items[SM_EFFECTS_MAX];
} SmEffectList;

typedef struct
{
    EditorMode from;        // for the trace and for test failure messages
    EditorMode to;
    bool moved;             // from != to: the mode on screen actually changed
    bool yes;               // CONFIRM_ANSWERED: what the callback is told
    SmEffectList effects;
} SmResult;

// The per-frame policy of a mode — the four switches over studio->mode that
// used to live in studio_tick, as data. Pure, and every field is asserted by
// the tests for all eleven modes.
typedef enum
{
    SM_SOUND_RAM,           // the running cart's own sfx/music
    SM_SOUND_CONFIG,        // the studio's config cart
    SM_SOUND_BANKS,         // the editor banks (editors builds only)
} SmSoundSource;

typedef enum
{
    SM_INPUT_KEEP,          // RUN: the cart owns the input
    SM_INPUT_CLEAR,         // menu and surf: clear the keys only
    SM_INPUT_CLEAR_PAD,     // the rest: clear the keys and the gamepad
} SmInputPolicy;

typedef struct
{
    SmSoundSource sound;
    SmInputPolicy input;
    bool clear_vbank1;      // the studio draws into vbank 1: wipe it first
    bool config_palette;    // the config cart's palette and the system font
} SmModePolicy;

void sm_init(SmState* state, SmFeatures features);
SmResult sm_dispatch(SmState* state, const SmEnv* env, const SmEvent* event);
SmModePolicy sm_mode_policy(EditorMode mode);

// Effect names, for the transition trace and for test failures.
const char* sm_effect_name(SmEffect effect);

// The editor tabs, in the order the toolbar draws them and Ctrl+PgUp/PgDn walks
// them. The count is a compile-time constant so the toolbar's layout arithmetic
// stays constant expressions; machine.c pins it to the ring with a
// static_assert, so the two cannot drift.
enum { SM_EDITOR_RING_COUNT = 5 };
const EditorMode* sm_editor_ring(void);

// A mode's name for the transition trace (TIC80_TRACE_MODES) and for test
// failure messages. "?" is not a mode: it is what an out-of-range value gets.
const char* sm_mode_name(EditorMode mode);
