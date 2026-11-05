/* The drawing API's contract: each function draws once and the whole
 * 240x136 frame is compared with a reference rasterizer, not a few pixels.
 * The line and ellipse families keep the original rasterizers as frozen
 * references; the picture is what must not move. */
#include "core/core.h"
#include "api.h"
#include "tilesheet.h"
#include "tools.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static tic_mem* Tic;

static tic_core* core(void)
{
    return (tic_core*)Tic;
}

static u8* screen(void)
{
    return Tic->ram->vram.screen.data;
}

static void reset(void)
{
    tic_api_clip(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    memset(screen(), 0, sizeof(tic_screen));

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        tic_tool_poke4(Tic->ram->vram.mapping, (u32)i, (u8)i);
}

static u8 pixel(s32 x, s32 y)
{
    return tic_tool_peek4(screen(), (u32)(y * TIC80_WIDTH + x));
}

static void expectPixel(s32 x, s32 y, u8 color)
{
    u8 got = pixel(x, y);

    if(got != color)
    {
        fprintf(stderr, "pixel (%d,%d) got %u want %u\n", x, y, got, color);
        assert(false);
    }
}

static void expectRect(s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for(s32 yy = y; yy < y + height; yy++)
        for(s32 xx = x; xx < x + width; xx++)
            assert(pixel(xx, yy) == color);
}

static void expectBlock(s32 x, s32 y, u8 color)
{
    for(s32 yy = y; yy < y + TIC_SPRITESIZE; yy++)
        for(s32 xx = x; xx < x + TIC_SPRITESIZE; xx++)
            expectPixel(xx, yy, color);
}

static void expectBorder(s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for(s32 xx = x; xx < x + width; xx++)
    {
        assert(pixel(xx, y) == color);
        assert(pixel(xx, y + height - 1) == color);
    }

    for(s32 yy = y; yy < y + height; yy++)
    {
        assert(pixel(x, yy) == color);
        assert(pixel(x + width - 1, yy) == color);
    }
}

static void fillTile(tic_tile* tile, u8 color)
{
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(tile->data, (u32)i, color);
}

static s32 countColor(u8 color)
{
    s32 count = 0;

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        if(tic_tool_peek4(screen(), (u32)i) == color)
            count++;

    return count;
}

static u8 expected[TIC80_WIDTH * TIC80_HEIGHT];

static double refEdge(double ax, double ay, double bx, double by, double px, double py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

// Independent edge-function rasterizer: the frame the API must produce.
static void refTri(float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    memset(expected, 0, sizeof(expected));

    double ax = x1, ay = y1;
    double bx = x2, by = y2;
    double cx = x3, cy = y3;
    double area = refEdge(ax, ay, bx, by, cx, cy);

    if((s32)floor(area) == 0)
        return;

    if(area < 0.0)
    {
        double tx = bx, ty = by;
        bx = cx, by = cy;
        cx = tx, cy = ty;
        area = -area;
    }

    const double center = 0.5 - FLT_EPSILON;
    const s32 l = core()->state.clip.l, t = core()->state.clip.t;
    const s32 r = core()->state.clip.r, b = core()->state.clip.b;

    for(s32 y = t; y < b; y++)
        for(s32 x = l; x < r; x++)
        {
            double px = x + center, py = y + center;
            double w0 = refEdge(bx, by, cx, cy, px, py) / area;
            double w1 = refEdge(cx, cy, ax, ay, px, py) / area;
            double w2 = refEdge(ax, ay, bx, by, px, py) / area;

            if(w0 > -DBL_EPSILON && w1 > -DBL_EPSILON && w2 > -DBL_EPSILON)
                expected[y * TIC80_WIDTH + x] = color;
        }
}

static void expectFrame(const char* what)
{
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
    {
        u8 got = tic_tool_peek4(screen(), (u32)i);

        if(got != expected[i])
        {
            fprintf(stderr, "%s: (%d,%d) got %u want %u\n", what, i % TIC80_WIDTH, i / TIC80_WIDTH, got, expected[i]);
            assert(false);
        }
    }
}

static void refReset(void)
{
    memset(expected, 0, sizeof(expected));
}

static void refCapture(void)
{
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        expected[i] = tic_tool_peek4(screen(), (u32)i);
}

static void refPixel(s32 x, s32 y, u8 color)
{
    const struct ClipRect* clip = &core()->state.clip;

    if(x < clip->l || y < clip->t || x >= clip->r || y >= clip->b)
        return;

    expected[y * TIC80_WIDTH + x] = color;
}

static void refHLine(s32 x, s32 y, s32 width, u8 color)
{
    const struct ClipRect* clip = &core()->state.clip;

    if(y < clip->t || y >= clip->b)
        return;

    s32 xl = x < clip->l ? clip->l : x;
    s32 xr = x + width > clip->r ? clip->r : x + width;

    for(s32 i = xl; i < xr; i++)
        expected[y * TIC80_WIDTH + i] = color;
}

static void refVLine(s32 x, s32 y, s32 height, u8 color)
{
    const struct ClipRect* clip = &core()->state.clip;

    if(x < clip->l || x >= clip->r)
        return;

    s32 yl = y < 0 ? 0 : y;
    s32 yr = y + height >= TIC80_HEIGHT ? TIC80_HEIGHT : y + height;

    for(s32 i = yl; i < yr; i++)
        refPixel(x, i, color);
}

static void refRect(s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for(s32 i = y; i < y + height; i++)
        refHLine(x, i, width, color);
}

static void refRectb(s32 x, s32 y, s32 width, s32 height, u8 color)
{
    refHLine(x, y, width, color);
    refHLine(x, y + height - 1, width, color);
    refVLine(x, y, height, color);
    refVLine(x + width - 1, y, height, color);
}

static void refCls(u8 color)
{
    const struct ClipRect* clip = &core()->state.clip;

    for(s32 y = clip->t; y < clip->b; y++)
        for(s32 x = clip->l; x < clip->r; x++)
            expected[y * TIC80_WIDTH + x] = color;
}

// Frozen copy of the API's line rasterizer.
static float refInitLine(float* x0, float* x1, float* y0, float* y1)
{
    if(*y0 > *y1)
    {
        float tx = *x0, ty = *y0;
        *x0 = *x1, *y0 = *y1;
        *x1 = tx, *y1 = ty;
    }

    float t = (*x1 - *x0) / (*y1 - *y0);

    if(*y0 < 0) *x0 -= *y0 * t, *y0 = 0;
    if(*y1 > TIC80_WIDTH) *x1 += (TIC80_WIDTH - *y0) * t, *y1 = TIC80_WIDTH;

    return t;
}

static void refLine(float x0, float y0, float x1, float y1, u8 color)
{
    if(fabs(x0 - x1) < fabs(y0 - y1))
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter): a raster counter, exact at one a step
        for(float t = refInitLine(&x0, &x1, &y0, &y1); y0 < y1; y0++, x0 += t)
            refPixel((s32)x0, (s32)y0, color);
    else
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter): a raster counter, exact at one a step
        for(float t = refInitLine(&y0, &y1, &x0, &x1); x0 < x1; x0++, y0 += t)
            refPixel((s32)x0, (s32)y0, color);

    refPixel((s32)x1, (s32)y1, color);
}

static void refPaint(s32 x, s32 y, u8 color, u8 border)
{
    const struct ClipRect* clip = &core()->state.clip;

    if(x < clip->l || y < clip->t || x >= clip->r || y >= clip->b)
        return;

    u8 original = expected[y * TIC80_WIDTH + x];

    if(original == color || original == border)
        return;

    static s32 queue[TIC80_WIDTH * TIC80_HEIGHT * 4 + 1];
    s32 head = 0, tail = 0;
    queue[tail++] = y * TIC80_WIDTH + x;

    while(head < tail)
    {
        s32 p = queue[head++];
        s32 px = p % TIC80_WIDTH, py = p / TIC80_WIDTH;

        if(px < clip->l || py < clip->t || px >= clip->r || py >= clip->b)
            continue;

        u8 pix = expected[p];
        bool inside = border == 255 ? pix == original : pix != color && pix != border;

        if(!inside)
            continue;

        expected[p] = color;
        queue[tail++] = p - 1;
        queue[tail++] = p + 1;
        queue[tail++] = p - TIC80_WIDTH;
        queue[tail++] = p + TIC80_WIDTH;
    }
}

// The ellipse rasterizers are the frozen reference: the API's frame has to
// stay exactly this.
#define REF_MIN_EXPONENT    0.3
#define REF_MAX_EXPONENT    8
#define REF_CIRCLE_EXPONENT 2

static s16 refLeft[TIC80_HEIGHT];
static s16 refRight[TIC80_HEIGHT];

static void refSidesInit(void)
{
    for(s32 i = 0; i < TIC80_HEIGHT; i++)
        refLeft[i] = TIC80_WIDTH, refRight[i] = -1;
}

static void refSidePixel(s32 x, s32 y)
{
    if(y >= 0 && y < TIC80_HEIGHT)
    {
        if(x < refLeft[y]) refLeft[y] = (s16)x;
        if(x > refRight[y]) refRight[y] = (s16)x;
    }
}

typedef void (*RefPixelFunc)(s32 x, s32 y, u8 color);

static void refElliPixel(s32 x, s32 y, u8 color)
{
    refPixel(x, y, color);
}

static void refElliSide(s32 x, s32 y, u8 color)
{
    (void)color;
    refSidePixel(x, y);
}

static void refElliBoth(s32 x, s32 y, u8 color)
{
    refPixel(x, y, color);
    refSidePixel(x, y);
}

static void refEllipse(s32 x0, s32 y0, s32 x1, s32 y1, u8 color, RefPixelFunc pix)
{
    if(x0 > x1 || y0 > y1)
        return;

    s64 a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;
    s64 dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a;
    s64 err = dx + dy + b1 * a * a, e2;

    if(x0 > x1)
    {
        x0 = x1;
        x1 += (s32)a;
    }
    if(y0 > y1) y0 = y1;
    y0 += (s32)((b + 1) / 2);
    y1 = (s32)(y0 - b1);
    a *= 8 * a;
    b1 = 8 * b * b;

    do
    {
        pix(x1, y0, color);
        pix(x0, y0, color);
        pix(x0, y1, color);
        pix(x1, y1, color);
        e2 = 2 * err;
        if(e2 <= dy)
        {
            y0++;
            y1--;
            err += dy += a;
        }
        if(e2 >= dx || 2 * err > dy)
        {
            x0++;
            x1--;
            err += dx += b1;
        }
    } while(x0 <= x1);

    while(y0 - y1 < b)
    {
        pix(x0 - 1, y0, color);
        pix(x1 + 1, y0++, color);
        pix(x0 - 1, y1, color);
        pix(x1 + 1, y1--, color);
    }
}

static void refSuperEllipseCapsule(s32 x0, s32 y0, s32 x1, s32 y1, s32 radius, float exponent, u8 color, RefPixelFunc pix)
{
    if(x0 > x1 || y0 > y1)
        return;

    s32 w = abs(x1 - x0), h = abs(y1 - y0);
    s32 smallest = w < h ? w : h;

    if(radius <= 0) radius = smallest / 2;
    if(radius > smallest / 2) radius = smallest / 2;

    for(s32 yy = y0 + radius; yy < y1 - radius; yy++)
    {
        pix(x0, yy, color);
        pix(x1, yy, color);
    }

    if(exponent <= 0)
        exponent = REF_CIRCLE_EXPONENT;
    else
        exponent = exponent < REF_MIN_EXPONENT ? REF_MIN_EXPONENT : (exponent > REF_MAX_EXPONENT ? REF_MAX_EXPONENT : exponent);

    float r = (float)radius;
    float rn = powf(r, exponent);
    s32 x = 0, y = radius;

    x0 += radius;
    x1 -= radius;
    y0 += radius;
    y1 -= radius;

    while(x <= y)
    {
        pix(x0 - x, y0 - y, color);
        pix(x0 - x, y1 + y, color);
        pix(x1 + x, y0 - y, color);
        pix(x1 + x, y1 + y, color);

        if(x != y)
        {
            pix(x0 - y, y0 - x, color);
            pix(x0 - y, y1 + x, color);
            pix(x1 + y, y0 - x, color);
            pix(x1 + y, y1 + x, color);
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

static void refSidesBuffer(s32 y0, s32 y1, u8 color)
{
    const struct ClipRect* clip = &core()->state.clip;
    s32 yt = y0 > clip->t ? y0 : clip->t;
    s32 yb = y1 + 1 < clip->b ? y1 + 1 : clip->b;

    for(s32 y = yt; y < yb; y++)
    {
        s32 xl = refLeft[y] > clip->l ? refLeft[y] : clip->l;
        s32 xr = refRight[y] + 1 < clip->r ? refRight[y] + 1 : clip->r;

        for(s32 x = xl; x < xr; x++)
            expected[y * TIC80_WIDTH + x] = color;
    }
}

static void refCirc(s32 x, s32 y, s32 r, u8 color)
{
    refSidesInit();
    refEllipse(x - r, y - r, x + r, y + r, 0, refElliSide);
    refSidesBuffer(y - r, y + r + 1, color);
}

static void refCircb(s32 x, s32 y, s32 r, u8 color)
{
    refEllipse(x - r, y - r, x + r, y + r, color, refElliPixel);
}

static void refElli(s32 x, s32 y, s32 a, s32 b, u8 color)
{
    refSidesInit();
    refEllipse(x - a, y - b, x + a, y + b, 0, refElliSide);
    refSidesBuffer(y - b, y + b + 1, color);
}

static void refEllib(s32 x, s32 y, s32 a, s32 b, u8 color)
{
    refEllipse(x - a, y - b, x + a, y + b, color, refElliPixel);
}

static void refRrect(s32 x, s32 y, s32 width, s32 height, u8 color, s32 radius, float exponent)
{
    if(radius == 0)
    {
        refRect(x, y, width, height, color);
        return;
    }

    refSidesInit();
    refSuperEllipseCapsule(x, y, x + width, y + height, radius, exponent, 0, refElliSide);
    refSidesBuffer(y, y + height + 1, color);
}

static void refRrectb(s32 x, s32 y, s32 width, s32 height, u8 color, s32 radius, float exponent)
{
    if(radius == 0)
    {
        refRectb(x, y, width, height, color);
        return;
    }

    refSidesInit();
    refSuperEllipseCapsule(x, y, x + width, y + height, radius, exponent, color, refElliBoth);
    refSidesBuffer(y, y, color);
    refSidesBuffer(y + height, y + height, color);
}

static bool refTransparent(u8 color, const u8* trans, s32 transCount)
{
    for(s32 i = 0; i < transCount; i++)
        if(trans[i] == color)
            return true;

    return false;
}

static u8 refTilePix(const tic_tile* tile, s32 x, s32 y)
{
    return tic_tool_peek4(tile->data, (u32)(y * TIC_SPRITESIZE + x));
}

static void refTile(const tic_tile* tile, s32 x, s32 y, s32 scale, tic_flip flip, tic_rotate rotate, const u8* trans, s32 transCount)
{
    s32 rot = rotate & 3;
    u32 orientation = flip & 3;

    if(rot == tic_90_rotate)
        orientation ^= 1;
    else if(rot == tic_180_rotate)
        orientation ^= 3;
    else if(rot == tic_270_rotate)
        orientation ^= 2;
    if(rot == tic_90_rotate || rot == tic_270_rotate)
        orientation |= 4;

    if(scale == 1)
    {
        for(s32 py = 0; py < TIC_SPRITESIZE; py++)
            for(s32 px = 0; px < TIC_SPRITESIZE; px++)
            {
                s32 ix = 0, iy = 0;

                switch(orientation)
                {
                    case 4: ix = py, iy = px; break;
                    case 6: ix = TIC_SPRITESIZE - 1 - py, iy = px; break;
                    case 5: ix = py, iy = TIC_SPRITESIZE - 1 - px; break;
                    case 7: ix = TIC_SPRITESIZE - 1 - py, iy = TIC_SPRITESIZE - 1 - px; break;
                    case 0: ix = px, iy = py; break;
                    case 2: ix = px, iy = TIC_SPRITESIZE - 1 - py; break;
                    case 1: ix = TIC_SPRITESIZE - 1 - px, iy = py; break;
                    case 3: ix = TIC_SPRITESIZE - 1 - px, iy = TIC_SPRITESIZE - 1 - py; break;
                }

                u8 color = refTilePix(tile, ix, iy);

                if(!refTransparent(color, trans, transCount))
                    refPixel(x + px, y + py, color);
            }
        return;
    }

    for(s32 py = 0; py < TIC_SPRITESIZE; py++)
        for(s32 px = 0; px < TIC_SPRITESIZE; px++)
        {
            s32 ix = orientation & 1 ? TIC_SPRITESIZE - 1 - px : px;
            s32 iy = orientation & 2 ? TIC_SPRITESIZE - 1 - py : py;

            if(orientation & 4)
            {
                s32 tmp = ix;
                ix = iy;
                iy = tmp;
            }

            u8 color = refTilePix(tile, ix, iy);

            if(!refTransparent(color, trans, transCount))
                refRect(x + px * scale, y + py * scale, scale, scale, color);
        }
}

static void refSprite(s32 index, s32 x, s32 y, s32 w, s32 h, s32 scale, tic_flip flip, tic_rotate rotate, const u8* trans, s32 transCount)
{
    s32 rot = rotate & 3;
    s32 fl = flip & 3;

    if(w == 1 && h == 1)
    {
        refTile(&Tic->ram->tiles.data[index], x, y, scale, flip, rotate, trans, transCount);
        return;
    }

    s32 step = TIC_SPRITESIZE * scale;
    s32 cols = TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE;

    for(s32 i = 0; i < w; i++)
        for(s32 j = 0; j < h; j++)
        {
            s32 mx = i, my = j;

            if(fl == tic_horz_flip || fl == (tic_horz_flip | tic_vert_flip)) mx = w - 1 - i;
            if(fl == tic_vert_flip || fl == (tic_horz_flip | tic_vert_flip)) my = h - 1 - j;

            if(rot == tic_180_rotate)
            {
                mx = w - 1 - mx;
                my = h - 1 - my;
            }
            else if(rot == tic_90_rotate)
            {
                if(fl == tic_no_flip || fl == (tic_horz_flip | tic_vert_flip))
                    my = h - 1 - my;
                else
                    mx = w - 1 - mx;
            }
            else if(rot == tic_270_rotate)
            {
                if(fl == tic_no_flip || fl == (tic_horz_flip | tic_vert_flip))
                    mx = w - 1 - mx;
                else
                    my = h - 1 - my;
            }

            const tic_tile* tile = &Tic->ram->tiles.data[index + mx + my * cols];

            if(rot == 0 || rot == 2)
                refTile(tile, x + i * step, y + j * step, scale, flip, rotate, trans, transCount);
            else
                refTile(tile, x + j * step, y + i * step, scale, flip, rotate, trans, transCount);
        }
}

static void refMap(s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, s32 scale, const u8* trans, s32 transCount)
{
    s32 size = TIC_SPRITESIZE * scale;

    for(s32 j = y; j < y + height; j++)
        for(s32 i = x; i < x + width; i++)
        {
            s32 mi = ((i % TIC_MAP_WIDTH) + TIC_MAP_WIDTH) % TIC_MAP_WIDTH;
            s32 mj = ((j % TIC_MAP_HEIGHT) + TIC_MAP_HEIGHT) % TIC_MAP_HEIGHT;
            u8 index = Tic->ram->map.data[mj * TIC_MAP_WIDTH + mi];

            refTile(&Tic->ram->tiles.data[index], sx + (i - x) * size, sy + (j - y) * size, scale, tic_no_flip, tic_no_rotate, trans, transCount);
        }
}

static tic_tilesheet refSheet(u8 segment)
{
    switch(segment)
    {
        case 0:
        case 1:
            return tic_tilesheet_get(segment, (u8*)&Tic->ram->font);
        default:
            return tic_tilesheet_get(segment, (u8*)&Tic->ram->tiles.data);
    }
}

static void refGlyph(const tic_tilesheet* face, s32 index, s32 x, s32 y, s32 scale, bool printMode, u8 printColor)
{
    tic_tileptr tile = tic_tilesheet_gettile(face, index, true);

    for(s32 gy = 0; gy < TIC_SPRITESIZE; gy++)
        for(s32 gx = 0; gx < TIC_SPRITESIZE; gx++)
        {
            u8 color = tic_tilesheet_gettilepix(&tile, gx, gy);

            if(printMode)
            {
                if(color == 1)
                    refRect(x + gx * scale, y + gy * scale, scale, scale, printColor);
            }
            else if(color != 255) // getPalette marks a transparent colour 255
                refRect(x + gx * scale, y + gy * scale, scale, scale, color);
        }
}

static void refPrintFixed(const char* text, s32 x, s32 y, u8 color, s32 scale, bool alt)
{
    tic_tilesheet face = refSheet(1);
    const tic_font_data* font = alt ? &Tic->ram->font.alt : &Tic->ram->font.regular;
    s32 pos = x;

    for(const char* p = text; *p; p++)
    {
        char sym = *p;

        if(sym == '\n')
        {
            pos = x;
            y += font->height * scale;
            continue;
        }

        refGlyph(&face, (alt ? TIC_FONT_CHARS : 0) + (u8)sym, pos, y, scale, true, color);
        pos += font->width * scale;
    }
}

static void refFontFixed(const char* text, s32 x, s32 y, s32 w, s32 h, s32 scale, bool alt)
{
    u8 segment = Tic->ram->vram.blit.segment >> 1;
    u8 flipmask = 1;
    while(segment >>= 1)
        flipmask <<= 1;

    tic_tilesheet face = refSheet(Tic->ram->vram.blit.segment ^ flipmask);
    s32 pos = x;

    for(const char* p = text; *p; p++)
    {
        char sym = *p;

        if(sym == '\n')
        {
            pos = x;
            y += h * scale;
            continue;
        }

        refGlyph(&face, (alt ? TIC_FONT_CHARS : 0) + (u8)sym, pos, y, scale, false, 0);
        pos += w * scale;
    }
}

static void testClip(void)
{
    tic_api_clip(Tic, -10, -10, 500, 500);
    assert(core()->state.clip.l == 0 && core()->state.clip.t == 0);
    assert(core()->state.clip.r == TIC80_WIDTH && core()->state.clip.b == TIC80_HEIGHT);

    tic_api_clip(Tic, 10, 20, 5, 6);
    assert(core()->state.clip.l == 10 && core()->state.clip.t == 20);
    assert(core()->state.clip.r == 15 && core()->state.clip.b == 26);

    tic_api_clip(Tic, 230, 130, 100, 100);
    assert(core()->state.clip.r == TIC80_WIDTH && core()->state.clip.b == TIC80_HEIGHT);

    reset();
    puts("clip ok");
}

static void testPix(void)
{
    reset();

    tic_api_pix(Tic, 10, 20, 5, false);
    expectPixel(10, 20, 5);
    expectPixel(9, 20, 0);
    expectPixel(11, 20, 0);
    expectPixel(10, 19, 0);
    expectPixel(10, 21, 0);

    assert(tic_api_pix(Tic, 10, 20, 0, true) == 5);
    assert(tic_api_pix(Tic, -1, 0, 0, true) == 0);
    assert(tic_api_pix(Tic, TIC80_WIDTH, 0, 0, true) == 0);
    assert(tic_api_pix(Tic, 0, TIC80_HEIGHT, 0, true) == 0);

    // Out-of-range writes are clipped.
    tic_api_pix(Tic, -1, 0, 7, false);
    tic_api_pix(Tic, TIC80_WIDTH, 0, 7, false);
    tic_api_pix(Tic, 0, -1, 7, false);
    tic_api_pix(Tic, 0, TIC80_HEIGHT, 7, false);

    // The palette maps the colour on the way in.
    tic_tool_poke4(Tic->ram->vram.mapping, 5, 9);
    tic_api_pix(Tic, 30, 40, 5, false);
    expectPixel(30, 40, 9);

    reset();

    // The clip rectangle decides which pixels are written.
    tic_api_clip(Tic, 10, 10, 3, 3);
    tic_api_pix(Tic, 9, 10, 6, false);
    tic_api_pix(Tic, 10, 10, 6, false);
    tic_api_pix(Tic, 12, 12, 6, false);
    tic_api_pix(Tic, 13, 12, 6, false);
    tic_api_pix(Tic, 12, 13, 6, false);
    expectPixel(9, 10, 0);
    expectPixel(10, 10, 6);
    expectPixel(12, 12, 6);
    expectPixel(13, 12, 0);
    expectPixel(12, 13, 0);

    // The whole frame: every write the reference makes, and nothing else.
    reset();
    refReset();
    tic_api_pix(Tic, 10, 20, 5, false);
    tic_api_pix(Tic, 0, 0, 1, false);
    tic_api_pix(Tic, TIC80_WIDTH - 1, TIC80_HEIGHT - 1, 2, false);
    tic_api_pix(Tic, -1, 5, 7, false);
    tic_api_pix(Tic, 5, -1, 7, false);
    tic_api_pix(Tic, TIC80_WIDTH, 5, 7, false);
    refPixel(10, 20, 5);
    refPixel(0, 0, 1);
    refPixel(TIC80_WIDTH - 1, TIC80_HEIGHT - 1, 2);
    expectFrame("pix frame");

    reset();
    puts("pix ok");
}

static void testRect(void)
{
    reset();

    tic_api_rect(Tic, 10, 20, 5, 3, 7);
    expectRect(10, 20, 5, 3, 7);
    expectPixel(9, 20, 0);
    expectPixel(15, 20, 0);
    expectPixel(10, 19, 0);
    expectPixel(10, 23, 0);

    // Zero and negative extents write nothing.
    reset();
    tic_api_rect(Tic, 10, 10, 0, 10, 7);
    tic_api_rect(Tic, 10, 10, 10, 0, 7);
    tic_api_rect(Tic, 10, 10, -5, 10, 7);
    tic_api_rect(Tic, 10, 10, 10, -5, 7);
    assert(tic_tool_empty(screen(), sizeof(tic_screen)));

    // The clip trims the rectangle.
    reset();
    tic_api_clip(Tic, 12, 21, 2, 1);
    tic_api_rect(Tic, 10, 20, 5, 3, 7);
    expectPixel(12, 21, 7);
    expectPixel(13, 21, 7);
    expectPixel(11, 21, 0);
    expectPixel(14, 21, 0);
    expectPixel(12, 20, 0);
    expectPixel(12, 22, 0);

    // The palette maps the colour on the way in.
    reset();
    tic_tool_poke4(Tic->ram->vram.mapping, 3, 11);
    tic_api_rect(Tic, 0, 0, 2, 2, 3);
    expectRect(0, 0, 2, 2, 11);

    // The row fill meets odd starts and both screen edges.
    reset();
    tic_api_rect(Tic, 1, 0, 1, 1, 7);
    tic_api_rect(Tic, 0, 1, 3, 1, 7);
    tic_api_rect(Tic, 239, 2, 1, 1, 7);
    tic_api_rect(Tic, 238, 3, 5, 1, 7);
    expectPixel(1, 0, 7);
    expectPixel(0, 1, 7);
    expectPixel(1, 1, 7);
    expectPixel(2, 1, 7);
    expectPixel(3, 1, 0);
    expectPixel(239, 2, 7);
    expectPixel(238, 3, 7);
    expectPixel(239, 3, 7);
    expectPixel(237, 3, 0);

    // An odd start and an odd end go through the edge nibbles.
    reset();
    tic_api_rect(Tic, 1, 60, 10, 1, 7);
    for(s32 x = 1; x <= 10; x++)
        expectPixel(x, 60, 7);
    expectPixel(0, 60, 0);
    expectPixel(11, 60, 0);

    tic_api_rect(Tic, 1, 61, 239, 1, 7);
    for(s32 x = 1; x < TIC80_WIDTH; x++)
        expectPixel(x, 61, 7);
    expectPixel(0, 61, 0);

    // A whole row and the whole screen.
    reset();
    tic_api_rect(Tic, 0, 50, TIC80_WIDTH, 1, 7);
    for(s32 x = 0; x < TIC80_WIDTH; x++)
        expectPixel(x, 50, 7);

    expectPixel(0, 49, 0);
    expectPixel(TIC80_WIDTH - 1, 49, 0);
    expectPixel(0, 51, 0);
    expectPixel(TIC80_WIDTH - 1, 51, 0);

    reset();
    tic_api_rect(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    expectRect(0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);

    // The whole frame for a set of rectangles.
    reset();
    refReset();
    tic_api_rect(Tic, 10, 20, 5, 3, 7);
    refRect(10, 20, 5, 3, 7);
    expectFrame("rect frame");

    reset();
    refReset();
    tic_api_rect(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    refRect(0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    expectFrame("rect full frame");

    reset();
    refReset();
    tic_api_clip(Tic, 12, 21, 2, 1);
    tic_api_rect(Tic, 10, 20, 5, 3, 7);
    refRect(10, 20, 5, 3, 7);
    expectFrame("rect clipped frame");

    reset();
    refReset();
    tic_tool_poke4(Tic->ram->vram.mapping, 3, 11);
    tic_api_rect(Tic, 0, 0, 2, 2, 3);
    refRect(0, 0, 2, 2, 11);
    expectFrame("rect palette frame");

    reset();
    refReset();
    tic_api_rect(Tic, 1, 0, 1, 1, 7);
    tic_api_rect(Tic, 0, 1, 3, 1, 7);
    tic_api_rect(Tic, 239, 2, 1, 1, 7);
    tic_api_rect(Tic, 238, 3, 5, 1, 7);
    tic_api_rect(Tic, 1, 60, 10, 1, 7);
    refRect(1, 0, 1, 1, 7);
    refRect(0, 1, 3, 1, 7);
    refRect(239, 2, 1, 1, 7);
    refRect(238, 3, 5, 1, 7);
    refRect(1, 60, 10, 1, 7);
    expectFrame("rect edges frame");

    reset();
    puts("rect ok");
}

static void testRectb(void)
{
    reset();

    tic_api_rectb(Tic, 10, 20, 5, 4, 7);
    expectBorder(10, 20, 5, 4, 7);
    expectPixel(11, 21, 0);
    expectPixel(13, 22, 0);
    expectPixel(9, 20, 0);
    expectPixel(15, 20, 0);
    expectPixel(10, 19, 0);
    expectPixel(10, 24, 0);

    // The clip trims the border: only its top row crosses this clip.
    reset();
    tic_api_clip(Tic, 12, 20, 1, 1);
    tic_api_rectb(Tic, 10, 20, 5, 4, 7);
    expectPixel(12, 20, 7);
    expectPixel(11, 20, 0);
    expectPixel(13, 20, 0);
    expectPixel(12, 19, 0);
    expectPixel(12, 21, 0);

    // A clip that crosses the vertical edges exercises their y-clamp.
    reset();
    tic_api_clip(Tic, 10, 22, 2, 4);
    tic_api_rectb(Tic, 10, 20, 5, 10, 7);
    expectPixel(10, 22, 7);
    expectPixel(10, 23, 7);
    expectPixel(10, 24, 7);
    expectPixel(10, 25, 7);
    expectPixel(10, 21, 0);
    expectPixel(10, 26, 0);
    expectPixel(11, 22, 0);

    // A zero-size border writes nothing; a 1x1 border is one pixel.
    reset();
    tic_api_rectb(Tic, 10, 20, 0, 0, 7);
    assert(tic_tool_empty(screen(), sizeof(tic_screen)));

    reset();
    tic_api_rectb(Tic, 10, 20, 1, 1, 7);
    expectPixel(10, 20, 7);
    expectPixel(11, 20, 0);
    expectPixel(10, 21, 0);

    // The border meets both screen edges.
    reset();
    tic_api_rectb(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    expectBorder(0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    expectPixel(1, 1, 0);
    expectPixel(TIC80_WIDTH - 2, TIC80_HEIGHT - 2, 0);

    // The whole frame for the border cases.
    reset();
    refReset();
    tic_api_rectb(Tic, 10, 20, 5, 4, 7);
    refRectb(10, 20, 5, 4, 7);
    expectFrame("rectb frame");

    reset();
    refReset();
    tic_api_clip(Tic, 12, 20, 1, 1);
    tic_api_rectb(Tic, 10, 20, 5, 4, 7);
    refRectb(10, 20, 5, 4, 7);
    expectFrame("rectb top clip frame");

    reset();
    refReset();
    tic_api_clip(Tic, 10, 22, 2, 4);
    tic_api_rectb(Tic, 10, 20, 5, 10, 7);
    refRectb(10, 20, 5, 10, 7);
    expectFrame("rectb side clip frame");

    reset();
    refReset();
    tic_api_rectb(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    refRectb(0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    expectFrame("rectb screen frame");

    reset();
    puts("rectb ok");
}

static void testCls(void)
{
    reset();

    // A full-screen cls overwrites every nibble.
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        tic_tool_poke4(screen(), (u32)i, (u8)(i & 0xf));

    tic_api_cls(Tic, 4);

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        assert(tic_tool_peek4(screen(), (u32)i) == 4);

    // A clipped cls touches only the clip and leaves the rest alone.
    reset();
    tic_api_pix(Tic, 0, 0, 9, false);
    tic_api_clip(Tic, 10, 20, 3, 2);
    tic_api_cls(Tic, 4);
    expectPixel(10, 20, 4);
    expectPixel(12, 21, 4);
    expectPixel(9, 20, 0);
    expectPixel(13, 20, 0);
    expectPixel(10, 19, 0);
    expectPixel(10, 22, 0);
    expectPixel(0, 0, 9);

    // A clipped cls wide enough for the row fill; an inverted clip draws
    // nothing and must not zero ZBuffer by a negative size.
    reset();
    tic_api_clip(Tic, 10, 20, 7, 2);
    tic_api_cls(Tic, 4);
    for(s32 y = 20; y < 22; y++)
        for(s32 x = 10; x < 17; x++)
            expectPixel(x, y, 4);
    expectPixel(17, 20, 0);
    expectPixel(10, 19, 0);
    expectPixel(10, 22, 0);

    reset();
    tic_api_clip(Tic, 10, 20, -5, 6);
    tic_api_cls(Tic, 4);
    tic_api_rect(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 7);
    assert(tic_tool_empty(screen(), sizeof(tic_screen)));

    // The palette maps the colour on the way in.
    reset();
    tic_tool_poke4(Tic->ram->vram.mapping, 4, 12);
    tic_api_cls(Tic, 4);
    expectPixel(0, 0, 12);
    expectPixel(TIC80_WIDTH - 1, TIC80_HEIGHT - 1, 12);

    // The whole frame for the clipped and palette cases.
    reset();
    refReset();
    tic_api_cls(Tic, 4);
    refCls(4);
    expectFrame("cls full frame");

    reset();
    refReset();
    tic_api_pix(Tic, 0, 0, 9, false);
    refPixel(0, 0, 9);
    tic_api_clip(Tic, 10, 20, 3, 2);
    tic_api_cls(Tic, 4);
    tic_api_pix(Tic, 0, 0, 9, false);
    refCls(4);
    expectFrame("cls clip frame");

    reset();
    refReset();
    tic_tool_poke4(Tic->ram->vram.mapping, 4, 12);
    tic_api_cls(Tic, 4);
    refCls(12);
    expectFrame("cls palette frame");

    reset();
    refReset();
    tic_api_clip(Tic, 10, 20, -5, 6);
    tic_api_cls(Tic, 4);
    refCls(4);
    expectFrame("cls inverted clip frame");

    reset();
    puts("cls ok");
}

static void testCirc(void)
{
    reset();
    tic_api_circ(Tic, 120, 68, 10, 7);
    expectPixel(120, 68, 7);
    expectPixel(120, 60, 7);
    expectPixel(120, 57, 0);
    expectPixel(109, 68, 0);
    expectPixel(131, 68, 0);

    // The clip trims the filled circle.
    reset();
    tic_api_clip(Tic, 110, 60, 5, 5);
    tic_api_circ(Tic, 120, 68, 20, 7);
    expectPixel(110, 60, 7);
    expectPixel(114, 64, 7);
    expectPixel(109, 60, 0);
    expectPixel(115, 60, 0);
    expectPixel(110, 59, 0);
    expectPixel(110, 65, 0);

    // The palette maps the colour on the way in.
    reset();
    tic_tool_poke4(Tic->ram->vram.mapping, 7, 3);
    tic_api_circ(Tic, 120, 68, 5, 7);
    expectPixel(120, 68, 3);

    // The whole frame against the frozen scanline reference.
    reset();
    refReset();
    tic_api_circ(Tic, 120, 68, 50, 7);
    refCirc(120, 68, 50, 7);
    expectFrame("circ frame");

    reset();
    refReset();
    tic_api_clip(Tic, 100, 50, 40, 30);
    tic_api_circ(Tic, 120, 68, 50, 7);
    refCirc(120, 68, 50, 7);
    expectFrame("circ clip frame");

    reset();
    puts("circ ok");
}

static void testElli(void)
{
    reset();
    tic_api_elli(Tic, 120, 68, 30, 20, 7);
    expectPixel(120, 68, 7);
    expectPixel(95, 68, 7);
    expectPixel(120, 78, 7);
    expectPixel(120, 89, 0);
    expectPixel(89, 68, 0);

    // The clip trims the filled ellipse.
    reset();
    tic_api_clip(Tic, 110, 60, 4, 4);
    tic_api_elli(Tic, 120, 68, 30, 20, 7);
    expectPixel(110, 60, 7);
    expectPixel(113, 63, 7);
    expectPixel(109, 60, 0);
    expectPixel(114, 60, 0);

    // The whole frame against the frozen scanline reference.
    reset();
    refReset();
    tic_api_elli(Tic, 120, 68, 60, 40, 7);
    refElli(120, 68, 60, 40, 7);
    expectFrame("elli frame");

    reset();
    refReset();
    tic_api_clip(Tic, 100, 50, 40, 30);
    tic_api_elli(Tic, 120, 68, 60, 40, 7);
    refElli(120, 68, 60, 40, 7);
    expectFrame("elli clip frame");

    reset();
    puts("elli ok");
}

static void testRrect(void)
{
    reset();
    tic_api_rrect(Tic, 60, 38, 120, 60, 7, 12, 0);
    expectPixel(120, 68, 7);
    expectPixel(60, 68, 7);
    expectPixel(120, 38, 7);
    expectPixel(59, 68, 0);
    expectPixel(120, 37, 0);
    expectPixel(60, 38, 0);
    expectPixel(179, 97, 0);

    // The clip trims the filled rounded rectangle.
    reset();
    tic_api_clip(Tic, 110, 60, 4, 4);
    tic_api_rrect(Tic, 60, 38, 120, 60, 7, 12, 0);
    expectPixel(110, 60, 7);
    expectPixel(113, 63, 7);
    expectPixel(109, 60, 0);
    expectPixel(114, 60, 0);

    // The whole frame against the frozen capsule reference.
    reset();
    refReset();
    tic_api_rrect(Tic, 60, 38, 120, 60, 7, 12, 0);
    refRrect(60, 38, 120, 60, 7, 12, 0);
    expectFrame("rrect frame");

    reset();
    refReset();
    tic_api_clip(Tic, 70, 45, 60, 30);
    tic_api_rrect(Tic, 60, 38, 120, 60, 7, 12, 0);
    refRrect(60, 38, 120, 60, 7, 12, 0);
    expectFrame("rrect clip frame");

    // A zero radius delegates to the plain rectangle.
    reset();
    refReset();
    tic_api_rrect(Tic, 60, 38, 120, 60, 7, 0, 0);
    refRect(60, 38, 120, 60, 7);
    expectFrame("rrect zero radius frame");

    reset();
    refReset();
    tic_api_rrectb(Tic, 60, 38, 120, 60, 7, 0, 0);
    refRectb(60, 38, 120, 60, 7);
    expectFrame("rrectb zero radius frame");

    reset();
    puts("rrect ok");
}

static void testTri(void)
{
    // The whole frame, against an independently written rasterizer.
    reset();
    tic_api_tri(Tic, 10, 10, 30, 10, 10, 30, 7);
    refTri(10, 10, 30, 10, 10, 30, 7);
    expectFrame("tri basic");

    // Edges outside the screen.
    reset();
    tic_api_tri(Tic, -20, 60, 260, 40, 120, -30, 7);
    refTri(-20, 60, 260, 40, 120, -30, 7);
    expectFrame("tri offscreen");

    // Both windings normalize to the same frame.
    reset();
    tic_api_tri(Tic, 30, 10, 10, 30, 10, 10, 7);
    refTri(30, 10, 10, 30, 10, 10, 7);
    expectFrame("tri reversed");

    // Fractional vertices.
    reset();
    tic_api_tri(Tic, 10.25f, 10.75f, 30.5f, 12.3f, 11.7f, 29.9f, 7);
    refTri(10.25f, 10.75f, 30.5f, 12.3f, 11.7f, 29.9f, 7);
    expectFrame("tri fractional");

    // The clip trims the filled triangle.
    reset();
    tic_api_clip(Tic, 15, 15, 5, 5);
    tic_api_tri(Tic, 0, 0, 40, 0, 0, 40, 7);
    refTri(0, 0, 40, 0, 0, 40, 7);
    expectFrame("tri clipped");

    // The palette maps the colour on the way in.
    reset();
    tic_tool_poke4(Tic->ram->vram.mapping, 7, 3);
    tic_api_tri(Tic, 10, 10, 30, 10, 10, 30, 7);
    refTri(10, 10, 30, 10, 10, 30, 3);
    expectFrame("tri palette");

    reset();
    puts("tri ok");
}

static void testTrib(void)
{
    reset();
    tic_api_trib(Tic, 10, 10, 30, 10, 10, 30, 7);
    expectPixel(10, 10, 7);
    expectPixel(30, 10, 7);
    expectPixel(10, 30, 7);
    expectPixel(12, 12, 0);
    expectPixel(20, 12, 0);
    expectPixel(9, 10, 0);
    expectPixel(31, 10, 0);

    // The whole frame: the three edges, nothing else.
    reset();
    refReset();
    tic_api_trib(Tic, 10, 10, 30, 10, 10, 30, 7);
    refLine(10, 10, 30, 10, 7);
    refLine(30, 10, 10, 30, 7);
    refLine(10, 30, 10, 10, 7);
    expectFrame("trib frame");

    reset();
    refReset();
    tic_api_trib(Tic, -20.5f, 60.25f, 260.75f, 40.5f, 120.5f, -30.25f, 7);
    refLine(-20.5f, 60.25f, 260.75f, 40.5f, 7);
    refLine(260.75f, 40.5f, 120.5f, -30.25f, 7);
    refLine(120.5f, -30.25f, -20.5f, 60.25f, 7);
    expectFrame("trib offscreen frame");

    reset();
    puts("trib ok");
}

static void testSpriteSegments(void)
{
    // The banks hold distinct colours, so a wrong bank shows.
    reset();
    memset(Tic->ram->tiles.data, 0x11, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0x22, sizeof(Tic->ram->sprites));

    Tic->ram->vram.blit.segment = 2;
    tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(10, 10, 1);
    expectPixel(9, 10, 0);
    expectPixel(18, 10, 0);

    reset();
    Tic->ram->vram.blit.segment = 3;
    tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(10, 10, 2);

    // 2bpp: page 1 reads tile 8 of the bank; a byte holds four pixels, so
    // one colour is 0x55 (1) or 0xaa (2), not 0x11/0x22.
    reset();
    memset(Tic->ram->tiles.data, 0x55, sizeof(Tic->ram->tiles));
    memset(Tic->ram->tiles.data[8].data, 0xaa, sizeof(tic_tile));

    Tic->ram->vram.blit.segment = 4;
    tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(10, 10, 1);

    reset();
    Tic->ram->vram.blit.segment = 5;
    tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(10, 10, 2);

    // 1bpp: page p reads tile 4p; colours are 0 and 1 only, so the screen
    // starts at 7 to tell an opaque zero from the background.
    memset(Tic->ram->tiles.data, 0xff, sizeof(Tic->ram->tiles));
    memset(Tic->ram->tiles.data[4].data, 0x00, sizeof(tic_tile));
    memset(Tic->ram->tiles.data[12].data, 0x00, sizeof(tic_tile));

    static const struct
    {
        u8 segment;
        u8 color;
    } pages[] = {{8, 1}, {9, 0}, {10, 1}, {11, 0}};

    for(s32 i = 0; i < (s32)(sizeof(pages) / sizeof(pages[0])); i++)
    {
        reset();
        tic_api_cls(Tic, 7);
        Tic->ram->vram.blit.segment = pages[i].segment;
        tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
        expectBlock(10, 10, pages[i].color);
        expectPixel(9, 10, 7);
    }

    // 1bpp fg: bank 1 lives at sprites, page 0 reads tile 0 there.
    reset();
    memset(Tic->ram->tiles.data, 0x00, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0xff, sizeof(Tic->ram->sprites));
    tic_api_cls(Tic, 7);
    Tic->ram->vram.blit.segment = 12;
    tic_api_spr(Tic, 0, 10, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(10, 10, 1);

    // An odd destination x takes the generic path, not the 4bpp pair fill.
    reset();
    memset(Tic->ram->tiles.data, 0x11, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0x22, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 2;
    tic_api_spr(Tic, 0, 11, 10, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectBlock(11, 10, 1);
    expectPixel(10, 10, 0);
    expectPixel(19, 10, 0);

    // The map path reads the same segment.
    reset();
    memset(Tic->ram->tiles.data, 0x55, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0x22, sizeof(Tic->ram->sprites));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    Tic->ram->vram.blit.segment = 4;
    tic_api_map(Tic, 0, 0, 1, 1, 0, 0, NULL, 0, 1, NULL, NULL);
    expectBlock(0, 0, 1);

    memset(Tic->ram->tiles.data, 0, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0, sizeof(Tic->ram->sprites));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));

    reset();
    puts("segments ok");
}

static void drawTtri(tic_texture_src src, float z, bool depth)
{
    tic_api_ttri(Tic, 10, 10, 30, 10, 10, 30,
                 0, 0, 7, 0, 0, 7,
                 src, NULL, 0,
                 z, z, z, depth);
}

static void testTtri(void)
{
    // Tile texture: a uniform 4bpp tile reads as colour 7.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    Tic->ram->vram.blit.segment = 2;
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(12, 12, 7);
    expectPixel(20, 12, 7);
    expectPixel(12, 20, 7);
    expectPixel(9, 10, 0);
    expectPixel(30, 30, 0);

    // The full frame: same coverage and colours as the flat triangle.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    Tic->ram->vram.blit.segment = 2;
    drawTtri(tic_tiles_texture, 0, false);
    refTri(10, 10, 30, 10, 10, 30, 7);
    expectFrame("ttri tiles frame");

    // The palette maps the sampled colour.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    Tic->ram->vram.blit.segment = 2;
    tic_tool_poke4(Tic->ram->vram.mapping, 7, 3);
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(12, 12, 3);

    // The clip trims the triangle.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    Tic->ram->vram.blit.segment = 2;
    tic_api_clip(Tic, 15, 15, 5, 5);
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(15, 15, 7);
    expectPixel(19, 19, 7);
    expectPixel(14, 15, 0);
    expectPixel(20, 15, 0);

    // 2bpp pages: a byte holds four pixels, so 0x55 is colour 1 and 0xaa
    // colour 2; page 1 reads tile 8 of the bank.
    reset();
    memset(Tic->ram->tiles.data, 0x55, sizeof(Tic->ram->tiles));
    memset(Tic->ram->tiles.data[8].data, 0xaa, sizeof(tic_tile));

    Tic->ram->vram.blit.segment = 4;
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(12, 12, 1);

    Tic->ram->vram.blit.segment = 5;
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(12, 12, 2);

    // 1bpp pages: page p reads tile 4p; the screen starts at 7 to tell an
    // opaque zero from the background.
    memset(Tic->ram->tiles.data, 0xff, sizeof(Tic->ram->tiles));
    memset(Tic->ram->tiles.data[4].data, 0x00, sizeof(tic_tile));
    memset(Tic->ram->tiles.data[12].data, 0x00, sizeof(tic_tile));

    static const struct
    {
        u8 segment;
        u8 color;
    } pages[] = {{8, 1}, {9, 0}, {10, 1}, {11, 0}};

    for(s32 i = 0; i < (s32)(sizeof(pages) / sizeof(pages[0])); i++)
    {
        reset();
        tic_api_cls(Tic, 7);
        Tic->ram->vram.blit.segment = pages[i].segment;
        drawTtri(tic_tiles_texture, 0, false);
        expectPixel(12, 12, pages[i].color);
    }

    // 4bpp fg: bank 1 lives at the sprites, colour 2.
    reset();
    memset(Tic->ram->tiles.data, 0x11, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0x22, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 3;
    drawTtri(tic_tiles_texture, 0, false);
    expectPixel(12, 12, 2);

    // Map texture: tile 0 of the map, 4bpp.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    Tic->ram->vram.blit.segment = 2;
    drawTtri(tic_map_texture, 0, false);
    expectPixel(12, 12, 7);
    refTri(10, 10, 30, 10, 10, 30, 7);
    expectFrame("ttri map frame");

    // The generic map path on a 2bpp page draws the same frame.
    reset();
    memset(Tic->ram->tiles.data, 0xff, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0xff, sizeof(Tic->ram->sprites));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    Tic->ram->vram.blit.segment = 4;
    drawTtri(tic_map_texture, 0, false);
    refTri(10, 10, 30, 10, 10, 30, 3);
    expectFrame("ttri map 2bpp frame");

    // Vbank texture: the current vbank's screen is the texture.
    reset();
    memset(core()->state.vbank.mem.screen.data, 0x77, sizeof(tic_screen));
    drawTtri(tic_vbank_texture, 0, false);
    expectPixel(12, 12, 7);
    refTri(10, 10, 30, 10, 10, 30, 7);
    expectFrame("ttri vbank frame");

    // Depth: ttri stores 1/z, so a smaller z (nearer) wins.
    reset();
    memset(Tic->ram->tiles.data, 0x77, sizeof(Tic->ram->tiles));
    Tic->ram->vram.blit.segment = 2;
    tic_api_cls(Tic, 0);
    drawTtri(tic_tiles_texture, 2, true);
    expectPixel(12, 12, 7);

    memset(Tic->ram->tiles.data, 0x33, sizeof(Tic->ram->tiles));
    drawTtri(tic_tiles_texture, 1, true);
    expectPixel(12, 12, 3);

    memset(Tic->ram->tiles.data, 0x55, sizeof(Tic->ram->tiles));
    drawTtri(tic_tiles_texture, 3, true);
    expectPixel(12, 12, 3);

    memset(Tic->ram->tiles.data, 0, sizeof(Tic->ram->tiles));
    memset(Tic->ram->sprites.data, 0, sizeof(Tic->ram->sprites));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    memset(core()->state.vbank.mem.screen.data, 0, sizeof(tic_screen));
    reset();
    puts("ttri ok");
}

static void testSpriteMap(void)
{
    reset();
    fillTile(&Tic->ram->tiles.data[0], 1);
    fillTile(&Tic->ram->tiles.data[1], 2);
    fillTile(&Tic->ram->tiles.data[16], 3);
    fillTile(&Tic->ram->tiles.data[17], 4);
    Tic->ram->vram.blit.segment = 2;

    // A 2x2 sprite reads tiles 0, 1, 16 and 17.
    tic_api_spr(Tic, 0, 10, 20, 2, 2, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    expectPixel(10, 20, 1);
    expectPixel(18, 20, 2);
    expectPixel(10, 28, 3);
    expectPixel(18, 28, 4);
    expectPixel(25, 35, 4);

    // The horizontal flip swaps the tile columns.
    reset();
    tic_api_spr(Tic, 0, 10, 20, 2, 2, NULL, 0, 1, tic_horz_flip, tic_no_rotate);
    expectPixel(10, 20, 2);
    expectPixel(18, 20, 1);
    expectPixel(10, 28, 4);
    expectPixel(18, 28, 3);

    // A chroma colour is transparent.
    reset();
    u8 chroma[] = {1};
    tic_api_spr(Tic, 0, 10, 20, 2, 2, chroma, 1, 1, tic_no_flip, tic_no_rotate);
    expectPixel(10, 20, 0);
    expectPixel(18, 20, 2);
    expectPixel(10, 28, 3);
    expectPixel(18, 28, 4);

    // A 2x2 map region reads the same four tiles.
    reset();
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    Tic->ram->map.data[0] = 0;
    Tic->ram->map.data[1] = 1;
    Tic->ram->map.data[TIC_MAP_WIDTH] = 16;
    Tic->ram->map.data[TIC_MAP_WIDTH + 1] = 17;
    tic_api_map(Tic, 0, 0, 2, 2, 10, 20, NULL, 0, 1, NULL, NULL);
    expectPixel(10, 20, 1);
    expectPixel(18, 20, 2);
    expectPixel(10, 28, 3);
    expectPixel(18, 28, 4);

    // The whole frame: the 2x2 block, then one patterned tile through every
    // orientation and scale.
    reset();
    refReset();
    tic_api_spr(Tic, 0, 10, 20, 2, 2, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    refSprite(0, 10, 20, 2, 2, 1, tic_no_flip, tic_no_rotate, NULL, 0);
    expectFrame("spr 2x2 frame");

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            tic_tool_poke4(Tic->ram->tiles.data[2].data, (u32)(y * TIC_SPRITESIZE + x), (u8)((x + y * 3) % 15 + 1));

    const tic_flip flips[] = {tic_no_flip, tic_horz_flip, tic_vert_flip, tic_horz_flip | tic_vert_flip};
    const tic_rotate rotates[] = {tic_no_rotate, tic_90_rotate, tic_180_rotate, tic_270_rotate};

    for(s32 f = 0; f < (s32)(sizeof(flips) / sizeof(flips[0])); f++)
        for(s32 r = 0; r < (s32)(sizeof(rotates) / sizeof(rotates[0])); r++)
        {
            reset();
            refReset();
            tic_api_spr(Tic, 2, 10, 20, 1, 1, NULL, 0, 1, flips[f], rotates[r]);
            refSprite(2, 10, 20, 1, 1, 1, flips[f], rotates[r], NULL, 0);
            expectFrame("spr orientation frame");
        }

    reset();
    refReset();
    tic_api_spr(Tic, 2, 10, 20, 1, 1, NULL, 0, 2, tic_no_flip, tic_no_rotate);
    refSprite(2, 10, 20, 1, 1, 2, tic_no_flip, tic_no_rotate, NULL, 0);
    expectFrame("spr scale frame");

    reset();
    refReset();
    u8 trans[] = {1};
    tic_api_spr(Tic, 2, 10, 20, 1, 1, trans, 1, 1, tic_no_flip, tic_no_rotate);
    refSprite(2, 10, 20, 1, 1, 1, tic_no_flip, tic_no_rotate, trans, 1);
    expectFrame("spr chroma frame");

    reset();
    refReset();
    tic_api_map(Tic, 0, 0, 2, 2, 10, 20, NULL, 0, 1, NULL, NULL);
    refMap(0, 0, 2, 2, 10, 20, 1, NULL, 0);
    expectFrame("map frame");

    memset(Tic->ram->tiles.data, 0, sizeof(Tic->ram->tiles));
    memset(Tic->ram->map.data, 0, sizeof(Tic->ram->map));
    reset();
    puts("sprite/map ok");
}

static void testLine(void)
{
    reset();
    tic_api_line(Tic, 10, 10, 20, 10, 7);
    for(s32 x = 10; x <= 20; x++)
        expectPixel(x, 10, 7);
    expectPixel(9, 10, 0);
    expectPixel(21, 10, 0);

    reset();
    tic_api_line(Tic, 10, 20, 10, 30, 7);
    for(s32 y = 20; y <= 30; y++)
        expectPixel(10, y, 7);
    expectPixel(10, 19, 0);
    expectPixel(10, 31, 0);

    reset();
    tic_api_line(Tic, 10, 10, 20, 20, 7);
    for(s32 i = 0; i <= 10; i++)
        expectPixel(10 + i, 10 + i, 7);
    expectPixel(9, 10, 0);

    // The clip trims the line.
    reset();
    tic_api_clip(Tic, 12, 10, 5, 1);
    tic_api_line(Tic, 10, 10, 20, 10, 7);
    for(s32 x = 12; x < 17; x++)
        expectPixel(x, 10, 7);
    expectPixel(11, 10, 0);
    expectPixel(17, 10, 0);

    // The whole frame against the reference line.
    reset();
    refReset();
    tic_api_line(Tic, 10, 20, 200, 20, 7);
    refLine(10, 20, 200, 20, 7);
    expectFrame("line horizontal frame");

    reset();
    refReset();
    tic_api_line(Tic, 120, 10, 120, 120, 7);
    refLine(120, 10, 120, 120, 7);
    expectFrame("line vertical frame");

    reset();
    refReset();
    tic_api_line(Tic, 5, 5, 230, 130, 7);
    refLine(5, 5, 230, 130, 7);
    expectFrame("line diagonal frame");

    reset();
    refReset();
    tic_api_clip(Tic, 100, 50, 40, 20);
    tic_api_line(Tic, 0, 0, 240, 136, 7);
    refLine(0, 0, 240, 136, 7);
    expectFrame("line clipped frame");

    reset();
    refReset();
    tic_api_line(Tic, -50.5f, -20.25f, 300.75f, 160.5f, 7);
    refLine(-50.5f, -20.25f, 300.75f, 160.5f, 7);
    expectFrame("line offscreen frame");

    reset();
    puts("line ok");
}

static void testEllipseBorders(void)
{
    reset();
    tic_api_circb(Tic, 120, 68, 10, 7);
    assert(countColor(7) > 0);
    expectPixel(120, 68, 0);
    expectPixel(100, 68, 0);
    expectPixel(140, 68, 0);

    reset();
    tic_api_ellib(Tic, 120, 68, 30, 20, 7);
    assert(countColor(7) > 0);
    expectPixel(120, 68, 0);
    expectPixel(80, 68, 0);
    expectPixel(160, 68, 0);

    reset();
    tic_api_rrectb(Tic, 60, 38, 120, 60, 7, 12, 0);
    assert(countColor(7) > 0);
    expectPixel(120, 38, 7);
    expectPixel(120, 98, 7);
    expectPixel(60, 68, 7);
    expectPixel(180, 68, 7);
    expectPixel(120, 68, 0);
    expectPixel(60, 38, 0);

    // The whole frame against the frozen references.
    reset();
    refReset();
    tic_api_circb(Tic, 120, 68, 10, 7);
    refCircb(120, 68, 10, 7);
    expectFrame("circb frame");

    reset();
    refReset();
    tic_api_ellib(Tic, 120, 68, 30, 20, 7);
    refEllib(120, 68, 30, 20, 7);
    expectFrame("ellib frame");

    reset();
    refReset();
    tic_api_rrectb(Tic, 60, 38, 120, 60, 7, 12, 0);
    refRrectb(60, 38, 120, 60, 7, 12, 0);
    expectFrame("rrectb frame");

    reset();
    refReset();
    tic_api_clip(Tic, 70, 45, 60, 30);
    tic_api_rrectb(Tic, 60, 38, 120, 60, 7, 12, 0);
    refRrectb(60, 38, 120, 60, 7, 12, 0);
    expectFrame("rrectb clip frame");

    reset();
    puts("ellipse borders ok");
}

static void testPrintFont(void)
{
    // print draws the system font and returns the advance.
    reset();
    s32 width = tic_api_print(Tic, "A", 10, 10, 15, false, 1, false);
    assert(width > 0);
    assert(countColor(15) > 0);
    expectPixel(0, 0, 0);

    // A scaled fixed glyph has more pixels.
    reset();
    width = tic_api_print(Tic, "A", 10, 10, 15, true, 2, false);
    assert(width > 0);
    assert(countColor(15) > 4);

    // The alt font is a separate face.
    reset();
    width = tic_api_print(Tic, "A", 10, 10, 15, false, 1, true);
    assert(width > 0);
    assert(countColor(15) > 0);

    // The custom font reads the sprite sheet (segment 2 -> fg bank).
    reset();
    memset(Tic->ram->sprites.data, 0x77, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 2;
    width = tic_api_font(Tic, "A", 10, 10, NULL, 0, 8, 8, true, 1, false);
    assert(width == 8);

    for(s32 y = 10; y < 18; y++)
        for(s32 x = 10; x < 18; x++)
            expectPixel(x, y, 7);

    expectPixel(9, 10, 0);
    expectPixel(18, 10, 0);

    // The alt index selects the second half of the sheet.
    reset();
    memset(Tic->ram->sprites.data, 0x77, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 2;
    width = tic_api_font(Tic, "A", 10, 10, NULL, 0, 8, 8, true, 1, true);
    assert(width == 8);
    expectPixel(10, 10, 7);

    // The whole frame: glyph placement, advance and the mapping, fixed mode.
    reset();
    refReset();
    tic_api_print(Tic, "AB", 10, 10, 15, true, 1, false);
    refPrintFixed("AB", 10, 10, 15, 1, false);
    expectFrame("print fixed frame");

    reset();
    refReset();
    tic_api_print(Tic, "A\nB", 10, 10, 15, true, 2, false);
    refPrintFixed("A\nB", 10, 10, 15, 2, false);
    expectFrame("print scale/newline frame");

    reset();
    refReset();
    tic_api_print(Tic, "AB", 10, 10, 15, true, 1, true);
    refPrintFixed("AB", 10, 10, 15, 1, true);
    expectFrame("print alt frame");

    reset();
    refReset();
    memset(Tic->ram->sprites.data, 0x77, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 2;
    tic_api_font(Tic, "AB", 10, 10, NULL, 0, 8, 8, true, 1, false);
    refFontFixed("AB", 10, 10, 8, 8, 1, false);
    expectFrame("font sprite sheet frame");

    reset();
    refReset();
    memset(Tic->ram->sprites.data, 0x77, sizeof(Tic->ram->sprites));
    Tic->ram->vram.blit.segment = 2;
    tic_api_font(Tic, "A", 10, 10, NULL, 0, 8, 8, true, 2, true);
    refFontFixed("A", 10, 10, 8, 8, 2, true);
    expectFrame("font alt scale frame");

    memset(Tic->ram->sprites.data, 0, sizeof(Tic->ram->sprites));
    reset();
    puts("print/font ok");
}

static void testPaint(void)
{
    // A whole-screen fill bounded by the screen border.
    reset();
    tic_api_rectb(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 1);
    tic_api_paint(Tic, 120, 68, 15, 1);
    expectPixel(0, 0, 1);
    expectPixel(TIC80_WIDTH - 1, TIC80_HEIGHT - 1, 1);
    expectPixel(1, 1, 15);
    expectPixel(TIC80_WIDTH - 2, TIC80_HEIGHT - 2, 15);
    expectPixel(120, 68, 15);

    // A region bounded by a border.
    reset();
    tic_api_rectb(Tic, 50, 40, 100, 50, 1);
    tic_api_paint(Tic, 100, 65, 15, 1);
    expectPixel(50, 40, 1);
    expectPixel(149, 89, 1);
    expectPixel(51, 41, 15);
    expectPixel(148, 88, 15);
    expectPixel(49, 40, 0);
    expectPixel(150, 65, 0);

    // The clip bounds the fill.
    reset();
    tic_api_rectb(Tic, 50, 40, 100, 50, 1);
    tic_api_clip(Tic, 60, 50, 20, 10);
    tic_api_paint(Tic, 70, 55, 15, 1);
    expectPixel(60, 50, 15);
    expectPixel(79, 59, 15);
    expectPixel(59, 50, 0);
    expectPixel(80, 50, 0);
    expectPixel(60, 49, 0);
    expectPixel(60, 60, 0);

    // A seed already in the fill colour does nothing.
    reset();
    tic_api_paint(Tic, 120, 68, 0, 1);
    assert(tic_tool_empty(screen(), sizeof(tic_screen)));

    // The whole frame for the fills, from the API's own pre-state.
    reset();
    tic_api_rectb(Tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 1);
    refCapture();
    tic_api_paint(Tic, 120, 68, 15, 1);
    refPaint(120, 68, 15, 1);
    expectFrame("paint full frame");

    reset();
    tic_api_rectb(Tic, 50, 40, 100, 50, 1);
    refCapture();
    tic_api_paint(Tic, 100, 65, 15, 1);
    refPaint(100, 65, 15, 1);
    expectFrame("paint region frame");

    reset();
    tic_api_rectb(Tic, 50, 40, 100, 50, 1);
    tic_api_clip(Tic, 60, 50, 20, 10);
    refCapture();
    tic_api_paint(Tic, 70, 55, 15, 1);
    refPaint(70, 55, 15, 1);
    expectFrame("paint clipped frame");

    reset();
    refCapture();
    tic_api_paint(Tic, 120, 68, 0, 1);
    refPaint(120, 68, 0, 1);
    expectFrame("paint no-op frame");

    reset();
    puts("paint ok");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testClip();
    testPix();
    testRect();
    testRectb();
    testCls();
    testCirc();
    testElli();
    testRrect();
    testSpriteSegments();
    testSpriteMap();
    testLine();
    testTri();
    testTrib();
    testTtri();
    testEllipseBorders();
    testPrintFont();
    testPaint();

    tic_core_close(Tic);
    puts("draw ok");

    return 0;
}
