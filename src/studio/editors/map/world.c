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
#include "api.h"
#include "studio/mouse.h"
#include "studio/ui.h"

#include <stdlib.h>
#include <string.h>

#define PREVIEW_SIZE (TIC80_WIDTH * TIC80_HEIGHT * TIC_PALETTE_BPP / BITS_IN_BYTE)

static bool keyPressed(World* world, tic_key key)
{
    if(key == tic_key_escape && world->map->view->frame.escaped)
        return false;

    return keyboard_pressed(world->tic, key);
}

static bool mousePos(World* world, const tic_rect* rect)
{
    return !world->map->view->frame.pointerOwned && mouse_over(world->tic, rect);
}

static bool mouseClicked(World* world, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_click(&world->map->deps.mouse[button], rect);
}

static bool mouseDown(World* world, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_down(&world->map->deps.mouse[button], rect);
}

static void openMap(World* world)
{
    world->map->deps.openMap(world->map->deps.host);
}

static void drawGrid(World* world)
{
    tic_mem* tic = world->tic;
    Map* map = world->map;
    u8 color = tic_color_grey;

    for(s32 c = 0; c < TIC80_WIDTH; c += TIC_MAP_SCREEN_WIDTH)
        tic_api_line(world->tic, (float)(c), 0, (float)c, TIC80_HEIGHT, color);

    for(s32 r = 0; r < TIC80_HEIGHT; r += TIC_MAP_SCREEN_HEIGHT)
        tic_api_line(world->tic, 0, (float)(r), TIC80_WIDTH, (float)r, color);

    tic_api_rectb(world->tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, color);

    tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    if(mousePos(world, &rect))
    {
        ui_cursor(world->tic, tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x;
        s32 my = tic_api_mouse(tic).y;

        if(mouseDown(world, &rect, tic_mouse_left))
        {
            map->scroll.x = (mx - TIC_MAP_SCREEN_WIDTH / 2) * TIC_SPRITESIZE;
            map->scroll.y = (my - TIC_MAP_SCREEN_HEIGHT / 2) * TIC_SPRITESIZE;
            if(map->scroll.x < 0)
                map->scroll.x += TIC_MAP_WIDTH * TIC_SPRITESIZE;
            if(map->scroll.y < 0)
                map->scroll.y += TIC_MAP_HEIGHT * TIC_SPRITESIZE;
        }

        if(mouseClicked(world, &rect, tic_mouse_left))
            openMap(world);
    }

    s32 x = map->scroll.x / TIC_SPRITESIZE;
    s32 y = map->scroll.y / TIC_SPRITESIZE;

    tic_api_rectb(world->tic, x, y, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_red);

    if(x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH)
        tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_red);

    if(y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
        tic_api_rectb(world->tic, x, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_red);

    if(x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH && y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
        tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_red);
}

void world_tick(World* world)
{
    tic_mem* tic = world->tic;

    // process scroll
    if(tic->ram->input.mouse.scrolly > 0)
    {
        openMap(world);
        return;
    }

    if(keyPressed(world, tic_key_tab)) openMap(world);

    memcpy(&tic->ram->vram, world->preview, PREVIEW_SIZE);

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_black);
        memcpy(tic->ram->vram.palette.data, world->map->view->cartPalette->data, sizeof(tic_palette));
        drawGrid(world);
    }
}

void world_scanline(tic_mem* tic, s32 row, void* data)
{
    World* world = data;
    if(row == 0)
        memcpy(&tic->ram->vram.palette, world->map->view->palette, sizeof(tic_palette));
}

void initWorld(World* world, Map* map)
{
    if(!world->preview)
        world->preview = malloc(PREVIEW_SIZE);

    *world = (World){
        .map = map,
        .tic = map->deps.tic,
        .preview = world->preview,
    };

    memset(world->preview, 0, PREVIEW_SIZE);
}

void worldRefresh(World* world)
{
    Map* map = world->map;

    memset(world->preview, 0, PREVIEW_SIZE);
    s32 colors[TIC_PALETTE_SIZE];

    u8 segment = (u8)(tic_blit_calc_segment(&map->sheet.blit));
    tic_tilesheet sheet = tic_tilesheet_get(segment, (u8*)map->view->tiles);

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
    {
        u8 index = map->src->data[i];

        if(index)
        {
            memset(colors, 0, sizeof colors);

            tic_tileptr tileptr = tic_tilesheet_gettile(&sheet, index, true);

            for(s32 y = 0; y < TIC_SPRITESIZE; y++)
                for(s32 x = 0; x < TIC_SPRITESIZE; x++)
                {
                    u8 color = tic_tilesheet_gettilepix(&tileptr, x, y);

                    if(color)
                        colors[color]++;
                }

            s32 max = 0;

            for(s32 c = 0; c < COUNT_OF(colors); c++)
                if(colors[c] > colors[max]) max = c;

            tic_tool_poke4(world->preview, i, (u8)(max));
        }
    }
}

void freeWorld(World* world)
{
    free(world->preview);
    free(world);
}
