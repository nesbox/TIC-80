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

#define TRANSPARENT_COLOR 255

typedef void(*PixelFunc)(tic_mem* memory, s32 x, s32 y, u8 color);

static tic_tilesheet getTileSheetFromSegment(tic_mem* memory, u8 segment)
{
    u8* src;
    switch (segment) {
    case 0:
    case 1:
        src = (u8*)&memory->ram->font; break;
    default:
        src = (u8*)&memory->ram->tiles.data; break;
    }

    return tic_tilesheet_get(segment, src);
}

static u8* getPalette(tic_mem* tic, u8* colors, u8 count)
{
    static u8 mapping[TIC_PALETTE_SIZE];
    for (s32 i = 0; i < TIC_PALETTE_SIZE; i++) mapping[i] = tic_tool_peek4(tic->ram->vram.mapping, i);
    for (s32 i = 0; i < count; i++) mapping[colors[i]] = TRANSPARENT_COLOR;
    return mapping;
}

static inline u8 mapColor(tic_mem* tic, u8 color)
{
    return tic_tool_peek4(tic->ram->vram.mapping, color & 0xf);
}

static inline void setPixel(tic_core* core, s32 x, s32 y, u8 color)
{
    const tic_vram* vram = &core->memory.ram->vram;

    if (x < core->state.clip.l || y < core->state.clip.t || x >= core->state.clip.r || y >= core->state.clip.b) return;

    tic_api_poke4((tic_mem*)core, y * TIC80_WIDTH + x, color);
}

static inline void setPixelFast(tic_core* core, s32 x, s32 y, u8 color)
{
    // does not do any CLIP checking, the caller needs to do that first
    tic_api_poke4((tic_mem*)core, y * TIC80_WIDTH + x, color);
}

static u8 getPixel(tic_core* core, s32 x, s32 y)
{
    return x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT
        ? 0
        : tic_api_peek4((tic_mem*)core, y * TIC80_WIDTH + x);
}

#define EARLY_CLIP(x, y, width, height) \
    ( \
        (((y)+(height)-1) < core->state.clip.t) \
        || (((x)+(width)-1) < core->state.clip.l) \
        || ((y) >= core->state.clip.b) \
        || ((x) >= core->state.clip.r) \
    )

static void drawHLine(tic_core* core, s32 x, s32 y, s32 width, u8 color)
{
    const tic_vram* vram = &core->memory.ram->vram;

    if (y < core->state.clip.t || core->state.clip.b <= y) return;

    s32 xl = MAX(x, core->state.clip.l);
    s32 xr = MIN(x + width, core->state.clip.r);
    s32 start = y * TIC80_WIDTH;

    for(s32 i = start + xl, end = start + xr; i < end; ++i)
        tic_api_poke4((tic_mem*)core, i, color);
}

static void drawVLine(tic_core* core, s32 x, s32 y, s32 height, u8 color)
{
    const tic_vram* vram = &core->memory.ram->vram;

    if (x < core->state.clip.l || core->state.clip.r <= x) return;

    s32 yl = y < 0 ? 0 : y;
    s32 yr = y + height >= TIC80_HEIGHT ? TIC80_HEIGHT : y + height;

    for (s32 i = yl; i < yr; ++i)
        setPixel(core, x, i, color);
}

static void drawRect(tic_core* core, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for (s32 i = y; i < y + height; ++i)
        drawHLine(core, x, i, width, color);
}

static void drawRectBorder(tic_core* core, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    drawHLine(core, x, y, width, color);
    drawHLine(core, x, y + height - 1, width, color);

    drawVLine(core, x, y, height, color);
    drawVLine(core, x + width - 1, y, height, color);
}

#define DRAW_TILE_BODY(X, Y) do {\
    for(s32 py=sy; py < ey; py++, y++) \
    { \
        s32 xx = x; \
        for(s32 px=sx; px < ex; px++, xx++) \
        { \
            u8 color = mapping[tic_tilesheet_gettilepix(tile, (X), (Y))];\
            if(color != TRANSPARENT_COLOR) setPixelFast(core, xx, y, color); \
        } \
    } \
    } while(0)

#define REVERT(X) (TIC_SPRITESIZE - 1 - (X))

static void drawTile(tic_core* core, tic_tileptr* tile, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    const tic_vram* vram = &core->memory.ram->vram;
    u8* mapping = getPalette(&core->memory, colors, count);

    rotate &= 3;
    u32 orientation = flip & 3;

    if (rotate == tic_90_rotate) orientation ^= 1;
    else if (rotate == tic_180_rotate) orientation ^= 3;
    else if (rotate == tic_270_rotate) orientation ^= 2;
    if (rotate == tic_90_rotate || rotate == tic_270_rotate) orientation |= 4;

    if (scale == 1) {
        // the most common path
        s32 sx, sy, ex, ey;
        sx = core->state.clip.l - x; if (sx < 0) sx = 0;
        sy = core->state.clip.t - y; if (sy < 0) sy = 0;
        ex = core->state.clip.r - x; if (ex > TIC_SPRITESIZE) ex = TIC_SPRITESIZE;
        ey = core->state.clip.b - y; if (ey > TIC_SPRITESIZE) ey = TIC_SPRITESIZE;
        y += sy;
        x += sx;
        switch (orientation) {
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

    if (EARLY_CLIP(x, y, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale)) return;

    for (s32 py = 0; py < TIC_SPRITESIZE; py++, y += scale)
    {
        s32 xx = x;
        for (s32 px = 0; px < TIC_SPRITESIZE; px++, xx += scale)
        {
            s32 ix = orientation & 1 ? TIC_SPRITESIZE - px - 1 : px;
            s32 iy = orientation & 2 ? TIC_SPRITESIZE - py - 1 : py;
            if (orientation & 4) {
                s32 tmp = ix; ix = iy; iy = tmp;
            }
            u8 color = mapping[tic_tilesheet_gettilepix(tile, ix, iy)];
            if (color != TRANSPARENT_COLOR) drawRect(core, xx, y, scale, scale, color);
        }
    }
}

#undef DRAW_TILE_BODY
#undef REVERT

static void drawSprite(tic_core* core, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    const tic_vram* vram = &core->memory.ram->vram;

    if (index < 0)
        return;

    rotate &= 3;
    flip &= 3;

    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram->vram.blit.segment);
    if (w == 1 && h == 1) {
        tic_tileptr tile = tic_tilesheet_gettile(&sheet, index, false);
        drawTile(core, &tile, x, y, colors, count, scale, flip, rotate);
    }
    else
    {
        s32 step = TIC_SPRITESIZE * scale;
        s32 cols = sheet.segment->sheet_width;

        const tic_flip vert_horz_flip = tic_horz_flip | tic_vert_flip;

        if (EARLY_CLIP(x, y, w * step, h * step)) return;

        for (s32 i = 0; i < w; i++)
        {
            for (s32 j = 0; j < h; j++)
            {
                s32 mx = i;
                s32 my = j;

                if (flip == tic_horz_flip || flip == vert_horz_flip) mx = w - 1 - i;
                if (flip == tic_vert_flip || flip == vert_horz_flip) my = h - 1 - j;

                if (rotate == tic_180_rotate)
                {
                    mx = w - 1 - mx;
                    my = h - 1 - my;
                }
                else if (rotate == tic_90_rotate)
                {
                    if (flip == tic_no_flip || flip == vert_horz_flip) my = h - 1 - my;
                    else mx = w - 1 - mx;
                }
                else if (rotate == tic_270_rotate)
                {
                    if (flip == tic_no_flip || flip == vert_horz_flip) mx = w - 1 - mx;
                    else my = h - 1 - my;
                }

                enum { Cols = TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE };


                tic_tileptr tile = tic_tilesheet_gettile(&sheet, index + mx + my * cols, false);
                if (rotate == 0 || rotate == 2)
                    drawTile(core, &tile, x + i * step, y + j * step, colors, count, scale, flip, rotate);
                else
                    drawTile(core, &tile, x + j * step, y + i * step, colors, count, scale, flip, rotate);
            }
        }
    }
}

static void drawMap(tic_core* core, const tic_map* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)
{
    const s32 size = TIC_SPRITESIZE * scale;

    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram->vram.blit.segment);

    for (s32 j = y, jj = sy; j < y + height; j++, jj += size)
        for (s32 i = x, ii = sx; i < x + width; i++, ii += size)
        {
            s32 mi = i;
            s32 mj = j;

            while (mi < 0) mi += TIC_MAP_WIDTH;
            while (mj < 0) mj += TIC_MAP_HEIGHT;
            while (mi >= TIC_MAP_WIDTH) mi -= TIC_MAP_WIDTH;
            while (mj >= TIC_MAP_HEIGHT) mj -= TIC_MAP_HEIGHT;

            s32 index = mi + mj * TIC_MAP_WIDTH;
            RemapResult retile = { *(src->data + index), tic_no_flip, tic_no_rotate };

            if (remap)
                remap(data, mi, mj, &retile);

            tic_tileptr tile = tic_tilesheet_gettile(&sheet, retile.index, true);
            drawTile(core, &tile, ii, jj, colors, count, scale, retile.flip, retile.rotate);
        }
}

static s32 drawChar(tic_core* core, tic_tileptr* font_char, s32 x, s32 y, s32 scale, bool fixed, u8* mapping)
{
    const tic_vram* vram = &core->memory.ram->vram;

    enum { Size = TIC_SPRITESIZE };

    s32 j = 0, start = 0, end = Size;

    if (!fixed) {
        for (s32 i = 0; i < Size; i++) {
            for (j = 0; j < Size; j++)
                if (mapping[tic_tilesheet_gettilepix(font_char, i, j)] != TRANSPARENT_COLOR) break;
            if (j < Size) break; else start++;
        }
        for (s32 i = Size - 1; i >= start; i--) {
            for (j = 0; j < Size; j++)
                if (mapping[tic_tilesheet_gettilepix(font_char, i, j)] != TRANSPARENT_COLOR) break;
            if (j < Size) break; else end--;
        }
    }
    s32 width = end - start;

    if (EARLY_CLIP(x, y, Size * scale, Size * scale)) return width;

    s32 colStart = start, colStep = 1, rowStart = 0, rowStep = 1;

    for (s32 i = 0, col = colStart, xs = x; i < width; i++, col += colStep, xs += scale)
    {
        for (s32 j = 0, row = rowStart, ys = y; j < Size; j++, row += rowStep, ys += scale)
        {
            u8 color = tic_tilesheet_gettilepix(font_char, col, row);
            if (mapping[color] != TRANSPARENT_COLOR)
                drawRect(core, xs, ys, scale, scale, mapping[color]);
        }
    }
    return width;
}

static s32 drawText(tic_core* core, tic_tilesheet* font_face, const char* text, s32 x, s32 y, s32 width, s32 height, bool fixed, u8* mapping, s32 scale, bool alt)
{
    s32 pos = x;
    s32 MAX = x;
    char sym = 0;

    while ((sym = *text++))
    {
        if (sym == '\n')
        {
            if (pos > MAX)
                MAX = pos;

            pos = x;
            y += height * scale;
        }
        else {
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
    tic_vram* vram = &memory->ram->vram;

    core->state.clip.l = x;
    core->state.clip.t = y;
    core->state.clip.r = x + width;
    core->state.clip.b = y + height;

    if (core->state.clip.l < 0) core->state.clip.l = 0;
    if (core->state.clip.t < 0) core->state.clip.t = 0;
    if (core->state.clip.r > TIC80_WIDTH) core->state.clip.r = TIC80_WIDTH;
    if (core->state.clip.b > TIC80_HEIGHT) core->state.clip.b = TIC80_HEIGHT;
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

    static const struct ClipRect EmptyClip = { 0, 0, TIC80_WIDTH, TIC80_HEIGHT };

    color = mapColor(tic, color);

    if (MEMCMP(core->state.clip, EmptyClip))
    {
        memset(&vram->screen, (color & 0xf) | (color << TIC_PALETTE_BPP), sizeof(tic_screen));
        ZEROMEM(ZBuffer);
    }
    else
    {
        for(s32 y = core->state.clip.t, start = y * TIC80_WIDTH; y < core->state.clip.b; ++y, start += TIC80_WIDTH)
            for(s32 x = core->state.clip.l, pixel = start + x; x < core->state.clip.r; ++x, ++pixel)
            {
                tic_api_poke4(tic, pixel, color);
                ZBuffer[pixel] = 0;
            }
    }
}

s32 tic_api_font(tic_mem* memory, const char* text, s32 x, s32 y, u8* trans_colors, u8 trans_count, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    u8* mapping = getPalette(memory, trans_colors, trans_count);

    // Compatibility : flip top and bottom of the spritesheet
    // to preserve tic_api_font's default target
    u8 segment = memory->ram->vram.blit.segment >> 1;
    u8 flipmask = 1; while (segment >>= 1) flipmask <<= 1;

    tic_tilesheet font_face = getTileSheetFromSegment(memory, memory->ram->vram.blit.segment ^ flipmask);
    return drawText((tic_core*)memory, &font_face, text, x, y, w, h, fixed, mapping, scale, alt);
}

s32 tic_api_print(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    u8 mapping[] = { 255, color };
    tic_tilesheet font_face = getTileSheetFromSegment(memory, 1);

    const tic_font_data* font = alt ? &memory->ram->font.alt : &memory->ram->font.regular;
    s32 width = font->width;

    // Compatibility : print uses reduced width for non-fixed space
    if (!fixed) width -= 2;
    return drawText((tic_core*)memory, &font_face, text, x, y, width, font->height, fixed, mapping, scale, alt);
}

void tic_api_spr(tic_mem* memory, s32 index, s32 x, s32 y, s32 w, s32 h, u8* trans_colors, u8 trans_count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    drawSprite((tic_core*)memory, index, x, y, w, h, trans_colors, trans_count, scale, flip, rotate);
}

static inline u8* getFlag(tic_mem* memory, s32 index, u8 flag)
{
    static u8 stub = 0;
    if (index >= TIC_FLAGS || flag >= BITS_IN_BYTE)
        return &stub;

    return memory->ram->flags.data + index;
}

bool tic_api_fget(tic_mem* memory, s32 index, u8 flag)
{
    return *getFlag(memory, index, flag) & (1 << flag);
}

void tic_api_fset(tic_mem* memory, s32 index, u8 flag, bool value)
{
    if (value)
        *getFlag(memory, index, flag) |= (1 << flag);
    else
        *getFlag(memory, index, flag) &= ~(1 << flag);
}

u8 tic_api_pix(tic_mem* memory, s32 x, s32 y, u8 color, bool get)
{
    tic_core* core = (tic_core*)memory;

    if (get) return getPixel(core, x, y);

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
    for (s32 i = 0; i < COUNT_OF(SidesBuffer.Left); i++)
        SidesBuffer.Left[i] = TIC80_WIDTH, SidesBuffer.Right[i] = -1;
}

static void setSidePixel(s32 x, s32 y)
{
    if (y >= 0 && y < TIC80_HEIGHT)
    {
        if (x < SidesBuffer.Left[y]) SidesBuffer.Left[y] = x;
        if (x > SidesBuffer.Right[y]) SidesBuffer.Right[y] = x;
    }
}

static void drawEllipse(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color, PixelFunc pix)
{
    if(x0 > x1 || y0 > y1)
        return;

    s64 a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1; /* values of diameter */
    s64 dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
    s64 err = dx + dy + b1 * a * a, e2; /* error of 1.step */

    if (x0 > x1) { x0 = x1; x1 += a; } /* if called with swapped pos32s */  
    if (y0 > y1) y0 = y1; /* .. exchange them */
    y0 += (b + 1) / 2; y1 = y0 - b1;   /* starting pixel */
    a *= 8 * a; b1 = 8 * b * b;

    do 
    {
        pix(memory, x1, y0, color); /*   I. Quadrant */
        pix(memory, x0, y0, color); /*  II. Quadrant */
        pix(memory, x0, y1, color); /* III. Quadrant */
        pix(memory, x1, y1, color); /*  IV. Quadrant */
        e2 = 2 * err;
        if (e2 <= dy) { y0++; y1--; err += dy += a; }  /* y step */ 
        if (e2 >= dx || 2 * err > dy) { x0++; x1--; err += dx += b1; } /* x step */
    } while (x0 <= x1);

    while (y0-y1 < b) 
    {  /* too early stop of flat ellipses a=1 */
        pix(memory, x0 - 1, y0,    color); /* -> finish tip of ellipse */
        pix(memory, x1 + 1, y0++,  color); 
        pix(memory, x0 - 1, y1,    color);
        pix(memory, x1 + 1, y1--,  color); 
    }
}

static void setElliPixel(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setPixel((tic_core*)tic, x, y, color);
}

static void setElliSide(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setSidePixel(x, y);
}

static void drawSidesBuffer(tic_mem* memory, s32 y0, s32 y1, u8 color)
{
    tic_vram* vram = &memory->ram->vram;

    tic_core* core = (tic_core*)memory;
    s32 yt = MAX(core->state.clip.t, y0);
    s32 yb = MIN(core->state.clip.b, y1 + 1);
    u8 final_color = mapColor(&core->memory, color);
    for (s32 y = yt; y < yb; y++) 
    {
        s32 xl = MAX(SidesBuffer.Left[y], core->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y] + 1, core->state.clip.r);
        s32 start = y * TIC80_WIDTH;

        for(s32 i = start + xl, end = start + xr; i < end; ++i)
            tic_api_poke4(memory, i, color);
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

static inline float initLine(float *x0, float *x1, float *y0, float *y1)
{
    if (*y0 > *y1)
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
    if(fabs(x0 - x1) < fabs(y0 - y1))
        for (float t = initLine(&x0, &x1, &y0, &y1); y0 < y1; y0++, x0 += t)
            setPixel((tic_core*)tic, x0, y0, color);
    else
        for (float t = initLine(&y0, &y1, &x0, &x1); x0 < x1; x0++, y0 += t)
            setPixel((tic_core*)tic, x0, y0, color);

    setPixel((tic_core*)tic, x1, y1, color);
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

typedef tic_color(*PixelShader)(const ShaderAttr* a, s32 pixel);

static inline double edgeFn(const Vec2* a, const Vec2* b, const Vec2* c)
{
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}

static void drawTri(tic_mem* tic, const Vec2* v0, const Vec2* v1, const Vec2* v2, PixelShader shader, void* data)
{
    ShaderAttr a = {data, v0, v1, v2};

    tic_core* core = (tic_core*)tic;
    const struct ClipRect* clip = &core->state.clip;

    tic_point min = {floor(MIN3(a.v[0]->x, a.v[1]->x, a.v[2]->x)), floor(MIN3(a.v[0]->y, a.v[1]->y, a.v[2]->y))};
    tic_point max = {ceil(MAX3(a.v[0]->x, a.v[1]->x, a.v[2]->x)), ceil(MAX3(a.v[0]->y, a.v[1]->y, a.v[2]->y))};

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
        Vec2 p = {min.x + Center, min.y + Center};

        s32 c = (i + 1) % 3, n = (i + 2) % 3;
        
        d[i].x = (a.v[c]->y - a.v[n]->y) / area;
        d[i].y = (a.v[n]->x - a.v[c]->x) / area;
        s.d[i] = edgeFn(a.v[c], a.v[n], &p) / area;
    }

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
                    tic_api_poke4(tic, pixel, color);
            }

            for(s32 i = 0; i != COUNT_OF(a.w.d); ++i)
                a.w.d[i] += d[i].x;
        }

        for(s32 i = 0; i != COUNT_OF(s.d); ++i)
            s.d[i] += d[i].y;
    }
}

static tic_color triColorShader(const ShaderAttr* a, s32 pixel){return *(u8*)a->data;}

void tic_api_tri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    color = mapColor(tic, color);
    drawTri(tic,
        &(Vec2){x1, y1},
        &(Vec2){x2, y2},
        &(Vec2){x3, y3}, 
        triColorShader, &color);
}

void tic_api_trib(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    tic_core* core = (tic_core*)tic;

    u8 finalColor = mapColor(tic, color);

    drawLine(tic, x1, y1, x2, y2, finalColor);
    drawLine(tic, x2, y2, x3, y3, finalColor);
    drawLine(tic, x3, y3, x1, y1, finalColor);
}

typedef struct
{
    Vec2 _;
    Vec3 d;
}TexVert;

typedef struct
{
    tic_tilesheet sheet;
    u8* mapping;
    const u8* map;
    const tic_vram* vram;
    bool depth;
} TexData;

static inline bool shaderStart(const ShaderAttr* a, Vec3* vars, s32 pixel)
{
    TexData* data = a->data;

    if(data->depth)
    {
        vars->z = 0;
        for(s32 i = 0; i != COUNT_OF(a->v); ++i)
        {
            const TexVert* t = (TexVert*)a->v[i];
            vars->z += a->w.d[i] * t->d.z;
        }

        if(ZBuffer[pixel] < vars->z);
        else return false;
    }

    vars->x = vars->y = 0;
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

static tic_color triTexMapShader(const ShaderAttr* a, s32 pixel)
{
    TexData* data = a->data;

    Vec3 vars;
    if(!shaderStart(a, &vars, pixel))
        return TRANSPARENT_COLOR;

    enum { MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE, MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE,
        WMask = TIC_SPRITESIZE - 1, HMask = TIC_SPRITESIZE - 1 };

    s32 iu = tic_modulo(vars.x, MapWidth);
    s32 iv = tic_modulo(vars.y, MapHeight);

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

    enum { WMask = TIC_SPRITESHEET_SIZE - 1, HMask = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS - 1 };

    return shaderEnd(a, &vars, pixel, data->mapping[tic_tilesheet_getpix(&data->sheet, (s32)vars.x & WMask, (s32)vars.y & HMask)]);
}

static tic_color triTexVbankShader(const ShaderAttr* a, s32 pixel)
{
    TexData* data = a->data;

    Vec3 vars;
    if(!shaderStart(a, &vars, pixel))
        return TRANSPARENT_COLOR;

    s32 iu = tic_modulo(vars.x, TIC80_WIDTH);
    s32 iv = tic_modulo(vars.y, TIC80_HEIGHT);

    return shaderEnd(a, &vars, pixel, data->mapping[tic_tool_peek4(data->vram->data, iv * TIC80_WIDTH + iu)]);
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
        .mapping = getPalette(tic, colors, count),
        .map = tic->ram->map.data,
        .vram = &((tic_core*)tic)->state.vbank.mem,
        .depth = depth,
    };

    TexVert t[] = 
    {
        {x1, y1, u1, v1, z1},
        {x2, y2, u2, v2, z2},
        {x3, y3, u3, v3, z3},
    };

    if(depth)
        for(s32 i = 0; i != COUNT_OF(t); ++i)
            t[i].d.x /= t[i].d.z, 
            t[i].d.y /= t[i].d.z, 
            t[i].d.z = 1.0 / t[i].d.z;

    static const PixelShader Shaders[] = 
    {
        [tic_tiles_texture] = triTexTileShader,
        [tic_map_texture]   = triTexMapShader,
        [tic_vbank_texture] = triTexVbankShader,
    };
    
    if(texsrc >= 0 && texsrc < COUNT_OF(Shaders))
        drawTri(tic,
            (const Vec2*)&t[0],
            (const Vec2*)&t[1],
            (const Vec2*)&t[2], 
            Shaders[texsrc], &texData);
}

void tic_api_map(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, u8 count, s32 scale, RemapFunc remap, void* data)
{
    drawMap((tic_core*)memory, &memory->ram->map, x, y, width, height, sx, sy, colors, count, scale, remap, data);
}

void tic_api_mset(tic_mem* memory, s32 x, s32 y, u8 value)
{
    if (x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

    tic_map* src = &memory->ram->map;
    *(src->data + y * TIC_MAP_WIDTH + x) = value;
}

u8 tic_api_mget(tic_mem* memory, s32 x, s32 y)
{
    if (x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;

    const tic_map* src = &memory->ram->map;
    return *(src->data + y * TIC_MAP_WIDTH + x);
}

void tic_api_line(tic_mem* memory, float x0, float y0, float x1, float y1, u8 color)
{
    drawLine(memory, x0, y0, x1, y1, mapColor(memory, color));
}

#if defined(BUILD_DEPRECATED)
#include "draw_dep.c"
#endif
