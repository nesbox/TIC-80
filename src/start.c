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

#include "start.h"

static void reset(Start* start)
{
	u8* tile = (u8*)start->tic->ram.tiles.data;

	start->tic->api.clear(start->tic, (tic_color_black));

	static const u8 Reset[] = {0x00, 0x06, 0x96, 0x00};
	u8 val = Reset[sizeof(Reset) * (start->ticks % TIC_FRAMERATE) / TIC_FRAMERATE];

	for(s32 i = 0; i < sizeof(tic_tile); i++) tile[i] = val;

	start->tic->api.map(start->tic, &start->tic->ram.map, &start->tic->ram.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT + (TIC80_HEIGHT % TIC_SPRITESIZE ? 1 : 0), 0, 0, -1, 1);
}

static void drawHeader(Start* start)
{
	start->tic->api.fixed_text(start->tic, TIC_NAME_FULL, STUDIO_TEXT_WIDTH, STUDIO_TEXT_HEIGHT, (tic_color_white), false);
	start->tic->api.fixed_text(start->tic, TIC_VERSION_LABEL, (sizeof(TIC_NAME_FULL) + 1) * STUDIO_TEXT_WIDTH, STUDIO_TEXT_HEIGHT, (tic_color_dark_gray), false);
	start->tic->api.fixed_text(start->tic, TIC_COPYRIGHT, STUDIO_TEXT_WIDTH, STUDIO_TEXT_HEIGHT*2, (tic_color_dark_gray), false);
}

static void header(Start* start)
{
	if(!start->play)
	{
		playSystemSfx(1);

		start->play = true;
	}

	drawHeader(start);
}

static void end(Start* start)
{
	if(start->play)
	{	
		start->tic->api.sfx_stop(start->tic, 0);
		start->play = false;
	}

	drawHeader(start);

	setStudioMode(TIC_CONSOLE_MODE);
}

static void tick(Start* start)
{
	if(!start->initialized)
	{
		start->phase = 1;
		start->ticks = 0;

		start->initialized = true;
	}

	start->tic->api.clear(start->tic, TIC_COLOR_BG);

	static void(*const steps[])(Start*) = {reset, header, end};

	steps[start->ticks / TIC_FRAMERATE](start);

	start->ticks++;
}

void initStart(Start* start, tic_mem* tic)
{
	*start = (Start)
	{
		.tic = tic,
		.initialized = false,
		.phase = 1,
		.ticks = 0,
		.tick = tick,
		.play = false,
	};
}
