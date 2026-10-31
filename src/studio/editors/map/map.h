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
#include "tilesheet.h"
#include "studio/anim.h"
#include "studio/clipboard.h"
#include "studio/keyboard.h"
#include "studio/mouse.h"

typedef struct Map Map;

// What the host gives the editor once: the machine, the pointer states, and the
// two mode switches only the studio can perform.
typedef struct
{
    void* host;
    tic_mem* tic;
    MouseState* mouse;

    void (*openWorld)(void* host);
    void (*openMap)(void* host);

} MapDeps;

// The frame: the bar's pointer flag, the clipboard gesture and the values the
// host refreshes every tick.
typedef struct
{
    struct
    {
        bool pointerOwned;
        ClipboardEvent clipboard;

    } frame;

    s32 editorTop;
    const tic_tiles* tiles;
    const tic_palette* palette;
    const tic_palette* cartPalette;

} MapView;

struct Map
{
    MapDeps deps;
    const MapView* view;

    tic_mem* tic;
    tic_map* src;

    s32 tickCounter;

    enum
    {
        MAP_DRAW_MODE = 0,
        MAP_DRAG_MODE,
        MAP_SELECT_MODE,
        MAP_FILL_MODE,
    } mode;

    struct
    {
        bool grid;
        bool draw;
        bool random;
        tic_point start;
        tic_point current;
    } canvas;

    struct
    {
        bool keep;
        tic_rect rect;
        tic_point start;
        bool drag;

        tic_blit blit;
    } sheet;

    struct
    {
        s32 x;
        s32 y;

        tic_point start;

        bool active;

    } scroll;

    struct
    {
        tic_rect rect;
        tic_point start;
        bool drag;
    } select;

    u8* paste;

    struct History* history;
    bool needsHistoryUpdate;

    struct
    {
        struct
        {
            s32 sheet;
            s32 bank;
            s32 page;
        } pos;

        Movie* movie;

        Movie idle;
        Movie show;
        Movie hide;
        Movie bank;
        Movie page;

    } anim;
};

extern const ClipboardOps MapClipboard;

void map_select_bank(Map*, s32 bank);
void map_select_page(Map*, s32 page);
void mapSetBpp(Map*, tic_bpp bpp);
void map_toggle_sheet(Map*);

void initMap(Map*, const MapDeps*, tic_map* src);
void map_set_view(Map*, const MapView*);
void map_tick(Map*);
void map_scanline(tic_mem* tic, s32 row, void* data);
void freeMap(Map* map);
