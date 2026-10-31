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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "tic.h"
#include "api.h"
#include "script.h"
#include "defines.h"
#include "tools.h"
#include "system.h"
#include "paths.h"
#include "rom.h"
#include "anim.h"
#include "keyboard.h"
#include "vimode.h"
#include "layout.h"
#include "clipboard.h"
#include "icons.h"
#include "ext/png.h"
#include "core/core.h"

#if defined(BUILD_RENDER_CACHE)
// Redirect studio drawing calls through the core API pointers to benefit from the draw cache
#define tic_api_cls(tic, ...)   (((tic_core*)(tic))->api.cls(tic, __VA_ARGS__))
#define tic_api_clip(tic, ...)  (((tic_core*)(tic))->api.clip(tic, __VA_ARGS__))
#define tic_api_print(tic, ...) (((tic_core*)(tic))->api.print(tic, __VA_ARGS__))
#define tic_api_font(tic, ...)  (((tic_core*)(tic))->api.font(tic, __VA_ARGS__))
#define tic_api_pix(tic, ...)   (((tic_core*)(tic))->api.pix(tic, __VA_ARGS__))
#define tic_api_rect(tic, ...)  (((tic_core*)(tic))->api.rect(tic, __VA_ARGS__))
#define tic_api_rectb(tic, ...) (((tic_core*)(tic))->api.rectb(tic, __VA_ARGS__))
#define tic_api_line(tic, ...)  (((tic_core*)(tic))->api.line(tic, __VA_ARGS__))
#define tic_api_circ(tic, ...)  (((tic_core*)(tic))->api.circ(tic, __VA_ARGS__))
#define tic_api_circb(tic, ...) (((tic_core*)(tic))->api.circb(tic, __VA_ARGS__))
#define tic_api_elli(tic, ...)  (((tic_core*)(tic))->api.elli(tic, __VA_ARGS__))
#define tic_api_ellib(tic, ...) (((tic_core*)(tic))->api.ellib(tic, __VA_ARGS__))
#define tic_api_paint(tic, ...) (((tic_core*)(tic))->api.paint(tic, __VA_ARGS__))
#define tic_api_tri(tic, ...)   (((tic_core*)(tic))->api.tri(tic, __VA_ARGS__))
#define tic_api_trib(tic, ...)  (((tic_core*)(tic))->api.trib(tic, __VA_ARGS__))
#define tic_api_ttri(tic, ...)  (((tic_core*)(tic))->api.ttri(tic, __VA_ARGS__))
#define tic_api_spr(tic, ...)   (((tic_core*)(tic))->api.spr(tic, __VA_ARGS__))
#define tic_api_map(tic, ...)   (((tic_core*)(tic))->api.map(tic, __VA_ARGS__))
#endif

#if defined(CRT_SHADER_SUPPORT)
// clang-format off
#   define CRT_CMD_PARAM(macro)                                 \
    macro(crt, bool, BOOLEAN, "", "enable CRT monitor effect")
// clang-format on
#else
#define CRT_CMD_PARAM(macro)
#endif

// clang-format off
#define CMD_PARAMS_LIST(macro)                                                              \
    macro(skip,         int,    BOOLEAN,    "",         "skip startup animation")           \
    macro(volume,       s32,    INTEGER,    "=<int>",   "global volume value [0-15]")       \
    macro(cli,          int,    BOOLEAN,    "",         "console only output")              \
    macro(fullscreen,   int,    BOOLEAN,    "",         "enable fullscreen mode")           \
    macro(vsync,        int,    BOOLEAN,    "",         "enable VSYNC")                     \
    macro(soft,         int,    BOOLEAN,    "",         "use software rendering")           \
    macro(fs,           char*,  STRING,     "=<str>",   "path to the file system folder")   \
    macro(scale,        s32,    INTEGER,    "=<int>",   "main window scale")                \
    macro(cmd,          char*,  STRING,     "=<str>",   "run commands in the console")      \
    macro(keepcmd,      int,    BOOLEAN,    "",         "re-execute commands on every run") \
    macro(version,      int,    BOOLEAN,    "",         "print program version")            \
    CRT_CMD_PARAM(macro)
// clang-format on

// clang-format off
#define SHOW_TOOLTIP(STUDIO, FORMAT, ...)        \
do{                                              \
    static const char Format[] = FORMAT;         \
    static char buf[STUDIO_TEXT_BUFFER_WIDTH];   \
    snprintf(buf, sizeof buf, Format, __VA_ARGS__); \
    showTooltip(STUDIO, buf);                    \
}while(0)
// clang-format on

typedef struct
{
    char* cart;
#define CMD_PARAMS_DEF(name, ctype, type, post, help) ctype name;
    CMD_PARAMS_LIST(CMD_PARAMS_DEF)
#undef CMD_PARAMS_DEF

#if defined(BUILD_EDITORS)
    const char* codeexport;
    const char* codeimport;
    s32 delay;
    s32 lowerlimit;
    s32 upperlimit;
    s32 battletime;

    int fft;
    int fftlist;
    int fftcaptureplaybackdevices;
    const char* fftdevice;
#endif
} StartArgs;

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
#if defined(BUILD_SURF)
    TIC_SURF_MODE,
#endif

    TIC_MODES_COUNT
} EditorMode;

// Where a build goes when there is nothing else to show, and where leaving a
// run or the browser goes: the console, the browser, or the menu.
#if defined(BUILD_EDITORS)
#define TIC_HOME_MODE TIC_CONSOLE_MODE
#elif defined(BUILD_SURF)
#define TIC_HOME_MODE TIC_SURF_MODE
#else
#define TIC_HOME_MODE TIC_MENU_MODE
#endif

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

void setCursor(Studio* studio, tic_cursor id);

bool checkMousePos(Studio* studio, const tic_rect* rect);
bool checkMouseClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button);
bool checkMouseDblClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button);
bool checkMouseDown(Studio* studio, const tic_rect* rect, tic_mouse_btn button);

void drawBitIcon(Studio* studio, s32 id, s32 x, s32 y, u8 color);

void studioRomLoaded(Studio* studio);
void studioRomSaved(Studio* studio);
void studioConfigChanged(Studio* studio);

EditorMode getStudioMode(Studio* studio);

// Where CODE and MAP content starts: 0 with autohide, 7 otherwise.
s32 getStudioEditorTop(Studio* studio);
void exitStudio(Studio* studio);

void setStudioViMode(Studio* studio, ViMode mode);
ViMode getStudioViMode(Studio* studio);
bool checkStudioViMode(Studio* studio, ViMode mode);

ClipboardEvent getClipboardEvent(Studio* studio);

void showTooltip(Studio* studio, const char* text);

typedef void (*ConfirmCallback)(Studio* studio, bool yes, void* data);
void confirmDialog(Studio* studio, const char** text, s32 rows, ConfirmCallback callback, void* data);
void confirmLoadCart(Studio* studio, ConfirmCallback callback, void* data);

bool studioCartChanged(Studio* studio);
void playSystemSfx(Studio* studio, s32 id);
bool studio_is_cart_loaded(Studio* studio);

struct tic_fs* studio_fs(Studio* studio);

CartName* studioCart(Studio* studio);
void studioSetCartName(Studio* studio, const char* name, const char* path);

bool studioLoadCart(Studio* studio, const char* path);
void studioReloadCart(Studio* studio);
void studioLoadByHash(Studio* studio, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data);
CartSaveResult studioSaveCart(Studio* studio, const char* name);
CartSaveResult studioAutoSave(Studio* studio);

void gotoMenu(Studio* studio);
void gotoCode(Studio* studio);
#if defined(BUILD_SURF)
void gotoSurf(Studio* studio);
void exitSurf(Studio* studio);
#endif

void runGame(Studio* studio, RunOrigin origin);
void exitGame(Studio* studio);
void resumeGame(Studio* studio);
bool studio_run_paused(Studio* studio);
void saveProject(Studio* studio);

bool studio_menu_over_player_run(Studio* studio);
void leaveRun(Studio* studio);

tic_tiles* getBankTiles(Studio* studio);
tic_palette* getBankPalette(Studio* studio, bool bank);
tic_flags* getBankFlags(Studio* studio);
tic_map* getBankMap(Studio* studio);

char getKeyboardText(Studio* studio);
bool keyWasPressed(Studio* studio, tic_key key);
bool keyWasPressedOnce(Studio* studio, tic_key key);
bool enterWasPressed(Studio* studio);
bool enterWasPressedOnce(Studio* studio);
bool ticEnterWasPressed(tic_mem* tic, s32 hold, s32 period);

typedef struct Config Config;
const StudioConfig* getConfig(Studio* studio);
Config* studio_config_get(Studio* studio);

const char* studioExportMusic(Studio* studio, s32 track, s32 bank, const char* filename);
const char* studioExportSfx(Studio* studio, s32 sfx, const char* filename);

tic_mem* getMemory(Studio* studio);

#if defined(BUILD_EDITORS)

// The code editor's view toggles, shared by the bar's menu and the editor's
// own hotkeys so both persist.
typedef enum
{
    CodeViewFont,
    CodeViewShadow,
    CodeViewLineNumbers,
    CodeViewSidebar,
    CodeViewStatusbar,
} CodeViewOption;

void studioCodeViewToggle(Studio*, CodeViewOption);

// The bar's frame: nothing typed reaches an editor while the bar owns the
// keys, and the Escape that closed it is not seen either.
bool studio_keys_captured(Studio* studio);
bool studio_keys_escaped(Studio* studio);

typedef struct
{
    char* exp;
    char* imp;

    struct
    {
        tic_code code;
        char postag[sizeof "-- pos: -2147483648,-2147483648\n"];
    } last;

    s32 delay;
    s32 ticks;

    struct
    {
        s32 lower;
        s32 upper;
        s32 current;
    } limit;

    struct
    {
        s32 started;
        s32 time;
        s32 left;

        bool hidetime;
    } battle;

} Bytebattle;

#endif
