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

#pragma once

#include "tools.h"

typedef struct
{
    u32    page_orig;
    u32    bank_orig;
    u32    nb_pages;
    u32    bank_size;
    u32    sheet_width;
    u32    tile_width;
    size_t ptr_size;
    u8     (*peek)(const void*, u32);
    void   (*poke)(void*, u32, u8);
} tic_blit_segment;

typedef struct
{
    const tic_blit_segment* segment;
    u8* ptr;
} tic_tilesheet;

typedef struct
{
    const tic_blit_segment* segment;
    u32 offset;
    u8* ptr;
} tic_tileptr;

tic_tilesheet tic_tilesheet_get(u8 segment, u8* ptr);
tic_tileptr tic_tilesheet_gettile(const tic_tilesheet* sheet, s32 index, bool local);

inline u8 tic_tilesheet_getpix(const tic_tilesheet* sheet, s32 x, s32 y)
{
    // tile coord
    u16 tile_index = ((y >> 3) << 4 ) + (x / sheet->segment->tile_width);
    // coord in tile
    u32 pix_addr = ((x & (sheet->segment->tile_width - 1)) + ((y & 7) * sheet->segment->tile_width)) ;
    return sheet->segment->peek(sheet->ptr+tile_index * sheet->segment->ptr_size, pix_addr);
}

inline void tic_tilesheet_setpix(const tic_tilesheet* sheet, s32 x, s32 y, u8 value)
{
    // tile coord
    u16 tile_index = ((y >> 3) << 4 ) + (x / sheet->segment->tile_width);
    // coord in tile
    u32 pix_addr = ((x & (sheet->segment->tile_width - 1)) + ((y & 7) * sheet->segment->tile_width)) ;
    sheet->segment->poke(sheet->ptr + tile_index * sheet->segment->ptr_size, pix_addr, value);
}

inline u8 tic_tilesheet_gettilepix(const tic_tileptr* tile, s32 x, s32 y)
{
    u32 addr = tile->offset + x + (y * tile->segment->tile_width);
    return tile->segment->peek(tile->ptr, addr);
}

inline void tic_tilesheet_settilepix(const tic_tileptr* tile, s32 x, s32 y, u8 value)
{
    u32 addr = tile->offset + x + (y * tile->segment->tile_width);
    tile->segment->poke(tile->ptr, addr, value);
}

typedef struct
{
    tic_bpp mode;
    u8 pages;
    u8 page;
    u8 bank;
} tic_blit;

inline s32 tic_blit_calc_segment(const tic_blit* blit)
{
    return blit->pages * (2 + blit->bank) + blit->page;
}

inline void tic_blit_update_bpp(tic_blit* blit, tic_bpp bpp)
{
    blit->mode = bpp;
    blit->pages = 4 / bpp;
    blit->page %= blit->pages;
}

inline s32 tic_blit_calc_index(const tic_blit* blit)
{
    return blit->bank * blit->pages * TIC_BANK_SPRITES + blit->page * TIC_SPRITESHEET_COLS;
}
