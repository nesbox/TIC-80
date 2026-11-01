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

#include "tic.h"

#include <stdbool.h>

// The export vocabulary: the console's dispatch, tab completion and usage, the
// bar's items and the command builder all read this one list. The label is
// only for the menu.
// clang-format off
#define EXPORT_GAME_LIST(macro)     \
    macro(win, "WIN")               \
    macro(linux, "LINUX")           \
    macro(linuxarm, "LINUX ARM")    \
    macro(mac, "MACOS")             \
    macro(macintel, "MAC INTEL")    \
    macro(html, "HTML")
// clang-format on

// clang-format off
#define EXPORT_ASSET_LIST(macro)    \
    macro(tiles, "TILES")           \
    macro(sprites, "SPRITES")       \
    macro(mapimg, "MAP")            \
    macro(screen, "SCREEN")         \
    macro(sfx, "SFX")               \
    macro(music, "MUSIC")           \
    macro(help, "HELP")
// clang-format on

// clang-format off
#define EXPORT_CMD_LIST(macro)  \
    EXPORT_GAME_LIST(macro)     \
    macro(binary, "BINARY")     \
    macro(map, "MAP DATA")      \
    EXPORT_ASSET_LIST(macro)
// clang-format on

// clang-format off
enum
{
#define EXPORT_ENUM(name, label) Export_##name,
    EXPORT_CMD_LIST(EXPORT_ENUM)
#undef EXPORT_ENUM
    Export_Count,
};
// clang-format on

// What the builder reads: the cart's name and the editor values the bar
// gathers. The banks and the ALONE toggle exist only in a PRO build.
typedef struct
{
    const char* cartName; // the loaded cart's name, may be empty
    s32 sfx;              // the sfx editor's id
    s32 music;            // the music editor's track
    bool alone;           // PRO: the bar's ALONE toggle
    s32 spriteBank;       // PRO: the bank the sprite kind exports from
    s32 mapBank;          // PRO: the bank the map and screen kinds export from
    s32 musicBank;        // PRO: the bank the music kind exports from
    bool spriteVbank1;    // the sprite editor's palette bank
} ExportState;

// Builds `export <kind> <file> [bank=][vbank=][id=][alone=1]` into `out`.
// False for a kind it does not know or a command that does not fit.
bool export_command(char* out, s32 size, s32 id, const ExportState* state);
