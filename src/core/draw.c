// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "api.h"
#include "core.h"
#include "tilesheet.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define TRANSPARENT_COLOR            255
#define SUPERELLIPSE_MIN_EXPONENT    0.3
#define SUPERELLIPSE_MAX_EXPONENT    8
#define SUPERELLIPSE_CIRCLE_EXPONENT 2

typedef void (*PixelFunc)(tic_mem* memory, s32 x, s32 y, u8 color);

static tic_tilesheet getTileSheetFromSegment(tic_mem* memory, u8 segment)
{
    u8* src;
    switch(segment)
    {
        case 0:
        case 1:
            src = (u8*)&memory->ram->font;
            break;
        default:
            src = (u8*)&memory->ram->tiles.data;
            break;
    }

    return tic_tilesheet_get(segment, src);
}

static u8* getPalette(tic_mem* tic, u8* colors, u8 count)
{
    static u8 mapping[TIC_PALETTE_SIZE];
    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        mapping[i] = tic_tool_peek4(tic->ram->vram.mapping, i);
    for(s32 i = 0; i < count; i++)
    {
        if(colors[i] < TIC_PALETTE_SIZE)
        {
            mapping[colors[i]] = TRANSPARENT_COLOR;
        }
    }
    return mapping;
}

static inline u8 mapColor(tic_mem* tic, u8 color)
{
    return tic_tool_peek4(tic->ram->vram.mapping, color & 0xf);
}

static inline u8* screenData(tic_core* core)
{
    return core->memory.ram->vram.screen.data;
}

static inline void setPixel(tic_core* core, s32 x, s32 y, u8 color)
{

    if(x < core->state.clip.l || y < core->state.clip.t || x >= core->state.clip.r || y >= core->state.clip.b) return;

    tic_tool_poke4(screenData(core), (u32)(y * TIC80_WIDTH + x), color);
}

static inline void setPixelFast(tic_core* core, s32 x, s32 y, u8 color)
{
    // does not do any CLIP checking, the caller needs to do that first
    tic_tool_poke4(screenData(core), (u32)(y * TIC80_WIDTH + x), color);
}

static inline u8 getPixel(tic_core* core, s32 x, s32 y)
{
    return x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT
               ? 0
               : tic_tool_peek4(screenData(core), (u32)(y * TIC80_WIDTH + x));
}

static inline u8 screenPixel(tic_core* core, s32 x, s32 y)
{
    return tic_tool_peek4(screenData(core), (u32)(y * TIC80_WIDTH + x));
}

// clang-format off
#define EARLY_CLIP(x, y, width, height) \
    ( \
        (((y)+(height)-1) < core->state.clip.t) \
        || (((x)+(width)-1) < core->state.clip.l) \
        || ((y) >= core->state.clip.b) \
        || ((x) >= core->state.clip.r) \
    )
// clang-format on

static void drawHLine(tic_core* core, s32 x, s32 y, s32 width, u8 color)
{

    // Glyphs and thin rects are often one pixel wide; the run machinery below
    // costs more than the one write.
    if(width == 1)
    {
        setPixel(core, x, y, color);
        return;
    }

    if(y < core->state.clip.t || core->state.clip.b <= y) return;

    s32 xl = MAX(x, core->state.clip.l);
    s32 xr = MIN(x + width, core->state.clip.r);

    if(xl >= xr) return;

    u8* screen = screenData(core);
    s32 first = y * TIC80_WIDTH + xl;
    s32 last = y * TIC80_WIDTH + xr; // exclusive

    // A short run is a few nibble writes; a memset call costs more than that.
    if(last - first <= 4)
    {
        for(s32 i = first; i < last; ++i)
            tic_tool_poke4(screen, (u32)i, color);

        return;
    }

    // The two edge nibbles keep the middle byte-aligned, where a run of
    // identical pixels is one memset byte per two pixels.
    if(first & 1)
        tic_tool_poke4(screen, (u32)first++, color);

    if(last & 1)
        tic_tool_poke4(screen, (u32)--last, color);

    if(first < last)
        memset(screen + first / 2, (color & 0xf) | (color << TIC_PALETTE_BPP), (size_t)(last - first) / 2);
}

static void drawVLine(tic_core* core, s32 x, s32 y, s32 height, u8 color)
{

    if(x < core->state.clip.l || core->state.clip.r <= x) return;

    s32 yl = MAX(y, core->state.clip.t);
    s32 yr = MIN(y + height, core->state.clip.b);
    u8* screen = screenData(core);

    for(s32 i = yl; i < yr; ++i)
        tic_tool_poke4(screen, (u32)(i * TIC80_WIDTH + x), color);
}

static void drawRect(tic_core* core, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for(s32 i = y; i < y + height; ++i)
        drawHLine(core, x, i, width, color);
}

static void drawRectBorder(tic_core* core, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    drawHLine(core, x, y, width, color);
    drawHLine(core, x, y + height - 1, width, color);

    drawVLine(core, x, y, height, color);
    drawVLine(core, x + width - 1, y, height, color);
}

static void drawTile4(tic_core* core, const u8* ptr, u32 offset, const u8* mapping, bool opaque,
                      s32 x, s32 y, s32 sx, s32 sy, s32 ex, s32 ey)
{
    u8* screen = screenData(core);

    for(s32 py = sy; py < ey; py++, y++)
    {
        s32 px = sx;
        s32 dest = x;
        u32 row = offset + (u32)(py * TIC_SPRITESIZE);

        if((dest & 1) && px < ex)
        {
            u8 color = mapping[tic_tool_peek4(ptr, row + (u32)px)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);

            px++;
            dest++;
        }

        for(; px + 1 < ex; px += 2, dest += 2)
        {
            u8 src = ptr[(row + (u32)px) >> 1];
            u8 m0 = mapping[src & 0xf];
            u8 m1 = mapping[src >> 4];

            if(opaque)
                screen[(y * TIC80_WIDTH + dest) >> 1] = m0 | (m1 << 4);
            else
            {
                u8* dst = &screen[(y * TIC80_WIDTH + dest) >> 1];
                u8 byte = (m0 != TRANSPARENT_COLOR ? m0 : (*dst & 0xf)) | (m1 != TRANSPARENT_COLOR ? (m1 << 4) : (*dst & 0xf0));
                *dst = byte;
            }
        }

        if(px < ex)
        {
            u8 color = mapping[tic_tool_peek4(ptr, row + (u32)px)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);
        }
    }
}

// The 2bpp/1bpp pair fill: two source pixels per screen byte, like the
// 4bpp path, with the same transparent read-modify-write.
static void drawTilePairs(tic_core* core, const u8* ptr, u32 offset, u32 tileWidth, bool is2bpp,
                          const u8* mapping, bool opaque, s32 x, s32 y, s32 sx, s32 sy, s32 ex, s32 ey)
{
    u8* screen = screenData(core);

    for(s32 py = sy; py < ey; py++, y++)
    {
        s32 px = sx;
        s32 dest = x;
        u32 row = offset + (u32)(py * tileWidth);

        if((dest & 1) && px < ex)
        {
            u8 color = mapping[is2bpp ? tic_tool_peek2(ptr, row + (u32)px) : tic_tool_peek1(ptr, row + (u32)px)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);

            px++;
            dest++;
        }

        for(; px + 1 < ex; px += 2, dest += 2)
        {
            u8 m0 = mapping[is2bpp ? tic_tool_peek2(ptr, row + (u32)px) : tic_tool_peek1(ptr, row + (u32)px)];
            u8 m1 = mapping[is2bpp ? tic_tool_peek2(ptr, row + (u32)(px + 1)) : tic_tool_peek1(ptr, row + (u32)(px + 1))];

            if(opaque)
                screen[(y * TIC80_WIDTH + dest) >> 1] = m0 | (m1 << 4);
            else
            {
                u8* dst = &screen[(y * TIC80_WIDTH + dest) >> 1];
                u8 byte = (m0 != TRANSPARENT_COLOR ? m0 : (*dst & 0xf)) | (m1 != TRANSPARENT_COLOR ? (m1 << 4) : (*dst & 0xf0));
                *dst = byte;
            }
        }

        if(px < ex)
        {
            u8 color = mapping[is2bpp ? tic_tool_peek2(ptr, row + (u32)px) : tic_tool_peek1(ptr, row + (u32)px)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);
        }
    }
}

static inline u8 tileRaw(const u8* ptr, bool is4bpp, bool is2bpp, bool is1bpp, u8 (*peek)(const void*, u32), u32 addr)
{
    return is4bpp   ? tic_tool_peek4(ptr, addr)
           : is2bpp ? tic_tool_peek2(ptr, addr)
           : is1bpp ? tic_tool_peek1(ptr, addr)
                    : peek(ptr, addr);
}

// Flipped and rotated tiles: the source address of the destination pair
// steps by a constant, so two pixels still go into one screen byte.
static void drawTilePairsOriented(tic_core* core, const u8* ptr, u32 offset, u32 tileWidth,
                                  bool is4bpp, bool is2bpp, bool is1bpp, u8 (*peek)(const void*, u32),
                                  u32 orientation, const u8* mapping, bool opaque,
                                  s32 x, s32 y, s32 sx, s32 sy, s32 ex, s32 ey)
{
    u8* screen = screenData(core);

    s32 dPix = 0;

    switch(orientation)
    {
        case 0: dPix = 1; break;
        case 1: dPix = -1; break;
        case 2: dPix = 1; break;
        case 3: dPix = -1; break;
        case 4: dPix = (s32)tileWidth; break;
        case 5: dPix = -(s32)tileWidth; break;
        case 6: dPix = (s32)tileWidth; break;
        case 7: dPix = -(s32)tileWidth; break;
    }

    for(s32 py = sy; py < ey; py++, y++)
    {
        s32 ix = 0, iy = 0;

        switch(orientation)
        {
            case 0: ix = sx, iy = py; break;
            case 1: ix = TIC_SPRITESIZE - 1 - sx, iy = py; break;
            case 2: ix = sx, iy = TIC_SPRITESIZE - 1 - py; break;
            case 3: ix = TIC_SPRITESIZE - 1 - sx, iy = TIC_SPRITESIZE - 1 - py; break;
            case 4: ix = py, iy = sx; break;
            case 5: ix = py, iy = TIC_SPRITESIZE - 1 - sx; break;
            case 6: ix = TIC_SPRITESIZE - 1 - py, iy = sx; break;
            case 7: ix = TIC_SPRITESIZE - 1 - py, iy = TIC_SPRITESIZE - 1 - sx; break;
        }

        u32 addr = offset + (u32)(ix + iy * (s32)tileWidth);
        s32 px = sx;
        s32 dest = x;

        if((dest & 1) && px < ex)
        {
            u8 color = mapping[tileRaw(ptr, is4bpp, is2bpp, is1bpp, peek, addr)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);

            px++;
            dest++;
            addr = (u32)((s32)addr + dPix);
        }

        for(; px + 1 < ex; px += 2, dest += 2, addr = (u32)((s32)addr + 2 * dPix))
        {
            u8 m0 = mapping[tileRaw(ptr, is4bpp, is2bpp, is1bpp, peek, addr)];
            u8 m1 = mapping[tileRaw(ptr, is4bpp, is2bpp, is1bpp, peek, (u32)((s32)addr + dPix))];

            if(opaque)
                screen[(y * TIC80_WIDTH + dest) >> 1] = m0 | (m1 << 4);
            else
            {
                u8* dst = &screen[(y * TIC80_WIDTH + dest) >> 1];
                u8 byte = (m0 != TRANSPARENT_COLOR ? m0 : (*dst & 0xf)) | (m1 != TRANSPARENT_COLOR ? (m1 << 4) : (*dst & 0xf0));
                *dst = byte;
            }
        }

        if(px < ex)
        {
            u8 color = mapping[tileRaw(ptr, is4bpp, is2bpp, is1bpp, peek, addr)];

            if(color != TRANSPARENT_COLOR)
                tic_tool_poke4(screen, (u32)(y * TIC80_WIDTH + dest), color);
        }
    }
}

// clang-format off
#define DRAW_TILE_BODY(X, Y) do {\
    for(s32 py=sy; py < ey; py++, y++) \
    { \
        s32 xx = x; \
        for(s32 px=sx; px < ex; px++, xx++) \
        { \
            u32 addr = offset + (u32)((X) + (Y) * tileWidth); \
            u8 raw = is4bpp ? tic_tool_peek4(ptr, addr) \
                     : is2bpp ? tic_tool_peek2(ptr, addr) \
                     : is1bpp ? tic_tool_peek1(ptr, addr) \
                     : peek(ptr, addr); \
            u8 color = mapping[raw]; \
            if(color != TRANSPARENT_COLOR) setPixelFast(core, xx, y, color); \
        } \
    } \
    } while(0)
// clang-format on

#define REVERT(X) (TIC_SPRITESIZE - 1 - (X))

static void drawTile(tic_core* core, tic_tileptr* tile, s32 x, s32 y, u8* mapping, s32 scale, tic_flip flip, tic_rotate rotate)
{
    const tic_blit_segment* segment = tile->segment;
    const u8* ptr = tile->ptr;
    u32 offset = tile->offset;
    u32 tileWidth = segment->tile_width;
    u8 (*peek)(const void*, u32) = segment->peek;
    bool is4bpp = tileWidth == TIC_SPRITESIZE && segment->ptr_size == sizeof(tic_tile);
    bool is2bpp = tileWidth == TIC_SPRITESIZE * 2 && segment->ptr_size == sizeof(tic_tile);
    bool is1bpp = tileWidth == TIC_SPRITESIZE * 4 && segment->ptr_size == sizeof(tic_tile);

    rotate &= 3;
    u32 orientation = flip & 3;

    if(rotate == tic_90_rotate)
        orientation ^= 1;
    else if(rotate == tic_180_rotate)
        orientation ^= 3;
    else if(rotate == tic_270_rotate)
        orientation ^= 2;
    if(rotate == tic_90_rotate || rotate == tic_270_rotate) orientation |= 4;

    if(scale == 1)
    {
        // the most common path
        s32 sx, sy, ex, ey;
        sx = core->state.clip.l - x;
        if(sx < 0) sx = 0;
        sy = core->state.clip.t - y;
        if(sy < 0) sy = 0;
        ex = core->state.clip.r - x;
        if(ex > TIC_SPRITESIZE) ex = TIC_SPRITESIZE;
        ey = core->state.clip.b - y;
        if(ey > TIC_SPRITESIZE) ey = TIC_SPRITESIZE;

        const s32 destX = x;
        y += sy;
        x += sx;

        if((destX & 1) == 0 && (is4bpp || is2bpp || is1bpp))
        {
            bool opaque = true;

            for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                if(mapping[i] == TRANSPARENT_COLOR)
                {
                    opaque = false;
                    break;
                }

            if(orientation == 0 && is4bpp)
            {
                if((offset & 1) == 0)
                {
                    drawTile4(core, ptr, offset, mapping, opaque, x, y, sx, sy, ex, ey);
                    return;
                }
            }
            else if(orientation == 0)
            {
                drawTilePairs(core, ptr, offset, tileWidth, is2bpp, mapping, opaque, x, y, sx, sy, ex, ey);
                return;
            }
            else
            {
                drawTilePairsOriented(core, ptr, offset, tileWidth, is4bpp, is2bpp, is1bpp, peek,
                                      orientation, mapping, opaque, x, y, sx, sy, ex, ey);
                return;
            }
        }

        switch(orientation)
        {
            case 4: DRAW_TILE_BODY(py, px); break;
            case 6: DRAW_TILE_BODY(REVERT(py), px); break;
            case 5: DRAW_TILE_BODY(py, REVERT(px)); break;
            case 7: DRAW_TILE_BODY(REVERT(py), REVERT(px)); break;
            case 0: DRAW_TILE_BODY(px, py); break;
            case 2: DRAW_TILE_BODY(px, REVERT(py)); break;
            case 1: DRAW_TILE_BODY(REVERT(px), py); break;
            case 3: DRAW_TILE_BODY(REVERT(px), REVERT(py)); break;
        }
        return;
    }

    if(scale == 2 && orientation == 0 && (x & 1) == 0 &&
       x >= core->state.clip.l && y >= core->state.clip.t &&
       x + TIC_SPRITESIZE * scale <= core->state.clip.r && y + TIC_SPRITESIZE * scale <= core->state.clip.b)
    {
        u8* screen = screenData(core);

        for(s32 py = 0; py < TIC_SPRITESIZE; py++)
            for(s32 px = 0; px < TIC_SPRITESIZE; px++)
            {
                u32 addr = offset + (u32)(px + py * (s32)tileWidth);
                u8 color = mapping[tileRaw(ptr, is4bpp, is2bpp, is1bpp, peek, addr)];

                if(color == TRANSPARENT_COLOR)
                    continue;

                u8 byte = (u8)(color | (color << 4));
                s32 dx = x + px * scale, dy = y + py * scale;

                screen[(dy * TIC80_WIDTH + dx) >> 1] = byte;
                screen[((dy + 1) * TIC80_WIDTH + dx) >> 1] = byte;
            }
        return;
    }

    if(EARLY_CLIP(x, y, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale)) return;

    for(s32 py = 0; py < TIC_SPRITESIZE; py++, y += scale)
    {
        s32 xx = x;
        for(s32 px = 0; px < TIC_SPRITESIZE; px++, xx += scale)
        {
            s32 ix = orientation & 1 ? TIC_SPRITESIZE - px - 1 : px;
            s32 iy = orientation & 2 ? TIC_SPRITESIZE - py - 1 : py;
            if(orientation & 4)
            {
                s32 tmp = ix;
                ix = iy;
                iy = tmp;
            }
            u32 addr = offset + (u32)(ix + iy * tileWidth);
            u8 raw = is4bpp   ? tic_tool_peek4(ptr, addr)
                     : is2bpp ? tic_tool_peek2(ptr, addr)
                     : is1bpp ? tic_tool_peek1(ptr, addr)
                              : peek(ptr, addr);
            u8 color = mapping[raw];
            if(color != TRANSPARENT_COLOR) drawRect(core, xx, y, scale, scale, color);
        }
    }
}

#undef DRAW_TILE_BODY
#undef REVERT

static void drawSprite(tic_core* core, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{

    if(index < 0)
        return;

    rotate &= 3;
    flip &= 3;

    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram->vram.blit.segment);
    u8* mapping = getPalette(&core->memory, colors, (u8)count);

    if(w == 1 && h == 1)
    {
        tic_tileptr tile = tic_tilesheet_gettile(&sheet, index, false);
        drawTile(core, &tile, x, y, mapping, scale, flip, rotate);
    }
    else
    {
        s32 step = TIC_SPRITESIZE * scale;
        s32 cols = sheet.segment->sheet_width;

        const tic_flip vert_horz_flip = tic_horz_flip | tic_vert_flip;

        if(EARLY_CLIP(x, y, w * step, h * step)) return;

        for(s32 i = 0; i < w; i++)
        {
            for(s32 j = 0; j < h; j++)
            {
                s32 mx = i;
                s32 my = j;

                if(flip == tic_horz_flip || flip == vert_horz_flip) mx = w - 1 - i;
                if(flip == tic_vert_flip || flip == vert_horz_flip) my = h - 1 - j;

                if(rotate == tic_180_rotate)
                {
                    mx = w - 1 - mx;
                    my = h - 1 - my;
                }
                else if(rotate == tic_90_rotate)
                {
                    if(flip == tic_no_flip || flip == vert_horz_flip)
                        my = h - 1 - my;
                    else
                        mx = w - 1 - mx;
                }
                else if(rotate == tic_270_rotate)
                {
                    if(flip == tic_no_flip || flip == vert_horz_flip)
                        mx = w - 1 - mx;
                    else
                        my = h - 1 - my;
                }

                enum
                {
                    Cols = TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE
                };

                tic_tileptr tile = tic_tilesheet_gettile(&sheet, index + mx + my * cols, false);
                if(rotate == 0 || rotate == 2)
                    drawTile(core, &tile, x + i * step, y + j * step, mapping, scale, flip, rotate);
                else
                    drawTile(core, &tile, x + j * step, y + i * step, mapping, scale, flip, rotate);
            }
        }
    }
}

static void drawMap(tic_core* core, const tic_map* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)
{
    const s32 size = TIC_SPRITESIZE * scale;

    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram->vram.blit.segment);
    u8* mapping = getPalette(&core->memory, colors, (u8)count);

    for(s32 j = y, jj = sy; j < y + height; j++, jj += size)
        for(s32 i = x, ii = sx; i < x + width; i++, ii += size)
        {
            s32 mi = tic_modulo(i, TIC_MAP_WIDTH);
            s32 mj = tic_modulo(j, TIC_MAP_HEIGHT);

            s32 index = mi + mj * TIC_MAP_WIDTH;
            RemapResult retile = {*(src->data + index), tic_no_flip, tic_no_rotate};

            if(remap)
                remap(data, mi, mj, &retile);

            tic_tileptr tile = tic_tilesheet_gettile(&sheet, retile.index, true);
            drawTile(core, &tile, ii, jj, mapping, scale, retile.flip, retile.rotate);
        }
}

static s32 drawChar(tic_core* core, tic_tileptr* font_char, s32 x, s32 y, s32 scale, bool fixed, u8* mapping)
{

    enum
    {
        Size = TIC_SPRITESIZE
    };

    const tic_blit_segment* segment = font_char->segment;
    const u8* ptr = font_char->ptr;
    u32 offset = font_char->offset;
    u32 tileWidth = segment->tile_width;
    u8 (*peek)(const void*, u32) = segment->peek;
    bool is4bpp = tileWidth == TIC_SPRITESIZE && segment->ptr_size == sizeof(tic_tile);
    bool is1bpp = tileWidth == TIC_SPRITESIZE && segment->ptr_size == 8;

#define GLYPH(X, Y) (is4bpp   ? tic_tool_peek4(ptr, offset + (u32)((X) + (Y) * tileWidth)) \
                     : is1bpp ? tic_tool_peek1(ptr, offset + (u32)((X) + (Y) * tileWidth)) \
                              : peek(ptr, offset + (u32)((X) + (Y) * tileWidth)))

    s32 j = 0, start = 0, end = Size;

    if(!fixed)
    {
        for(s32 i = 0; i < Size; i++)
        {
            for(j = 0; j < Size; j++)
                if(mapping[GLYPH(i, j)] != TRANSPARENT_COLOR) break;
            if(j < Size)
                break;
            else
                start++;
        }
        for(s32 i = Size - 1; i >= start; i--)
        {
            for(j = 0; j < Size; j++)
                if(mapping[GLYPH(i, j)] != TRANSPARENT_COLOR) break;
            if(j < Size)
                break;
            else
                end--;
        }
    }
    s32 width = end - start;

    if(EARLY_CLIP(x, y, Size * scale, Size * scale)) return width;

    s32 colStart = start, colStep = 1, rowStart = 0, rowStep = 1;

    for(s32 i = 0, col = colStart, xs = x; i < width; i++, col += colStep, xs += scale)
    {
        for(s32 gy = 0, row = rowStart, ys = y; gy < Size; gy++, row += rowStep, ys += scale)
        {
            u8 color = mapping[GLYPH(col, row)];

            if(color != TRANSPARENT_COLOR)
            {
                if(scale == 1)
                    setPixel(core, xs, ys, color);
                else
                    drawRect(core, xs, ys, scale, scale, color);
            }
        }
    }
    return width;
}

#undef GLYPH

static s32 drawText(tic_core* core, tic_tilesheet* font_face, const char* text, s32 x, s32 y, s32 width, s32 height, bool fixed, u8* mapping, s32 scale, bool alt)
{
    s32 pos = x;
    s32 MAX = x;
    char sym = 0;

    while((sym = *text++))
    {
        if(sym == '\n')
        {
            if(pos > MAX)
                MAX = pos;

            pos = x;
            y += height * scale;
        }
        else
        {
            tic_tileptr font_char = tic_tilesheet_gettile(font_face, alt * TIC_FONT_CHARS + sym, true);
            s32 size = drawChar(core, &font_char, pos, y, scale, fixed, mapping);
            pos += ((!fixed && size) ? size + 1 : width) * scale;
        }
    }

    return pos > MAX ? pos - x : MAX - x;
}

void tic_api_clip(tic_mem* memory, s32 x, s32 y, s32 width, s32 height)
{
    tic_core* core = (tic_core*)memory;

    core->state.clip.l = x;
    core->state.clip.t = y;
    core->state.clip.r = x + width;
    core->state.clip.b = y + height;

    if(core->state.clip.l < 0) core->state.clip.l = 0;
    if(core->state.clip.t < 0) core->state.clip.t = 0;
    if(core->state.clip.r > TIC80_WIDTH) core->state.clip.r = TIC80_WIDTH;
    if(core->state.clip.b > TIC80_HEIGHT) core->state.clip.b = TIC80_HEIGHT;
}

void tic_api_rect(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    tic_core* core = (tic_core*)memory;

    drawRect(core, x, y, width, height, mapColor(memory, color));
}

static double ZBuffer[TIC80_WIDTH * TIC80_HEIGHT];

void tic_api_cls(tic_mem* tic, u8 color)
{
    tic_core* core = (tic_core*)tic;
    tic_vram* vram = &tic->ram->vram;

    static const struct ClipRect EmptyClip = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    color = mapColor(tic, color);

    if(MEMCMP(core->state.clip, EmptyClip))
    {
        memset(&vram->screen, (color & 0xf) | (color << TIC_PALETTE_BPP), sizeof(tic_screen));
        ZEROMEM(ZBuffer);
    }
    else
    {
        s32 x = core->state.clip.l;
        s32 width = core->state.clip.r - x;

        if(width > 0)
            for(s32 y = core->state.clip.t; y < core->state.clip.b; ++y)
            {
                drawHLine(core, x, y, width, color);
                memset(&ZBuffer[y * TIC80_WIDTH + x], 0, sizeof(double) * (size_t)width);
            }
    }
}

s32 tic_api_font(tic_mem* memory, const char* text, s32 x, s32 y, u8* trans_colors, u8 trans_count, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    u8* mapping = getPalette(memory, trans_colors, trans_count);

    // Compatibility : flip top and bottom of the spritesheet
    // to preserve tic_api_font's default target
    u8 segment = memory->ram->vram.blit.segment >> 1;
    u8 flipmask = 1;
    while(segment >>= 1)
        flipmask <<= 1;

    tic_tilesheet font_face = getTileSheetFromSegment(memory, memory->ram->vram.blit.segment ^ flipmask);
    return drawText((tic_core*)memory, &font_face, text, x, y, w, h, fixed, mapping, scale, alt);
}

s32 tic_api_print(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    u8 mapping[] = {255, color};
    tic_tilesheet font_face = getTileSheetFromSegment(memory, 1);

    const tic_font_data* font = alt ? &memory->ram->font.alt : &memory->ram->font.regular;
    s32 width = font->width;

    // Compatibility : print uses reduced width for non-fixed space
    if(!fixed) width -= 2;
    return drawText((tic_core*)memory, &font_face, text, x, y, width, font->height, fixed, mapping, scale, alt);
}

void tic_api_spr(tic_mem* memory, s32 index, s32 x, s32 y, s32 w, s32 h, u8* trans_colors, u8 trans_count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    drawSprite((tic_core*)memory, index, x, y, w, h, trans_colors, trans_count, scale, flip, rotate);
}

static inline u8* getFlag(tic_mem* memory, s32 index, u8 flag)
{
    static u8 stub = 0;
    if(index >= TIC_FLAGS || flag >= BITS_IN_BYTE)
        return &stub;

    return memory->ram->flags.data + index;
}

bool tic_api_fget(tic_mem* memory, s32 index, u8 flag)
{
    return *getFlag(memory, index, flag) & (1 << flag);
}

void tic_api_fset(tic_mem* memory, s32 index, u8 flag, bool value)
{
    if(value)
        *getFlag(memory, index, flag) |= (1 << flag);
    else
        *getFlag(memory, index, flag) &= ~(1 << flag);
}

u8 tic_api_pix(tic_mem* memory, s32 x, s32 y, u8 color, bool get)
{
    tic_core* core = (tic_core*)memory;

    if(get) return getPixel(core, x, y);

    setPixel(core, x, y, mapColor(memory, color));
    return 0;
}

void tic_api_rectb(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    tic_core* core = (tic_core*)memory;

    drawRectBorder(core, x, y, width, height, mapColor(memory, color));
}

static struct
{
    s16 Left[TIC80_HEIGHT];
    s16 Right[TIC80_HEIGHT];
} SidesBuffer;

static void initSidesBuffer()
{
    for(s32 i = 0; i < COUNT_OF(SidesBuffer.Left); i++)
        SidesBuffer.Left[i] = TIC80_WIDTH, SidesBuffer.Right[i] = -1;
}

static void setSidePixel(s32 x, s32 y)
{
    if(y >= 0 && y < TIC80_HEIGHT)
    {
        if(x < SidesBuffer.Left[y]) SidesBuffer.Left[y] = (s16)(x);
        if(x > SidesBuffer.Right[y]) SidesBuffer.Right[y] = (s16)(x);
    }
}

static void drawEllipse(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color, PixelFunc pix)
{
    if(x0 > x1 || y0 > y1)
        return;

    s64 a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;      /* values of diameter */
    s64 dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
    s64 err = dx + dy + b1 * a * a, e2;                      /* error of 1.step */

    if(x0 > x1)
    {
        x0 = x1;
        x1 += (s32)a;
    } /* if called with swapped pos32s */
    if(y0 > y1) y0 = y1; /* .. exchange them */
    y0 += (s32)((b + 1) / 2);
    y1 = (s32)(y0 - b1); /* starting pixel */
    a *= 8 * a;
    b1 = 8 * b * b;

    do
    {
        pix(memory, x1, y0, color); /*   I. Quadrant */
        pix(memory, x0, y0, color); /*  II. Quadrant */
        pix(memory, x0, y1, color); /* III. Quadrant */
        pix(memory, x1, y1, color); /*  IV. Quadrant */
        e2 = 2 * err;
        if(e2 <= dy)
        {
            y0++;
            y1--;
            err += dy += a;
        } /* y step */
        if(e2 >= dx || 2 * err > dy)
        {
            x0++;
            x1--;
            err += dx += b1;
        } /* x step */
    } while(x0 <= x1);

    while(y0 - y1 < b)
    {                                   /* too early stop of flat ellipses a=1 */
        pix(memory, x0 - 1, y0, color); /* -> finish tip of ellipse */
        pix(memory, x1 + 1, y0++, color);
        pix(memory, x0 - 1, y1, color);
        pix(memory, x1 + 1, y1--, color);
    }
}

static void drawSuperEllipseCapsule(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, int radius, float exponent, u8 color, PixelFunc pix)
{
    if(x0 > x1 || y0 > y1)
        return;

    s32 w = abs(x1 - x0), h = abs(y1 - y0);

    // non positive radius should default to half the smallest dimension
    if(radius <= 0) radius = MIN(w, h) / 2;
    // radius should never be more than half the smallest dimension
    if(radius > MIN(w, h) / 2) radius = MIN(w, h) / 2;

    // fill the edges for a tall superellipse capsule
    for(int yy = y0 + radius; yy < y1 - radius; yy++)
    {
        pix(memory, x0, yy, color);
        pix(memory, x1, yy, color);
    }

    // exponent <= 0 should default to a circle, otherwise clamp it to sane range
    if(exponent <= 0)
        exponent = SUPERELLIPSE_CIRCLE_EXPONENT;
    else
        exponent = (float)CLAMP(exponent, SUPERELLIPSE_MIN_EXPONENT, SUPERELLIPSE_MAX_EXPONENT);

    float r = (float)radius;
    float rn = powf(r, exponent);
    int x = 0, y = radius;

    x0 += radius;
    x1 -= radius;
    y0 += radius;
    y1 -= radius;

    while(x <= y)
    {
        pix(memory, x0 - x, y0 - y, color);
        pix(memory, x0 - x, y1 + y, color);
        pix(memory, x1 + x, y0 - y, color);
        pix(memory, x1 + x, y1 + y, color);

        if(x != y)
        {
            pix(memory, x0 - y, y0 - x, color);
            pix(memory, x0 - y, y1 + x, color);
            pix(memory, x1 + y, y0 - x, color);
            pix(memory, x1 + y, y1 + x, color);
        }

        float fx = (float)x;
        float fy = (float)y;

        float fxp0 = powf(fx, exponent);
        float fxp1 = powf(fx + 1.0f, exponent);
        float fyp0 = powf(fy, exponent);
        float fyp1 = powf(fy - 1.0f, exponent);

        float eh = fabsf(fxp1 + fyp0 - rn);
        float ed = fabsf(fxp1 + fyp1 - rn);
        float ev = fabsf(fxp0 + fyp1 - rn);

        if(eh <= ed && eh <= ev && x < y - 1)
            x++;
        else if(ev <= ed && x < y - 1)
            y--;
        else
        {
            x++;
            y--;
        }
    }
}

static void setElliPixel(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setPixel((tic_core*)tic, x, y, color);
}

static void setElliSide(tic_mem* tic, s32 x, s32 y, u8 color)
{
    (void)color;
    (void)tic;
    setSidePixel(x, y);
}

static void setElliBoth(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setPixel((tic_core*)tic, x, y, color);
    setSidePixel(x, y);
}

static void drawSidesBuffer(tic_mem* memory, s32 y0, s32 y1, u8 color)
{

    tic_core* core = (tic_core*)memory;
    s32 yt = MAX(core->state.clip.t, y0);
    s32 yb = MIN(core->state.clip.b, y1 + 1);
    for(s32 y = yt; y < yb; y++)
    {
        s32 xl = MAX(SidesBuffer.Left[y], core->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y] + 1, core->state.clip.r);

        drawHLine(core, xl, y, xr - xl, color);
    }
}

void tic_api_circ(tic_mem* memory, s32 x, s32 y, s32 r, u8 color)
{
    initSidesBuffer();
    drawEllipse(memory, x - r, y - r, x + r, y + r, 0, setElliSide);
    drawSidesBuffer(memory, y - r, y + r + 1, mapColor(memory, color));
}

void tic_api_circb(tic_mem* memory, s32 x, s32 y, s32 r, u8 color)
{
    drawEllipse(memory, x - r, y - r, x + r, y + r, mapColor(memory, color), setElliPixel);
}

void tic_api_elli(tic_mem* memory, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    initSidesBuffer();
    drawEllipse(memory, x - a, y - b, x + a, y + b, 0, setElliSide);
    drawSidesBuffer(memory, y - b, y + b + 1, mapColor(memory, color));
}

void tic_api_ellib(tic_mem* memory, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    drawEllipse(memory, x - a, y - b, x + a, y + b, mapColor(memory, color), setElliPixel);
}

void tic_api_rrect(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color, s32 radius, float exponent)
{
    // zero radius should just defer to a regular rect
    if(radius == 0)
    {
        tic_api_rect(memory, x, y, width, height, color);
        return;
    }

    initSidesBuffer();
    drawSuperEllipseCapsule(memory, x, y, x + width, y + height, radius, exponent, 0, setElliSide);
    drawSidesBuffer(memory, y, y + height + 1, mapColor(memory, color));
}

void tic_api_rrectb(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color, s32 radius, float exponent)
{
    // zero radius should just defer to a regular rect
    if(radius == 0)
    {
        tic_api_rectb(memory, x, y, width, height, color);
        return;
    }

    initSidesBuffer();
    color = mapColor(memory, color);
    drawSuperEllipseCapsule(memory, x, y, x + width, y + height, radius, exponent, color, setElliBoth);
    drawSidesBuffer(memory, y, y, color);
    drawSidesBuffer(memory, y + height, y + height, color);
}

static inline float initLine(float* x0, float* x1, float* y0, float* y1)
{
    if(*y0 > *y1)
    {
        SWAP(*x0, *x1, float);
        SWAP(*y0, *y1, float);
    }

    float t = (*x1 - *x0) / (*y1 - *y0);

    if(*y0 < 0) *x0 -= *y0 * t, *y0 = 0;
    if(*y1 > TIC80_WIDTH) *x1 += (TIC80_WIDTH - *y0) * t, *y1 = TIC80_WIDTH;

    return t;
}

static void drawLine(tic_mem* tic, float x0, float y0, float x1, float y1, u8 color)
{
    // the counter steps by one and the fraction it starts at is the line's own:
    // an integer counter would draw a different last pixel
    if(fabs(x0 - x1) < fabs(y0 - y1))
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter): a raster counter, exact at one a step
        for(float t = initLine(&x0, &x1, &y0, &y1); y0 < y1; y0++, x0 += t)
            setPixel((tic_core*)tic, (s32)(x0), (s32)y0, color);
    else
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter): a raster counter, exact at one a step
        for(float t = initLine(&y0, &y1, &x0, &x1); x0 < x1; x0++, y0 += t)
            setPixel((tic_core*)tic, (s32)(x0), (s32)y0, color);

    setPixel((tic_core*)tic, (s32)(x1), (s32)y1, color);
}

// Queue frame for floodFill.
// Filled horizontal segment of scanline y for xl <= x <= xr.
// Parent segment was on line y – dy. dy = 1 or –1.
typedef struct
{
    s32 y;
    s32 xl;
    s32 xr;
    s32 dy;
} FillSegment;

#define FILLQUEUESIZE 400
static struct
{
    FillSegment seg[FILLQUEUESIZE];
    size_t ini;  // index of empty next in
    size_t outi; // index of next out
} fillQueue;

static inline void fillEnqueue(tic_core* tic, s32 y, s32 xl, s32 xr, s32 dy)
{
    size_t nextini = (fillQueue.ini + 1) % FILLQUEUESIZE;
    if(nextini == fillQueue.outi)
        return; // queue full
    if(y + dy < tic->state.clip.t || y + dy >= tic->state.clip.b)
        return;
    FillSegment* qseg = &fillQueue.seg[fillQueue.ini];
    qseg->y = y;
    qseg->xl = xl;
    qseg->xr = xr;
    qseg->dy = dy;
    fillQueue.ini = nextini;
}

static inline bool fillDequeue(s32* y, s32* xl, s32* xr, s32* dy)
{
    if(fillQueue.ini == fillQueue.outi)
        return false; // queue empty
    FillSegment* qseg = &fillQueue.seg[fillQueue.outi];
    *y = qseg->y + qseg->dy;
    *xl = qseg->xl;
    *xr = qseg->xr;
    *dy = qseg->dy;
    fillQueue.outi = (fillQueue.outi + 1) % FILLQUEUESIZE;
    return true;
}

static inline bool floodFillInside(u8 pix, u8 paint, u8 border, u8 original)
{
    return border == 255 ? pix == original : pix != paint && pix != border;
}

// "A Seed Fill Algorithm", Paul S. Heckbert, Graphics Gems, Andrew Glassner
// https://github.com/erich666/GraphicsGems/blob/master/gems/SeedFill.c
static void floodFill(tic_core* tic, s32 x, s32 y, u8 color, u8 border)
{
    if(x < tic->state.clip.l || y < tic->state.clip.t || x >= tic->state.clip.r || y >= tic->state.clip.b)
        return;
    u8 ov = screenPixel(tic, x, y);
    if(ov == color || ov == border)
        return;
    fillQueue.ini = fillQueue.outi = 0;
    fillEnqueue(tic, y, x, x, 1);      // needed in some cases
    fillEnqueue(tic, y + 1, x, x, -1); // seed segment
    s32 l, x1, x2, dy;
    while(fillDequeue(&y, &x1, &x2, &dy))
    {
        // segment of scan line y-dy for x1<=x<=x2 was previously filled,
        // now explore adjacent pixels in scan line y
        s32 left = x1;
        while(left >= tic->state.clip.l && floodFillInside(screenPixel(tic, left, y), color, border, ov))
            left--;

        if(left < x1)
            drawHLine(tic, left + 1, y, x1 - left, color);

        if(left >= x1)
        {
            x = x1;
            goto floodFill_skip;
        }

        l = left + 1;
        if(l < x1)
            fillEnqueue(tic, y, l, x1 - 1, -dy); // check leak left
        x = x1 + 1;
        do
        {
            s32 start = x;
            while(x < tic->state.clip.r && floodFillInside(screenPixel(tic, x, y), color, border, ov))
                x++;

            if(x > start)
                drawHLine(tic, start, y, x - start, color);

            fillEnqueue(tic, y, l, x - 1, dy);
            if(x > x2 + 1)
                fillEnqueue(tic, y, x2 + 1, x - 1, -dy); // check leak right
        floodFill_skip:
            for(x++; x <= x2 && !floodFillInside(screenPixel(tic, x, y), color, border, ov); x++)
                ;
            l = x;
        } while(x <= x2);
    }
}

typedef union
{
    struct
    {
        double x, y;
    };

    double d[2];
} Vec2;

typedef union
{
    struct
    {
        double x, y, z;
    };

    double d[3];
} Vec3;

typedef struct
{
    void* data;
    const Vec2* v[3];
    Vec3 w;
} ShaderAttr;

typedef tic_color (*PixelShader)(const ShaderAttr* a, s32 pixel);

static inline double edgeFn(const Vec2* a, const Vec2* b, const Vec2* c)
{
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}

#if defined(_MSC_VER)
#define DRAW_NOINLINE      __declspec(noinline)
#define DRAW_ALWAYS_INLINE __forceinline
#else
#define DRAW_NOINLINE      __attribute__((noinline))
#define DRAW_ALWAYS_INLINE inline __attribute__((always_inline))
#endif

// Forced into the per-texture wrappers so their literal shaders inline.
static DRAW_ALWAYS_INLINE void drawTriBody(tic_mem* tic, const Vec2* v0, const Vec2* v1, const Vec2* v2, PixelShader shader, void* data)
{
    ShaderAttr a = {.data = data, .v = {v0, v1, v2}};

    tic_core* core = (tic_core*)tic;
    const struct ClipRect* clip = &core->state.clip;

    tic_point min = {(s32)(floor(MIN3(a.v[0]->x, a.v[1]->x, a.v[2]->x))), (s32)(floor(MIN3(a.v[0]->y, a.v[1]->y, a.v[2]->y)))};
    tic_point max = {(s32)(ceil(MAX3(a.v[0]->x, a.v[1]->x, a.v[2]->x))), (s32)(ceil(MAX3(a.v[0]->y, a.v[1]->y, a.v[2]->y)))};

    min.x = MAX(min.x, clip->l);
    min.y = MAX(min.y, clip->t);
    max.x = MIN(max.x, clip->r);
    max.y = MIN(max.y, clip->b);

    if(min.x >= max.x || min.y >= max.y) return;

    double area = edgeFn(a.v[0], a.v[1], a.v[2]);
    if((s32)floor(area) == 0) return;
    if(area < 0.0)
    {
        SWAP(a.v[1], a.v[2], const Vec2*);
        area = -area;
    }

    Vec2 d[3];
    Vec3 s;

    for(s32 i = 0; i != COUNT_OF(s.d); ++i)
    {
        // pixel center
        const double Center = 0.5 - FLT_EPSILON;
        Vec2 p = {{min.x + Center, min.y + Center}};

        s32 c = (i + 1) % 3, n = (i + 2) % 3;

        d[i].x = (a.v[c]->y - a.v[n]->y) / area;
        d[i].y = (a.v[n]->x - a.v[c]->x) / area;
        s.d[i] = edgeFn(a.v[c], a.v[n], &p) / area;
    }

    u8* screen = screenData(core);

    for(s32 y = min.y, start = min.y * TIC80_WIDTH + min.x; y < max.y; ++y, start += TIC80_WIDTH)
    {
        for(s32 i = 0; i != COUNT_OF(a.w.d); ++i)
            a.w.d[i] = s.d[i];

        for(s32 x = min.x, pixel = start; x < max.x; ++x, ++pixel)
        {
            if(a.w.x > -DBL_EPSILON && a.w.y > -DBL_EPSILON && a.w.z > -DBL_EPSILON)
            {
                u8 color = shader(&a, pixel);
                if(color != TRANSPARENT_COLOR)
                    tic_tool_poke4(screen, (u32)pixel, color);
            }

            for(s32 i = 0; i != COUNT_OF(a.w.d); ++i)
                a.w.d[i] += d[i].x;
        }

        for(s32 i = 0; i != COUNT_OF(s.d); ++i)
            s.d[i] += d[i].y;
    }
}

static void drawTri(tic_mem* tic, const Vec2* v0, const Vec2* v1, const Vec2* v2, PixelShader shader, void* data)
{
    drawTriBody(tic, v0, v1, v2, shader, data);
}

static tic_color triColorShader(const ShaderAttr* a, s32 pixel)
{
    (void)pixel;
    return *(u8*)a->data;
}

// A scanline fill for the flat triangle: each row's inside interval is the
// min/max of the edge crossings at the pixel centres, filled with drawHLine.
static void drawTriSpans(tic_mem* tic, const Vec2* v[3], u8 color)
{
    tic_core* core = (tic_core*)tic;
    const struct ClipRect* clip = &core->state.clip;

    double area = (v[1]->x - v[0]->x) * (v[2]->y - v[0]->y) - (v[1]->y - v[0]->y) * (v[2]->x - v[0]->x);

    if((s32)floor(area) == 0)
        return;

    double miny = v[0]->y, maxy = v[0]->y;

    for(s32 i = 1; i < 3; i++)
    {
        if(v[i]->y < miny) miny = v[i]->y;
        if(v[i]->y > maxy) maxy = v[i]->y;
    }

    s32 top = (s32)floor(miny);
    s32 bottom = (s32)ceil(maxy);

    if(top < clip->t) top = clip->t;
    if(bottom > clip->b) bottom = clip->b;

    if(top >= bottom)
        return;

    const double center = 0.5 - FLT_EPSILON;

    for(s32 y = top; y < bottom; y++)
    {
        double yc = y + center;
        double lo = 0.0, hi = 0.0;
        bool hit = false;

        for(s32 i = 0; i < 3; i++)
        {
            const Vec2* a = v[i];
            const Vec2* b = v[(i + 1) % 3];

            if(a->y == b->y)
                continue;

            double t = (yc - a->y) / (b->y - a->y);

            if(t < 0.0 || t > 1.0)
                continue;

            double x = a->x + t * (b->x - a->x);

            if(!hit)
            {
                lo = hi = x;
                hit = true;
            }
            else
            {
                if(x < lo) lo = x;
                if(x > hi) hi = x;
            }
        }

        if(!hit)
            continue;

        s32 left = (s32)ceil(lo - center);
        s32 right = (s32)floor(hi - center) + 1;

        if(left < clip->l) left = clip->l;
        if(right > clip->r) right = clip->r;

        if(left < right)
            drawHLine(core, left, y, right - left, color);
    }
}

void tic_api_tri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    color = mapColor(tic, color);

    if(!isfinite(x1) || !isfinite(y1) || !isfinite(x2) || !isfinite(y2) || !isfinite(x3) || !isfinite(y3) ||
       fabsf(x1) > 32768 || fabsf(y1) > 32768 || fabsf(x2) > 32768 || fabsf(y2) > 32768 || fabsf(x3) > 32768 || fabsf(y3) > 32768)
    {
        drawTri(tic,
                &(Vec2){{x1, y1}},
                &(Vec2){{x2, y2}},
                &(Vec2){{x3, y3}},
                triColorShader, &color);
        return;
    }

    const Vec2* v[3] = {&(Vec2){{x1, y1}}, &(Vec2){{x2, y2}}, &(Vec2){{x3, y3}}};
    drawTriSpans(tic, v, color);
}

void tic_api_trib(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{

    u8 finalColor = mapColor(tic, color);

    drawLine(tic, x1, y1, x2, y2, finalColor);
    drawLine(tic, x2, y2, x3, y3, finalColor);
    drawLine(tic, x3, y3, x1, y1, finalColor);
}

typedef struct
{
    Vec2 _;
    Vec3 d;
} TexVert;

typedef struct
{
    tic_tilesheet sheet;
    u8* mapping;
    const u8* map;
    const tic_vram* vram;
    bool depth;
    bool tile4;
    u32 bankOrig;
} TexData;

static inline bool shaderStart(const ShaderAttr* a, Vec3* vars, s32 pixel)
{
    TexData* data = a->data;

    vars->x = vars->y = vars->z = 0;

    if(data->depth)
    {
        for(s32 i = 0; i != COUNT_OF(a->v); ++i)
        {
            const TexVert* t = (TexVert*)a->v[i];
            vars->z += a->w.d[i] * t->d.z;
        }

        if(ZBuffer[pixel] < vars->z)
            ;
        else
            return false;
    }
    for(s32 i = 0; i != COUNT_OF(a->v); ++i)
    {
        const TexVert* t = (TexVert*)a->v[i];
        vars->x += a->w.d[i] * t->d.x;
        vars->y += a->w.d[i] * t->d.y;
    }

    if(data->depth)
        vars->x /= vars->z,
            vars->y /= vars->z;

    return true;
}

static inline tic_color shaderEnd(const ShaderAttr* a, const Vec3* vars, s32 pixel, tic_color color)
{
    TexData* data = a->data;

    if(data->depth && color != TRANSPARENT_COLOR)
        ZBuffer[pixel] = vars->z;

    return color;
}

// Kept out of line: an inlined modulo pushes the shaders past the inliner.
static DRAW_NOINLINE s32 mapWrapSlow(s32 value, s32 size)
{
    return tic_modulo(value, size);
}

static inline s32 mapWrap(s32 value, s32 size)
{
    return (u32)value < (u32)size ? value : mapWrapSlow(value, size);
}

static tic_color triTexMapShader(const ShaderAttr* a, s32 pixel)
{
    TexData* data = a->data;

    Vec3 vars;
    if(!shaderStart(a, &vars, pixel))
        return TRANSPARENT_COLOR;

    enum
    {
        MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE,
        MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE,
        WMask = TIC_SPRITESIZE - 1,
        HMask = TIC_SPRITESIZE - 1
    };

    s32 iu = mapWrap((s32)(floor(vars.x)), MapWidth);
    s32 iv = mapWrap((s32)(floor(vars.y)), MapHeight);

    u8 idx = data->map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];
    tic_tileptr tile = tic_tilesheet_gettile(&data->sheet, idx, true);

    return shaderEnd(a, &vars, pixel, data->mapping[tic_tilesheet_gettilepix(&tile, iu & WMask, iv & HMask)]);
}

static tic_color triTexTileShader(const ShaderAttr* a, s32 pixel)
{
    TexData* data = a->data;

    Vec3 vars;
    if(!shaderStart(a, &vars, pixel))
        return TRANSPARENT_COLOR;

    enum
    {
        HMask = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS - 1
    };

    return shaderEnd(a, &vars, pixel, data->mapping[tic_tilesheet_getpix(&data->sheet, (s32)floor(vars.x) & (TIC_SPRITESHEET_SIZE * data->sheet.segment->nb_pages - 1), (s32)floor(vars.y) & HMask)]);
}

static tic_color triTexVbankShader(const ShaderAttr* a, s32 pixel)
{
    TexData* data = a->data;

    Vec3 vars;
    if(!shaderStart(a, &vars, pixel))
        return TRANSPARENT_COLOR;

    s32 iu = mapWrap((s32)(floor(vars.x)), TIC80_WIDTH);
    s32 iv = mapWrap((s32)(floor(vars.y)), TIC80_HEIGHT);

    return shaderEnd(a, &vars, pixel, data->mapping[tic_tool_peek4(data->vram->data, iv * TIC80_WIDTH + iu)]);
}

static inline void shaderUV(const ShaderAttr* a, Vec3* vars)
{
    vars->x = vars->y = 0;

    for(s32 i = 0; i != COUNT_OF(a->v); ++i)
    {
        const TexVert* t = (TexVert*)a->v[i];
        vars->x += a->w.d[i] * t->d.x;
        vars->y += a->w.d[i] * t->d.y;
    }
}

static tic_color triTexTileShaderFlat(const ShaderAttr* a, s32 pixel)
{
    (void)pixel;

    TexData* data = a->data;

    Vec3 vars;
    shaderUV(a, &vars);

    s32 u = (s32)floor(vars.x);
    s32 v = (s32)floor(vars.y);

    return data->mapping[tic_tilesheet_getpix(&data->sheet, u & (TIC_SPRITESHEET_SIZE * data->sheet.segment->nb_pages - 1), v & (TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS - 1))];
}

static DRAW_NOINLINE tic_color mapPixelGeneric(TexData* data, s32 idx, s32 iu, s32 iv)
{
    enum
    {
        WMask = TIC_SPRITESIZE - 1,
        HMask = TIC_SPRITESIZE - 1
    };

    tic_tileptr tile = tic_tilesheet_gettile(&data->sheet, idx, true);

    return data->mapping[tic_tilesheet_gettilepix(&tile, iu & WMask, iv & HMask)];
}

static tic_color triTexMapShaderFlat(const ShaderAttr* a, s32 pixel)
{
    (void)pixel;

    TexData* data = a->data;

    Vec3 vars;
    shaderUV(a, &vars);

    enum
    {
        MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE,
        MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE,
        WMask = TIC_SPRITESIZE - 1,
        HMask = TIC_SPRITESIZE - 1
    };

    s32 iu = mapWrap((s32)(floor(vars.x)), MapWidth);
    s32 iv = mapWrap((s32)(floor(vars.y)), MapHeight);

    u8 idx = data->map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];

    if(!data->tile4)
        return mapPixelGeneric(data, idx, iu, iv);

    u32 tile = data->bankOrig * 256 + ((u32)idx >> 4) * 16 + ((u32)idx & 15);
    u32 pix = ((u32)iu & WMask) + ((u32)iv & HMask) * 8;

    return data->mapping[tic_tool_peek4(data->sheet.ptr, tile * 64 + pix)];
}

static tic_color triTexVbankShaderFlat(const ShaderAttr* a, s32 pixel)
{
    (void)pixel;

    TexData* data = a->data;

    Vec3 vars;
    shaderUV(a, &vars);

    s32 iu = mapWrap((s32)(floor(vars.x)), TIC80_WIDTH);
    s32 iv = mapWrap((s32)(floor(vars.y)), TIC80_HEIGHT);

    return data->mapping[tic_tool_peek4(data->vram->data, iv * TIC80_WIDTH + iu)];
}

// One call site per flat shader; a merged loop calls the shader indirectly.
static DRAW_NOINLINE void drawTtriMapFlat(tic_mem* tic, const TexVert* t, TexData* data)
{
    drawTriBody(tic, (const Vec2*)&t[0], (const Vec2*)&t[1], (const Vec2*)&t[2], triTexMapShaderFlat, data);
}

static DRAW_NOINLINE void drawTtriVbankFlat(tic_mem* tic, const TexVert* t, TexData* data)
{
    drawTriBody(tic, (const Vec2*)&t[0], (const Vec2*)&t[1], (const Vec2*)&t[2], triTexVbankShaderFlat, data);
}

// The scanline span fill for the flat textured triangle: one inside
// interval per row, with the UV stepped along it.
static bool drawTtriSpans(tic_mem* tic, const TexVert t[3], TexData* data, u32 src)
{
    tic_core* core = (tic_core*)tic;
    const struct ClipRect* clip = &core->state.clip;

    const TexVert* v[3] = {&t[0], &t[1], &t[2]};

    for(s32 i = 0; i < 3; i++)
        if(!isfinite(v[i]->_.x) || !isfinite(v[i]->_.y) || fabsf((float)v[i]->_.x) > 32768 || fabsf((float)v[i]->_.y) > 32768)
            return false;

    double area = (v[1]->_.x - v[0]->_.x) * (v[2]->_.y - v[0]->_.y) - (v[1]->_.y - v[0]->_.y) * (v[2]->_.x - v[0]->_.x);

    if((s32)floor(area) == 0)
        return false;

    if(area < 0.0)
    {
        const TexVert* tmp = v[1];
        v[1] = v[2];
        v[2] = tmp;
        area = -area;
    }

    if(src != tic_tiles_texture && src != tic_map_texture && src != tic_vbank_texture)
        return false;

    if(src == tic_map_texture && !data->tile4)
        return false;

    s32 maskU = 0, maskV = 0;

    if(src == tic_tiles_texture)
    {
        maskU = (s32)(TIC_SPRITESHEET_SIZE * data->sheet.segment->nb_pages - 1);
        maskV = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS - 1;
    }

    double miny = v[0]->_.y, maxy = v[0]->_.y;

    for(s32 i = 1; i < 3; i++)
    {
        if(v[i]->_.y < miny) miny = v[i]->_.y;
        if(v[i]->_.y > maxy) maxy = v[i]->_.y;
    }

    s32 top = (s32)floor(miny);
    s32 bottom = (s32)ceil(maxy);

    if(top < clip->t) top = clip->t;
    if(bottom > clip->b) bottom = clip->b;

    if(top >= bottom)
        return true;

    const double center = 0.5 - FLT_EPSILON;
    u8* screen = screenData(core);

    for(s32 y = top; y < bottom; y++)
    {
        double yc = y + center;
        double lo = 0.0, hi = 0.0;
        bool hit = false;

        for(s32 i = 0; i < 3; i++)
        {
            const Vec2* a = &v[i]->_;
            const Vec2* b = &v[(i + 1) % 3]->_;

            if(a->y == b->y)
                continue;

            double k = (yc - a->y) / (b->y - a->y);

            if(k < 0.0 || k > 1.0)
                continue;

            double x = a->x + k * (b->x - a->x);

            if(!hit)
            {
                lo = hi = x;
                hit = true;
            }
            else
            {
                if(x < lo) lo = x;
                if(x > hi) hi = x;
            }
        }

        if(!hit)
            continue;

        s32 left = (s32)ceil(lo - center);
        s32 right = (s32)floor(hi - center) + 1;

        if(left < clip->l) left = clip->l;
        if(right > clip->r) right = clip->r;

        if(left >= right)
            continue;

        double px = left + center;
        double w[3], dw[3];

        for(s32 i = 0; i < 3; i++)
        {
            const Vec2* a = &v[(i + 1) % 3]->_;
            const Vec2* b = &v[(i + 2) % 3]->_;

            w[i] = ((b->x - a->x) * (yc - a->y) - (b->y - a->y) * (px - a->x)) / area;
            dw[i] = (a->y - b->y) / area;
        }

        double u = w[0] * v[0]->d.x + w[1] * v[1]->d.x + w[2] * v[2]->d.x;
        double vv = w[0] * v[0]->d.y + w[1] * v[1]->d.y + w[2] * v[2]->d.y;
        double du = dw[0] * v[0]->d.x + dw[1] * v[1]->d.x + dw[2] * v[2]->d.x;
        double dv = dw[0] * v[0]->d.y + dw[1] * v[1]->d.y + dw[2] * v[2]->d.y;

        enum
        {
            MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE,
            MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE
        };

        switch(src)
        {
            case tic_tiles_texture:
                if(data->sheet.segment->peek == tic_tool_peek4 && data->sheet.segment->tile_width == TIC_SPRITESIZE &&
                   data->sheet.segment->nb_pages == 1 && data->sheet.segment->page_orig == 0)
                {
                    u32 bankOrig = data->sheet.segment->bank_orig;

                    for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                    {
                        s32 iu = (s32)floor(u) & maskU;
                        s32 iv = (s32)floor(vv) & maskV;
                        u32 tile = ((((iv >> 7) + bankOrig) & 1) << 8) + (((iv & 127) >> 3) << 4) + ((iu & 127) >> 3);
                        u32 pix = ((u32)iu & 7) + ((u32)iv & 7) * 8;
                        u8 color = data->mapping[tic_tool_peek4(data->sheet.ptr, tile * 64 + pix)];

                        if(color != TRANSPARENT_COLOR)
                            tic_tool_poke4(screen, (u32)pixel, color);
                    }
                }
                else if(data->sheet.segment->peek == tic_tool_peek2 && data->sheet.segment->tile_width == TIC_SPRITESIZE * 2 &&
                        data->sheet.segment->nb_pages == 2)
                {
                    u32 bankOrig = data->sheet.segment->bank_orig;
                    u32 pageOrig = data->sheet.segment->page_orig;

                    for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                    {
                        s32 iu = (s32)floor(u) & maskU;
                        s32 iv = (s32)floor(vv) & maskV;
                        u32 tile = ((((iv >> 7) + bankOrig) & 1) << 8) + (((iv & 127) >> 3) << 4) +
                                   (((((iu >> 7) + (s32)pageOrig) & 1)) << 3) + ((iu & 127) >> 4);
                        u32 pix = ((u32)iu & 15) + ((u32)iv & 7) * 16;
                        u8 color = data->mapping[tic_tool_peek2(data->sheet.ptr, tile * 128 + pix)];

                        if(color != TRANSPARENT_COLOR)
                            tic_tool_poke4(screen, (u32)pixel, color);
                    }
                }
                else if(data->sheet.segment->peek == tic_tool_peek1 && data->sheet.segment->tile_width == TIC_SPRITESIZE * 4 &&
                        data->sheet.segment->nb_pages == 4)
                {
                    u32 bankOrig = data->sheet.segment->bank_orig;
                    u32 pageOrig = data->sheet.segment->page_orig;

                    for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                    {
                        s32 iu = (s32)floor(u) & maskU;
                        s32 iv = (s32)floor(vv) & maskV;
                        u32 tile = ((((iv >> 7) + bankOrig) & 1) << 8) + (((iv & 127) >> 3) << 4) +
                                   (((((iu >> 7) + (s32)pageOrig) & 3)) << 2) + ((iu & 127) >> 5);
                        u32 pix = ((u32)iu & 31) + ((u32)iv & 7) * 32;
                        u8 color = data->mapping[tic_tool_peek1(data->sheet.ptr, tile * 256 + pix)];

                        if(color != TRANSPARENT_COLOR)
                            tic_tool_poke4(screen, (u32)pixel, color);
                    }
                }
                else
                {
                    for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                    {
                        s32 iu = (s32)floor(u) & maskU;
                        s32 iv = (s32)floor(vv) & maskV;
                        u8 color = data->mapping[tic_tilesheet_getpix(&data->sheet, iu, iv)];

                        if(color != TRANSPARENT_COLOR)
                            tic_tool_poke4(screen, (u32)pixel, color);
                    }
                }
                break;

            case tic_map_texture:
                for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                {
                    s32 iu = mapWrap((s32)floor(u), MapWidth);
                    s32 iv = mapWrap((s32)floor(vv), MapHeight);
                    u8 idx = data->map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];
                    u32 tile = data->bankOrig * 256 + ((u32)idx >> 4) * 16 + ((u32)idx & 15);
                    u32 pix = ((u32)iu & 7) + ((u32)iv & 7) * 8;
                    u8 color = data->mapping[tic_tool_peek4(data->sheet.ptr, tile * 64 + pix)];

                    if(color != TRANSPARENT_COLOR)
                        tic_tool_poke4(screen, (u32)pixel, color);
                }
                break;

            case tic_vbank_texture:
                for(s32 x = left, pixel = y * TIC80_WIDTH + left; x < right; x++, pixel++, u += du, vv += dv)
                {
                    s32 iu = mapWrap((s32)floor(u), TIC80_WIDTH);
                    s32 iv = mapWrap((s32)floor(vv), TIC80_HEIGHT);
                    u8 color = data->mapping[tic_tool_peek4(data->vram->data, iv * TIC80_WIDTH + iu)];

                    if(color != TRANSPARENT_COLOR)
                        tic_tool_poke4(screen, (u32)pixel, color);
                }
                break;

            default:
                break;
        }
    }

    return true;
}

void tic_api_ttri(tic_mem* tic,
                  float x1, float y1,
                  float x2, float y2,
                  float x3, float y3,
                  float u1, float v1,
                  float u2, float v2,
                  float u3, float v3,
                  tic_texture_src texsrc, u8* colors, s32 count,
                  float z1, float z2, float z3, bool depth)
{
    // do not use depth if user passed z=0.0
    if(z1 < FLT_EPSILON || z2 < FLT_EPSILON || z3 < FLT_EPSILON)
        depth = false;

    TexData texData =
        {
            .sheet = getTileSheetFromSegment(tic, tic->ram->vram.blit.segment),
            .mapping = getPalette(tic, colors, (u8)(count)),
            .map = tic->ram->map.data,
            .vram = &((tic_core*)tic)->state.vbank.mem,
            .depth = depth,
        };

    const tic_blit_segment* sheetSegment = texData.sheet.segment;
    texData.tile4 = sheetSegment->ptr_size == sizeof(tic_tile) && sheetSegment->tile_width == TIC_SPRITESIZE &&
                    sheetSegment->nb_pages == 1 && sheetSegment->page_orig == 0 &&
                    sheetSegment->sheet_width == TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE;
    texData.bankOrig = sheetSegment->bank_orig;

    TexVert t[] =
        {
            {._ = {{x1, y1}}, .d = {{u1, v1, z1}}},
            {._ = {{x2, y2}}, .d = {{u2, v2, z2}}},
            {._ = {{x3, y3}}, .d = {{u3, v3, z3}}},
        };

    if(depth)
        for(s32 i = 0; i != COUNT_OF(t); ++i)
            t[i].d.x /= t[i].d.z,
                t[i].d.y /= t[i].d.z,
                t[i].d.z = 1.0 / t[i].d.z;

    // The enum is signed where the compiler keeps int for it (MSVC does), so
    // the cast is what turns a negative source into an out-of-range one.
    const u32 src = (u32)texsrc;

    if(src >= (u32)(tic_vbank_texture + 1))
        return;

#define DRAW_TTRI(SHADER) drawTri(tic, (const Vec2*)&t[0], (const Vec2*)&t[1], (const Vec2*)&t[2], SHADER, &texData)

    if(depth)
    {
        switch(src)
        {
            case tic_tiles_texture: DRAW_TTRI(triTexTileShader); break;
            case tic_map_texture: DRAW_TTRI(triTexMapShader); break;
            case tic_vbank_texture: DRAW_TTRI(triTexVbankShader); break;
            default: break;
        }
    }
    else if(!drawTtriSpans(tic, t, &texData, src))
    {
        switch(src)
        {
            case tic_tiles_texture: DRAW_TTRI(triTexTileShaderFlat); break;
            case tic_map_texture: drawTtriMapFlat(tic, t, &texData); break;
            case tic_vbank_texture: drawTtriVbankFlat(tic, t, &texData); break;
            default: break;
        }
    }

#undef DRAW_TTRI
}

void tic_api_map(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, u8 count, s32 scale, RemapFunc remap, void* data)
{
    drawMap((tic_core*)memory, &memory->ram->map, x, y, width, height, sx, sy, colors, count, scale, remap, data);
}

void tic_api_mset(tic_mem* memory, s32 x, s32 y, u8 value)
{
    if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

    tic_map* src = &memory->ram->map;
    *(src->data + y * TIC_MAP_WIDTH + x) = value;
}

u8 tic_api_mget(tic_mem* memory, s32 x, s32 y)
{
    if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;

    const tic_map* src = &memory->ram->map;
    return *(src->data + y * TIC_MAP_WIDTH + x);
}

void tic_api_line(tic_mem* memory, float x0, float y0, float x1, float y1, u8 color)
{
    tic_core* core = (tic_core*)memory;
    u8 mapped = mapColor(memory, color);

    // Integer endpoints on a row or a column are the write path's row and
    // column fills; anything else keeps the float rasterizer.
    if(isfinite(x0) && isfinite(y0) && isfinite(x1) && isfinite(y1) &&
       fabsf(x0) <= 32768 && fabsf(y0) <= 32768 && fabsf(x1) <= 32768 && fabsf(y1) <= 32768 &&
       x0 == (float)(s32)x0 && y0 == (float)(s32)y0 && x1 == (float)(s32)x1 && y1 == (float)(s32)y1)
    {
        s32 ix0 = (s32)x0, iy0 = (s32)y0, ix1 = (s32)x1, iy1 = (s32)y1;

        if(iy0 == iy1)
        {
            if(ix0 > ix1)
            {
                s32 t = ix0;
                ix0 = ix1;
                ix1 = t;
            }

            if(ix0 < 0) ix0 = 0;
            if(ix1 > TIC80_WIDTH) ix1 = TIC80_WIDTH;

            drawHLine(core, ix0, iy0, ix1 - ix0 + 1, mapped);
            return;
        }

        if(ix0 == ix1)
        {
            if(iy0 > iy1)
            {
                s32 t = iy0;
                iy0 = iy1;
                iy1 = t;
            }

            if(iy0 < 0) iy0 = 0;
            if(iy1 > TIC80_WIDTH) iy1 = TIC80_WIDTH;

            drawVLine(core, ix0, iy0, iy1 - iy0 + 1, mapped);
            return;
        }
    }

    drawLine(memory, x0, y0, x1, y1, mapped);
}

void tic_api_paint(tic_mem* memory, s32 x, s32 y, u8 color, u8 bordercolor)
{
    bordercolor = bordercolor == 255 ? 255 : mapColor(memory, bordercolor);
    floodFill((tic_core*)memory, x, y, mapColor(memory, color), bordercolor);
}

#if defined(BUILD_DEPRECATED)
#include "draw_dep.c"
#endif
