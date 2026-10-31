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

typedef struct Sprite Sprite;

// What the host gives the editor once: the machine, the pointer states and the
// tooltip ask.
typedef struct
{
    void* host;
    tic_mem* tic;
    MouseState* mouse;

    void (*tooltip)(void* host, const char* text);

} SpriteDeps;

// The frame: the bar's flags, the cart's bank0 tiles and palette, and the
// current sprite bank's palettes and flags.
typedef struct
{
    struct
    {
        bool captured;
        bool pointerOwned;
        ClipboardEvent clipboard;

    } frame;

    const tic_tiles* cartTiles;
    const tic_palette* cartPalette;
    tic_palette* vbank0;
    tic_palette* vbank1;
    tic_flags* flags;

} SpriteView;

struct Sprite
{
    SpriteDeps deps;
    const SpriteView* view;

    tic_mem* tic;

    tic_tiles* src;
    tic_tilesheet sheet;

    u32 tickCounter;

    struct
    {
        bool start;
        tic_point lastPos; // brush cell on the canvas, or -1 if none
    } draw;

    u16 index;
    u8 color;
    u8 color2;
    u8 canvasSize;
    tic_point spritePos;
    bool hexindex;

    tic_blit blit;

    struct
    {
        u8 size;
        bool symmetryHorizontal;
        bool symmetryVertical;
    } brush;

    struct
    {
        bool edit;
        bool vbank1;
        s32 focus;
        const tic_palette* preview;
    } palette;

    struct
    {
        tic_rect rect;
        tic_point start;
        bool drag;
        u8* back;
        u8* front;
    } select;

    struct
    {
        bool edit;
        bool show;
        s32 pos;
    } flags;

    enum
    {
        SPRITE_DRAW_MODE,
        SPRITE_PICK_MODE,
        SPRITE_SELECT_MODE,
        SPRITE_FILL_MODE,
    } mode;

    struct History* history;
    bool needsHistoryUpdate;

    struct
    {
        struct
        {
            s32 bank;
            s32 page;
        } pos;

        Movie* movie;

        Movie idle;
        Movie bank;
        Movie page;

    } anim;
};

typedef void (*SpritePaintPixel)(Sprite*, s32 x, s32 y, u8 color);

typedef struct
{
    s32 cell_w, cell_h, cols, rows, length;
} tic_palette_dimensions;

extern const ClipboardOps SpriteClipboard;

void initSprite(Sprite*, const SpriteDeps*, tic_tiles* src);
void sprite_set_view(Sprite*, const SpriteView*);
void sprite_tick(Sprite*);
void sprite_scanline(tic_mem* tic, s32 row, void* data);
void freeSprite(Sprite*);

enum
{
    SPRITE_TOOL_FLIP_HORZ,
    SPRITE_TOOL_FLIP_VERT,
    SPRITE_TOOL_ROTATE,
    SPRITE_TOOL_ERASE,
};

void spriteSelectBank(Sprite*, s32 bank);
void spriteSelectPage(Sprite*, s32 page);
void spriteSetBpp(Sprite*, tic_bpp);
void spriteSetSize(Sprite*, s32 size);
void spriteSetBrushSize(Sprite*, s32 size);
void spriteSetMode(Sprite*, s32 mode);
void spriteToolAction(Sprite*, s32 tool);
