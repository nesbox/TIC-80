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
#include "ticapi.h"
#include "defines.h"
#include "tools.h"
#include "ext/file_dialog.h"
#include "system.h"

#define TIC_LOCAL ".local/"
#define TIC_LOCAL_VERSION TIC_LOCAL TIC_VERSION_LABEL "/"
#define TIC_CACHE TIC_LOCAL "cache/"

#define TOOLBAR_SIZE 7
#define STUDIO_TEXT_WIDTH (TIC_FONT_WIDTH)
#define STUDIO_TEXT_HEIGHT (TIC_FONT_HEIGHT+1)
#define STUDIO_TEXT_BUFFER_WIDTH (TIC80_WIDTH / STUDIO_TEXT_WIDTH)
#define STUDIO_TEXT_BUFFER_HEIGHT (TIC80_HEIGHT / STUDIO_TEXT_HEIGHT)

#define TIC_COLOR_BG tic_color_0
#define DEFAULT_CHMOD 0755

#define CONFIG_TIC "config.tic"
#define CONFIG_TIC_PATH TIC_LOCAL_VERSION CONFIG_TIC

#define KEYMAP_COUNT (sizeof(tic80_gamepads) * BITS_IN_BYTE)
#define KEYMAP_SIZE (KEYMAP_COUNT)
#define KEYMAP_DAT "keymap.dat"
#define KEYMAP_DAT_PATH TIC_LOCAL_VERSION KEYMAP_DAT

#define CART_EXT ".tic"
#define PROJECT_LUA_EXT ".lua"
#define PROJECT_MOON_EXT ".moon"
#define PROJECT_JS_EXT ".js"
#define PROJECT_WREN_EXT ".wren"
#define PROJECT_SQUIRREL_EXT ".nut"
#define PROJECT_FENNEL_EXT ".fnl"

#define SHOW_TOOLTIP(FORMAT, ...)           \
{                                           \
    static const char Format[] = FORMAT;    \
    static char buf[sizeof Format];         \
    sprintf(buf, Format, __VA_ARGS__);      \
    showTooltip(buf);                       \
}

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
    TIC_DIALOG_MODE,
    TIC_MENU_MODE,
    TIC_SURF_MODE,
} EditorMode;

typedef struct
{
    s32 x, y;
} tic_point;

typedef struct
{
    s32 x, y, w, h;
} tic_rect;

void setCursor(tic_cursor id);

s32 getMouseX();
s32 getMouseY();
bool checkMousePos(const tic_rect* rect);
bool checkMouseClick(const tic_rect* rect, tic_mouse_btn button);
bool checkMouseDown(const tic_rect* rect, tic_mouse_btn button);

void drawToolbar(tic_mem* tic, bool bg);
void drawBitIcon(s32 x, s32 y, const u8* ptr, u8 color);
void drawBitIcon16(tic_mem* tic, s32 x, s32 y, const u16* ptr, u8 color);

void studioRomLoaded();
void studioRomSaved();
void studioConfigChanged();

void setStudioMode(EditorMode mode);
void resumeRunMode();
EditorMode getStudioMode();
void exitStudio();

u32 zip(u8* dest, size_t destSize, const u8* source, size_t size);
u32 unzip(u8* dest, size_t bufSize, const u8* source, size_t size);

void str2buf(const char* str, s32 size, void* buf, bool flip);
void toClipboard(const void* data, s32 size, bool flip);
bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces);

typedef enum
{
    TIC_CLIPBOARD_NONE,
    TIC_CLIPBOARD_CUT,
    TIC_CLIPBOARD_COPY,
    TIC_CLIPBOARD_PASTE,
} ClipboardEvent;

ClipboardEvent getClipboardEvent();

typedef enum
{
    TIC_TOOLBAR_CUT,
    TIC_TOOLBAR_COPY,
    TIC_TOOLBAR_PASTE,
    TIC_TOOLBAR_UNDO,
    TIC_TOOLBAR_REDO,
} StudioEvent;

void setStudioEvent(StudioEvent event);
void showTooltip(const char* text);

tic_key* getKeymap();

void setSpritePixel(tic_tile* tiles, s32 x, s32 y, u8 color);
u8 getSpritePixel(tic_tile* tiles, s32 x, s32 y);

typedef void(*DialogCallback)(bool yes, void* data);
void showDialog(const char** text, s32 rows, DialogCallback callback, void* data);
void hideDialog();

void hideGameMenu();

bool studioCartChanged();
void playSystemSfx(s32 id);

void runGameFromSurf();
void gotoCode();
void gotoSurf();
void exitFromGameMenu();
void runProject();
void drawBGAnimation(tic_mem* tic, s32 ticks);

tic_tiles* getBankTiles();
tic_palette* getBankPalette();
tic_flags* getBankFlags();
tic_map* getBankMap();

char getKeyboardText();
bool keyWasPressed(tic_key key);
bool anyKeyWasPressed();

const StudioConfig* getConfig();
System* getSystem();

const char* md5str(const void* data, s32 length);
bool hasProjectExt(const char* name);
void sfx_stop(tic_mem* tic, s32 channel);
const char* studioExportMusic(s32 track);
const char* studioExportSfx(s32 sfx);
