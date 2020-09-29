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

#define TRANSPARENT_COLOR 255

static tic_tilesheet getTileSheetFromSegment(tic_mem* memory, u8 segment)
{
    u8* src;
    switch (segment) {
    case 0:
    case 1:
        src = (u8*)&memory->ram.font.data; break;
    default:
        src = (u8*)&memory->ram.tiles.data; break;
    }

    return tic_tilesheet_get(segment, src);
}

static u8* getPalette(tic_mem* tic, u8* colors, u8 count)
{
    static u8 mapping[TIC_PALETTE_SIZE];
    for (s32 i = 0; i < TIC_PALETTE_SIZE; i++) mapping[i] = tic_tool_peek4(tic->ram.vram.mapping, i);
    for (s32 i = 0; i < count; i++) mapping[colors[i]] = TRANSPARENT_COLOR;
    return mapping;
}

static inline u8 mapColor(tic_mem* tic, u8 color)
{
    return tic_tool_peek4(tic->ram.vram.mapping, color & 0xf);
}

static void setPixel(tic_core* core, s32 x, s32 y, u8 color)
{
    if (x < core->state.clip.l || y < core->state.clip.t || x >= core->state.clip.r || y >= core->state.clip.b) return;

    core->state.setpix(&core->memory, x, y, color);
}

static u8 getPixel(tic_core* core, s32 x, s32 y)
{
    if (x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT) return 0;

    return core->state.getpix(&core->memory, x, y);
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
    if (y < core->state.clip.t || core->state.clip.b <= y) return;

    s32 xl = MAX(x, core->state.clip.l);
    s32 xr = MIN(x + width, core->state.clip.r);

    core->state.drawhline(&core->memory, xl, xr, y, color);
}

static void drawVLine(tic_core* core, s32 x, s32 y, s32 height, u8 color)
{
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
            if(color != TRANSPARENT_COLOR) core->state.setpix(&core->memory, xx, y, color); \
        } \
    } \
    } while(0)

#define REVERT(X) (TIC_SPRITESIZE - 1 - (X))

static void drawTile(tic_core* core, tic_tileptr* tile, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    u8* mapping = getPalette(&core->memory, colors, count);

    rotate &= 0b11;
    u32 orientation = flip & 0b11;

    if (rotate == tic_90_rotate) orientation ^= 0b001;
    else if (rotate == tic_180_rotate) orientation ^= 0b011;
    else if (rotate == tic_270_rotate) orientation ^= 0b010;
    if (rotate == tic_90_rotate || rotate == tic_270_rotate) orientation |= 0b100;

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
        case 0b100: DRAW_TILE_BODY(py, px); break;
        case 0b110: DRAW_TILE_BODY(REVERT(py), px); break;
        case 0b101: DRAW_TILE_BODY(py, REVERT(px)); break;
        case 0b111: DRAW_TILE_BODY(REVERT(py), REVERT(px)); break;
        case 0b000: DRAW_TILE_BODY(px, py); break;
        case 0b010: DRAW_TILE_BODY(px, REVERT(py)); break;
        case 0b001: DRAW_TILE_BODY(REVERT(px), py); break;
        case 0b011: DRAW_TILE_BODY(REVERT(px), REVERT(py)); break;
        }
        return;
    }

    if (EARLY_CLIP(x, y, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale)) return;

    for (s32 py = 0; py < TIC_SPRITESIZE; py++, y += scale)
    {
        s32 xx = x;
        for (s32 px = 0; px < TIC_SPRITESIZE; px++, xx += scale)
        {
            s32 ix = orientation & 0b001 ? TIC_SPRITESIZE - px - 1 : px;
            s32 iy = orientation & 0b010 ? TIC_SPRITESIZE - py - 1 : py;
            if (orientation & 0b100) {
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
    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram.vram.blit.segment);
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

    tic_tilesheet sheet = getTileSheetFromSegment(&core->memory, core->memory.ram.vram.blit.segment);

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
            tic_tileptr font_char = tic_tilesheet_gettile(font_face, alt * TIC_FONT_CHARS / 2 + sym, true);
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

void tic_api_cls(tic_mem* memory, u8 color)
{
    static const tic_clip_data EmptyClip = { 0, 0, TIC80_WIDTH, TIC80_HEIGHT };

    tic_core* core = (tic_core*)memory;

    if (memcmp(&core->state.clip, &EmptyClip, sizeof(tic_clip_data)) == 0)
    {
        color &= 0b00001111;
        memset(memory->ram.vram.screen.data, color | (color << TIC_PALETTE_BPP), sizeof(memory->ram.vram.screen.data));
    }
    else
    {
        tic_api_rect(memory, core->state.clip.l, core->state.clip.t, core->state.clip.r - core->state.clip.l, core->state.clip.b - core->state.clip.t, color);
    }
}

s32 tic_api_font(tic_mem* memory, const char* text, s32 x, s32 y, u8 chromakey, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    u8* mapping = getPalette(memory, &chromakey, 1);

    // Compatibility : flip top and bottom of the spritesheet
    // to preserve tic_api_font's default target
    u8 segment = memory->ram.vram.blit.segment >> 1;
    u8 flipmask = 1; while (segment >>= 1) flipmask <<= 1;

    tic_tilesheet font_face = getTileSheetFromSegment(memory, memory->ram.vram.blit.segment ^ flipmask);
    return drawText((tic_core*)memory, &font_face, text, x, y, w, h, fixed, mapping, scale, alt);
}

s32 tic_api_print(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    u8 mapping[] = { 255, color };
    tic_tilesheet font_face = getTileSheetFromSegment(memory, 1);
    // Compatibility : print uses reduced width for non-fixed space
    u8 width = alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH;
    if (!fixed) width -= 2;
    return drawText((tic_core*)memory, &font_face, text, x, y, width, TIC_FONT_HEIGHT, fixed, mapping, scale, alt);
}

void tic_api_spr(tic_mem* memory, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    drawSprite((tic_core*)memory, index, x, y, w, h, colors, count, scale, flip, rotate);
}

static inline u8* getFlag(tic_mem* memory, s32 index, u8 flag)
{
    static u8 stub = 0;
    if (index >= TIC_FLAGS || flag >= BITS_IN_BYTE)
        return &stub;

    return memory->ram.flags.data + index;
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
    s32 ULeft[TIC80_HEIGHT];
    s32 VLeft[TIC80_HEIGHT];
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

static void setSideTexPixel(s32 x, s32 y, float u, float v)
{
    s32 yy = y;
    if (yy >= 0 && yy < TIC80_HEIGHT)
    {
        if (x < SidesBuffer.Left[yy])
        {
            SidesBuffer.Left[yy] = x;
            SidesBuffer.ULeft[yy] = (s32)(u * 65536.0f);
            SidesBuffer.VLeft[yy] = (s32)(v * 65536.0f);
        }
        if (x > SidesBuffer.Right[yy])
        {
            SidesBuffer.Right[yy] = x;
        }
    }
}

void tic_api_circ(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
    tic_core* core = (tic_core*)memory;

    initSidesBuffer();

    s32 r = radius;
    s32 x = -r, y = 0, err = 2 - 2 * r;
    do
    {
        setSidePixel(xm - x, ym + y);
        setSidePixel(xm - y, ym - x);
        setSidePixel(xm + x, ym - y);
        setSidePixel(xm + y, ym + x);

        r = err;
        if (r <= y) err += ++y * 2 + 1;
        if (r > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);

    s32 yt = MAX(core->state.clip.t, ym - radius);
    s32 yb = MIN(core->state.clip.b, ym + radius + 1);
    u8 final_color = mapColor(&core->memory, color);
    for (s32 y = yt; y < yb; y++) {
        s32 xl = MAX(SidesBuffer.Left[y], core->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y] + 1, core->state.clip.r);
        core->state.drawhline(&core->memory, xl, xr, y, final_color);
    }
}

void tic_api_circb(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
    tic_core* core = (tic_core*)memory;
    u8 final_color = mapColor(memory, color);
    s32 r = radius;
    s32 x = -r, y = 0, err = 2 - 2 * r;
    do {
        setPixel(core, xm - x, ym + y, final_color);
        setPixel(core, xm - y, ym - x, final_color);
        setPixel(core, xm + x, ym - y, final_color);
        setPixel(core, xm + y, ym + x, final_color);
        r = err;
        if (r <= y) err += ++y * 2 + 1;
        if (r > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
}

typedef void(*linePixelFunc)(tic_mem* memory, s32 x, s32 y, u8 color);
static void ticLine(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color, linePixelFunc func)
{
    if (y0 > y1)
    {
        SWAP(x0, x1, s32);
        SWAP(y0, y1, s32);
    }

    s32 dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    s32 dy = y1 - y0;
    s32 err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;)
    {
        func(memory, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0++; }
    }
}

static void triPixelFunc(tic_mem* memory, s32 x, s32 y, u8 color)
{
    setSidePixel(x, y);
}

void tic_api_tri(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color)
{
    tic_core* core = (tic_core*)memory;

    initSidesBuffer();

    ticLine(memory, x1, y1, x2, y2, color, triPixelFunc);
    ticLine(memory, x2, y2, x3, y3, color, triPixelFunc);
    ticLine(memory, x3, y3, x1, y1, color, triPixelFunc);

    u8 final_color = mapColor(&core->memory, color);
    s32 yt = MAX(core->state.clip.t, MIN(y1, MIN(y2, y3)));
    s32 yb = MIN(core->state.clip.b, MAX(y1, MAX(y2, y3)) + 1);

    for (s32 y = yt; y < yb; y++) {
        s32 xl = MAX(SidesBuffer.Left[y], core->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y] + 1, core->state.clip.r);
        core->state.drawhline(&core->memory, xl, xr, y, final_color);
    }
}


typedef struct
{
    float x, y, u, v;
} TexVert;


static void ticTexLine(tic_mem* memory, TexVert* v0, TexVert* v1)
{
    TexVert* top = v0;
    TexVert* bot = v1;

    if (bot->y < top->y)
    {
        top = v1;
        bot = v0;
    }

    float dy = bot->y - top->y;
    float step_x = (bot->x - top->x);
    float step_u = (bot->u - top->u);
    float step_v = (bot->v - top->v);

    if ((s32)dy != 0)
    {
        step_x /= dy;
        step_u /= dy;
        step_v /= dy;
    }

    float x = top->x;
    float y = top->y;
    float u = top->u;
    float v = top->v;

    if (y < .0f)
    {
        y = .0f - y;

        x += step_x * y;
        u += step_u * y;
        v += step_v * y;

        y = .0f;
    }

    s32 botY = (s32)bot->y;
    if (botY > TIC80_HEIGHT)
        botY = TIC80_HEIGHT;

    for (; y < botY; ++y)
    {
        setSideTexPixel((s32)x, (s32)y, u, v);
        x += step_x;
        u += step_u;
        v += step_v;
    }
}

static void drawTexturedTriangle(tic_core* core, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    tic_mem* memory = &core->memory;
    u8* mapping = getPalette(memory, colors, count);
    TexVert V0, V1, V2;

    const u8* map = memory->ram.map.data;
    tic_tilesheet sheet = getTileSheetFromSegment(memory, memory->ram.vram.blit.segment);

    V0.x = x1;  V0.y = y1;  V0.u = u1;  V0.v = v1;
    V1.x = x2;  V1.y = y2;  V1.u = u2;  V1.v = v2;
    V2.x = x3;  V2.y = y3;  V2.u = u3;  V2.v = v3;

    //  calculate the slope of the surface 
    //  use floats here 
    float denom = (V0.x - V2.x) * (V1.y - V2.y) - (V1.x - V2.x) * (V0.y - V2.y);
    if (denom == 0.0)
    {
        return;
    }
    float id = 1.0f / denom;
    float dudx, dvdx;
    //  this is the UV slope across the surface
    dudx = ((V0.u - V2.u) * (V1.y - V2.y) - (V1.u - V2.u) * (V0.y - V2.y)) * id;
    dvdx = ((V0.v - V2.v) * (V1.y - V2.y) - (V1.v - V2.v) * (V0.y - V2.y)) * id;
    //  convert to fixed
    s32 dudxs = (s32)(dudx * 65536.0f);
    s32 dvdxs = (s32)(dvdx * 65536.0f);
    //  fill the buffer 
    initSidesBuffer();
    //  parse each line and decide where in the buffer to store them ( left or right ) 
    ticTexLine(memory, &V0, &V1);
    ticTexLine(memory, &V1, &V2);
    ticTexLine(memory, &V2, &V0);

    for (s32 y = 0; y < TIC80_HEIGHT; y++)
    {
        //  if it's backwards skip it
        s32 width = SidesBuffer.Right[y] - SidesBuffer.Left[y];
        //  if it's off top or bottom , skip this line
        if ((y < core->state.clip.t) || (y > core->state.clip.b))
            width = 0;
        if (width > 0)
        {
            s32 u = SidesBuffer.ULeft[y];
            s32 v = SidesBuffer.VLeft[y];
            s32 left = SidesBuffer.Left[y];
            s32 right = SidesBuffer.Right[y];
            //  check right edge, and CLAMP it
            if (right > core->state.clip.r)
                right = core->state.clip.r;
            //  check left edge and offset UV's if we are off the left 
            if (left < core->state.clip.l)
            {
                s32 dist = core->state.clip.l - SidesBuffer.Left[y];
                u += dudxs * dist;
                v += dvdxs * dist;
                left = core->state.clip.l;
            }
            //  are we drawing from the map . ok then at least check before the inner loop
            if (use_map == true)
            {
                for (s32 x = left; x < right; ++x)
                {
                    enum { MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE, MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE };
                    s32 iu = (u >> 16) % MapWidth;
                    s32 iv = (v >> 16) % MapHeight;

                    while (iu < 0) iu += MapWidth;
                    while (iv < 0) iv += MapHeight;

                    u8 tileindex = map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];
                    tic_tileptr tile = tic_tilesheet_gettile(&sheet, tileindex, true);

                    u8 color = mapping[tic_tilesheet_gettilepix(&tile, iu & 7, iv & 7)];
                    if (color != TRANSPARENT_COLOR)
                        setPixel(core, x, y, color);
                    u += dudxs;
                    v += dvdxs;
                }
            }
            else
            {
                //  direct from tile ram 
                for (s32 x = left; x < right; ++x)
                {
                    enum { SheetWidth = TIC_SPRITESHEET_SIZE, SheetHeight = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS };
                    s32 iu = (u >> 16) & (SheetWidth - 1);
                    s32 iv = (v >> 16) & (SheetHeight - 1);

                    u8 color = mapping[tic_tilesheet_getpix(&sheet, iu, iv)];
                    if (color != TRANSPARENT_COLOR)
                        setPixel(core, x, y, color);
                    u += dudxs;
                    v += dvdxs;
                }
            }
        }
    }
}

void tic_api_textri(tic_mem* memory, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    drawTexturedTriangle((tic_core*)memory, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map, colors, count);
}

void tic_api_map(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)
{
    drawMap((tic_core*)memory, &memory->ram.map, x, y, width, height, sx, sy, colors, count, scale, remap, data);
}

void tic_api_mset(tic_mem* memory, s32 x, s32 y, u8 value)
{
    if (x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

    tic_map* src = &memory->ram.map;
    *(src->data + y * TIC_MAP_WIDTH + x) = value;
}

u8 tic_api_mget(tic_mem* memory, s32 x, s32 y)
{
    if (x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;

    const tic_map* src = &memory->ram.map;
    return *(src->data + y * TIC_MAP_WIDTH + x);
}

static inline void setLinePixel(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setPixel((tic_core*)tic, x, y, color);
}

void tic_api_line(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color)
{
    ticLine(memory, x0, y0, x1, y1, mapColor(memory, color), setLinePixel);
}
