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

#include "toolbar.h"

// The strip links against the core alone: no Studio, and nothing that only
// studio.c provides. tests/toolbar.c builds this file without it.

void toolbar_icon(tic_mem* tic, const tic_tiles* tiles, s32 id, s32 x, s32 y, u8 color)
{
    const tic_tile* tile = &tiles->data[id];

    for(s32 i = 0, sx = x, ex = sx + TIC_SPRITESIZE; i != TIC_SPRITESIZE * TIC_SPRITESIZE; ++i, ++x)
    {
        if(x == ex)
        {
            x = sx;
            y++;
        }

        if(tic_tool_peek4(tile, i))
            tic_api_pix(tic, x, y, color, false);
    }
}

void toolbar_cursor(tic_mem* tic, tic_cursor id)
{
    VBANK(tic, 0)
    {
        tic->ram->vram.vars.cursor.sprite = id;
    }
}

void toolbar_playClick(tic_mem* tic, const tic_sfx* sfx, s32 id)
{
    const tic_sample* effect = &sfx->samples.data[id];

    tic_api_sfx(tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, MAX_VOLUME, effect->speed);
}
