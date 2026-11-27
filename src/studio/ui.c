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

#include "ui.h"
#include "tic_assert.h"

// Four cells to a sprite; the page picks the four sprites after the first.
#define UI_CELLS_PER_SPRITE 4

// The copy below writes the faces through a byte pointer.
static_assert(sizeof(tic_font) == sizeof(tic_font_data) * 2, "tic_font");

u8 ui_cell_row(const tic_tiles* tiles, s32 id, s32 row)
{
    const s32 page = id / TIC_FONT_CHARS;
    const s32 cell = id % TIC_FONT_CHARS;
    const tic_tile* tile = &tiles->data[(cell / TIC_SPRITESHEET_COLS) * TIC_SPRITESHEET_COLS + page * UI_CELLS_PER_SPRITE + (cell % TIC_SPRITESHEET_COLS) / UI_CELLS_PER_SPRITE];

    return tile->data[row * UI_CELLS_PER_SPRITE + cell % UI_CELLS_PER_SPRITE];
}

void ui_font_cells(tic_font* font, const tic_tiles* tiles)
{
    *font = (tic_font){
        .regular =
            {
                {0},
                {
                    {
                        .width = TIC_FONT_WIDTH,
                        .height = TIC_FONT_HEIGHT,
                    },
                },
            },
        .alt =
            {
                {0},
                {
                    {
                        .width = TIC_ALTFONT_WIDTH,
                        .height = TIC_FONT_HEIGHT,
                    },
                },
            }};

    u8* dst = (u8*)font;

    for(s32 i = 0; i < TIC_FONT_CHARS * 2; i++)
    {
        // The last cell of each page carries the width/height pair, not a glyph.
        if(i % TIC_FONT_CHARS == TIC_FONT_CHARS - 1)
            continue;

        for(s32 y = 0; y < TIC_SPRITESIZE; y++)
            dst[i * BITS_IN_BYTE + y] = ui_cell_row(tiles, i, y);
    }
}

void ui_icon(tic_mem* tic, const tic_tiles* tiles, s32 id, s32 x, s32 y, u8 color)
{
    for(s32 row = 0; row < TIC_SPRITESIZE; row++)
    {
        const u8 bits = ui_cell_row(tiles, id, row);

        for(s32 col = 0; col < TIC_SPRITESIZE; col++)
            if(bits & (1 << col))
                tic_api_pix(tic, x + col, y + row, color, false);
    }
}

void ui_cursor(tic_mem* tic, tic_cursor id)
{
    VBANK(tic, 0)
    {
        tic->ram->vram.vars.cursor.sprite = id;
    }
}
