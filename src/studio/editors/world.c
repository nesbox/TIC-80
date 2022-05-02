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

#include "world.h"
#include "map.h"

#define PREVIEW_SIZE (TIC80_WIDTH * TIC80_HEIGHT * TIC_PALETTE_BPP / BITS_IN_BYTE)

static void drawGrid(World* world)
{
    tic_mem* tic = world->tic;
    Map* map = world->map;
    u8 color = tic_color_grey;

    for(s32 c = 0; c < TIC80_WIDTH; c += TIC_MAP_SCREEN_WIDTH)
        tic_api_line(world->tic, c, 0, c, TIC80_HEIGHT, color);

    for(s32 r = 0; r < TIC80_HEIGHT; r += TIC_MAP_SCREEN_HEIGHT)
        tic_api_line(world->tic, 0, r, TIC80_WIDTH, r, color);

    tic_api_rectb(world->tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, color);

    tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    if(checkMousePos(world->studio, &rect))
    {
        setCursor(world->studio, tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x;
        s32 my = tic_api_mouse(tic).y;

        if(checkMouseDown(world->studio, &rect, tic_mouse_left))
        {
            map->scroll.x = (mx - TIC_MAP_SCREEN_WIDTH/2) * TIC_SPRITESIZE;
            map->scroll.y = (my - TIC_MAP_SCREEN_HEIGHT/2) * TIC_SPRITESIZE;
            if(map->scroll.x < 0)
                map->scroll.x += TIC_MAP_WIDTH * TIC_SPRITESIZE;
            if(map->scroll.y < 0)
                map->scroll.y += TIC_MAP_HEIGHT * TIC_SPRITESIZE;
        }

        if(checkMouseClick(world->studio, &rect, tic_mouse_left))
            setStudioMode(world->studio, TIC_MAP_MODE);
    }

    s32 x = map->scroll.x / TIC_SPRITESIZE;
    s32 y = map->scroll.y / TIC_SPRITESIZE;

    tic_api_rectb(world->tic, x, y, TIC_MAP_SCREEN_WIDTH+1, TIC_MAP_SCREEN_HEIGHT+1, tic_color_red);

    if(x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH)
        tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y, TIC_MAP_SCREEN_WIDTH+1, TIC_MAP_SCREEN_HEIGHT+1, tic_color_red);

    if(y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
        tic_api_rectb(world->tic, x, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH+1, TIC_MAP_SCREEN_HEIGHT+1, tic_color_red);

    if(x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH && y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
        tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH+1, TIC_MAP_SCREEN_HEIGHT+1, tic_color_red);
}

static void tick(World* world)
{
    tic_mem* tic = world->tic;

    // process scroll
    if(tic->ram->input.mouse.scrolly > 0) 
    {
        setStudioMode(world->studio, TIC_MAP_MODE);
        return;
    }

    if(keyWasPressed(world->studio, tic_key_tab)) setStudioMode(world->studio, TIC_MAP_MODE);

    memcpy(&tic->ram->vram, world->preview, PREVIEW_SIZE);

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_black);
        memcpy(tic->ram->vram.palette.data, getConfig(world->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));
        drawGrid(world);
    }
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    World* world = data;
    if(row == 0)
        memcpy(&tic->ram->vram.palette, getBankPalette(world->studio, false), sizeof(tic_palette));
}

void initWorld(World* world, Studio* studio, Map* map)
{
    if(!world->preview)
        world->preview = malloc(PREVIEW_SIZE);

    *world = (World)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .map = map,
        .tick = tick,
        .preview = world->preview,
        .scanline = scanline,
    };

    memset(world->preview, 0, PREVIEW_SIZE);
    s32 colors[TIC_PALETTE_SIZE];

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
    {
        u8 index = getBankMap(world->studio)->data[i];

        if(index)
        {
            memset(colors, 0, sizeof colors);

            tic_tile* tile = &getBankTiles(world->studio)->data[index];

            for(s32 p = 0; p < TIC_SPRITESIZE * TIC_SPRITESIZE; p++)
            {
                u8 color = tic_tool_peek4(tile, p);

                if(color)
                    colors[color]++;
            }

            s32 max = 0;

            for(s32 c = 0; c < COUNT_OF(colors); c++)
                if(colors[c] > colors[max]) max = c;

            tic_tool_poke4(world->preview, i, max);
        }
    }
}

void freeWorld(World* world)
{
    free(world->preview);
    free(world);
}