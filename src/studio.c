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

#include "studio.h"

#include "start.h"
#include "console.h"
#include "run.h"
#include "sprite.h"
#include "map.h"
#include "world.h"
#include "sfx.h"
#include "music.h"
#include "history.h"
#include "config.h"
#include "code.h"
#include "dialog.h"
#include "menu.h"
#include "surf.h"

#include "fs.h"

#include <zlib.h>
#include "net.h"
#include "ext/gif.h"
#include "ext/md5.h"

// #define TEXTURE_SIZE (TIC80_FULLWIDTH)
// #define STUDIO_PIXEL_FORMAT SDL_PIXELFORMAT_ARGB8888
#define FRAME_SIZE (TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32))
// #define OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
// #define OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)
#define POPUP_DUR (TIC_FRAMERATE*2)

#define KEYBOARD_HOLD 20
#define KEYBOARD_PERIOD 3

#if defined(TIC80_PRO)
#define TIC_EDITOR_BANKS (TIC_BANKS)
#else
#define TIC_EDITOR_BANKS 1
#endif

typedef struct
{
	u8 data[16];
} CartHash;

typedef struct
{
	bool down;
	bool click;

	SDL_Point start;
	SDL_Point end;

} MouseState;

static const EditorMode Modes[] =
{
	TIC_CODE_MODE,
	TIC_SPRITE_MODE,
	TIC_MAP_MODE,
	TIC_SFX_MODE,
	TIC_MUSIC_MODE,
};

static struct
{
	Studio studio;

	tic80_local* tic80local;

	struct
	{
		CartHash hash;
		u64 mdate;
	}cart;

	// SDL_Window* window;
	// SDL_Renderer* renderer;
	// SDL_Texture* texture;

	// struct
	// {
	// 	SDL_AudioSpec 		spec;
	// 	SDL_AudioDeviceID 	device;
	// 	SDL_AudioCVT 		cvt;
	// } audio;

	// SDL_Joystick* joysticks[TIC_GAMEPADS];

	EditorMode mode;
	EditorMode prevMode;
	EditorMode dialogMode;

	struct
	{
		// SDL_Point cursor;
		// u32 button;

		MouseState state[3];

		// SDL_Texture* texture;
		// const u8* src;
		SDL_SystemCursor system;
	} mouse;

	struct
	{
		SDL_Point pos;
		bool active;
	} gesture;

	// const u8* keyboard;

	tic_key keycodes[KEYMAP_COUNT];

	// struct
	// {
	// 	tic80_gamepads keyboard;
	// 	tic80_gamepads touch;
	// 	tic80_gamepads joystick;

	// 	SDL_Texture* texture;

	// 	bool show;
	// 	s32 counter;
	// 	s32 alpha;
	// 	bool backProcessed;

	// 	struct
	// 	{
	// 		s32 size;
	// 		SDL_Point axis;
	// 		SDL_Point a;
	// 		SDL_Point b;
	// 		SDL_Point x;
	// 		SDL_Point y;
	// 	} part;
	// } gamepad;

	struct
	{
		bool show;
		bool chained;

		union
		{
			struct
			{
				s8 code;
				s8 sprites;
				s8 map;
				s8 sfx;
				s8 music;
			} index;

			s8 indexes[COUNT_OF(Modes)];
		};

	} bank;

	struct
	{
		s32 counter;
		char message[STUDIO_TEXT_BUFFER_WIDTH];
	} popup;

	struct
	{
		char text[STUDIO_TEXT_BUFFER_WIDTH];
	} tooltip;

	struct
	{
		bool record;

		u32* buffer;
		s32 frames;
		s32 frame;

	} video;

	bool fullscreen;

	struct
	{
		Code* 	code;
		Sprite* sprite;
		Map* 	map;
		Sfx* 	sfx;
		Music* 	music;
	} editor[TIC_EDITOR_BANKS];

	struct
	{
		Start* start;
		Console* console;
		Run* run;
		World* world;
		Config* config;
		Dialog* dialog;
		Menu* menu;
		Surf* surf;
	};

	FileSystem* fs;

	bool missedFrame;

	s32 argc;
	char **argv;
	s32 samplerate;

} studioImpl =
{
	.tic80local = NULL,
	// .tic = NULL,

	// .window = NULL,
	// .renderer = NULL,
	// .texture = NULL,
	// .audio = 
	// {
	// 	.device = 0,
	// },

	.cart = 
	{
		.mdate = 0,
	},

	// .joysticks = {NULL, NULL, NULL, NULL},

	.mode = TIC_START_MODE,
	.prevMode = TIC_CODE_MODE,
	.dialogMode = TIC_CONSOLE_MODE,

	// .mouse =
	// {
	// 	.cursor = {-1, -1},
	// 	.button = 0,
	// 	.src = NULL,
	// 	.texture = NULL,
	// 	.system = SDL_SYSTEM_CURSOR_ARROW,
	// },

	.gesture =
	{
		.pos = {0, 0},
		.active = false,
	},

	// .keyboard = NULL,
	.keycodes =
	{
		tic_key_up,
		tic_key_down,
		tic_key_left,
		tic_key_right,

		tic_key_z, // a
		tic_key_x, // b
		tic_key_a, // x
		tic_key_s, // y
	},

	// .gamepad =
	// {
	// 	.show = false,
	// },

	.bank = 
	{
		.show = false,
		.chained = true,
	},

	.popup =
	{
		.counter = 0,
		.message = "\0",
	},

	.tooltip =
	{
		.text = "\0",
	},

	.video =
	{
		.record = false,
		.buffer = NULL,
		.frames = 0,
	},

	.fullscreen = false,
	.missedFrame = false,
	.argc = 0,
	.argv = NULL,
};


char getKeyboardText()
{
	tic_mem* tic = studioImpl.studio.tic;

	static const char Symbols[] = 	"abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
	static const char Shift[] = 	"ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";

	enum{Count = sizeof Symbols};

	for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
	{
		tic_key key = tic->ram.input.keyboard.keys[i];

		if(key > 0 && key < Count && tic->api.keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
			return tic->api.key(tic, tic_key_shift) ? Shift[key-1] : Symbols[key-1];
	}

	return 0;
}

bool isKeyWasDown(tic_key key)
{
	tic_mem* tic = studioImpl.studio.tic;
	return tic->api.keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool isAnyKeyWasDown()
{
	tic_mem* tic = studioImpl.studio.tic;

	for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
	{
		tic_key key = tic->ram.input.keyboard.keys[i];

		if(tic->api.keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
			return true;
	}

	return false;
}

tic_tiles* getBankTiles()
{
	return &studioImpl.studio.tic->cart.banks[studioImpl.bank.index.sprites].tiles;
}

tic_map* getBankMap()
{
	return &studioImpl.studio.tic->cart.banks[studioImpl.bank.index.map].map;
}

void playSystemSfx(s32 id)
{
	const tic_sample* effect = &studioImpl.studio.tic->config.bank0.sfx.samples.data[id];
	studioImpl.studio.tic->api.sfx_ex(studioImpl.studio.tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, 0);
}

static void md5(const void* voidData, s32 length, u8* digest)
{
	enum {Size = 512};

	const u8* data = voidData;

	MD5_CTX c;
	MD5_Init(&c);

	while (length > 0)
	{
		MD5_Update(&c, data, length > Size ? Size: length);

		length -= Size;
		data += Size;
	}

	MD5_Final(digest, &c);
}

static u8* getSpritePtr(tic_tile* tiles, s32 x, s32 y)
{
	enum { SheetCols = (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE) };
	return tiles[x / TIC_SPRITESIZE + y / TIC_SPRITESIZE * SheetCols].data;
}


void setSpritePixel(tic_tile* tiles, s32 x, s32 y, u8 color)
{
	// TODO: check spritesheet rect
	tic_tool_poke4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE, color);
}

u8 getSpritePixel(tic_tile* tiles, s32 x, s32 y)
{
	// TODO: check spritesheet rect
	return tic_tool_peek4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE);
}

void toClipboard(const void* data, s32 size, bool flip)
{
	if(data)
	{
		enum {Len = 2};

		char* clipboard = (char*)SDL_malloc(size*Len + 1);

		if(clipboard)
		{
			char* ptr = clipboard;

			for(s32 i = 0; i < size; i++, ptr+=Len)
			{
				sprintf(ptr, "%02x", ((u8*)data)[i]);

				if(flip)
				{
					char tmp = ptr[0];
					ptr[0] = ptr[1];
					ptr[1] = tmp;
				}
			}

			SDL_SetClipboardText(clipboard);
			SDL_free(clipboard);
		}
	}
}

void str2buf(const char* str, s32 size, void* buf, bool flip)
{
	char val[] = "0x00";
	const char* ptr = str;

	for(s32 i = 0; i < size/2; i++)
	{
		if(flip)
		{
			val[3] = *ptr++;
			val[2] = *ptr++;
		}
		else
		{
			val[2] = *ptr++;
			val[3] = *ptr++;
		}

		((u8*)buf)[i] = (u8)strtol(val, NULL, 16);
	}
}

static void removeWhiteSpaces(char* str)
{
	s32 i = 0;
	s32 len = strlen(str);

	for (s32 j = 0; j < len; j++)
		if(!SDL_isspace(str[j]))
			str[i++] = str[j];

	str[i] = '\0';
}

bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces)
{
	if(data)
	{
		if(SDL_HasClipboardText())
		{
			char* clipboard = SDL_GetClipboardText();

			if(clipboard)
			{
				if (remove_white_spaces)
					removeWhiteSpaces(clipboard);
							
				bool valid = strlen(clipboard) == size * 2;

				if(valid) str2buf(clipboard, strlen(clipboard), data, flip);

				SDL_free(clipboard);

				return valid;
			}
		}
	}

	return false;
}

void showTooltip(const char* text)
{
	strncpy(studioImpl.tooltip.text, text, sizeof studioImpl.tooltip.text - 1);
}

static void drawExtrabar(tic_mem* tic)
{
	enum {Size = 7};

	s32 x = (COUNT_OF(Modes) + 1) * Size + 17 * TIC_FONT_WIDTH;
	s32 y = 0;

	static const u8 Icons[] =
	{
		0b00000000,
		0b00101000,
		0b00101000,
		0b00010000,
		0b01101100,
		0b01101100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01111000,
		0b01001000,
		0b01011100,
		0b01110100,
		0b00011100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00111000,
		0b01000100,
		0b01111100,
		0b01101100,
		0b01111100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00011000,
		0b00110000,
		0b01111100,
		0b00110000,
		0b00011000,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00110000,
		0b00011000,
		0b01111100,
		0b00011000,
		0b00110000,
		0b00000000,
		0b00000000,
	};

	static const s32 Colors[] = {8, 9, 6, 5, 5};
	static const StudioEvent Events[] = {TIC_TOOLBAR_CUT, TIC_TOOLBAR_COPY, TIC_TOOLBAR_PASTE,	TIC_TOOLBAR_UNDO, TIC_TOOLBAR_REDO};
	static const char* Tips[] = {"CUT [ctrl+x]", "COPY [ctrl+c]", "PASTE [ctrl+v]", "UNDO [ctrl+z]", "REDO [ctrl+y]"};

	for(s32 i = 0; i < sizeof Icons / BITS_IN_BYTE; i++)
	{
		SDL_Rect rect = {x + i*Size, y, Size, Size};

		u8 bgcolor = (tic_color_white);
		u8 color = (tic_color_light_blue);

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			color = Colors[i];
			showTooltip(Tips[i]);

			if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
			{
				bgcolor = color;
				color = (tic_color_white);
			}
			else if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			{
				setStudioEvent(Events[i]);
			}
		}

		studioImpl.studio.tic->api.rect(tic, x + i * Size, y, Size, Size, bgcolor);
		drawBitIcon(x + i * Size, y, Icons + i*BITS_IN_BYTE, color);
	}
}

const StudioConfig* getConfig()
{
	return &studioImpl.config->data;
}

#if defined (TIC80_PRO)

static void drawBankIcon(s32 x, s32 y)
{
	tic_mem* tic = studioImpl.studio.tic;

	SDL_Rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

	static const u8 Icon[] =
	{
		0b00000000,
		0b01111100,
		0b01000100,
		0b01000100,
		0b01111100,
		0b01111000,
		0b00000000,
		0b00000000,
	};

	bool over = false;
	s32 mode = 0;

	for(s32 i = 0; i < COUNT_OF(Modes); i++)
		if(Modes[i] == studioImpl.mode)
		{
			mode = i;
			break;
		}

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		over = true;

		showTooltip("SWITCH BANK");

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			studioImpl.bank.show = !studioImpl.bank.show;
	}

	if(studioImpl.bank.show)
	{
		drawBitIcon(x, y, Icon, tic_color_red);

		enum{Size = TOOLBAR_SIZE};

		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			SDL_Rect rect = {x + 2 + (i+1)*Size, 0, Size, Size};

			bool over = false;
			if(checkMousePos(&rect))
			{
				setCursor(SDL_SYSTEM_CURSOR_HAND);
				over = true;

				if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				{
					if(studioImpl.bank.chained) 
						SDL_memset(studioImpl.bank.indexes, i, sizeof studioImpl.bank.indexes);
					else studioImpl.bank.indexes[mode] = i;
				}
			}

			if(i == studioImpl.bank.indexes[mode])
				tic->api.rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_red);

			tic->api.draw_char(tic, '0' + i, rect.x+1, rect.y+1, i == studioImpl.bank.indexes[mode] ? tic_color_white : over ? tic_color_red : tic_color_peach);

		}

		{
			static const u8 PinIcon[] =
			{
				0b00000000,
				0b00111000,
				0b00101000,
				0b01111100,
				0b00010000,
				0b00010000,
				0b00000000,
				0b00000000,
			};

			SDL_Rect rect = {x + 4 + (TIC_EDITOR_BANKS+1)*Size, 0, Size, Size};

			bool over = false;

			if(checkMousePos(&rect))
			{
				setCursor(SDL_SYSTEM_CURSOR_HAND);

				over = true;

				if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				{
					studioImpl.bank.chained = !studioImpl.bank.chained;

					if(studioImpl.bank.chained)
						SDL_memset(studioImpl.bank.indexes, studioImpl.bank.indexes[mode], sizeof studioImpl.bank.indexes);
				}
			}

			drawBitIcon(rect.x, rect.y, PinIcon, studioImpl.bank.chained ? tic_color_black : over ? tic_color_dark_gray : tic_color_light_blue);
		}
	}
	else
	{
		drawBitIcon(x, y, Icon, over ? tic_color_red : tic_color_peach);
	}
}

#endif

void drawToolbar(tic_mem* tic, u8 color, bool bg)
{
	if(bg)
		studioImpl.studio.tic->api.rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

	static const u8 TabIcon[] =
	{
		0b11111110,
		0b11111110,
		0b11111110,
		0b11111110,
		0b11111110,
		0b11111110,
		0b11111110,
		0b00000000,
	};

	static const u8 Icons[] =
	{
		0b00000000,
		0b01101100,
		0b01000100,
		0b01000100,
		0b01000100,
		0b01101100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00111000,
		0b01010100,
		0b01111100,
		0b01111100,
		0b01010100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01101100,
		0b01101100,
		0b00000000,
		0b01101100,
		0b01101100,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00011000,
		0b00110100,
		0b01110100,
		0b00110100,
		0b00011000,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00111100,
		0b00100100,
		0b00100100,
		0b01101100,
		0b01101100,
		0b00000000,
		0b00000000,
	};

	enum {Size = 7};

	static const char* Tips[] = {"CODE EDITOR [f1]", "SPRITE EDITOR [f2]", "MAP EDITOR [f3]", "SFX EDITOR [f4]", "MUSIC EDITOR [f5]",};

	s32 mode = -1;

	for(s32 i = 0; i < COUNT_OF(Modes); i++)
	{
		SDL_Rect rect = {i * Size, 0, Size, Size};

		bool over = false;

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			over = true;

			showTooltip(Tips[i]);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				setStudioMode(Modes[i]);
		}

		if(getStudioMode() == Modes[i]) mode = i;

		if(mode == i)
			drawBitIcon(i * Size, 0, TabIcon, color);

		if(mode == i)
			drawBitIcon(i * Size, 1, Icons + i * BITS_IN_BYTE, tic_color_black);

		drawBitIcon(i * Size, 0, Icons + i * BITS_IN_BYTE, mode == i ? (tic_color_white) : (over ? (tic_color_dark_gray) : (tic_color_light_blue)));
	}

	if(mode >= 0) drawExtrabar(tic);

	static const char* Names[] =
	{
		"CODE EDITOR",
		"SPRITE EDITOR",
		"MAP EDITOR",
		"SFX EDITOR",
		"MUSIC EDITOR",
	};

#if defined (TIC80_PRO)
	enum {TextOffset = (COUNT_OF(Modes) + 2) * Size - 2};
	drawBankIcon(COUNT_OF(Modes) * Size + 2, 0);
#else
	enum {TextOffset = (COUNT_OF(Modes) + 1) * Size};
#endif

	if(mode >= 0 && !studioImpl.bank.show)
	{
		if(strlen(studioImpl.tooltip.text))
		{
			studioImpl.studio.tic->api.text(tic, studioImpl.tooltip.text, TextOffset, 1, (tic_color_black));
		}
		else
		{
			studioImpl.studio.tic->api.text(tic, Names[mode], TextOffset, 1, (tic_color_dark_gray));
		}
	}
}

void setStudioEvent(StudioEvent event)
{
	switch(studioImpl.mode)
	{
	case TIC_CODE_MODE: 	
		{
			Code* code = studioImpl.editor[studioImpl.bank.index.code].code;
			code->event(code, event); 			
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = studioImpl.editor[studioImpl.bank.index.sprites].sprite;
			sprite->event(sprite, event); 
		}
	break;
	case TIC_MAP_MODE:
		{
			Map* map = studioImpl.editor[studioImpl.bank.index.map].map;
			map->event(map, event);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = studioImpl.editor[studioImpl.bank.index.sfx].sfx;
			sfx->event(sfx, event);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = studioImpl.editor[studioImpl.bank.index.music].music;
			music->event(music, event);
		}
		break;
	default: break;
	}
}

ClipboardEvent getClipboardEvent(SDL_Keycode keycode)
{
	SDL_Keymod keymod = SDL_GetModState();

	if(keymod & TIC_MOD_CTRL)
	{
		switch(keycode)
		{
		case SDLK_INSERT:
		case SDLK_c: return TIC_CLIPBOARD_COPY;
		case SDLK_x: return TIC_CLIPBOARD_CUT;
		case SDLK_v: return TIC_CLIPBOARD_PASTE;
		}
	}
	else if(keymod & KMOD_SHIFT)
	{
		switch(keycode)
		{
		case SDLK_DELETE: return TIC_CLIPBOARD_CUT;
		case SDLK_INSERT: return TIC_CLIPBOARD_PASTE;
		}
	}

	return TIC_CLIPBOARD_NONE;
}

// const u8* getKeyboard()
// {
// 	return studioImpl.keyboard;
// }

static void showPopupMessage(const char* text)
{
	studioImpl.popup.counter = POPUP_DUR;
	strcpy(studioImpl.popup.message, text);
}

static void exitConfirm(bool yes, void* data)
{
	studioImpl.studio.quit = yes;
}

void exitStudio()
{
	if(studioImpl.mode != TIC_START_MODE && studioCartChanged())
	{
		static const char* Rows[] =
		{
			"YOU HAVE",
			"UNSAVED CHANGES",
			"",
			"DO YOU REALLY WANT",
			"TO EXIT?",
		};

		showDialog(Rows, COUNT_OF(Rows), exitConfirm, NULL);
	}
	else exitConfirm(true, NULL);
}

void drawBitIcon(s32 x, s32 y, const u8* ptr, u8 color)
{
	for(s32 i = 0; i < TIC_SPRITESIZE; i++, ptr++)
		for(s32 col = 0; col < TIC_SPRITESIZE; col++)
			if(*ptr & 1 << col)
				studioImpl.studio.tic->api.pixel(studioImpl.studio.tic, x + TIC_SPRITESIZE - col - 1, y + i, color);
}

static void initWorldMap()
{
	initWorld(studioImpl.world, studioImpl.studio.tic, studioImpl.editor[studioImpl.bank.index.map].map);
}

static void initRunMode()
{
	initRun(studioImpl.run, studioImpl.console, studioImpl.studio.tic);
}

static void initSurfMode()
{
	initSurf(studioImpl.surf, studioImpl.studio.tic, studioImpl.console);
}

void gotoSurf()
{
	initSurfMode();
	setStudioMode(TIC_SURF_MODE);
}

void gotoCode()
{
	setStudioMode(TIC_CODE_MODE);
}

static void initMenuMode()
{
	initMenu(studioImpl.menu, studioImpl.studio.tic, studioImpl.fs);
}

void runGameFromSurf()
{
	studioImpl.studio.tic->api.reset(studioImpl.studio.tic);
	setStudioMode(TIC_RUN_MODE);
	studioImpl.prevMode = TIC_SURF_MODE;
}

void exitFromGameMenu()
{
	if(studioImpl.prevMode == TIC_SURF_MODE)
	{
		setStudioMode(TIC_SURF_MODE);
	}
	else
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	studioImpl.console->showGameMenu = false;
}

void resumeRunMode()
{
	studioImpl.mode = TIC_RUN_MODE;
}

static void showSoftKeyboard()
{
	if(studioImpl.mode == TIC_CONSOLE_MODE || studioImpl.mode == TIC_CODE_MODE)
		if(!SDL_IsTextInputActive())
			SDL_StartTextInput();
}

void setStudioMode(EditorMode mode)
{
	if(mode != studioImpl.mode)
	{
		EditorMode prev = studioImpl.mode;

		if(prev == TIC_RUN_MODE)
			studioImpl.studio.tic->api.pause(studioImpl.studio.tic);

		if(mode != TIC_RUN_MODE)
			studioImpl.studio.tic->api.reset(studioImpl.studio.tic);

		switch (prev)
		{
		case TIC_START_MODE:
		case TIC_CONSOLE_MODE:
		case TIC_RUN_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
			break;
		case TIC_SURF_MODE:
			studioImpl.prevMode = TIC_CODE_MODE;
			break;
		default: studioImpl.prevMode = prev; break;
		}

		switch(mode)
		{
		case TIC_WORLD_MODE: initWorldMap(); break;
		case TIC_RUN_MODE: initRunMode(); break;
		case TIC_SURF_MODE: studioImpl.surf->resume(studioImpl.surf); break;
		default: break;
		}

		studioImpl.mode = mode;

		showSoftKeyboard();
	}
}

EditorMode getStudioMode()
{
	return studioImpl.mode;
}

void changeStudioMode(s32 dir)
{
    const size_t modeCount = sizeof(Modes)/sizeof(Modes[0]);
    for(size_t i = 0; i < modeCount; i++)
    {
        if(studioImpl.mode == Modes[i])
        {
            setStudioMode(Modes[(i+dir+modeCount) % modeCount]);
            return;
        }
    }
}

static void showGameMenu()
{
	studioImpl.studio.tic->api.pause(studioImpl.studio.tic);
	studioImpl.studio.tic->api.reset(studioImpl.studio.tic);

	initMenuMode();
	studioImpl.mode = TIC_MENU_MODE;
}

void hideGameMenu()
{
	studioImpl.studio.tic->api.resume(studioImpl.studio.tic);
	studioImpl.mode = TIC_RUN_MODE;
}

s32 getMouseX()
{
	tic_mem* tic = studioImpl.studio.tic;

	return tic->ram.input.mouse.x;
}

s32 getMouseY()
{
	tic_mem* tic = studioImpl.studio.tic;

	return tic->ram.input.mouse.y;
}

bool checkMousePos(const SDL_Rect* rect)
{
	SDL_Point pos = {getMouseX(), getMouseY()};
	return SDL_PointInRect(&pos, rect);
}

bool checkMouseClick(const SDL_Rect* rect, s32 button)
{
	MouseState* state = &studioImpl.mouse.state[button - 1];

	bool value = state->click
		&& SDL_PointInRect(&state->start, rect)
		&& SDL_PointInRect(&state->end, rect);

	if(value) state->click = false;

	return value;
}

bool checkMouseDown(const SDL_Rect* rect, s32 button)
{
	MouseState* state = &studioImpl.mouse.state[button - 1];

	return state->down && SDL_PointInRect(&state->start, rect);
}


bool getGesturePos(SDL_Point* pos)
{
	if(studioImpl.gesture.active)
	{
		*pos = studioImpl.gesture.pos;

		return true;
	}

	return false;
}

void setCursor(SDL_SystemCursor id)
{
	if(id != SDL_SYSTEM_CURSOR_ARROW)
		studioImpl.mouse.system = id;
}

void hideDialog()
{
	if(studioImpl.dialogMode == TIC_RUN_MODE)
	{
		studioImpl.studio.tic->api.resume(studioImpl.studio.tic);
		studioImpl.mode = TIC_RUN_MODE;
	}
	else setStudioMode(studioImpl.dialogMode);
}

void showDialog(const char** text, s32 rows, DialogCallback callback, void* data)
{
	if(studioImpl.mode != TIC_DIALOG_MODE)
	{
		initDialog(studioImpl.dialog, studioImpl.studio.tic, text, rows, callback, data);
		studioImpl.dialogMode = studioImpl.mode;
		setStudioMode(TIC_DIALOG_MODE);
	}
}

static void resetBanks()
{
	SDL_memset(studioImpl.bank.indexes, 0, sizeof studioImpl.bank.indexes);
}

static void initModules()
{
	tic_mem* tic = studioImpl.studio.tic;

	resetBanks();

	for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
	{
		initCode(studioImpl.editor[i].code, studioImpl.studio.tic, &tic->cart.banks[i].code);
		initSprite(studioImpl.editor[i].sprite, studioImpl.studio.tic, &tic->cart.banks[i].tiles);
		initMap(studioImpl.editor[i].map, studioImpl.studio.tic, &tic->cart.banks[i].map);
		initSfx(studioImpl.editor[i].sfx, studioImpl.studio.tic, &tic->cart.banks[i].sfx);
		initMusic(studioImpl.editor[i].music, studioImpl.studio.tic, &tic->cart.banks[i].music);
	}

	initWorldMap();
}

static void updateHash()
{
	md5(&studioImpl.studio.tic->cart, sizeof(tic_cartridge), studioImpl.cart.hash.data);
}

static void updateMDate()
{
	studioImpl.cart.mdate = fsMDate(studioImpl.console->fs, studioImpl.console->romName);
}

static void updateTitle()
{
	// char name[FILENAME_MAX] = TIC_TITLE;

	// if(strlen(studioImpl.console->romName))
	// 	sprintf(name, "%s [%s]", TIC_TITLE, studioImpl.console->romName);

	// SDL_SetWindowTitle(studioImpl.window, name);
}

void studioRomSaved()
{
	updateTitle();
	updateHash();
	updateMDate();
}

void studioRomLoaded()
{
	initModules();

	updateTitle();
	updateHash();
	updateMDate();
}

bool studioCartChanged()
{
	CartHash hash;
	md5(&studioImpl.studio.tic->cart, sizeof(tic_cartridge), hash.data);

	return memcmp(hash.data, studioImpl.cart.hash.data, sizeof(CartHash)) != 0;
}

// static void updateGamepadParts();

// static void calcTextureRect(SDL_Rect* rect)
// {
// 	SDL_GetWindowSize(studioImpl.window, &rect->w, &rect->h);

// 	if (rect->w * TIC80_HEIGHT < rect->h * TIC80_WIDTH)
// 	{
// 		s32 discreteWidth = rect->w - rect->w % TIC80_WIDTH;
// 		s32 discreteHeight = TIC80_HEIGHT * discreteWidth / TIC80_WIDTH;

// 		rect->x = (rect->w - discreteWidth) / 2;

// 		rect->y = rect->w > rect->h 
// 			? (rect->h - discreteHeight) / 2 
// 			: OFFSET_LEFT*discreteWidth/TIC80_WIDTH;

// 		rect->w = discreteWidth;
// 		rect->h = discreteHeight;

// 	}
// 	else
// 	{
// 		s32 discreteHeight = rect->h - rect->h % TIC80_HEIGHT;
// 		s32 discreteWidth = TIC80_WIDTH * discreteHeight / TIC80_HEIGHT;

// 		rect->x = (rect->w - discreteWidth) / 2;
// 		rect->y = (rect->h - discreteHeight) / 2;

// 		rect->w = discreteWidth;
// 		rect->h = discreteHeight;
// 	}
// }

tic_key* getKeymap()
{
	return studioImpl.keycodes;
}

static void processGamepadMapping()
{
	tic_mem* tic = studioImpl.studio.tic;

	for(s32 i = 0; i < KEYMAP_COUNT; i++)
		if(tic->api.key(tic, studioImpl.keycodes[i]))
			tic->ram.input.gamepads.data |= 1 << i;
}

// #if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)

// static bool checkTouch(const SDL_Rect* rect, s32* x, s32* y)
// {
// 	s32 devices = SDL_GetNumTouchDevices();
// 	s32 width = 0, height = 0;
// 	SDL_GetWindowSize(studioImpl.window, &width, &height);

// 	for (s32 i = 0; i < devices; i++)
// 	{
// 		SDL_TouchID id = SDL_GetTouchDevice(i);

// 		// very strange, but on Android id always == 0
// 		//if (id)
// 		{
// 			s32 fingers = SDL_GetNumTouchFingers(id);

// 			if(fingers)
// 			{
// 				studioImpl.gamepad.counter = 0;

// 				if (!studioImpl.gamepad.show)
// 				{
// 					studioImpl.gamepad.alpha = getConfig()->theme.gamepad.touch.alpha;
// 					SDL_SetTextureAlphaMod(studioImpl.gamepad.texture, studioImpl.gamepad.alpha);
// 					studioImpl.gamepad.show = true;
// 					return false;
// 				}
// 			}

// 			for (s32 f = 0; f < fingers; f++)
// 			{
// 				SDL_Finger* finger = SDL_GetTouchFinger(id, f);

// 				if (finger && finger->pressure > 0.0f)
// 				{
// 					SDL_Point point = { (s32)(finger->x * width), (s32)(finger->y * height) };
// 					if (SDL_PointInRect(&point, rect))
// 					{
// 						*x = point.x;
// 						*y = point.y;
// 						return true;
// 					}
// 				}
// 			}
// 		}
// 	}

// 	return false;
// }

// static void processTouchGamepad()
// {
// 	studioImpl.gamepad.touch.data = 0;

// 	const s32 size = studioImpl.gamepad.part.size;
// 	s32 x = 0, y = 0;

// 	{
// 		SDL_Rect axis = {studioImpl.gamepad.part.axis.x, studioImpl.gamepad.part.axis.y, size*3, size*3};

// 		if(checkTouch(&axis, &x, &y))
// 		{
// 			x -= axis.x;
// 			y -= axis.y;

// 			s32 xt = x / size;
// 			s32 yt = y / size;

// 			if(yt == 0) studioImpl.gamepad.touch.first.up = true;
// 			else if(yt == 2) studioImpl.gamepad.touch.first.down = true;

// 			if(xt == 0) studioImpl.gamepad.touch.first.left = true;
// 			else if(xt == 2) studioImpl.gamepad.touch.first.right = true;

// 			if(xt == 1 && yt == 1)
// 			{
// 				xt = (x - size)/(size/3);
// 				yt = (y - size)/(size/3);

// 				if(yt == 0) studioImpl.gamepad.touch.first.up = true;
// 				else if(yt == 2) studioImpl.gamepad.touch.first.down = true;

// 				if(xt == 0) studioImpl.gamepad.touch.first.left = true;
// 				else if(xt == 2) studioImpl.gamepad.touch.first.right = true;
// 			}
// 		}
// 	}

// 	{
// 		SDL_Rect a = {studioImpl.gamepad.part.a.x, studioImpl.gamepad.part.a.y, size, size};
// 		if(checkTouch(&a, &x, &y)) studioImpl.gamepad.touch.first.a = true;
// 	}

// 	{
// 		SDL_Rect b = {studioImpl.gamepad.part.b.x, studioImpl.gamepad.part.b.y, size, size};
// 		if(checkTouch(&b, &x, &y)) studioImpl.gamepad.touch.first.b = true;
// 	}

// 	{
// 		SDL_Rect xb = {studioImpl.gamepad.part.x.x, studioImpl.gamepad.part.x.y, size, size};
// 		if(checkTouch(&xb, &x, &y)) studioImpl.gamepad.touch.first.x = true;
// 	}

// 	{
// 		SDL_Rect yb = {studioImpl.gamepad.part.y.x, studioImpl.gamepad.part.y.y, size, size};
// 		if(checkTouch(&yb, &x, &y)) studioImpl.gamepad.touch.first.y = true;
// 	}
// }

// #endif

// static s32 getAxisMask(SDL_Joystick* joystick)
// {
// 	s32 mask = 0;

// 	s32 axesCount = SDL_JoystickNumAxes(joystick);

// 	for (s32 a = 0; a < axesCount; a++)
// 	{
// 		s32 axe = SDL_JoystickGetAxis(joystick, a);

// 		if (axe)
// 		{
// 			if (a == 0)
// 			{
// 				if (axe > 16384) mask |= SDL_HAT_RIGHT;
// 				else if(axe < -16384) mask |= SDL_HAT_LEFT;
// 			}
// 			else if (a == 1)
// 			{
// 				if (axe > 16384) mask |= SDL_HAT_DOWN;
// 				else if (axe < -16384) mask |= SDL_HAT_UP;
// 			}
// 		}
// 	}

// 	return mask;
// }

// static s32 getJoystickHatMask(s32 hat)
// {
// 	tic80_gamepads gamepad;
// 	gamepad.data = 0;

// 	gamepad.first.up = hat & SDL_HAT_UP;
// 	gamepad.first.down = hat & SDL_HAT_DOWN;
// 	gamepad.first.left = hat & SDL_HAT_LEFT;
// 	gamepad.first.right = hat & SDL_HAT_RIGHT;

// 	return gamepad.data;
// }

static bool isGameMenu()
{
	return (studioImpl.mode == TIC_RUN_MODE && studioImpl.console->showGameMenu) || studioImpl.mode == TIC_MENU_MODE;
}

// static void processJoysticksWithMouseInput()
// {
// 	s32 index = 0;

// 	for(s32 i = 0; i < COUNT_OF(studioImpl.joysticks); i++)
// 	{
// 		SDL_Joystick* joystick = studioImpl.joysticks[i];

// 		if(joystick && SDL_JoystickGetAttached(joystick))
// 		{
// 			tic80_gamepad* gamepad = NULL;

// 			switch(index)
// 			{
// 			case 0: gamepad = &studioImpl.gamepad.joystick.first; break;
// 			case 1: gamepad = &studioImpl.gamepad.joystick.second; break;
// 			}

// 			if(gamepad)
// 			{
// 				s32 numButtons = SDL_JoystickNumButtons(joystick);
// 				for(s32 i = 5; i < numButtons; i++)
// 				{
// 					s32 back = SDL_JoystickGetButton(joystick, i);

// 					if(back)
// 					{
// 						if(!studioImpl.gamepad.backProcessed)
// 						{
// 							if(isGameMenu())
// 							{
// 								studioImpl.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
// 								studioImpl.gamepad.backProcessed = true;
// 							}
// 						}

// 						return;
// 					}
// 				}

// 				studioImpl.gamepad.backProcessed = false;

// 				index++;
// 			}
// 		}
// 	}
// }

// static void processJoysticks()
// {
// 	studioImpl.gamepad.joystick.data = 0;
// 	s32 index = 0;

// 	for(s32 i = 0; i < COUNT_OF(studioImpl.joysticks); i++)
// 	{
// 		SDL_Joystick* joystick = studioImpl.joysticks[i];

// 		if(joystick && SDL_JoystickGetAttached(joystick))
// 		{
// 			tic80_gamepad* gamepad = NULL;

// 			switch(index)
// 			{
// 			case 0: gamepad = &studioImpl.gamepad.joystick.first; break;
// 			case 1: gamepad = &studioImpl.gamepad.joystick.second; break;
// 			}

// 			if(gamepad)
// 			{
// 				gamepad->data |= getJoystickHatMask(getAxisMask(joystick));

// 				for (s32 h = 0; h < SDL_JoystickNumHats(joystick); h++)
// 					gamepad->data |= getJoystickHatMask(SDL_JoystickGetHat(joystick, h));

// 				s32 numButtons = SDL_JoystickNumButtons(joystick);
// 				if(numButtons >= 2)
// 				{
// 					gamepad->a = SDL_JoystickGetButton(joystick, 0);
// 					gamepad->b = SDL_JoystickGetButton(joystick, 1);

// 					if(numButtons >= 4)
// 					{
// 						gamepad->x = SDL_JoystickGetButton(joystick, 2);
// 						gamepad->y = SDL_JoystickGetButton(joystick, 3);

// 						for(s32 i = 5; i < numButtons; i++)
// 						{
// 							s32 back = SDL_JoystickGetButton(joystick, i);

// 							if(back)
// 							{
// 								if(!studioImpl.gamepad.backProcessed)
// 								{
// 									if(isGameMenu())
// 									{
// 										studioImpl.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
// 										studioImpl.gamepad.backProcessed = true;
// 									}
// 								}

// 								return;
// 							}
// 						}

// 						studioImpl.gamepad.backProcessed = false;
// 					}
// 				}

// 				index++;
// 			}
// 		}
// 	}

// 	if(studioImpl.mode == TIC_CONSOLE_MODE && studioImpl.gamepad.joystick.data)
// 	{
// 		studioImpl.gamepad.joystick.data = 0;
// 		setStudioMode(TIC_SURF_MODE);
// 	}
// }

// static void processGamepad()
// {
// 	studioImpl.studio.tic->ram.input.gamepads.data = 0;

// 	studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.keyboard.data;
// 	studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.touch.data;
// 	studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.joystick.data;
// }

static void processGesture()
{
	SDL_TouchID id = SDL_GetTouchDevice(0);
	s32 fingers = SDL_GetNumTouchFingers(id);

	enum{Fingers = 2};

	if(fingers == Fingers)
	{
		SDL_Point point = {0, 0};

		for(s32 f = 0; f < fingers; f++)
		{
			SDL_Finger* finger = SDL_GetTouchFinger(id, 0);

			point.x += (s32)(finger->x * TIC80_WIDTH);
			point.y += (s32)(finger->y * TIC80_HEIGHT);
		}

		point.x /= Fingers;
		point.y /= Fingers;

		studioImpl.gesture.pos = point;

		studioImpl.gesture.active = true;
	}
}

// static void processMouse()
// {
// 	studioImpl.mouse.button = SDL_GetMouseState(&studioImpl.mouse.cursor.x, &studioImpl.mouse.cursor.y);

// 	{
// 		SDL_Rect rect = {0, 0, 0, 0};
// 		calcTextureRect(&rect);

// 		if(rect.w) studioImpl.mouse.cursor.x = (studioImpl.mouse.cursor.x - rect.x) * TIC80_WIDTH / rect.w;
// 		if(rect.h) studioImpl.mouse.cursor.y = (studioImpl.mouse.cursor.y - rect.y) * TIC80_HEIGHT / rect.h;
// 	}

// 	for(int i = 0; i < COUNT_OF(studioImpl.mouse.state); i++)
// 	{
// 		MouseState* state = &studioImpl.mouse.state[i];

// 		if(!state->down && (studioImpl.mouse.button & SDL_BUTTON(i + 1)))
// 		{
// 			state->down = true;

// 			state->start.x = studioImpl.mouse.cursor.x;
// 			state->start.y = studioImpl.mouse.cursor.y;
// 		}
// 		else if(state->down && !(studioImpl.mouse.button & SDL_BUTTON(i + 1)))
// 		{
// 			state->end.x = studioImpl.mouse.cursor.x;
// 			state->end.y = studioImpl.mouse.cursor.y;

// 			state->click = true;
// 			state->down = false;
// 		}
// 	}
// }

static void goFullscreen()
{
	// studioImpl.fullscreen = !studioImpl.fullscreen;
	// SDL_SetWindowFullscreen(studioImpl.window, studioImpl.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void runProject()
{
	studioImpl.studio.tic->api.reset(studioImpl.studio.tic);

	if(studioImpl.mode == TIC_RUN_MODE)
	{
		initRunMode();
	}
	else setStudioMode(TIC_RUN_MODE);
}

static void saveProject()
{
	CartSaveResult rom = studioImpl.console->save(studioImpl.console);

	if(rom == CART_SAVE_OK)
	{
		char buffer[FILENAME_MAX];
		sprintf(buffer, "%s SAVED :)", studioImpl.console->romName);

		for(s32 i = 0; i < (s32)strlen(buffer); i++)
			buffer[i] = SDL_toupper(buffer[i]);

		showPopupMessage(buffer);
	}
	else if(rom == CART_SAVE_MISSING_NAME) showPopupMessage("SAVE: MISSING CART NAME :|");
	else showPopupMessage("SAVE ERROR :(");
}

static void screen2buffer(u32* buffer, const u32* pixels, SDL_Rect rect)
{
	pixels += rect.y * TIC80_FULLWIDTH;

	for(s32 i = 0; i < rect.h; i++)
	{
		SDL_memcpy(buffer, pixels + rect.x, rect.w * sizeof(pixels[0]));
		pixels += TIC80_FULLWIDTH;
		buffer += rect.w;
	}
}

static void setCoverImage()
{
	// tic_mem* tic = studioImpl.studio.tic;

	// if(studioImpl.mode == TIC_RUN_MODE)
	// {
	// 	enum {Pitch = TIC80_FULLWIDTH*sizeof(u32)};

	// 	tic->api.blit(tic, tic->api.scanline, tic->api.overlap, NULL);

	// 	u32* buffer = SDL_malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32));

	// 	if(buffer)
	// 	{
	// 		SDL_Rect rect = {OFFSET_LEFT, OFFSET_TOP, TIC80_WIDTH, TIC80_HEIGHT};

	// 		screen2buffer(buffer, tic->screen, rect);

	// 		gif_write_animation(studioImpl.studio.tic->cart.cover.data, &studioImpl.studio.tic->cart.cover.size,
	// 			TIC80_WIDTH, TIC80_HEIGHT, (const u8*)buffer, 1, TIC_FRAMERATE, 1);

	// 		SDL_free(buffer);

	// 		showPopupMessage("COVER IMAGE SAVED :)");
	// 	}
	// }
}

static void onVideoExported(GetResult result, void* data)
{
	if(result == FS_FILE_NOT_DOWNLOADED)
		showPopupMessage("GIF NOT EXPORTED :|");
	else if (result == FS_FILE_DOWNLOADED)
		showPopupMessage("GIF EXPORTED :)");
}

static void stopVideoRecord()
{
	if(studioImpl.video.buffer)
	{
		{
			s32 size = 0;
			u8* data = SDL_malloc(FRAME_SIZE * studioImpl.video.frame);

			gif_write_animation(data, &size, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, (const u8*)studioImpl.video.buffer, studioImpl.video.frame, TIC_FRAMERATE, getConfig()->gifScale);

			fsGetFileData(onVideoExported, "screen.gif", data, size, DEFAULT_CHMOD, NULL);
		}

		SDL_free(studioImpl.video.buffer);
		studioImpl.video.buffer = NULL;
	}

	studioImpl.video.record = false;
}

#if !defined(__EMSCRIPTEN__)

static void startVideoRecord()
{
	if(studioImpl.video.record)
	{
		stopVideoRecord();
	}
	else
	{
		studioImpl.video.frames = getConfig()->gifLength * TIC_FRAMERATE;
		studioImpl.video.buffer = SDL_malloc(FRAME_SIZE * studioImpl.video.frames);

		if(studioImpl.video.buffer)
		{
			studioImpl.video.record = true;
			studioImpl.video.frame = 0;
		}
	}
}

#endif

static void takeScreenshot()
{
	studioImpl.video.frames = 1;
	studioImpl.video.buffer = SDL_malloc(FRAME_SIZE);

	if(studioImpl.video.buffer)
	{
		studioImpl.video.record = true;
		studioImpl.video.frame = 0;
	}
}

static bool processShortcuts(SDL_KeyboardEvent* event)
{
	if(event->repeat) return false;

	SDL_Keymod mod = event->keysym.mod;

	if(studioImpl.mode == TIC_START_MODE) return true;
	if(studioImpl.mode == TIC_CONSOLE_MODE && !studioImpl.console->active) return true;

	if(isGameMenu())
	{
		switch(event->keysym.sym)
		{
		case SDLK_ESCAPE:
		case SDLK_AC_BACK:
			studioImpl.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
			// studioImpl.gamepad.backProcessed = true;
			return true;
		case SDLK_F11:
			goFullscreen();
			return true;
		case SDLK_RETURN:
			if(mod & KMOD_RALT)
			{
				goFullscreen();
				return true;
			}
			break;
		case SDLK_F7:
			setCoverImage();
			return true;
		case SDLK_F8:
			takeScreenshot();
			return true;
#if !defined(__EMSCRIPTEN__)
		case SDLK_F9:
			startVideoRecord();
			return true;
#endif
		default:
			return false;
		}
	}

	if(mod & KMOD_LALT)
	{
		switch(event->keysym.sym)
		{
		case SDLK_BACKQUOTE: setStudioMode(TIC_CONSOLE_MODE); return true;
		case SDLK_1: setStudioMode(TIC_CODE_MODE); return true;
		case SDLK_2: setStudioMode(TIC_SPRITE_MODE); return true;
		case SDLK_3: setStudioMode(TIC_MAP_MODE); return true;
		case SDLK_4: setStudioMode(TIC_SFX_MODE); return true;
		case SDLK_5: setStudioMode(TIC_MUSIC_MODE); return true;
		default:  break;
		}
	}
    else if(mod & KMOD_LCTRL)
    {
        switch(event->keysym.sym)
        {
        case SDLK_PAGEUP: changeStudioMode(-1); return true;
        case SDLK_PAGEDOWN: changeStudioMode(1); return true;
        }
    }
	else
	{
		switch(event->keysym.sym)
		{
		case SDLK_F1: setStudioMode(TIC_CODE_MODE); return true;
		case SDLK_F2: setStudioMode(TIC_SPRITE_MODE); return true;
		case SDLK_F3: setStudioMode(TIC_MAP_MODE); return true;
		case SDLK_F4: setStudioMode(TIC_SFX_MODE); return true;
		case SDLK_F5: setStudioMode(TIC_MUSIC_MODE); return true;
		case SDLK_F7: setCoverImage(); return true;
		case SDLK_F8: takeScreenshot(); return true;
#if !defined(__EMSCRIPTEN__)
		case SDLK_F9: startVideoRecord(); return true;
#endif
		default:  break;
		}
	}

	switch(event->keysym.sym)
	{
	case SDLK_q:
		if(mod & TIC_MOD_CTRL)
		{
			exitStudio();
			return true;
		}
		break;
	case SDLK_r:
		if(mod & TIC_MOD_CTRL)
		{
			runProject();
			return true;
		}
		break;
	case SDLK_s:
		if(mod & TIC_MOD_CTRL)
		{
			saveProject();
			return true;
		}
		break;
	case SDLK_F11: goFullscreen(); return true;
	case SDLK_RETURN:
		if(mod & KMOD_RALT)
		{
			goFullscreen();
			return true;
		}
		else if(mod & TIC_MOD_CTRL)
		{
			runProject();
			return true;
		}
		break;
	case SDLK_ESCAPE:
	case SDLK_AC_BACK:
		{
			Code* code = studioImpl.editor[studioImpl.bank.index.code].code;

			if(studioImpl.mode == TIC_CODE_MODE && code->mode != TEXT_EDIT_MODE)
			{
				code->escape(code);
				return true;
			}

			if(studioImpl.mode == TIC_DIALOG_MODE)
			{
				studioImpl.dialog->escape(studioImpl.dialog);
				return true;
			}

			setStudioMode(studioImpl.mode == TIC_CONSOLE_MODE ? studioImpl.prevMode : TIC_CONSOLE_MODE);
		}
		return true;
	default: break;
	}

	return false;
}

// static void processGamepad()
// {
// 	// processKeyboardGamepad();

// #if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
// 	processTouchGamepad();
// #endif
// 	processJoysticks();
	
// 	{
// 		studioImpl.studio.tic->ram.input.gamepads.data = 0;

// 		// studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.keyboard.data;
// 		studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.touch.data;
// 		studioImpl.studio.tic->ram.input.gamepads.data |= studioImpl.gamepad.joystick.data;
// 	}
// }

// static void processMouseInput()
// {
// 	processJoysticksWithMouseInput();

// 	s32 x = studioImpl.mouse.cursor.x;
// 	s32 y = studioImpl.mouse.cursor.y;

// 	if(x < 0) x = 0;
// 	if(y < 0) y = 0;
// 	if(x >= TIC80_WIDTH) x = TIC80_WIDTH-1;
// 	if(y >= TIC80_HEIGHT) y = TIC80_HEIGHT-1;

// 	tic80_mouse* mouse = &studioImpl.studio.tic->ram.input.mouse;
// 	mouse->x = x;
// 	mouse->y = y;
// 	mouse->left = studioImpl.mouse.state[0].down ? 1 : 0;
// 	mouse->middle = studioImpl.mouse.state[1].down ? 1 : 0;
// 	mouse->right = studioImpl.mouse.state[2].down ? 1 : 0;
// }

// static void processKeyboardInput()
// {
// 	static const u8 KeyboardCodes[] = 
// 	{
// 		#include "keycodes.c"
// 	};

// 	tic80_input* input = &studioImpl.studio.tic->ram.input;
// 	input->keyboard.data = 0;

// 	studioImpl.keyboard = SDL_GetKeyboardState(NULL);

// 	for(s32 i = 0, c = 0; i < COUNT_OF(KeyboardCodes) && c < COUNT_OF(input->keyboard.keys); i++)
// 		if(studioImpl.keyboard[i] && KeyboardCodes[i] > tic_key_unknown)
// 			input->keyboard.keys[c++] = KeyboardCodes[i];
// }

#if defined(TIC80_PRO)

static void reloadConfirm(bool yes, void* data)
{
	if(yes)
		studioImpl.console->updateProject(studioImpl.console);
}

#endif

// SDL_Event* pollEvent()
// {
// 	static SDL_Event event;

// 	if(SDL_PollEvent(&event))
// 	{
// 		switch(event.type)
// 		{
// 		case SDL_KEYDOWN:
// 			if(processShortcuts(&event.key)) return NULL;
// 			break;
// 		case SDL_JOYDEVICEADDED:
// 			{
// 				s32 id = event.jdevice.which;

// 				if (id < TIC_GAMEPADS)
// 				{
// 					if(studioImpl.joysticks[id])
// 						SDL_JoystickClose(studioImpl.joysticks[id]);

// 					studioImpl.joysticks[id] = SDL_JoystickOpen(id);
// 				}
// 			}
// 			break;

// 		case SDL_JOYDEVICEREMOVED:
// 			{
// 				s32 id = event.jdevice.which;

// 				if (id < TIC_GAMEPADS && studioImpl.joysticks[id])
// 				{
// 					SDL_JoystickClose(studioImpl.joysticks[id]);
// 					studioImpl.joysticks[id] = NULL;
// 				}
// 			}
// 			break;
// 		case SDL_WINDOWEVENT:
// 			switch(event.window.event)
// 			{
// 			case SDL_WINDOWEVENT_RESIZED: updateGamepadParts(); break;
// 			case SDL_WINDOWEVENT_FOCUS_GAINED:

// #if defined(TIC80_PRO)

// 				if(studioImpl.mode != TIC_START_MODE)
// 				{
// 					Console* console = studioImpl.console;

// 					u64 mdate = fsMDate(console->fs, console->romName);

// 					if(studioImpl.cart.mdate && mdate > studioImpl.cart.mdate)
// 					{
// 						if(studioCartChanged())
// 						{
// 							static const char* Rows[] =
// 							{
// 								"",
// 								"CART HAS CHANGED!",
// 								"",
// 								"DO YOU WANT",
// 								"TO RELOAD IT?"
// 							};

// 							showDialog(Rows, COUNT_OF(Rows), reloadConfirm, NULL);
// 						}
// 						else console->updateProject(console);						
// 					}
// 				}

// #endif
// 				{
// 					Code* code = studioImpl.editor[studioImpl.bank.index.code].code;
// 					studioImpl.console->codeLiveReload.reload(studioImpl.console, code->src);
// 					if(studioImpl.console->codeLiveReload.active && code->update)
// 						code->update(code);
// 				}
// 				break;
// 			}
// 			break;
// 		case SDL_FINGERUP:
// 			showSoftKeyboard();
// 			break;
// 		case SDL_QUIT:
// 			exitStudio();
// 			break;
// 		default:
// 			break;
// 		}

// 		return &event;
// 	}

// 	if(studioImpl.mode != TIC_RUN_MODE)
// 		processGesture();

// 	if(!studioImpl.gesture.active)
// 		processMouse();

// 	if(studioImpl.mode == TIC_RUN_MODE)
// 	{
// 		if(studioImpl.studio.tic->input.gamepad) 	processGamepadInput();
// 		if(studioImpl.studio.tic->input.mouse) 	processMouseInput();
// 		if(studioImpl.studio.tic->input.keyboard) 	processKeyboardInput();
// 	}
// 	else
// 	{
// 		processGamepadInput();
// 	}

// 	return NULL;
// }

// static void transparentBlit(u32* out, s32 pitch)
// {
// 	const u8* in = studioImpl.studio.tic->ram.vram.screen.data;
// 	const u8* end = in + sizeof(studioImpl.studio.tic->ram.vram.screen);
// 	const u32* pal = tic_palette_blit(&studioImpl.studio.tic->config.palette);
// 	const u32 Delta = (pitch/sizeof *out - TIC80_WIDTH);

// 	s32 col = 0;

// 	while(in != end)
// 	{
// 		u8 low = *in & 0x0f;
// 		u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
// 		*out++ = low ? (*(pal + low) | 0xff000000) : 0;
// 		*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;
// 		in++;

// 		col += BITS_IN_BYTE / TIC_PALETTE_BPP;

// 		if (col == TIC80_WIDTH)
// 		{
// 			col = 0;
// 			out += Delta;
// 		}
// 	}
// }

// static void blitSound()
// {
// 	SDL_PauseAudioDevice(studioImpl.audio.device, 0);

// 	if(studioImpl.audio.cvt.needed)
// 	{
// 		SDL_memcpy(studioImpl.audio.cvt.buf, studioImpl.studio.tic->samples.buffer, studioImpl.studio.tic->samples.size);
// 		SDL_ConvertAudio(&studioImpl.audio.cvt);
// 		SDL_QueueAudio(studioImpl.audio.device, studioImpl.audio.cvt.buf, studioImpl.audio.cvt.len_cvt);
// 	}
// 	else SDL_QueueAudio(studioImpl.audio.device, studioImpl.studio.tic->samples.buffer, studioImpl.studio.tic->samples.size);
// }

static void drawRecordLabel(u32* frame, s32 sx, s32 sy, const u32* color)
{
	static const u16 RecLabel[] =
	{
		0b0111001100110011,
		0b1111101010100100,
		0b1111101100110100,
		0b1111101010100100,
		0b0111001010110011,
	};

	for(s32 y = 0; y < 5; y++)
	{
		for(s32 x = 0; x < sizeof RecLabel[0]*BITS_IN_BYTE; x++)
		{
			if(RecLabel[y] & (1 << x))
				memcpy(&frame[sx + 15 - x + ((y+sy) << TIC80_FULLWIDTH_BITS)], color, sizeof *color);
		}
	}
}

static void drawDesyncLabel(u32* frame)
{
	if(getConfig()->showSync && studioImpl.missedFrame)
	{
		static const u16 DesyncLabel[] =
		{
			0b0110101010010011,
			0b1000101011010100,
			0b1110111010110100,
			0b0010001010010100,
			0b1100110010010011,
		};
		
		enum{sx = TIC80_WIDTH-24, sy = 8, Cols = sizeof DesyncLabel[0]*BITS_IN_BYTE, Rows = COUNT_OF(DesyncLabel)};

		const u32* pal = tic_palette_blit(&studioImpl.studio.tic->config.palette);
		const u32* color = &pal[tic_color_red];

		for(s32 y = 0; y < Rows; y++)
		{
			for(s32 x = 0; x < Cols; x++)
			{
				if(DesyncLabel[y] & (1 << x))
					memcpy(&frame[sx + Cols - 1 - x + ((y+sy) << TIC80_FULLWIDTH_BITS)], color, sizeof *color);
			}
		}
	}
}

static void recordFrame(u32* pixels)
{
	if(studioImpl.video.record)
	{
		if(studioImpl.video.frame < studioImpl.video.frames)
		{
			SDL_Rect rect = {0, 0, TIC80_FULLWIDTH, TIC80_FULLHEIGHT};
			screen2buffer(studioImpl.video.buffer + (TIC80_FULLWIDTH*TIC80_FULLHEIGHT) * studioImpl.video.frame, pixels, rect);

			if(studioImpl.video.frame % TIC_FRAMERATE < TIC_FRAMERATE / 2)
			{
				const u32* pal = tic_palette_blit(&studioImpl.studio.tic->config.palette);
				drawRecordLabel(pixels, TIC80_WIDTH-24, 8, &pal[tic_color_red]);
			}

			studioImpl.video.frame++;

		}
		else
		{
			stopVideoRecord();
		}
	}
}

// static void blitTexture()
// {
// 	tic_mem* tic = studioImpl.studio.tic;
// 	SDL_Rect rect = {0, 0, 0, 0};
// 	calcTextureRect(&rect);

// 	void* pixels = NULL;
// 	s32 pitch = 0;
// 	SDL_LockTexture(studioImpl.texture, NULL, &pixels, &pitch);

// 	tic_scanline scanline = NULL;
// 	tic_overlap overlap = NULL;
// 	void* data = NULL;

// 	switch(studioImpl.mode)
// 	{
// 	case TIC_RUN_MODE:
// 		scanline = tic->api.scanline;
// 		overlap = tic->api.overlap;
// 		break;
// 	case TIC_SPRITE_MODE:
// 		{
// 			Sprite* sprite = studioImpl.editor[studioImpl.bank.index.sprites].sprite;
// 			overlap = sprite->overlap;
// 			data = sprite;
// 		}
// 		break;
// 	case TIC_MAP_MODE:
// 		{
// 			Map* map = studioImpl.editor[studioImpl.bank.index.map].map;
// 			overlap = map->overlap;
// 			data = map;
// 		}
// 		break;
// 	default:
// 		break;
// 	}

// 	tic->api.blit(tic, scanline, overlap, data);
// 	SDL_memcpy(pixels, tic->screen, sizeof tic->screen);

// 	recordFrame(pixels);
// 	drawDesyncLabel(pixels);

// 	SDL_UnlockTexture(studioImpl.texture);

// 	{
// 		enum {Header = OFFSET_TOP};
// 		SDL_Rect srcRect = {0, 0, TIC80_FULLWIDTH, Header};
// 		SDL_Rect dstRect = {0};
// 		SDL_GetWindowSize(studioImpl.window, &dstRect.w, &dstRect.h);
// 		dstRect.h = rect.y;
// 		SDL_RenderCopy(studioImpl.renderer, studioImpl.texture, &srcRect, &dstRect);
// 	}

// 	{
// 		enum {Header = OFFSET_TOP};
// 		SDL_Rect srcRect = {0, TIC80_FULLHEIGHT - Header, TIC80_FULLWIDTH, Header};
// 		SDL_Rect dstRect = {0};
// 		SDL_GetWindowSize(studioImpl.window, &dstRect.w, &dstRect.h);
// 		dstRect.y = rect.y + rect.h;
// 		dstRect.h = rect.y;
// 		SDL_RenderCopy(studioImpl.renderer, studioImpl.texture, &srcRect, &dstRect);
// 	}

// 	{
// 		enum {Header = OFFSET_TOP};
// 		enum {Left = OFFSET_LEFT};
// 		SDL_Rect srcRect = {0, Header, Left, TIC80_HEIGHT};
// 		SDL_Rect dstRect = {0};
// 		SDL_GetWindowSize(studioImpl.window, &dstRect.w, &dstRect.h);
// 		dstRect.y = rect.y;
// 		dstRect.h = rect.h;
// 		SDL_RenderCopy(studioImpl.renderer, studioImpl.texture, &srcRect, &dstRect);
// 	}

// 	{
// 		enum {Top = OFFSET_TOP};
// 		enum {Left = OFFSET_LEFT};

// 		SDL_Rect srcRect = {Left, Top, TIC80_WIDTH, TIC80_HEIGHT};

// 		SDL_RenderCopy(studioImpl.renderer, studioImpl.texture, &srcRect, &rect);
// 	}
// }

static void blitCursor(const u8* in)
{
	// if(!studioImpl.mouse.texture)
	// {
	// 	studioImpl.mouse.texture = SDL_CreateTexture(studioImpl.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TIC_SPRITESIZE, TIC_SPRITESIZE);
	// 	SDL_SetTextureBlendMode(studioImpl.mouse.texture, SDL_BLENDMODE_BLEND);
	// }

	// if(studioImpl.mouse.src != in)
	// {
	// 	studioImpl.mouse.src = in;

	// 	void* pixels = NULL;
	// 	s32 pitch = 0;
	// 	SDL_LockTexture(studioImpl.mouse.texture, NULL, &pixels, &pitch);

	// 	{
	// 		const u8* end = in + sizeof(tic_tile);
	// 		const u32* pal = tic_palette_blit(&studioImpl.studio.tic->ram.vram.palette);
	// 		u32* out = pixels;

	// 		while(in != end)
	// 		{
	// 			u8 low = *in & 0x0f;
	// 			u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
	// 			*out++ = low ? (*(pal + low) | 0xff000000) : 0;
	// 			*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;

	// 			in++;
	// 		}
	// 	}

	// 	SDL_UnlockTexture(studioImpl.mouse.texture);
	// }

	// SDL_Rect rect = {0, 0, 0, 0};
	// calcTextureRect(&rect);
	// s32 scale = rect.w / TIC80_WIDTH;

	// SDL_Rect src = {0, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
	// SDL_Rect dst = {0, 0, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale};

	// SDL_GetMouseState(&dst.x, &dst.y);

	// if(getConfig()->theme.cursor.pixelPerfect)
	// {
	// 	dst.x -= (dst.x - rect.x) % scale;
	// 	dst.y -= (dst.y - rect.y) % scale;
	// }

	// if(SDL_GetWindowFlags(studioImpl.window) & SDL_WINDOW_MOUSE_FOCUS)
	// 	SDL_RenderCopy(studioImpl.renderer, studioImpl.mouse.texture, &src, &dst);
}

static void renderCursor()
{
	if(studioImpl.mode == TIC_RUN_MODE && !studioImpl.studio.tic->input.mouse)
	{
		SDL_ShowCursor(SDL_DISABLE);
		return;
	}
	if(studioImpl.mode == TIC_RUN_MODE && studioImpl.studio.tic->ram.vram.vars.cursor)
	{
		SDL_ShowCursor(SDL_DISABLE);
		blitCursor(studioImpl.studio.tic->ram.sprites.data[studioImpl.studio.tic->ram.vram.vars.cursor].data);
		return;
	}

	SDL_ShowCursor(getConfig()->theme.cursor.sprite >= 0 ? SDL_DISABLE : SDL_ENABLE);

	if(getConfig()->theme.cursor.sprite >= 0)
		blitCursor(studioImpl.studio.tic->config.bank0.tiles.data[getConfig()->theme.cursor.sprite].data);
}

static void useSystemPalette()
{
	memcpy(studioImpl.studio.tic->ram.vram.palette.data, studioImpl.studio.tic->config.palette.data, sizeof(tic_palette));
}

static void drawPopup()
{
	if(studioImpl.popup.counter > 0)
	{
		studioImpl.popup.counter--;

		s32 anim = 0;

		enum{Dur = TIC_FRAMERATE/2};

		if(studioImpl.popup.counter < Dur)
			anim = -((Dur - studioImpl.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);
		else if(studioImpl.popup.counter >= (POPUP_DUR - Dur))
			anim = (((POPUP_DUR - Dur) - studioImpl.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);

		studioImpl.studio.tic->api.rect(studioImpl.studio.tic, 0, anim, TIC80_WIDTH, TIC_FONT_HEIGHT+1, (tic_color_red));
		studioImpl.studio.tic->api.text(studioImpl.studio.tic, studioImpl.popup.message, 
			(s32)(TIC80_WIDTH - strlen(studioImpl.popup.message)*TIC_FONT_WIDTH)/2,
			anim + 1, (tic_color_white));
	}
}

static void renderStudio()
{
	tic_mem* tic = studioImpl.studio.tic;

	showTooltip("");

	studioImpl.gesture.active = false;
	// studioImpl.mouse.cursor.x = studioImpl.mouse.cursor.y = -1;
	// for(int i = 0; i < COUNT_OF(studioImpl.mouse.state); i++)
	// 	studioImpl.mouse.state[i].click = false;

	{
		const tic_sfx* sfx = NULL;
		const tic_music* music = NULL;

		switch(studioImpl.mode)
		{
		case TIC_RUN_MODE:
			sfx = &studioImpl.studio.tic->ram.sfx;
			music = &studioImpl.studio.tic->ram.music;
			break;
		case TIC_START_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
		case TIC_SURF_MODE:
			sfx = &studioImpl.studio.tic->config.bank0.sfx;
			music = &studioImpl.studio.tic->config.bank0.music;
			break;
		default:
			sfx = &studioImpl.studio.tic->cart.banks[studioImpl.bank.index.sfx].sfx;
			music = &studioImpl.studio.tic->cart.banks[studioImpl.bank.index.music].music;
		}

		studioImpl.studio.tic->api.tick_start(studioImpl.studio.tic, sfx, music);
	}

	switch(studioImpl.mode)
	{
	case TIC_START_MODE:	studioImpl.start->tick(studioImpl.start); break;
	case TIC_CONSOLE_MODE: 	studioImpl.console->tick(studioImpl.console); break;
	case TIC_RUN_MODE: 		studioImpl.run->tick(studioImpl.run); break;
	case TIC_CODE_MODE: 	
		{
			Code* code = studioImpl.editor[studioImpl.bank.index.code].code;
			code->tick(code);
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = studioImpl.editor[studioImpl.bank.index.sprites].sprite;
			sprite->tick(sprite);		
		}
		break;
	case TIC_MAP_MODE:
		{
			Map* map = studioImpl.editor[studioImpl.bank.index.map].map;
			map->tick(map);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = studioImpl.editor[studioImpl.bank.index.sfx].sfx;
			sfx->tick(sfx);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = studioImpl.editor[studioImpl.bank.index.music].music;
			music->tick(music);
		}
		break;

	case TIC_WORLD_MODE:	studioImpl.world->tick(studioImpl.world); break;
	case TIC_DIALOG_MODE:	studioImpl.dialog->tick(studioImpl.dialog); break;
	case TIC_MENU_MODE:		studioImpl.menu->tick(studioImpl.menu); break;
	case TIC_SURF_MODE:		studioImpl.surf->tick(studioImpl.surf); break;
	default: break;
	}

	drawPopup();

	if(getConfig()->noSound)
		SDL_memset(tic->ram.registers, 0, sizeof tic->ram.registers);

	studioImpl.studio.tic->api.tick_end(studioImpl.studio.tic);

	if(studioImpl.mode != TIC_RUN_MODE)
		useSystemPalette();
	
	// blitTexture();

	// renderCursor();
}

// static void updateGamepadParts()
// {
// 	s32 tileSize = TIC_SPRITESIZE;
// 	s32 offset = 0;
// 	SDL_Rect rect;

// 	const s32 JoySize = 3;
// 	SDL_GetWindowSize(studioImpl.window, &rect.w, &rect.h);

// 	if(rect.w < rect.h)
// 	{
// 		tileSize = rect.w / 2 / JoySize;
// 		offset = (rect.h * 2 - JoySize * tileSize) / 3;
// 	}
// 	else
// 	{
// 		tileSize = rect.w / 5 / JoySize;
// 		offset = (rect.h - JoySize * tileSize) / 2;
// 	}

// 	studioImpl.gamepad.part.size = tileSize;
// 	studioImpl.gamepad.part.axis = (SDL_Point){0, offset};
// 	studioImpl.gamepad.part.a = (SDL_Point){rect.w - 2*tileSize, 2*tileSize + offset};
// 	studioImpl.gamepad.part.b = (SDL_Point){rect.w - 1*tileSize, 1*tileSize + offset};
// 	studioImpl.gamepad.part.x = (SDL_Point){rect.w - 3*tileSize, 1*tileSize + offset};
// 	studioImpl.gamepad.part.y = (SDL_Point){rect.w - 2*tileSize, 0*tileSize + offset};
// }

// static void renderGamepad()
// {
// 	if(studioImpl.gamepad.show || studioImpl.gamepad.alpha); else return;

// 	const s32 tileSize = studioImpl.gamepad.part.size;
// 	const SDL_Point axis = studioImpl.gamepad.part.axis;
// 	typedef struct { bool press; s32 x; s32 y;} Tile;
// 	const Tile Tiles[] =
// 	{
// 		{studioImpl.studio.tic->ram.input.gamepads.first.up, 		axis.x + 1*tileSize, axis.y + 0*tileSize},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.down, 	axis.x + 1*tileSize, axis.y + 2*tileSize},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.left, 	axis.x + 0*tileSize, axis.y + 1*tileSize},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.right, 	axis.x + 2*tileSize, axis.y + 1*tileSize},

// 		{studioImpl.studio.tic->ram.input.gamepads.first.a, 		studioImpl.gamepad.part.a.x, studioImpl.gamepad.part.a.y},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.b, 		studioImpl.gamepad.part.b.x, studioImpl.gamepad.part.b.y},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.x, 		studioImpl.gamepad.part.x.x, studioImpl.gamepad.part.x.y},
// 		{studioImpl.studio.tic->ram.input.gamepads.first.y, 		studioImpl.gamepad.part.y.x, studioImpl.gamepad.part.y.y},
// 	};

// 	enum {ButtonsCount = 8};

// 	for(s32 i = 0; i < COUNT_OF(Tiles); i++)
// 	{
// 		const Tile* tile = Tiles + i;
// 		SDL_Rect src = {(tile->press ? ButtonsCount + i : i) * TIC_SPRITESIZE, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
// 		SDL_Rect dest = {tile->x, tile->y, tileSize, tileSize};

// 		SDL_RenderCopy(studioImpl.renderer, studioImpl.gamepad.texture, &src, &dest);
// 	}

// 	if(!studioImpl.gamepad.show && studioImpl.gamepad.alpha)
// 	{
// 		enum {Step = 3};

// 		if(studioImpl.gamepad.alpha - Step >= 0) studioImpl.gamepad.alpha -= Step;
// 		else studioImpl.gamepad.alpha = 0;

// 		SDL_SetTextureAlphaMod(studioImpl.gamepad.texture, studioImpl.gamepad.alpha);
// 	}

// 	studioImpl.gamepad.counter = studioImpl.gamepad.touch.data ? 0 : studioImpl.gamepad.counter+1;

// 	// wait 5 seconds and hide touch gamepad
// 	if(studioImpl.gamepad.counter >= 5 * TIC_FRAMERATE)
// 		studioImpl.gamepad.show = false;
// }

// static void tick()
// {
// 	if(!studioImpl.fs) return;

// 	if(studioImpl.studio.quit)
// 	{
// #if defined __EMSCRIPTEN__
// 		studioImpl.studio.tic->api.clear(studioImpl.studio.tic, TIC_COLOR_BG);
// 		blitTexture();
// 		emscripten_cancel_main_loop();
// #endif
// 		return;
// 	}

// 	SDL_SystemCursor cursor = studioImpl.mouse.system;
// 	studioImpl.mouse.system = SDL_SYSTEM_CURSOR_ARROW;

// 	SDL_RenderClear(studioImpl.renderer);

// 	renderStudio();

// 	if(studioImpl.mode == TIC_RUN_MODE && studioImpl.studio.tic->input.gamepad)
// 		renderGamepad();

// 	if(studioImpl.mode == TIC_MENU_MODE || studioImpl.mode == TIC_SURF_MODE)
// 		renderGamepad();

// 	if(studioImpl.mouse.system != cursor)
// 		SDL_SetCursor(SDL_CreateSystemCursor(studioImpl.mouse.system));

// 	SDL_RenderPresent(studioImpl.renderer);

// 	blitSound();
// }

// static void initSound()
// {
// 	SDL_AudioSpec want =
// 	{
// 		.freq = 44100,
// 		.format = AUDIO_S16,
// 		.channels = 1,
// 		.userdata = NULL,
// 	};

// 	studioImpl.audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &studioImpl.audio.spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

// 	SDL_BuildAudioCVT(&studioImpl.audio.cvt, want.format, want.channels, studioImpl.audio.spec.freq, studioImpl.audio.spec.format, studioImpl.audio.spec.channels, studioImpl.audio.spec.freq);

// 	if(studioImpl.audio.cvt.needed)
// 	{
// 		studioImpl.audio.cvt.len = studioImpl.audio.spec.freq * sizeof studioImpl.studio.tic->samples.buffer[0] / TIC_FRAMERATE;
// 		studioImpl.audio.cvt.buf = SDL_malloc(studioImpl.audio.cvt.len * studioImpl.audio.cvt.len_mult);
// 	}
// }

// static void initTouchGamepad()
// {
// 	if (!studioImpl.renderer)
// 		return;

// 	studioImpl.studio.tic->api.map(studioImpl.studio.tic, &studioImpl.studio.tic->config.bank0.map, &studioImpl.studio.tic->config.bank0.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, -1, 1);

// 	if(!studioImpl.gamepad.texture)
// 	{
// 		studioImpl.gamepad.texture = SDL_CreateTexture(studioImpl.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);
// 		SDL_SetTextureBlendMode(studioImpl.gamepad.texture, SDL_BLENDMODE_BLEND);
// 	}

// 	{
// 		void* pixels = NULL;
// 		s32 pitch = 0;

// 		SDL_LockTexture(studioImpl.gamepad.texture, NULL, &pixels, &pitch);
// 		transparentBlit(pixels, pitch);
// 		SDL_UnlockTexture(studioImpl.gamepad.texture);
// 	}

// 	updateGamepadParts();
// }

static void updateSystemFont()
{
	SDL_memset(studioImpl.studio.tic->font.data, 0, sizeof(tic_font));

	for(s32 i = 0; i < TIC_FONT_CHARS; i++)
		for(s32 y = 0; y < TIC_SPRITESIZE; y++)
			for(s32 x = 0; x < TIC_SPRITESIZE; x++)
				if(tic_tool_peek4(&studioImpl.studio.tic->config.bank0.sprites.data[i], TIC_SPRITESIZE*(y+1) - x-1))
					studioImpl.studio.tic->font.data[i*BITS_IN_BYTE+y] |= 1 << x;
}

void studioConfigChanged()
{
	Code* code = studioImpl.editor[studioImpl.bank.index.code].code;
	if(code->update)
		code->update(code);

	// initTouchGamepad();
	updateSystemFont();
}

// static void setWindowIcon()
// {
// 	enum{ Size = 64, TileSize = 16, ColorKey = 14, Cols = TileSize / TIC_SPRITESIZE, Scale = Size/TileSize};
// 	studioImpl.studio.tic->api.clear(studioImpl.studio.tic, 0);

// 	u32* pixels = SDL_malloc(Size * Size * sizeof(u32));

// 	const u32* pal = tic_palette_blit(&studioImpl.studio.tic->config.palette);

// 	for(s32 j = 0, index = 0; j < Size; j++)
// 		for(s32 i = 0; i < Size; i++, index++)
// 		{
// 			u8 color = getSpritePixel(studioImpl.studio.tic->config.bank0.tiles.data, i/Scale, j/Scale);
// 			pixels[index] = color == ColorKey ? 0 : pal[color];
// 		}

// 	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, Size, Size,
// 		sizeof(s32) * BITS_IN_BYTE, Size * sizeof(s32),
// 		0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

// 	SDL_SetWindowIcon(studioImpl.window, surface);
// 	SDL_FreeSurface(surface);
// 	SDL_free(pixels);
// }

u32 unzip(u8** dest, const u8* source, size_t size)
{
	// TODO: remove this size
	enum{DestSize = 10*1024*1024};

	unsigned long destSize = DestSize;

	u8* buffer = (u8*)SDL_malloc(destSize);

	if(buffer)
	{
		if(uncompress(buffer, &destSize, source, (unsigned long)size) == Z_OK)
		{
			*dest = (u8*)SDL_malloc(destSize+1);
			memcpy(*dest, buffer, destSize);
			(*dest)[destSize] = '\0';
		}

		SDL_free(buffer);

		return destSize;
	}

	return 0;
}

static void initKeymap()
{
	// FileSystem* fs = studioImpl.fs;

	// s32 size = 0;
	// u8* data = (u8*)fsLoadFile(fs, KEYMAP_DAT_PATH, &size);

	// if(data)
	// {
	// 	if(size == KEYMAP_SIZE)
	// 		memcpy(getKeymap(), data, KEYMAP_SIZE);

	// 	SDL_free(data);
	// }
}

static void onFSInitialized(FileSystem* fs)
{
	studioImpl.fs = fs;

	// SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
	// SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	// SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

// 	studioImpl.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
// 		(TIC80_FULLWIDTH) * STUDIO_UI_SCALE,
// 		(TIC80_FULLHEIGHT) * STUDIO_UI_SCALE,
// 		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
// #if defined(__CHIP__)
// 			| SDL_WINDOW_FULLSCREEN_DESKTOP
// #endif
// 		);

	// initSound();

	studioImpl.tic80local = (tic80_local*)tic80_create(studioImpl.samplerate);
	studioImpl.studio.tic = studioImpl.tic80local->memory;

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			studioImpl.editor[i].code	= SDL_calloc(1, sizeof(Code));
			studioImpl.editor[i].sprite	= SDL_calloc(1, sizeof(Sprite));
			studioImpl.editor[i].map 	= SDL_calloc(1, sizeof(Map));
			studioImpl.editor[i].sfx 	= SDL_calloc(1, sizeof(Sfx));
			studioImpl.editor[i].music 	= SDL_calloc(1, sizeof(Music));
		}

		studioImpl.start 	= SDL_calloc(1, sizeof(Start));
		studioImpl.console 	= SDL_calloc(1, sizeof(Console));
		studioImpl.run 		= SDL_calloc(1, sizeof(Run));
		studioImpl.world 	= SDL_calloc(1, sizeof(World));
		studioImpl.config 	= SDL_calloc(1, sizeof(Config));
		studioImpl.dialog 	= SDL_calloc(1, sizeof(Dialog));
		studioImpl.menu 	= SDL_calloc(1, sizeof(Menu));
		studioImpl.surf 	= SDL_calloc(1, sizeof(Surf));
	}

	fsMakeDir(fs, TIC_LOCAL);
	initConfig(studioImpl.config, studioImpl.studio.tic, studioImpl.fs);

	initKeymap();

	initStart(studioImpl.start, studioImpl.studio.tic);
	initConsole(studioImpl.console, studioImpl.studio.tic, studioImpl.fs, studioImpl.config, studioImpl.argc, studioImpl.argv);
	initSurfMode();

	initRunMode();

	initModules();

	if(studioImpl.console->skipStart)
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	if(studioImpl.console->goFullscreen)
	{
		goFullscreen();
	}

	// // set the window icon before renderer is created (issues on Linux)
	// setWindowIcon();

// 	studioImpl.renderer = SDL_CreateRenderer(studioImpl.window, -1, 
// #if defined(__CHIP__)
// 		SDL_RENDERER_SOFTWARE
// #elif defined(__EMSCRIPTEN__)
// 		SDL_RENDERER_ACCELERATED
// #else
// 		SDL_RENDERER_ACCELERATED | (getConfig()->useVsync ? SDL_RENDERER_PRESENTVSYNC : 0)
// #endif
// 	);

// 	studioImpl.texture = SDL_CreateTexture(studioImpl.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	// initTouchGamepad();
}

#if defined(__EMSCRIPTEN__)

#define DEFAULT_CART "cart.tic"

static void onEmscriptenWget(const char* file)
{
	studioImpl.argv[1] = DEFAULT_CART;
	createFileSystem(NULL, onFSInitialized);
}

static void onEmscriptenWgetError(const char* error) {}

static void emstick()
{
	static double nextTick = -1.0;

	studioImpl.missedFrame = false;

	if(nextTick < 0.0)
		nextTick = emscripten_get_now();

	nextTick += 1000.0/TIC_FRAMERATE;
	tick();
	double delay = nextTick - emscripten_get_now();

	if(delay < 0.0)
	{
		nextTick -= delay;
		studioImpl.missedFrame = true;
	}
	else
		emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, delay);
}

#endif

Studio* studioInit(s32 argc, char **argv, s32 samplerate)
{
	setbuf(stdout, NULL);
	studioImpl.argc = argc;
	studioImpl.argv = argv;
	studioImpl.samplerate = samplerate;

#if defined(__EMSCRIPTEN__)

	if(studioImpl.argc == 2)
	{
		emscripten_async_wget(studioImpl.argv[1], DEFAULT_CART, onEmscriptenWget, onEmscriptenWgetError);
	}
	else createFileSystem(NULL, onFSInitialized);

	emscripten_set_main_loop(emstick, TIC_FRAMERATE, 1);

#else

	createFileSystem(argc > 1 && fsExists(argv[1]) ? fsBasename(argv[1]) : NULL, onFSInitialized);

#endif

	return &studioImpl.studio;
}

static void processMouseStates()
{
	for(int i = 0; i < COUNT_OF(studioImpl.mouse.state); i++)
		studioImpl.mouse.state[i].click = false;

	tic_mem* tic = studioImpl.studio.tic;

	for(int i = 0; i < COUNT_OF(studioImpl.mouse.state); i++)
	{
		MouseState* state = &studioImpl.mouse.state[i];

		if(!state->down && tic->ram.input.mouse.btns & (1 << i))
		{
			state->down = true;

			state->start.x = tic->ram.input.mouse.x;
			state->start.y = tic->ram.input.mouse.y;
		}
		else if(state->down && tic->ram.input.mouse.btns & (1 << i))
		{
			state->end.x = tic->ram.input.mouse.x;
			state->end.y = tic->ram.input.mouse.y;

			state->click = true;
			state->down = false;
		}
	}
}

void studioTick(void* pixels)
{
	processMouseStates();
	processGamepadMapping();

	renderStudio();

	tic_mem* tic = studioImpl.studio.tic;
	
	{
		tic_scanline scanline = NULL;
		tic_overlap overlap = NULL;
		void* data = NULL;

		switch(studioImpl.mode)
		{
		case TIC_RUN_MODE:
			scanline = tic->api.scanline;
			overlap = tic->api.overlap;
			break;
		case TIC_SPRITE_MODE:
			{
				Sprite* sprite = studioImpl.editor[studioImpl.bank.index.sprites].sprite;
				overlap = sprite->overlap;
				data = sprite;
			}
			break;
		case TIC_MAP_MODE:
			{
				Map* map = studioImpl.editor[studioImpl.bank.index.map].map;
				overlap = map->overlap;
				data = map;
			}
			break;
		default:
			break;
		}

		tic->api.blit(tic, scanline, overlap, data);
		SDL_memcpy(pixels, tic->screen, sizeof tic->screen);

		recordFrame(pixels);
		drawDesyncLabel(pixels);
	}
}

void studioClose()
{
	closeNet(studioImpl.surf->net);

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			SDL_free(studioImpl.editor[i].code);
			SDL_free(studioImpl.editor[i].sprite);
			SDL_free(studioImpl.editor[i].map);
			SDL_free(studioImpl.editor[i].sfx);
			SDL_free(studioImpl.editor[i].music);
		}

		SDL_free(studioImpl.start);
		SDL_free(studioImpl.console);
		SDL_free(studioImpl.run);
		SDL_free(studioImpl.world);
		SDL_free(studioImpl.config);
		SDL_free(studioImpl.dialog);
		SDL_free(studioImpl.menu);
		SDL_free(studioImpl.surf);
	}

	if(studioImpl.tic80local)
		tic80_delete((tic80*)studioImpl.tic80local);

	// if(studioImpl.audio.cvt.buf)
	// 	SDL_free(studioImpl.audio.cvt.buf);

	// SDL_DestroyTexture(studioImpl.gamepad.texture);
	// SDL_DestroyTexture(studioImpl.texture);

	// if(studioImpl.mouse.texture)
	// 	SDL_DestroyTexture(studioImpl.mouse.texture);

		// SDL_DestroyRenderer(studioImpl.renderer);
	// SDL_DestroyWindow(studioImpl.window);

// #if !defined (__MACOSX__)
	// stucks here on macos
	// SDL_CloseAudioDevice(studioImpl.audio.device);
	// SDL_Quit();
// #endif

	// exit(0);
}