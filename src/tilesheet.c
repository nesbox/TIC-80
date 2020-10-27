// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com
//                    Damien de Lemeny @ddelemeny // hello@ddelemeny.me

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

#include "tilesheet.h"

#include <stdlib.h>

static const tic_blit_segment segments[] = {
    //   +page +nb_pages 
    //   |  +bank +bank_size
    //   |  |  |  |     +sheet_width
    //   |  |  |  |     |   +tile_width
        {0, 0, 1, 256,  16, 8,  TIC_SPRITESIZE,   tic_tool_peek1, tic_tool_poke1}, // system gfx
        {0, 0, 1, 256,  16, 8,  TIC_SPRITESIZE,   tic_tool_peek1, tic_tool_poke1}, // system font
        {0, 0, 1, 256,  16, 8,  sizeof(tic_tile), tic_tool_peek4, tic_tool_poke4}, // 4bpp p0 bg
        {0, 1, 1, 256,  16, 8,  sizeof(tic_tile), tic_tool_peek4, tic_tool_poke4}, // 4bpp p0 fg

        {0, 0, 2, 512,  32, 16, sizeof(tic_tile), tic_tool_peek2, tic_tool_poke2}, // 2bpp p0 bg
        {1, 0, 2, 512,  32, 16, sizeof(tic_tile), tic_tool_peek2, tic_tool_poke2}, // 2bpp p1 bg
        {0, 1, 2, 512,  32, 16, sizeof(tic_tile), tic_tool_peek2, tic_tool_poke2}, // 2bpp p0 fg
        {1, 1, 2, 512,  32, 16, sizeof(tic_tile), tic_tool_peek2, tic_tool_poke2}, // 2bpp p1 fg

        {0, 0, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p0 bg
        {1, 0, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p1 bg
        {2, 0, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p2 bg
        {3, 0, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p3 bg
        {0, 1, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p0 fg
        {1, 1, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p1 fg
        {2, 1, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p2 fg
        {3, 1, 4, 1024, 64, 32, sizeof(tic_tile), tic_tool_peek1, tic_tool_poke1}, // 1bpp p3 fg
};

extern u8 tic_tilesheet_getpix(const tic_tilesheet* sheet, s32 x, s32 y);
extern void tic_tilesheet_setpix(const tic_tilesheet* sheet, s32 x, s32 y, u8 value);
extern u8 tic_tilesheet_gettilepix(const tic_tileptr* tile, s32 x, s32 y);
extern void tic_tilesheet_settilepix(const tic_tileptr* tile, s32 x, s32 y, u8 value);

tic_tilesheet tic_tilesheet_get(u8 segment, u8* ptr)
{
    return (tic_tilesheet) { &segments[segment], ptr };
}

tic_tileptr tic_tilesheet_gettile(const tic_tilesheet* sheet, s32 index, bool local)
{
    enum { Cols = 16, Size = 8 };
    const tic_blit_segment* segment = sheet->segment;

    s32 bank, page, iy, ix;
    if (local) {
        index = index & 255;
        bank = segment->bank_orig;
        page = segment->page_orig;
        div_t ixy = div(index, Cols);
        iy = ixy.quot;
        ix = ixy.rem;
    }
    else {
        // reindex
        div_t ia = div(index, segment->bank_size);    // bank, bank_index
        div_t ib = div(ia.rem, segment->sheet_width); // yi, bank_xi
        div_t ic = div(ib.rem, Cols);                // page, xi
        bank = (ia.quot + segment->bank_orig) % 2;
        page = (ic.quot + segment->page_orig) % segment->nb_pages;
        iy = ib.quot % Cols;
        ix = ic.rem;
    }

    div_t xdiv = div(ix, segment->nb_pages);    // xbuffer, xoffset
    u32 ptr_offset = (bank * Cols + iy) * Cols + page * Cols / segment->nb_pages + xdiv.quot;
    u8* ptr = sheet->ptr + segment->ptr_size * ptr_offset;
    u32 offset = (xdiv.rem * Size);

    return (tic_tileptr) { segment, offset, ptr };
}

extern s32 tic_blit_calc_segment(const tic_blit* blit);
extern void tic_blit_update_bpp(tic_blit* blit, tic_bpp bpp);
extern s32 tic_blit_calc_index(const tic_blit* blit);
