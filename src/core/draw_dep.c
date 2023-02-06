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

typedef struct
{
    float x, y, u, v;
} TexVertDep;

static struct
{
    s16 Left[TIC80_HEIGHT];
    s16 Right[TIC80_HEIGHT];
    s32 ULeft[TIC80_HEIGHT];
    s32 VLeft[TIC80_HEIGHT];
} SidesBufferDep;

static void setSideTexPixel(s32 x, s32 y, float u, float v)
{
    s32 yy = y;
    if (yy >= 0 && yy < TIC80_HEIGHT)
    {
        if (x < SidesBufferDep.Left[yy])
        {
            SidesBufferDep.Left[yy] = x;
            SidesBufferDep.ULeft[yy] = (s32)(u * 65536.0f);
            SidesBufferDep.VLeft[yy] = (s32)(v * 65536.0f);
        }
        if (x > SidesBufferDep.Right[yy])
        {
            SidesBufferDep.Right[yy] = x;
        }
    }
}

static void ticTexLine(tic_mem* memory, TexVertDep* v0, TexVertDep* v1)
{
    TexVertDep* top = v0;
    TexVertDep* bot = v1;

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

void tic_core_textri_dep(tic_core* core, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    tic_mem* memory = &core->memory;
    tic_vram* vram = &memory->ram->vram;

    u8* mapping = getPalette(memory, colors, count);
    TexVertDep V0, V1, V2;

    const u8* map = memory->ram->map.data;
    tic_tilesheet sheet = getTileSheetFromSegment(memory, memory->ram->vram.blit.segment);

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
    for (s32 i = 0; i < COUNT_OF(SidesBuffer.Left); i++)
        SidesBufferDep.Left[i] = TIC80_WIDTH, SidesBufferDep.Right[i] = -1;

    //  parse each line and decide where in the buffer to store them ( left or right ) 
    ticTexLine(memory, &V0, &V1);
    ticTexLine(memory, &V1, &V2);
    ticTexLine(memory, &V2, &V0);

    for (s32 y = 0; y < TIC80_HEIGHT; y++)
    {
        //  if it's backwards skip it
        s32 width = SidesBufferDep.Right[y] - SidesBufferDep.Left[y];
        //  if it's off top or bottom , skip this line
        if ((y < core->state.clip.t) || (y > core->state.clip.b))
            width = 0;
        if (width > 0)
        {
            s32 u = SidesBufferDep.ULeft[y];
            s32 v = SidesBufferDep.VLeft[y];
            s32 left = SidesBufferDep.Left[y];
            s32 right = SidesBufferDep.Right[y];
            //  check right edge, and CLAMP it
            if (right > core->state.clip.r)
                right = core->state.clip.r;
            //  check left edge and offset UV's if we are off the left 
            if (left < core->state.clip.l)
            {
                s32 dist = core->state.clip.l - SidesBufferDep.Left[y];
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