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

#include "keymap.h"
#include "fs.h"

#define BUTTONS_COUNT BITS_IN_BYTE

static const char* ButtonNames[] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y"};

static void saveMapping(Keymap* keymap)
{
	fsSaveRootFile(keymap->fs, KEYMAP_DAT_PATH, getKeymap(), KEYMAP_SIZE, true);
}

static void processKeydown(Keymap* keymap, SDL_Keysym* keysum)
{
	SDL_Scancode scancode = keysum->scancode;

	switch(scancode)
	{
	case SDL_SCANCODE_ESCAPE: break;
	default:
		if(keymap->button >= 0)
		{
			SDL_Scancode* codes = getKeymap();
			codes[keymap->button] = scancode;
			keymap->button = -1;

			saveMapping(keymap);
		}
	}
}

static void drawPlayer(Keymap* keymap, s32 x, s32 y, s32 id)
{
	{
		char label[] = "PLAYER #%i";
		sprintf(label, label, id+1);
		keymap->tic->api.text(keymap->tic, label, x, y, (tic_color_white));
	}


	enum{OffsetX = 32, OffsetY = 16, Height = TIC_FONT_HEIGHT+1, Buttons = BUTTONS_COUNT};

	y += OffsetY;

	SDL_Scancode* codes = getKeymap();

	for(s32 i = 0; i < COUNT_OF(ButtonNames); i++)
	{
		SDL_Rect rect = {x, y + i * Height, (TIC80_WIDTH-OffsetX)/2, Height};

		bool over = false;
		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				keymap->button = id * Buttons + i;
		}

		s32 button = id * Buttons + i;
		bool selected = keymap->button == button;

		if(over)
			keymap->tic->api.rect(keymap->tic, rect.x-1, rect.y-1, rect.w, rect.h, (tic_color_dark_red));

		if(selected)
			keymap->tic->api.rect(keymap->tic, rect.x-1, rect.y-1, rect.w, rect.h, (tic_color_white));

		keymap->tic->api.text(keymap->tic, ButtonNames[i], rect.x, rect.y, selected ? (tic_color_black) : (tic_color_gray));

		keymap->tic->api.text(keymap->tic, SDL_GetKeyName(SDL_GetKeyFromScancode(codes[button])), rect.x + OffsetX, rect.y, selected ? (tic_color_black) : (tic_color_white));
	}
}

static void drawCenterText(Keymap* keymap, const char* text, s32 y, u8 color)
{
	keymap->tic->api.fixed_text(keymap->tic, text, (TIC80_WIDTH - (s32)strlen(text) * TIC_FONT_WIDTH)/2, y, color);
}

static void drawKeymap(Keymap* keymap)
{
	keymap->tic->api.rect(keymap->tic, 0, 0, TIC80_WIDTH, TIC_FONT_HEIGHT * 3, (tic_color_white));

	{
		static const char Label[] = "CONFIGURE BUTTONS MAPPING";
		keymap->tic->api.text(keymap->tic, Label, (TIC80_WIDTH - sizeof Label * TIC_FONT_WIDTH)/2, TIC_FONT_HEIGHT, (tic_color_black));
	}

	drawPlayer(keymap, 16, 40, 0);
	drawPlayer(keymap, 120+16, 40, 1);

	if(keymap->button < 0)
	{
		if(keymap->ticks % TIC_FRAMERATE < TIC_FRAMERATE/2)
			drawCenterText(keymap, "SELECT BUTTON", 120, (tic_color_white));
	}
	else
	{
		char label[256];
		sprintf(label, "PRESS A KEY FOR '%s'", ButtonNames[keymap->button % BUTTONS_COUNT]);
		drawCenterText(keymap, label, 120, (tic_color_white));
		drawCenterText(keymap, "ESC TO CANCEL", 126, (tic_color_white));
	}
}

static void escape(Keymap* keymap)
{
	if(keymap->button < 0)
		setStudioMode(TIC_CONSOLE_MODE);
	else keymap->button = -1;
}

static void tick(Keymap* keymap)
{
	keymap->ticks++;

	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_KEYDOWN:
			processKeydown(keymap, &event->key.keysym);
			break;
		}
	}

	keymap->tic->api.clear(keymap->tic, TIC_COLOR_BG);

	drawKeymap(keymap);
}

void initKeymap(Keymap* keymap, tic_mem* tic, FileSystem* fs)
{
	*keymap = (Keymap)
	{
		.tic = tic,
		.fs = fs,
		.tick = tick,
		.escape = escape,
		.ticks = 0,
		.button = -1,
	};

	s32 size = 0;
	char* data = (char*)fsLoadFile(fs, KEYMAP_DAT_PATH, &size);

	if(data)
	{
		if(size == KEYMAP_SIZE)
			memcpy(getKeymap(), data, KEYMAP_SIZE);

		SDL_free(data);
	}
}