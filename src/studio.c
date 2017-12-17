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
#include "keymap.h"
#include "code.h"
#include "dialog.h"
#include "menu.h"
#include "surf.h"

#include "fs.h"

#include <zlib.h>
#include "ext/net/SDL_net.h"
#include "ext/gif.h"
#include "ext/md5.h"

#define STUDIO_UI_SCALE 3

#define TEXTURE_SIZE (TIC80_FULLWIDTH)

#define MAX_CONTROLLERS 4
#define STUDIO_PIXEL_FORMAT SDL_PIXELFORMAT_ARGB8888

#define FRAME_SIZE (TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32))

#define OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
#define OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)

#define POPUP_DUR (TIC_FRAMERATE*2)

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
	tic80_local* tic80local;
	tic_mem* tic;

	struct
	{
		CartHash hash;
		u64 mdate;
	}cart;

	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;

	SDL_Joystick* joysticks[MAX_CONTROLLERS];

	EditorMode mode;
	EditorMode prevMode;
	EditorMode dialogMode;

	struct
	{
		SDL_Point cursor;
		u32 button;

		MouseState state[3];

		SDL_Texture* texture;
		const u8* src;
		SDL_SystemCursor system;
	} mouse;

	struct
	{
		SDL_Point pos;
		bool active;
	} gesture;

	const u8* keyboard;

	SDL_Scancode keycodes[KEYMAP_COUNT];

	struct
	{
		tic80_input keyboard;
		tic80_input touch;
		tic80_input joystick;

		SDL_Texture* texture;

		bool show;
		s32 counter;
		s32 alpha;
		bool backProcessed;

		struct
		{
			s32 size;
			SDL_Point axis;
			SDL_Point a;
			SDL_Point b;
			SDL_Point x;
			SDL_Point y;
		} part;
	}gamepad;

	struct
	{
		bool show;

		union
		{
			struct
			{
				s32 code;
				s32 sprites;
				s32 map;
				s32 sfx;
				s32 music;
			} index;

			s32 indexes[COUNT_OF(Modes)];
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
		Keymap* keymap;
		Dialog* dialog;
		Menu* menu;
		Surf* surf;
	};

	FileSystem* fs;

	bool quitFlag;

	s32 argc;
	char **argv;

	float* floatSamples;

} studio =
{
	.tic80local = NULL,
	.tic = NULL,

	.window = NULL,
	.renderer = NULL,
	.texture = NULL,
	.audioDevice = 0,

	.cart = 
	{
		.mdate = 0,
	},

	.joysticks = {NULL, NULL, NULL, NULL},

	.mode = TIC_START_MODE,
	.prevMode = TIC_CODE_MODE,
	.dialogMode = TIC_CONSOLE_MODE,

	.mouse =
	{
		.cursor = {-1, -1},
		.button = 0,
		.src = NULL,
		.texture = NULL,
		.system = SDL_SYSTEM_CURSOR_ARROW,
	},

	.gesture =
	{
		.pos = {0, 0},
		.active = false,
	},

	.keyboard = NULL,
	.keycodes =
	{
		SDL_SCANCODE_UP,
		SDL_SCANCODE_DOWN,
		SDL_SCANCODE_LEFT,
		SDL_SCANCODE_RIGHT,

		SDL_SCANCODE_Z, // a
		SDL_SCANCODE_X, // b
		SDL_SCANCODE_A, // x
		SDL_SCANCODE_S, // y

		0, 0, 0, 0, 0, 0, 0, 0,
	},

	.gamepad =
	{
		.show = false,
	},

	.bank = 
	{
		.show = false,
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
	.quitFlag = false,
	.argc = 0,
	.argv = NULL,
	.floatSamples = NULL,
};

tic_tiles* getBankTiles()
{
	return &studio.tic->cart.banks[studio.bank.index.sprites].tiles;
}

tic_map* getBankMap()
{
	return &studio.tic->cart.banks[studio.bank.index.map].map;
}

void playSystemSfx(s32 id)
{
	const tic_sample* effect = &studio.tic->config.bank0.sfx.samples.data[id];
	studio.tic->api.sfx_ex(studio.tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, 0);
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
	strcpy(studio.tooltip.text, text);
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

		studio.tic->api.rect(tic, x + i * Size, y, Size, Size, bgcolor);
		drawBitIcon(x + i * Size, y, Icons + i*BITS_IN_BYTE, color);
	}
}

const StudioConfig* getConfig()
{
	return &studio.config->data;
}

#if defined (TIC80_PRO)

static void drawBankIcon(s32 x, s32 y)
{
	tic_mem* tic = studio.tic;

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
		if(Modes[i] == studio.mode)
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
			studio.bank.show = !studio.bank.show;
	}

	if(studio.bank.show)
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
					studio.bank.indexes[mode] = i;
			}

			if(i == studio.bank.indexes[mode])
			{
				tic->api.rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_red);
			}

			tic->api.draw_char(tic, '0' + i, rect.x+1, rect.y+1, i == studio.bank.indexes[mode] ? tic_color_white : over ? tic_color_red : tic_color_peach);
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
		studio.tic->api.rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

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

	if(mode >= 0 && !studio.bank.show)
	{
		if(strlen(studio.tooltip.text))
		{
			studio.tic->api.text(tic, studio.tooltip.text, TextOffset, 1, (tic_color_black));
		}
		else
		{
			studio.tic->api.text(tic, Names[mode], TextOffset, 1, (tic_color_dark_gray));
		}
	}
}

void setStudioEvent(StudioEvent event)
{
	switch(studio.mode)
	{
	case TIC_CODE_MODE: 	
		{
			Code* code = studio.editor[studio.bank.index.code].code;
			code->event(code, event); 			
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = studio.editor[studio.bank.index.sprites].sprite;
			sprite->event(sprite, event); 
		}
	break;
	case TIC_MAP_MODE:
		{
			Map* map = studio.editor[studio.bank.index.map].map;
			map->event(map, event);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = studio.editor[studio.bank.index.sfx].sfx;
			sfx->event(sfx, event);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = studio.editor[studio.bank.index.music].music;
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

const u8* getKeyboard()
{
	return studio.keyboard;
}

static void showPopupMessage(const char* text)
{
	studio.popup.counter = POPUP_DUR;
	strcpy(studio.popup.message, text);
}

static void exitConfirm(bool yes, void* data)
{
	studio.quitFlag = yes;
}

void exitStudio()
{
	if(studio.mode != TIC_START_MODE && studioCartChanged())
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
				studio.tic->api.pixel(studio.tic, x + TIC_SPRITESIZE - col - 1, y + i, color);
}

static void initWorldMap()
{
	initWorld(studio.world, studio.tic, studio.editor[studio.bank.index.map].map);
}

static void initRunMode()
{
	initRun(studio.run, studio.console, studio.tic);
}

static void initSurfMode()
{
	initSurf(studio.surf, studio.tic, studio.console);
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
	initMenu(studio.menu, studio.tic, studio.fs);
}

void runGameFromSurf()
{
	studio.tic->api.reset(studio.tic);
	setStudioMode(TIC_RUN_MODE);
	studio.prevMode = TIC_SURF_MODE;
}

void exitFromGameMenu()
{
	if(studio.prevMode == TIC_SURF_MODE)
	{
		setStudioMode(TIC_SURF_MODE);
	}
	else
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	studio.console->showGameMenu = false;
}

void setStudioMode(EditorMode mode)
{
	if(mode != studio.mode)
	{
		EditorMode prev = studio.mode;

		if(prev == TIC_RUN_MODE)
			studio.tic->api.pause(studio.tic);

		if(mode != TIC_RUN_MODE)
			studio.tic->api.reset(studio.tic);

		switch (prev)
		{
		case TIC_START_MODE:
		case TIC_CONSOLE_MODE:
		case TIC_RUN_MODE:
		case TIC_KEYMAP_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
			break;
		case TIC_SURF_MODE:
			studio.prevMode = TIC_CODE_MODE;
			break;
		default: studio.prevMode = prev; break;
		}

		switch(mode)
		{
		case TIC_WORLD_MODE: initWorldMap(); break;
		case TIC_RUN_MODE: initRunMode(); break;
		case TIC_SURF_MODE: studio.surf->resume(studio.surf); break;
		default: break;
		}

		studio.mode = mode;
	}
}

EditorMode getStudioMode()
{
	return studio.mode;
}

static void showGameMenu()
{
	studio.tic->api.pause(studio.tic);
	studio.tic->api.reset(studio.tic);

	initMenuMode();
	studio.mode = TIC_MENU_MODE;
}

void hideGameMenu()
{
	studio.tic->api.resume(studio.tic);
	studio.mode = TIC_RUN_MODE;
}

s32 getMouseX()
{
	return studio.mouse.cursor.x;
}

s32 getMouseY()
{
	return studio.mouse.cursor.y;
}

bool checkMousePos(const SDL_Rect* rect)
{
	return SDL_PointInRect(&studio.mouse.cursor, rect);
}

bool checkMouseClick(const SDL_Rect* rect, s32 button)
{
	MouseState* state = &studio.mouse.state[button - 1];

	bool value = state->click
		&& SDL_PointInRect(&state->start, rect)
		&& SDL_PointInRect(&state->end, rect);

	if(value) state->click = false;

	return value;
}

bool checkMouseDown(const SDL_Rect* rect, s32 button)
{
	MouseState* state = &studio.mouse.state[button - 1];

	return state->down && SDL_PointInRect(&state->start, rect);
}


bool getGesturePos(SDL_Point* pos)
{
	if(studio.gesture.active)
	{
		*pos = studio.gesture.pos;

		return true;
	}

	return false;
}

void setCursor(SDL_SystemCursor id)
{
	if(id != SDL_SYSTEM_CURSOR_ARROW)
		studio.mouse.system = id;
}

void hideDialog()
{
	if(studio.dialogMode == TIC_RUN_MODE)
	{
		studio.tic->api.resume(studio.tic);
		studio.mode = TIC_RUN_MODE;
	}
	else setStudioMode(studio.dialogMode);
}

void showDialog(const char** text, s32 rows, DialogCallback callback, void* data)
{
	if(studio.mode != TIC_DIALOG_MODE)
	{
		initDialog(studio.dialog, studio.tic, text, rows, callback, data);
		studio.dialogMode = studio.mode;
		setStudioMode(TIC_DIALOG_MODE);
	}
}

static void initModules()
{
	tic_mem* tic = studio.tic;

	for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
	{
		initCode(studio.editor[i].code, studio.tic, &tic->cart.banks[i].code);
		initSprite(studio.editor[i].sprite, studio.tic, &tic->cart.banks[i].tiles);
		initMap(studio.editor[i].map, studio.tic, &tic->cart.banks[i].map);
		initSfx(studio.editor[i].sfx, studio.tic, &tic->cart.banks[i].sfx);
		initMusic(studio.editor[i].music, studio.tic, &tic->cart.banks[i].music);
	}

	initWorldMap();
}

static void updateHash()
{
	md5(&studio.tic->cart, sizeof(tic_cartridge), studio.cart.hash.data);
}

static void updateMDate()
{
	studio.cart.mdate = fsMDate(studio.console->fs, studio.console->romName);
}

static void updateTitle()
{
	char name[FILENAME_MAX] = TIC_TITLE;

	if(strlen(studio.console->romName))
		sprintf(name, "%s [%s]", TIC_TITLE, studio.console->romName);

	SDL_SetWindowTitle(studio.window, name);
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
	md5(&studio.tic->cart, sizeof(tic_cartridge), hash.data);

	return memcmp(hash.data, studio.cart.hash.data, sizeof(CartHash)) != 0;
}

static void updateGamepadParts();

static void calcTextureRect(SDL_Rect* rect)
{
	SDL_GetWindowSize(studio.window, &rect->w, &rect->h);

	if (rect->w * TIC80_HEIGHT < rect->h * TIC80_WIDTH)
	{
		s32 discreteWidth = rect->w - rect->w % TIC80_WIDTH;
		s32 discreteHeight = TIC80_HEIGHT * discreteWidth / TIC80_WIDTH;

		rect->x = (rect->w - discreteWidth) / 2;

		rect->y = rect->w > rect->h 
			? (rect->h - discreteHeight) / 2 
			: OFFSET_LEFT*discreteWidth/TIC80_WIDTH;

		rect->w = discreteWidth;
		rect->h = discreteHeight;

	}
	else
	{
		s32 discreteHeight = rect->h - rect->h % TIC80_HEIGHT;
		s32 discreteWidth = TIC80_WIDTH * discreteHeight / TIC80_HEIGHT;

		rect->x = (rect->w - discreteWidth) / 2;
		rect->y = (rect->h - discreteHeight) / 2;

		rect->w = discreteWidth;
		rect->h = discreteHeight;
	}
}

SDL_Scancode* getKeymap()
{
	return studio.keycodes;
}

static void processKeyboard()
{
	studio.keyboard = SDL_GetKeyboardState(NULL);

	studio.gamepad.keyboard.data = 0;

	for(s32 i = 0; i < KEYMAP_COUNT; i++)
		if(studio.keyboard[studio.keycodes[i]])
			studio.gamepad.keyboard.data |= 1 << i;
}

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)

static bool checkTouch(const SDL_Rect* rect, s32* x, s32* y)
{
	s32 devices = SDL_GetNumTouchDevices();
	s32 width = 0, height = 0;
	SDL_GetWindowSize(studio.window, &width, &height);

	for (s32 i = 0; i < devices; i++)
	{
		SDL_TouchID id = SDL_GetTouchDevice(i);

		// very strange, but on Android id always == 0
		//if (id)
		{
			s32 fingers = SDL_GetNumTouchFingers(id);

			if(fingers)
			{
				studio.gamepad.counter = 0;

				if (!studio.gamepad.show)
				{
					studio.gamepad.alpha = getConfig()->theme.gamepad.touch.alpha;
					SDL_SetTextureAlphaMod(studio.gamepad.texture, studio.gamepad.alpha);
					studio.gamepad.show = true;
					return false;
				}
			}

			for (s32 f = 0; f < fingers; f++)
			{
				SDL_Finger* finger = SDL_GetTouchFinger(id, f);

				if (finger && finger->pressure > 0.0f)
				{
					SDL_Point point = { (s32)(finger->x * width), (s32)(finger->y * height) };
					if (SDL_PointInRect(&point, rect))
					{
						*x = point.x;
						*y = point.y;
						return true;
					}
				}
			}
		}
	}

	return false;
}

static void processTouchGamepad()
{
	studio.gamepad.touch.data = 0;

	const s32 size = studio.gamepad.part.size;
	s32 x = 0, y = 0;

	{
		SDL_Rect axis = {studio.gamepad.part.axis.x, studio.gamepad.part.axis.y, size*3, size*3};

		if(checkTouch(&axis, &x, &y))
		{
			x -= axis.x;
			y -= axis.y;

			s32 xt = x / size;
			s32 yt = y / size;

			if(yt == 0) studio.gamepad.touch.first.up = true;
			else if(yt == 2) studio.gamepad.touch.first.down = true;

			if(xt == 0) studio.gamepad.touch.first.left = true;
			else if(xt == 2) studio.gamepad.touch.first.right = true;

			if(xt == 1 && yt == 1)
			{
				xt = (x - size)/(size/3);
				yt = (y - size)/(size/3);

				if(yt == 0) studio.gamepad.touch.first.up = true;
				else if(yt == 2) studio.gamepad.touch.first.down = true;

				if(xt == 0) studio.gamepad.touch.first.left = true;
				else if(xt == 2) studio.gamepad.touch.first.right = true;
			}
		}
	}

	{
		SDL_Rect a = {studio.gamepad.part.a.x, studio.gamepad.part.a.y, size, size};
		if(checkTouch(&a, &x, &y)) studio.gamepad.touch.first.a = true;
	}

	{
		SDL_Rect b = {studio.gamepad.part.b.x, studio.gamepad.part.b.y, size, size};
		if(checkTouch(&b, &x, &y)) studio.gamepad.touch.first.b = true;
	}

	{
		SDL_Rect xb = {studio.gamepad.part.x.x, studio.gamepad.part.x.y, size, size};
		if(checkTouch(&xb, &x, &y)) studio.gamepad.touch.first.x = true;
	}

	{
		SDL_Rect yb = {studio.gamepad.part.y.x, studio.gamepad.part.y.y, size, size};
		if(checkTouch(&yb, &x, &y)) studio.gamepad.touch.first.y = true;
	}
}

#endif

static s32 getAxisMask(SDL_Joystick* joystick)
{
	s32 mask = 0;

	s32 axesCount = SDL_JoystickNumAxes(joystick);

	for (s32 a = 0; a < axesCount; a++)
	{
		s32 axe = SDL_JoystickGetAxis(joystick, a);

		if (axe)
		{
			if (a == 0)
			{
				if (axe > 16384) mask |= SDL_HAT_RIGHT;
				else if(axe < -16384) mask |= SDL_HAT_LEFT;
			}
			else if (a == 1)
			{
				if (axe > 16384) mask |= SDL_HAT_DOWN;
				else if (axe < -16384) mask |= SDL_HAT_UP;
			}
		}
	}

	return mask;
}

static s32 getJoystickHatMask(s32 hat)
{
	tic80_input gamepad;
	gamepad.data = 0;

	gamepad.first.up = hat & SDL_HAT_UP;
	gamepad.first.down = hat & SDL_HAT_DOWN;
	gamepad.first.left = hat & SDL_HAT_LEFT;
	gamepad.first.right = hat & SDL_HAT_RIGHT;

	return gamepad.data;
}

static bool isGameMenu()
{
	return (studio.mode == TIC_RUN_MODE && studio.console->showGameMenu) || studio.mode == TIC_MENU_MODE;
}

static void processJoysticksWithMouseInput()
{
	s32 index = 0;

	for(s32 i = 0; i < COUNT_OF(studio.joysticks); i++)
	{
		SDL_Joystick* joystick = studio.joysticks[i];

		if(joystick && SDL_JoystickGetAttached(joystick))
		{
			tic80_gamepad* gamepad = NULL;

			switch(index)
			{
			case 0: gamepad = &studio.gamepad.joystick.first; break;
			case 1: gamepad = &studio.gamepad.joystick.second; break;
			}

			if(gamepad)
			{
				s32 numButtons = SDL_JoystickNumButtons(joystick);
				for(s32 i = 5; i < numButtons; i++)
				{
					s32 back = SDL_JoystickGetButton(joystick, i);

					if(back)
					{
						if(!studio.gamepad.backProcessed)
						{
							if(isGameMenu())
							{
								studio.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
								studio.gamepad.backProcessed = true;
							}
						}

						return;
					}
				}

				studio.gamepad.backProcessed = false;

				index++;
			}
		}
	}
}

static void processJoysticks()
{
	studio.gamepad.joystick.data = 0;
	s32 index = 0;

	for(s32 i = 0; i < COUNT_OF(studio.joysticks); i++)
	{
		SDL_Joystick* joystick = studio.joysticks[i];

		if(joystick && SDL_JoystickGetAttached(joystick))
		{
			tic80_gamepad* gamepad = NULL;

			switch(index)
			{
			case 0: gamepad = &studio.gamepad.joystick.first; break;
			case 1: gamepad = &studio.gamepad.joystick.second; break;
			}

			if(gamepad)
			{
				gamepad->data |= getJoystickHatMask(getAxisMask(joystick));

				for (s32 h = 0; h < SDL_JoystickNumHats(joystick); h++)
					gamepad->data |= getJoystickHatMask(SDL_JoystickGetHat(joystick, h));

				s32 numButtons = SDL_JoystickNumButtons(joystick);
				if(numButtons >= 2)
				{
					gamepad->a = SDL_JoystickGetButton(joystick, 0);
					gamepad->b = SDL_JoystickGetButton(joystick, 1);

					if(numButtons >= 4)
					{
						gamepad->x = SDL_JoystickGetButton(joystick, 2);
						gamepad->y = SDL_JoystickGetButton(joystick, 3);

						for(s32 i = 5; i < numButtons; i++)
						{
							s32 back = SDL_JoystickGetButton(joystick, i);

							if(back)
							{
								if(!studio.gamepad.backProcessed)
								{
									if(isGameMenu())
									{
										studio.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
										studio.gamepad.backProcessed = true;
									}
								}

								return;
							}
						}

						studio.gamepad.backProcessed = false;
					}
				}

				index++;
			}
		}
	}

	if(studio.mode == TIC_CONSOLE_MODE && studio.gamepad.joystick.data)
	{
		studio.gamepad.joystick.data = 0;
		setStudioMode(TIC_SURF_MODE);
	}
}

static void processGamepad()
{
	studio.tic->ram.vram.input.gamepad.data = 0;

	studio.tic->ram.vram.input.gamepad.data |= studio.gamepad.keyboard.data;
	studio.tic->ram.vram.input.gamepad.data |= studio.gamepad.touch.data;
	studio.tic->ram.vram.input.gamepad.data |= studio.gamepad.joystick.data;
	studio.tic->ram.vram.input.gamepad.data &= studio.tic->ram.vram.vars.mask.data |
		(studio.tic->ram.vram.vars.mask.data << (sizeof(tic80_gamepad)*BITS_IN_BYTE));
}

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

		studio.gesture.pos = point;

		studio.gesture.active = true;
	}
}

static void processMouse()
{
	studio.mouse.button = SDL_GetMouseState(&studio.mouse.cursor.x, &studio.mouse.cursor.y);

	{
		SDL_Rect rect = {0, 0, 0, 0};
		calcTextureRect(&rect);

		if(rect.w) studio.mouse.cursor.x = (studio.mouse.cursor.x - rect.x) * TIC80_WIDTH / rect.w;
		if(rect.h) studio.mouse.cursor.y = (studio.mouse.cursor.y - rect.y) * TIC80_HEIGHT / rect.h;
	}

	for(int i = 0; i < COUNT_OF(studio.mouse.state); i++)
	{
		MouseState* state = &studio.mouse.state[i];

		if(!state->down && (studio.mouse.button & SDL_BUTTON(i + 1)))
		{
			state->down = true;

			state->start.x = studio.mouse.cursor.x;
			state->start.y = studio.mouse.cursor.y;
		}
		else if(state->down && !(studio.mouse.button & SDL_BUTTON(i + 1)))
		{
			state->end.x = studio.mouse.cursor.x;
			state->end.y = studio.mouse.cursor.y;

			state->click = true;
			state->down = false;
		}
	}
}

static void goFullscreen()
{
	studio.fullscreen = !studio.fullscreen;
	SDL_SetWindowFullscreen(studio.window, studio.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void runProject()
{
	studio.tic->api.reset(studio.tic);

	if(studio.mode == TIC_RUN_MODE)
	{
		initRunMode();
	}
	else setStudioMode(TIC_RUN_MODE);
}

static void saveProject()
{
	CartSaveResult rom = studio.console->save(studio.console);

	if(rom == CART_SAVE_OK)
	{
		char buffer[FILENAME_MAX];
		sprintf(buffer, "%s SAVED :)", studio.console->romName);

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
	tic_mem* tic = studio.tic;

	if(studio.mode == TIC_RUN_MODE)
	{
		enum {Pitch = TIC80_FULLWIDTH*sizeof(u32)};

		tic->api.blit(tic, tic->api.scanline, tic->api.overlap, NULL);

		u32* buffer = SDL_malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32));

		if(buffer)
		{
			SDL_Rect rect = {OFFSET_LEFT, OFFSET_TOP, TIC80_WIDTH, TIC80_HEIGHT};

			screen2buffer(buffer, tic->screen, rect);

			gif_write_animation(studio.tic->cart.cover.data, &studio.tic->cart.cover.size,
				TIC80_WIDTH, TIC80_HEIGHT, (const u8*)buffer, 1, TIC_FRAMERATE, 1);

			SDL_free(buffer);

			showPopupMessage("COVER IMAGE SAVED :)");
		}
	}
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
	if(studio.video.buffer)
	{
		{
			s32 size = 0;
			u8* data = SDL_malloc(FRAME_SIZE * studio.video.frame);

			gif_write_animation(data, &size, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, (const u8*)studio.video.buffer, studio.video.frame, TIC_FRAMERATE, getConfig()->gifScale);

			fsGetFileData(onVideoExported, "screen.gif", data, size, DEFAULT_CHMOD, NULL);
		}

		SDL_free(studio.video.buffer);
		studio.video.buffer = NULL;
	}

	studio.video.record = false;
}

#if !defined(__EMSCRIPTEN__)

static void startVideoRecord()
{
	if(studio.video.record)
	{
		stopVideoRecord();
	}
	else
	{
		studio.video.frames = getConfig()->gifLength * TIC_FRAMERATE;
		studio.video.buffer = SDL_malloc(FRAME_SIZE * studio.video.frames);

		if(studio.video.buffer)
		{
			studio.video.record = true;
			studio.video.frame = 0;
		}
	}
}

#endif

static void takeScreenshot()
{
	studio.video.frames = 1;
	studio.video.buffer = SDL_malloc(FRAME_SIZE);

	if(studio.video.buffer)
	{
		studio.video.record = true;
		studio.video.frame = 0;
	}
}

static bool processShortcuts(SDL_KeyboardEvent* event)
{
	if(event->repeat) return false;

	SDL_Keymod mod = event->keysym.mod;

	if(studio.mode == TIC_START_MODE) return true;
	if(studio.mode == TIC_CONSOLE_MODE && !studio.console->active) return true;

	if(isGameMenu())
	{
		switch(event->keysym.sym)
		{
		case SDLK_ESCAPE:
		case SDLK_AC_BACK:
			studio.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
			studio.gamepad.backProcessed = true;
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
			Code* code = studio.editor[studio.bank.index.code].code;

			if(studio.mode == TIC_CODE_MODE && code->mode != TEXT_EDIT_MODE)
			{
				code->escape(code);
				return true;
			}

			// TODO: move this to keymap
			if(studio.mode == TIC_KEYMAP_MODE)
			{
				studio.keymap->escape(studio.keymap);
				return true;
			}

			if(studio.mode == TIC_DIALOG_MODE)
			{
				studio.dialog->escape(studio.dialog);
				return true;
			}

			setStudioMode(studio.mode == TIC_CONSOLE_MODE ? studio.prevMode : TIC_CONSOLE_MODE);
		}
		return true;
	default: break;
	}

	return false;
}

static void processGamepadInput()
{
	processKeyboard();

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
	processTouchGamepad();
#endif
	processJoysticks();
	processGamepad();
}

static void processMouseInput()
{
	processJoysticksWithMouseInput();

	s32 x = studio.mouse.cursor.x;
	s32 y = studio.mouse.cursor.y;

	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x >= TIC80_WIDTH) x = TIC80_WIDTH-1;
	if(y >= TIC80_HEIGHT) y = TIC80_HEIGHT-1;

	studio.tic->ram.vram.input.gamepad.mouse = x + y * TIC80_WIDTH;
	studio.tic->ram.vram.input.gamepad.pressed = studio.mouse.state->down ? 1 : 0;
}

#if defined(TIC80_PRO)

static void reloadConfirm(bool yes, void* data)
{
	if(yes)
		studio.console->updateProject(studio.console);
}

#endif

SDL_Event* pollEvent()
{
	static SDL_Event event;

	if(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_KEYDOWN:
			if(processShortcuts(&event.key)) return NULL;
			break;
		case SDL_JOYDEVICEADDED:
			{
				s32 id = event.jdevice.which;

				if (id < MAX_CONTROLLERS)
				{
					if(studio.joysticks[id])
						SDL_JoystickClose(studio.joysticks[id]);

					studio.joysticks[id] = SDL_JoystickOpen(id);
				}
			}
			break;

		case SDL_JOYDEVICEREMOVED:
			{
				s32 id = event.jdevice.which;

				if (id < MAX_CONTROLLERS && studio.joysticks[id])
				{
					SDL_JoystickClose(studio.joysticks[id]);
					studio.joysticks[id] = NULL;
				}
			}
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED: updateGamepadParts(); break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:

#if defined(TIC80_PRO)

				if(studio.mode != TIC_START_MODE)
				{
					Console* console = studio.console;

					u64 mdate = fsMDate(console->fs, console->romName);

					if(studio.cart.mdate && mdate > studio.cart.mdate)
					{
						if(studioCartChanged())
						{
							static const char* Rows[] =
							{
								"",
								"CART HAS CHANGED!",
								"",
								"DO YOU WANT",
								"TO RELOAD IT?"
							};

							showDialog(Rows, COUNT_OF(Rows), reloadConfirm, NULL);
						}
						else console->updateProject(console);						
					}
				}

#endif
				{
					Code* code = studio.editor[studio.bank.index.code].code;
					studio.console->codeLiveReload.reload(studio.console, code->src);
					if(studio.console->codeLiveReload.active && code->update)
						code->update(code);
				}
				break;
			}
			break;
		case SDL_FINGERUP:
			if(SDL_HasScreenKeyboardSupport() && !SDL_IsTextInputActive())
				if(studio.mode == TIC_CONSOLE_MODE || studio.mode == TIC_CODE_MODE)
					SDL_StartTextInput();
			break;
		case SDL_QUIT:
			exitStudio();
			break;
		default:
			break;
		}

		return &event;
	}

	if(studio.mode != TIC_RUN_MODE)
		processGesture();

	if(!studio.gesture.active)
		processMouse();

	if(studio.mode == TIC_RUN_MODE)
	{
		switch(studio.tic->input)
		{
		case tic_gamepad_input:
			processGamepadInput();
			break;

		case tic_mouse_input:
			processMouseInput();
			break;
		}
	}
	else
	{
		processGamepadInput();
	}

	return NULL;
}

static void transparentBlit(u32* out, s32 pitch)
{
	const u8* in = studio.tic->ram.vram.screen.data;
	const u8* end = in + sizeof(studio.tic->ram.vram.screen);
	const u32* pal = tic_palette_blit(&studio.tic->config.palette);
	const u32 Delta = (pitch/sizeof *out - TIC80_WIDTH);

	s32 col = 0;

	while(in != end)
	{
		u8 low = *in & 0x0f;
		u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
		*out++ = low ? (*(pal + low) | 0xff000000) : 0;
		*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;
		in++;

		col += BITS_IN_BYTE / TIC_PALETTE_BPP;

		if (col == TIC80_WIDTH)
		{
			col = 0;
			out += Delta;
		}
	}
}

static void blitSound()
{
	s32 samples = studio.audioSpec.freq / TIC_FRAMERATE;

	// TODO: use SDL_ConvertAudio to covert format
	if(studio.audioSpec.format == AUDIO_F32)
	{
		if(!studio.floatSamples)
			studio.floatSamples = SDL_malloc(samples * sizeof studio.floatSamples[0]);

		s16* ptr = studio.tic->samples.buffer;
		s16* end = ptr + samples;
		float* out = studio.floatSamples;

		while(ptr != end) *out++ = *ptr++ / 32767.0f;

		SDL_QueueAudio(studio.audioDevice, studio.floatSamples, samples * sizeof studio.floatSamples[0]);
	}
	else if (studio.audioSpec.format == AUDIO_S16)
		SDL_QueueAudio(studio.audioDevice, studio.tic->samples.buffer, studio.tic->samples.size);
}

static void drawRecordLabel(u32* frame, s32 pitch, s32 sx, s32 sy, const u32* color)
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

static void recordFrame(u32* pixels)
{
	if(studio.video.record)
	{
		if(studio.video.frame < studio.video.frames)
		{
			SDL_Rect rect = {0, 0, TIC80_FULLWIDTH, TIC80_FULLHEIGHT};
			screen2buffer(studio.video.buffer + (TIC80_FULLWIDTH*TIC80_FULLHEIGHT) * studio.video.frame, pixels, rect);

			if(studio.video.frame % TIC_FRAMERATE < TIC_FRAMERATE / 2)
			{
				const u32* pal = tic_palette_blit(&studio.tic->config.palette);
				drawRecordLabel(pixels, TIC80_FULLWIDTH, TIC80_WIDTH-24, 8, &pal[tic_color_red]);
			}

			studio.video.frame++;

		}
		else
		{
			stopVideoRecord();
		}
	}
}

static void blitTexture()
{
	tic_mem* tic = studio.tic;
	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);

	void* pixels = NULL;
	s32 pitch = 0;
	SDL_LockTexture(studio.texture, NULL, &pixels, &pitch);

	tic_scanline scanline = NULL;
	tic_overlap overlap = NULL;
	void* data = NULL;

	switch(studio.mode)
	{
	case TIC_RUN_MODE:
		scanline = tic->api.scanline;
		overlap = tic->api.overlap;
		break;
	case TIC_SPRITE_MODE:
		{
			Sprite* sprite = studio.editor[studio.bank.index.sprites].sprite;
			overlap = sprite->overlap;
			data = sprite;
		}
		break;
	case TIC_MAP_MODE:
		{
			Map* map = studio.editor[studio.bank.index.map].map;
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

	SDL_UnlockTexture(studio.texture);

	{
		enum {Header = OFFSET_TOP};
		SDL_Rect srcRect = {0, 0, TIC80_FULLWIDTH, Header};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(studio.window, &dstRect.w, &dstRect.h);
		dstRect.h = rect.y;
		SDL_RenderCopy(studio.renderer, studio.texture, &srcRect, &dstRect);
	}

	{
		enum {Header = OFFSET_TOP};
		SDL_Rect srcRect = {0, TIC80_FULLHEIGHT - Header, TIC80_FULLWIDTH, Header};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(studio.window, &dstRect.w, &dstRect.h);
		dstRect.y = rect.y + rect.h;
		dstRect.h = rect.y;
		SDL_RenderCopy(studio.renderer, studio.texture, &srcRect, &dstRect);
	}

	{
		enum {Header = OFFSET_TOP};
		enum {Left = OFFSET_LEFT};
		SDL_Rect srcRect = {0, Header, Left, TIC80_HEIGHT};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(studio.window, &dstRect.w, &dstRect.h);
		dstRect.y = rect.y;
		dstRect.h = rect.h;
		SDL_RenderCopy(studio.renderer, studio.texture, &srcRect, &dstRect);
	}

	{
		enum {Top = OFFSET_TOP};
		enum {Left = OFFSET_LEFT};

		SDL_Rect srcRect = {Left, Top, TIC80_WIDTH, TIC80_HEIGHT};

		SDL_RenderCopy(studio.renderer, studio.texture, &srcRect, &rect);
	}
}

static void blitCursor(const u8* in)
{
	if(!studio.mouse.texture)
	{
		studio.mouse.texture = SDL_CreateTexture(studio.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TIC_SPRITESIZE, TIC_SPRITESIZE);
		SDL_SetTextureBlendMode(studio.mouse.texture, SDL_BLENDMODE_BLEND);
	}

	if(studio.mouse.src != in)
	{
		studio.mouse.src = in;

		void* pixels = NULL;
		s32 pitch = 0;
		SDL_LockTexture(studio.mouse.texture, NULL, &pixels, &pitch);

		{
			const u8* end = in + sizeof(tic_tile);
			const u32* pal = tic_palette_blit(&studio.tic->ram.vram.palette);
			u32* out = pixels;

			while(in != end)
			{
				u8 low = *in & 0x0f;
				u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
				*out++ = low ? (*(pal + low) | 0xff000000) : 0;
				*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;

				in++;
			}
		}

		SDL_UnlockTexture(studio.mouse.texture);
	}

	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);
	s32 scale = rect.w / TIC80_WIDTH;

	SDL_Rect src = {0, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
	SDL_Rect dst = {0, 0, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale};

	SDL_GetMouseState(&dst.x, &dst.y);

	if(getConfig()->theme.cursor.pixelPerfect)
	{
		dst.x -= (dst.x - rect.x) % scale;
		dst.y -= (dst.y - rect.y) % scale;
	}

	if(SDL_GetWindowFlags(studio.window) & SDL_WINDOW_MOUSE_FOCUS)
		SDL_RenderCopy(studio.renderer, studio.mouse.texture, &src, &dst);
}

static void renderCursor()
{
	if(studio.mode == TIC_RUN_MODE &&
		studio.tic->input == tic_mouse_input &&
		studio.tic->ram.vram.vars.cursor)
		{
			SDL_ShowCursor(SDL_DISABLE);
			blitCursor(studio.tic->ram.sprites.data[studio.tic->ram.vram.vars.cursor].data);
			return;
		}

	SDL_ShowCursor(getConfig()->theme.cursor.sprite >= 0 ? SDL_DISABLE : SDL_ENABLE);

	if(getConfig()->theme.cursor.sprite >= 0)
		blitCursor(studio.tic->config.bank0.tiles.data[getConfig()->theme.cursor.sprite].data);
}

static void useSystemPalette()
{
	memcpy(studio.tic->ram.vram.palette.data, studio.tic->config.palette.data, sizeof(tic_palette));
}

static void drawPopup()
{
	if(studio.popup.counter > 0)
	{
		studio.popup.counter--;

		s32 anim = 0;

		enum{Dur = TIC_FRAMERATE/2};

		if(studio.popup.counter < Dur)
			anim = -((Dur - studio.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);
		else if(studio.popup.counter >= (POPUP_DUR - Dur))
			anim = (((POPUP_DUR - Dur) - studio.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);

		studio.tic->api.rect(studio.tic, 0, anim, TIC80_WIDTH, TIC_FONT_HEIGHT+1, (tic_color_red));
		studio.tic->api.text(studio.tic, studio.popup.message, 
			(s32)(TIC80_WIDTH - strlen(studio.popup.message)*TIC_FONT_WIDTH)/2,
			anim + 1, (tic_color_white));
	}
}

static void renderStudio()
{
	tic_mem* tic = studio.tic;

	showTooltip("");

	studio.gesture.active = false;
	studio.mouse.cursor.x = studio.mouse.cursor.y = -1;
	for(int i = 0; i < COUNT_OF(studio.mouse.state); i++)
		studio.mouse.state[i].click = false;

	{
		const tic_sfx* sfx = NULL;
		const tic_music* music = NULL;

		switch(studio.mode)
		{
		case TIC_RUN_MODE:
			sfx = &studio.tic->ram.sfx;
			music = &studio.tic->ram.music;
			break;
		case TIC_START_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
		case TIC_SURF_MODE:
			sfx = &studio.tic->config.bank0.sfx;
			music = &studio.tic->config.bank0.music;
			break;
		default:
			sfx = &studio.tic->cart.banks[studio.bank.index.sfx].sfx;
			music = &studio.tic->cart.banks[studio.bank.index.music].music;
		}

		studio.tic->api.tick_start(studio.tic, sfx, music);
	}

	switch(studio.mode)
	{
	case TIC_START_MODE:	studio.start->tick(studio.start); break;
	case TIC_CONSOLE_MODE: 	studio.console->tick(studio.console); break;
	case TIC_RUN_MODE: 		studio.run->tick(studio.run); break;
	case TIC_CODE_MODE: 	
		{
			Code* code = studio.editor[studio.bank.index.code].code;
			code->tick(code);
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = studio.editor[studio.bank.index.sprites].sprite;
			sprite->tick(sprite);		
		}
		break;
	case TIC_MAP_MODE:
		{
			Map* map = studio.editor[studio.bank.index.map].map;
			map->tick(map);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = studio.editor[studio.bank.index.sfx].sfx;
			sfx->tick(sfx);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = studio.editor[studio.bank.index.music].music;
			music->tick(music);
		}
		break;

	case TIC_WORLD_MODE:	studio.world->tick(studio.world); break;
	case TIC_KEYMAP_MODE:	studio.keymap->tick(studio.keymap); break;
	case TIC_DIALOG_MODE:	studio.dialog->tick(studio.dialog); break;
	case TIC_MENU_MODE:		studio.menu->tick(studio.menu); break;
	case TIC_SURF_MODE:		studio.surf->tick(studio.surf); break;
	default: break;
	}

	drawPopup();

	if(getConfig()->noSound)
		SDL_memset(tic->ram.registers, 0, sizeof tic->ram.registers);

	studio.tic->api.tick_end(studio.tic);

	blitSound();

	if(studio.mode != TIC_RUN_MODE)
		useSystemPalette();
	
	blitTexture();

	renderCursor();
}

static void updateGamepadParts()
{
	s32 tileSize = TIC_SPRITESIZE;
	s32 offset = 0;
	SDL_Rect rect;

	const s32 JoySize = 3;
	SDL_GetWindowSize(studio.window, &rect.w, &rect.h);

	if(rect.w < rect.h)
	{
		tileSize = rect.w / 2 / JoySize;
		offset = (rect.h * 2 - JoySize * tileSize) / 3;
	}
	else
	{
		tileSize = rect.w / 5 / JoySize;
		offset = (rect.h - JoySize * tileSize) / 2;
	}

	studio.gamepad.part.size = tileSize;
	studio.gamepad.part.axis = (SDL_Point){0, offset};
	studio.gamepad.part.a = (SDL_Point){rect.w - 2*tileSize, 2*tileSize + offset};
	studio.gamepad.part.b = (SDL_Point){rect.w - 1*tileSize, 1*tileSize + offset};
	studio.gamepad.part.x = (SDL_Point){rect.w - 3*tileSize, 1*tileSize + offset};
	studio.gamepad.part.y = (SDL_Point){rect.w - 2*tileSize, 0*tileSize + offset};
}

static void renderGamepad()
{
	if(studio.gamepad.show || studio.gamepad.alpha); else return;

	const s32 tileSize = studio.gamepad.part.size;
	const SDL_Point axis = studio.gamepad.part.axis;
	typedef struct { bool press; s32 x; s32 y;} Tile;
	const Tile Tiles[] =
	{
		{studio.tic->ram.vram.input.gamepad.first.up, 		axis.x + 1*tileSize, axis.y + 0*tileSize},
		{studio.tic->ram.vram.input.gamepad.first.down, 	axis.x + 1*tileSize, axis.y + 2*tileSize},
		{studio.tic->ram.vram.input.gamepad.first.left, 	axis.x + 0*tileSize, axis.y + 1*tileSize},
		{studio.tic->ram.vram.input.gamepad.first.right, 	axis.x + 2*tileSize, axis.y + 1*tileSize},

		{studio.tic->ram.vram.input.gamepad.first.a, 		studio.gamepad.part.a.x, studio.gamepad.part.a.y},
		{studio.tic->ram.vram.input.gamepad.first.b, 		studio.gamepad.part.b.x, studio.gamepad.part.b.y},
		{studio.tic->ram.vram.input.gamepad.first.x, 		studio.gamepad.part.x.x, studio.gamepad.part.x.y},
		{studio.tic->ram.vram.input.gamepad.first.y, 		studio.gamepad.part.y.x, studio.gamepad.part.y.y},
	};

	enum {ButtonsCount = 8};

	for(s32 i = 0; i < COUNT_OF(Tiles); i++)
	{
		if(studio.tic->ram.vram.vars.mask.data & (1 << i))
		{
			const Tile* tile = Tiles + i;
			SDL_Rect src = {(tile->press ? ButtonsCount + i : i) * TIC_SPRITESIZE, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
			SDL_Rect dest = {tile->x, tile->y, tileSize, tileSize};

			SDL_RenderCopy(studio.renderer, studio.gamepad.texture, &src, &dest);
		}
	}

	if(!studio.gamepad.show && studio.gamepad.alpha)
	{
		enum {Step = 3};

		if(studio.gamepad.alpha - Step >= 0) studio.gamepad.alpha -= Step;
		else studio.gamepad.alpha = 0;

		SDL_SetTextureAlphaMod(studio.gamepad.texture, studio.gamepad.alpha);
	}

	studio.gamepad.counter = studio.gamepad.touch.data ? 0 : studio.gamepad.counter+1;

	// wait 5 seconds and hide touch gamepad
	if(studio.gamepad.counter >= 5 * TIC_FRAMERATE)
		studio.gamepad.show = false;
}

static void tick()
{
	if(!studio.fs) return;

	if(studio.quitFlag)
	{
#if defined __EMSCRIPTEN__
		studio.tic->api.clear(studio.tic, TIC_COLOR_BG);
		blitTexture();
		emscripten_cancel_main_loop();
#endif
		return;
	}

	SDL_SystemCursor cursor = studio.mouse.system;
	studio.mouse.system = SDL_SYSTEM_CURSOR_ARROW;

	SDL_RenderClear(studio.renderer);

	renderStudio();

	if(studio.mode == TIC_RUN_MODE && studio.tic->input == tic_gamepad_input)
		renderGamepad();

	if(studio.mode == TIC_MENU_MODE || studio.mode == TIC_SURF_MODE)
		renderGamepad();

	if(studio.mouse.system != cursor)
		SDL_SetCursor(SDL_CreateSystemCursor(studio.mouse.system));

	SDL_RenderPresent(studio.renderer);
}

static void initSound()
{
	SDL_AudioSpec want =
	{
		.freq = 44100,
		.format = AUDIO_S16,
		.channels = 1,
		.userdata = NULL,
	};

	studio.audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &studio.audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);

	if(studio.audioDevice)
		SDL_PauseAudioDevice(studio.audioDevice, 0);
}

static void initTouchGamepad()
{
	if (!studio.renderer)
		return;

	studio.tic->api.map(studio.tic, &studio.tic->config.bank0.map, &studio.tic->config.bank0.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, -1, 1);

	if(!studio.gamepad.texture)
	{
		studio.gamepad.texture = SDL_CreateTexture(studio.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);
		SDL_SetTextureBlendMode(studio.gamepad.texture, SDL_BLENDMODE_BLEND);
	}

	{
		void* pixels = NULL;
		s32 pitch = 0;

		SDL_LockTexture(studio.gamepad.texture, NULL, &pixels, &pitch);
		transparentBlit(pixels, pitch);
		SDL_UnlockTexture(studio.gamepad.texture);
	}

	updateGamepadParts();
}

static void updateSystemFont()
{
	SDL_memset(studio.tic->font.data, 0, sizeof(tic_font));

	for(s32 i = 0; i < TIC_FONT_CHARS; i++)
		for(s32 y = 0; y < TIC_SPRITESIZE; y++)
			for(s32 x = 0; x < TIC_SPRITESIZE; x++)
				if(tic_tool_peek4(&studio.tic->config.bank0.sprites.data[i], TIC_SPRITESIZE*(y+1) - x-1))
					studio.tic->font.data[i*BITS_IN_BYTE+y] |= 1 << x;
}

void studioConfigChanged()
{
	Code* code = studio.editor[studio.bank.index.code].code;
	if(code->update)
		code->update(code);

	initTouchGamepad();
	updateSystemFont();
}

static void setWindowIcon()
{
	enum{ Size = 64, TileSize = 16, ColorKey = 14, Cols = TileSize / TIC_SPRITESIZE, Scale = Size/TileSize};
	studio.tic->api.clear(studio.tic, 0);

	u32* pixels = SDL_malloc(Size * Size * sizeof(u32));

	const u32* pal = tic_palette_blit(&studio.tic->config.palette);

	for(s32 j = 0, index = 0; j < Size; j++)
		for(s32 i = 0; i < Size; i++, index++)
		{
			u8 color = getSpritePixel(studio.tic->config.bank0.tiles.data, i/Scale, j/Scale);
			pixels[index] = color == ColorKey ? 0 : pal[color];
		}

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, Size, Size,
		sizeof(s32) * BITS_IN_BYTE, Size * sizeof(s32),
		0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

	SDL_SetWindowIcon(studio.window, surface);
	SDL_FreeSurface(surface);
	SDL_free(pixels);
}

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

static void onFSInitialized(FileSystem* fs)
{
	studio.fs = fs;

	SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	SDLNet_Init();

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	studio.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(TIC80_FULLWIDTH) * STUDIO_UI_SCALE,
		(TIC80_FULLHEIGHT) * STUDIO_UI_SCALE,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
#if defined(__CHIP__)
			| SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
		);

	initSound();

	studio.tic80local = (tic80_local*)tic80_create(studio.audioSpec.freq);
	studio.tic = studio.tic80local->memory;

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			studio.editor[i].code	= SDL_calloc(1, sizeof(Code));
			studio.editor[i].sprite	= SDL_calloc(1, sizeof(Sprite));
			studio.editor[i].map 	= SDL_calloc(1, sizeof(Map));
			studio.editor[i].sfx 	= SDL_calloc(1, sizeof(Sfx));
			studio.editor[i].music 	= SDL_calloc(1, sizeof(Music));
		}

		studio.start 	= SDL_calloc(1, sizeof(Start));
		studio.console 	= SDL_calloc(1, sizeof(Console));
		studio.run 		= SDL_calloc(1, sizeof(Run));
		studio.world 	= SDL_calloc(1, sizeof(World));
		studio.config 	= SDL_calloc(1, sizeof(Config));
		studio.keymap 	= SDL_calloc(1, sizeof(Keymap));
		studio.dialog 	= SDL_calloc(1, sizeof(Dialog));
		studio.menu 	= SDL_calloc(1, sizeof(Menu));
		studio.surf 	= SDL_calloc(1, sizeof(Surf));
	}

	fsMakeDir(fs, TIC_LOCAL);
	initConfig(studio.config, studio.tic, studio.fs);
	initKeymap(studio.keymap, studio.tic, studio.fs);

	initStart(studio.start, studio.tic);
	initConsole(studio.console, studio.tic, studio.fs, studio.config, studio.argc, studio.argv);
	initSurfMode();

	initRunMode();

	initModules();

	if(studio.console->skipStart)
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	if(studio.console->goFullscreen)
	{
		goFullscreen();
	}

	// set the window icon before renderer is created (issues on Linux)
	setWindowIcon();

	studio.renderer = SDL_CreateRenderer(studio.window, -1, 
#if defined(__CHIP__)
		SDL_RENDERER_SOFTWARE
#else
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
#endif
	);

	studio.texture = SDL_CreateTexture(studio.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	initTouchGamepad();
}

#if defined(__EMSCRIPTEN__)

#define DEFAULT_CART "cart.tic"

void onEmscriptenWget(const char* file)
{
	studio.argv[1] = DEFAULT_CART;
	createFileSystem(NULL, onFSInitialized);
}

void onEmscriptenWgetError(const char* error) {}

#endif

s32 main(s32 argc, char **argv)
{
	setbuf(stdout, NULL);
	studio.argc = argc;
	studio.argv = argv;

#if defined(__EMSCRIPTEN__)

	if(studio.argc == 2)
	{
		emscripten_async_wget(studio.argv[1], DEFAULT_CART, onEmscriptenWget, onEmscriptenWgetError);
	}
	else createFileSystem(NULL, onFSInitialized);

	emscripten_set_main_loop(tick, TIC_FRAMERATE == 60 ? 0 : TIC_FRAMERATE, 1);
#else

	createFileSystem(argc > 1 && fsExists(argv[1]) ? fsBasename(argv[1]) : NULL, onFSInitialized);

	{
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;
		
		while (!studio.quitFlag)
		{
			nextTick += Delta;
			tick();

			s64 delay = nextTick - SDL_GetPerformanceCounter();

			if(delay > 0)
				SDL_Delay((u32)(delay * 1000 / SDL_GetPerformanceFrequency()));
			else nextTick -= delay;
		}
	}

#endif

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			SDL_free(studio.editor[i].code);
			SDL_free(studio.editor[i].sprite);
			SDL_free(studio.editor[i].map);
			SDL_free(studio.editor[i].sfx);
			SDL_free(studio.editor[i].music);
		}

		SDL_free(studio.start);
		SDL_free(studio.console);
		SDL_free(studio.run);
		SDL_free(studio.world);
		SDL_free(studio.config);
		SDL_free(studio.keymap);
		SDL_free(studio.dialog);
		SDL_free(studio.menu);
		SDL_free(studio.surf);
	}

	if(studio.tic80local)
		tic80_delete((tic80*)studio.tic80local);

	if(studio.floatSamples)
		SDL_free(studio.floatSamples);

	SDL_DestroyTexture(studio.gamepad.texture);
	SDL_DestroyTexture(studio.texture);

	if(studio.mouse.texture)
		SDL_DestroyTexture(studio.mouse.texture);

	SDL_DestroyRenderer(studio.renderer);
	SDL_DestroyWindow(studio.window);

#if !defined (__MACOSX__)
	// stucks here on macos
	SDL_CloseAudioDevice(studio.audioDevice);
	SDL_Quit();
#endif

	SDLNet_Quit();
	exit(0);

	return 0;
}
