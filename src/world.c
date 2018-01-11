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
	Map* map = world->map;
	u8 color = (tic_color_light_blue);

	for(s32 c = 0; c < TIC80_WIDTH; c += TIC_MAP_SCREEN_WIDTH)
		world->tic->api.line(world->tic, c, 0, c, TIC80_HEIGHT, color);

	for(s32 r = 0; r < TIC80_HEIGHT; r += TIC_MAP_SCREEN_HEIGHT)
		world->tic->api.line(world->tic, 0, r, TIC80_WIDTH, r, color);

	world->tic->api.rect_border(world->tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, color);

	SDL_Rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		s32 mx = getMouseX();
		s32 my = getMouseY();

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			map->scroll.x = (mx - TIC_MAP_SCREEN_WIDTH/2) * TIC_SPRITESIZE;
			map->scroll.y = (my - TIC_MAP_SCREEN_HEIGHT/2) * TIC_SPRITESIZE;
		}

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			setStudioMode(TIC_MAP_MODE);
	}

	world->tic->api.rect_border(world->tic, map->scroll.x / TIC_SPRITESIZE, map->scroll.y / TIC_SPRITESIZE, 
		TIC_MAP_SCREEN_WIDTH+1, TIC_MAP_SCREEN_HEIGHT+1, (tic_color_red));
}

static void processKeydown(World* world, SDL_Keycode keycode)
{
	switch(keycode)
	{
	case SDLK_TAB: setStudioMode(TIC_MAP_MODE); break;
	}
}

static void tick(World* world)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_KEYDOWN:
			processKeydown(world, event->key.keysym.sym);
			break;
		}
	}

	SDL_memcpy(&world->tic->ram.vram, world->preview, PREVIEW_SIZE);

	drawGrid(world);
}

void initWorld(World* world, tic_mem* tic, Map* map)
{
	if(!world->preview)
		world->preview = SDL_malloc(PREVIEW_SIZE);

	*world = (World)
	{
		.tic = tic,
		.map = map,
		.tick = tick,
		.preview = world->preview,
	};

	SDL_memset(world->preview, 0, PREVIEW_SIZE);
	s32 colors[TIC_PALETTE_SIZE];

	for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
	{
		u8 index = getBankMap()->data[i];

		if(index)
		{
			SDL_memset(colors, 0, sizeof colors);

			tic_tile* tile = &getBankTiles()->data[index];

			for(s32 p = 0; p < TIC_SPRITESIZE * TIC_SPRITESIZE; p++)
			{
				u8 color = tic_tool_peek4(tile, p);

				if(color)
					colors[color]++;
			}

			s32 max = 0;

			for(s32 c = 0; c < SDL_arraysize(colors); c++)
				if(colors[c] > colors[max]) max = c;

			tic_tool_poke4(world->preview, i, max);
		}

	}
}