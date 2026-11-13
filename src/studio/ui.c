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

// Four cells to a sprite, laid out as the sprite editor's 1bpp page 1.
#define UI_CELLS_PER_SPRITE 4

u8 ui_cell_row(const tic_tiles* tiles, s32 id, s32 row)
{
    const tic_tile* cell = &tiles->data[(id / TIC_SPRITESHEET_COLS) * TIC_SPRITESHEET_COLS + (id % TIC_SPRITESHEET_COLS) / UI_CELLS_PER_SPRITE];

    return cell->data[row * UI_CELLS_PER_SPRITE + id % UI_CELLS_PER_SPRITE];
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
