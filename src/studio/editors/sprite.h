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

#include "studio/studio.h"
#include "tilesheet.h"

typedef struct Sprite Sprite;

struct Sprite
{
    Studio* studio;
    tic_mem* tic;

    tic_tiles* src;
    tic_tilesheet sheet;

    u32 tickCounter;

    struct 
    {
        bool start;
        tic_point last;
    } draw;

    u16 index;
    u8 color;
    u8 color2;
    u8 size;
    u8 brushSize;
    u16 x,y;
    bool advanced;

    tic_blit blit;

    struct
    {
        bool edit;
        bool vbank1;
        s32 focus;
    } palette;

    struct
    {
        tic_rect rect;
        tic_point start;
        bool drag;
        u8* back;
        u8* front;
    } select;

    enum
    {
        SPRITE_DRAW_MODE,
        SPRITE_PICK_MODE,
        SPRITE_SELECT_MODE,
        SPRITE_FILL_MODE,
    } mode;

    struct History* history;

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

    void (*tick)(Sprite*);
    void (*event)(Sprite*, StudioEvent);
    void (*scanline)(tic_mem* tic, s32 row, void* data);
};

typedef struct
{
    s32 cell_w, cell_h, cols, rows, length;
} tic_palette_dimensions;

void initSprite(Sprite*, Studio* studio, tic_tiles* src);
void freeSprite(Sprite*);

