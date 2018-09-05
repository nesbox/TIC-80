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

#include "ext/gif.h"
#include "ext/md5.h"

#include <zlib.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define FRAME_SIZE (TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32))
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

	tic_point start;
	tic_point end;

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
	System* system;

	tic80_local* tic80local;

	struct
	{
		CartHash hash;
		u64 mdate;
	}cart;

	EditorMode mode;
	EditorMode prevMode;
	EditorMode dialogMode;

	struct
	{
		MouseState state[3];
	} mouse;

	tic_key keycodes[KEYMAP_COUNT];

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

} impl =
{
	.tic80local = NULL,
	.cart = 
	{
		.mdate = 0,
	},

	.mode = TIC_START_MODE,
	.prevMode = TIC_CODE_MODE,
	.dialogMode = TIC_CONSOLE_MODE,

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

	.missedFrame = false,
	.argc = 0,
	.argv = NULL,
};


char getKeyboardText()
{
    tic_mem* tic = impl.studio.tic;

    static const char Symbols[] = 	" abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
    static const char Shift[] =		" ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";

    enum{Count = sizeof Symbols};

    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
    {
        tic_key key = tic->ram.input.keyboard.keys[i];

        if(key > 0 && key < Count && tic->api.keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
        {
            bool caps = tic->api.key(tic, tic_key_capslock);
            bool shift = tic->api.key(tic, tic_key_shift);

            return caps
                ? key >= tic_key_a && key <= tic_key_z 
                    ? shift ? Symbols[key] : Shift[key]
                    : shift ? Shift[key] : Symbols[key]
                : shift ? Shift[key] : Symbols[key];
        }
    }

    return 0;
}

bool keyWasPressed(tic_key key)
{
	tic_mem* tic = impl.studio.tic;
	return tic->api.keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool anyKeyWasPressed()
{
	tic_mem* tic = impl.studio.tic;

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
	return &impl.studio.tic->cart.banks[impl.bank.index.sprites].tiles;
}

tic_map* getBankMap()
{
	return &impl.studio.tic->cart.banks[impl.bank.index.map].map;
}

tic_palette* getBankPalette()
{
	return &impl.studio.tic->cart.banks[impl.bank.index.sprites].palette;
}

void playSystemSfx(s32 id)
{
	const tic_sample* effect = &impl.config->cart.bank0.sfx.samples.data[id];
	impl.studio.tic->api.sfx_ex(impl.studio.tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, 0);
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

		char* clipboard = (char*)malloc(size*Len + 1);

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

			getSystem()->setClipboardText(clipboard);
			free(clipboard);
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
		if(!isspace(str[j]))
			str[i++] = str[j];

	str[i] = '\0';
}

bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces)
{
	if(data)
	{
		if(getSystem()->hasClipboardText())
		{
			char* clipboard = getSystem()->getClipboardText();

			if(clipboard)
			{
				if (remove_white_spaces)
					removeWhiteSpaces(clipboard);
							
				bool valid = strlen(clipboard) == size * 2;

				if(valid) str2buf(clipboard, strlen(clipboard), data, flip);

				getSystem()->freeClipboardText(clipboard);

				return valid;
			}
		}
	}

	return false;
}

void showTooltip(const char* text)
{
	strncpy(impl.tooltip.text, text, sizeof impl.tooltip.text - 1);
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
		tic_rect rect = {x + i*Size, y, Size, Size};

		u8 bgcolor = (tic_color_white);
		u8 color = (tic_color_light_blue);

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			color = Colors[i];
			showTooltip(Tips[i]);

			if(checkMouseDown(&rect, tic_mouse_left))
			{
				bgcolor = color;
				color = (tic_color_white);
			}
			else if(checkMouseClick(&rect, tic_mouse_left))
			{
				setStudioEvent(Events[i]);
			}
		}

		impl.studio.tic->api.rect(tic, x + i * Size, y, Size, Size, bgcolor);
		drawBitIcon(x + i * Size, y, Icons + i*BITS_IN_BYTE, color);
	}
}

const StudioConfig* getConfig()
{
	return &impl.config->data;
}

static bool isGamepadMode()
{
	return impl.mode == TIC_RUN_MODE
		|| impl.mode == TIC_SURF_MODE
		|| impl.mode == TIC_MENU_MODE;
}

#if defined (TIC80_PRO)

static void drawBankIcon(s32 x, s32 y)
{
	tic_mem* tic = impl.studio.tic;

	tic_rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

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
		if(Modes[i] == impl.mode)
		{
			mode = i;
			break;
		}

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		over = true;

		showTooltip("SWITCH BANK");

		if(checkMouseClick(&rect, tic_mouse_left))
			impl.bank.show = !impl.bank.show;
	}

	if(impl.bank.show)
	{
		drawBitIcon(x, y, Icon, tic_color_red);

		enum{Size = TOOLBAR_SIZE};

		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			tic_rect rect = {x + 2 + (i+1)*Size, 0, Size, Size};

			bool over = false;
			if(checkMousePos(&rect))
			{
				setCursor(tic_cursor_hand);
				over = true;

				if(checkMouseClick(&rect, tic_mouse_left))
				{
					if(impl.bank.chained) 
						memset(impl.bank.indexes, i, sizeof impl.bank.indexes);
					else impl.bank.indexes[mode] = i;
				}
			}

			if(i == impl.bank.indexes[mode])
				tic->api.rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_red);

			tic->api.draw_char(tic, '0' + i, rect.x+1, rect.y+1, i == impl.bank.indexes[mode] ? tic_color_white : over ? tic_color_red : tic_color_peach, false);

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

			tic_rect rect = {x + 4 + (TIC_EDITOR_BANKS+1)*Size, 0, Size, Size};

			bool over = false;

			if(checkMousePos(&rect))
			{
				setCursor(tic_cursor_hand);

				over = true;

				if(checkMouseClick(&rect, tic_mouse_left))
				{
					impl.bank.chained = !impl.bank.chained;

					if(impl.bank.chained)
						memset(impl.bank.indexes, impl.bank.indexes[mode], sizeof impl.bank.indexes);
				}
			}

			drawBitIcon(rect.x, rect.y, PinIcon, impl.bank.chained ? tic_color_black : over ? tic_color_dark_gray : tic_color_light_blue);
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
		impl.studio.tic->api.rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

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
		tic_rect rect = {i * Size, 0, Size, Size};

		bool over = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			over = true;

			showTooltip(Tips[i]);

			if(checkMouseClick(&rect, tic_mouse_left))
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

	if(mode >= 0 && !impl.bank.show)
	{
		if(strlen(impl.tooltip.text))
		{
			impl.studio.tic->api.text(tic, impl.tooltip.text, TextOffset, 1, (tic_color_black), false);
		}
		else
		{
			impl.studio.tic->api.text(tic, Names[mode], TextOffset, 1, (tic_color_dark_gray), false);
		}
	}
}

void setStudioEvent(StudioEvent event)
{
	switch(impl.mode)
	{
	case TIC_CODE_MODE: 	
		{
			Code* code = impl.editor[impl.bank.index.code].code;
			code->event(code, event); 			
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = impl.editor[impl.bank.index.sprites].sprite;
			sprite->event(sprite, event); 
		}
	break;
	case TIC_MAP_MODE:
		{
			Map* map = impl.editor[impl.bank.index.map].map;
			map->event(map, event);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = impl.editor[impl.bank.index.sfx].sfx;
			sfx->event(sfx, event);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = impl.editor[impl.bank.index.music].music;
			music->event(music, event);
		}
		break;
	default: break;
	}
}

ClipboardEvent getClipboardEvent()
{
	tic_mem* tic = impl.studio.tic;

	bool shift = tic->api.key(tic, tic_key_shift);
	bool ctrl = tic->api.key(tic, tic_key_ctrl);

	if(ctrl)
	{
		if(keyWasPressed(tic_key_insert) || keyWasPressed(tic_key_c)) return TIC_CLIPBOARD_COPY;
		else if(keyWasPressed(tic_key_x)) return TIC_CLIPBOARD_CUT;
		else if(keyWasPressed(tic_key_v)) return TIC_CLIPBOARD_PASTE;
	}
	else if(shift)
	{
		if(keyWasPressed(tic_key_delete)) return TIC_CLIPBOARD_CUT;
		else if(keyWasPressed(tic_key_insert)) return TIC_CLIPBOARD_PASTE;
	}

	return TIC_CLIPBOARD_NONE;
}

static void showPopupMessage(const char* text)
{
	impl.popup.counter = POPUP_DUR;
	strcpy(impl.popup.message, text);
}

static void exitConfirm(bool yes, void* data)
{
	impl.studio.quit = yes;
}

void exitStudio()
{
	if(impl.mode != TIC_START_MODE && studioCartChanged())
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
				impl.studio.tic->api.pixel(impl.studio.tic, x + TIC_SPRITESIZE - col - 1, y + i, color);
}

static void initWorldMap()
{
	initWorld(impl.world, impl.studio.tic, impl.editor[impl.bank.index.map].map);
}

static void initRunMode()
{
	initRun(impl.run, impl.console, impl.studio.tic);
}

static void initSurfMode()
{
	initSurf(impl.surf, impl.studio.tic, impl.console);
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
	initMenu(impl.menu, impl.studio.tic, impl.fs);
}

void runGameFromSurf()
{
	impl.studio.tic->api.reset(impl.studio.tic);
	setStudioMode(TIC_RUN_MODE);
	impl.prevMode = TIC_SURF_MODE;
}

void exitFromGameMenu()
{
	if(impl.prevMode == TIC_SURF_MODE)
	{
		setStudioMode(TIC_SURF_MODE);
	}
	else
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	impl.console->showGameMenu = false;
}

void resumeRunMode()
{
	impl.mode = TIC_RUN_MODE;
}

void setStudioMode(EditorMode mode)
{
	if(mode != impl.mode)
	{
		EditorMode prev = impl.mode;

		if(prev == TIC_RUN_MODE)
			impl.studio.tic->api.pause(impl.studio.tic);

		if(mode != TIC_RUN_MODE)
			impl.studio.tic->api.reset(impl.studio.tic);

		switch (prev)
		{
		case TIC_START_MODE:
		case TIC_CONSOLE_MODE:
		case TIC_RUN_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
			break;
		case TIC_SURF_MODE:
			impl.prevMode = TIC_CODE_MODE;
			break;
		default: impl.prevMode = prev; break;
		}

		switch(mode)
		{
		case TIC_WORLD_MODE: initWorldMap(); break;
		case TIC_RUN_MODE: initRunMode(); break;
		case TIC_SURF_MODE: impl.surf->resume(impl.surf); break;
		default: break;
		}

		impl.mode = mode;
	}
}

EditorMode getStudioMode()
{
	return impl.mode;
}

void changeStudioMode(s32 dir)
{
	const size_t modeCount = sizeof(Modes)/sizeof(Modes[0]);
	for(size_t i = 0; i < modeCount; i++)
	{
		if(impl.mode == Modes[i])
		{
			setStudioMode(Modes[(i+dir+modeCount) % modeCount]);
			return;
		}
	}
}

static void showGameMenu()
{
	impl.studio.tic->api.pause(impl.studio.tic);
	impl.studio.tic->api.reset(impl.studio.tic);

	initMenuMode();
	impl.mode = TIC_MENU_MODE;
}

void hideGameMenu()
{
	impl.studio.tic->api.resume(impl.studio.tic);
	impl.mode = TIC_RUN_MODE;
}

s32 getMouseX()
{
	tic_mem* tic = impl.studio.tic;

	return tic->ram.input.mouse.x;
}

s32 getMouseY()
{
	tic_mem* tic = impl.studio.tic;

	return tic->ram.input.mouse.y;
}

static inline bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
	return (pt->x >= rect->x) 
		&& (pt->x < (rect->x + rect->w)) 
		&& (pt->y >= rect->y)
		&& (pt->y < (rect->y + rect->h));
}

bool checkMousePos(const tic_rect* rect)
{
	tic_point pos = {getMouseX(), getMouseY()};
	return pointInRect(&pos, rect);
}

bool checkMouseClick(const tic_rect* rect, tic_mouse_btn button)
{
	MouseState* state = &impl.mouse.state[button];

	bool value = state->click
		&& pointInRect(&state->start, rect)
		&& pointInRect(&state->end, rect);

	if(value) state->click = false;

	return value;
}

bool checkMouseDown(const tic_rect* rect, tic_mouse_btn button)
{
	MouseState* state = &impl.mouse.state[button];

	return state->down && pointInRect(&state->start, rect);
}

void setCursor(tic_cursor id)
{
	tic_mem* tic = impl.studio.tic;

	tic->ram.vram.vars.cursor.sprite = id;
}

void hideDialog()
{
	if(impl.dialogMode == TIC_RUN_MODE)
	{
		impl.studio.tic->api.resume(impl.studio.tic);
		impl.mode = TIC_RUN_MODE;
	}
	else setStudioMode(impl.dialogMode);
}

void showDialog(const char** text, s32 rows, DialogCallback callback, void* data)
{
	if(impl.mode != TIC_DIALOG_MODE)
	{
		initDialog(impl.dialog, impl.studio.tic, text, rows, callback, data);
		impl.dialogMode = impl.mode;
		setStudioMode(TIC_DIALOG_MODE);
	}
}

static void resetBanks()
{
	memset(impl.bank.indexes, 0, sizeof impl.bank.indexes);
}

static void initModules()
{
	tic_mem* tic = impl.studio.tic;

	resetBanks();

	for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
	{
		initCode(impl.editor[i].code, impl.studio.tic, &tic->cart.code);
		initSprite(impl.editor[i].sprite, impl.studio.tic, &tic->cart.banks[i].tiles);
		initMap(impl.editor[i].map, impl.studio.tic, &tic->cart.banks[i].map);
		initSfx(impl.editor[i].sfx, impl.studio.tic, &tic->cart.banks[i].sfx);
		initMusic(impl.editor[i].music, impl.studio.tic, &tic->cart.banks[i].music);
	}

	initWorldMap();
}

static void updateHash()
{
	md5(&impl.studio.tic->cart, sizeof(tic_cartridge), impl.cart.hash.data);
}

static void updateMDate()
{
	impl.cart.mdate = fsMDate(impl.console->fs, impl.console->romName);
}

static void updateTitle()
{
	char name[FILENAME_MAX] = TIC_TITLE;

	if(strlen(impl.console->romName))
		sprintf(name, "%s [%s]", TIC_TITLE, impl.console->romName);

	impl.system->setWindowTitle(name);
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
	md5(&impl.studio.tic->cart, sizeof(tic_cartridge), hash.data);

	return memcmp(hash.data, impl.cart.hash.data, sizeof(CartHash)) != 0;
}

tic_key* getKeymap()
{
	return impl.keycodes;
}

static void processGamepadMapping()
{
	tic_mem* tic = impl.studio.tic;

	for(s32 i = 0; i < KEYMAP_COUNT; i++)
		if(impl.keycodes[i] && tic->api.key(tic, impl.keycodes[i]))
			tic->ram.input.gamepads.data |= 1 << i;
}

static bool isGameMenu()
{
	return (impl.mode == TIC_RUN_MODE && impl.console->showGameMenu) || impl.mode == TIC_MENU_MODE;
}

static void goFullscreen()
{
	impl.system->goFullscreen();
}

void runProject()
{
	impl.studio.tic->api.reset(impl.studio.tic);

	if(impl.mode == TIC_RUN_MODE)
	{
		initRunMode();
	}
	else setStudioMode(TIC_RUN_MODE);
}

static void saveProject()
{
	CartSaveResult rom = impl.console->save(impl.console);

	if(rom == CART_SAVE_OK)
	{
		char buffer[FILENAME_MAX];
		sprintf(buffer, "%s SAVED :)", impl.console->romName);

		for(s32 i = 0; i < (s32)strlen(buffer); i++)
			buffer[i] = toupper(buffer[i]);

		showPopupMessage(buffer);
	}
	else if(rom == CART_SAVE_MISSING_NAME) showPopupMessage("SAVE: MISSING CART NAME :|");
	else showPopupMessage("SAVE ERROR :(");
}

static void screen2buffer(u32* buffer, const u32* pixels, tic_rect rect)
{
	pixels += rect.y * TIC80_FULLWIDTH;

	for(s32 i = 0; i < rect.h; i++)
	{
		memcpy(buffer, pixels + rect.x, rect.w * sizeof(pixels[0]));
		pixels += TIC80_FULLWIDTH;
		buffer += rect.w;
	}
}

static void setCoverImage()
{
	tic_mem* tic = impl.studio.tic;

	if(impl.mode == TIC_RUN_MODE)
	{
		enum {Pitch = TIC80_FULLWIDTH*sizeof(u32)};

		tic->api.blit(tic, tic->api.scanline, tic->api.overline, NULL);

		u32* buffer = malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32));

		if(buffer)
		{
			enum{OffsetLeft = (TIC80_FULLWIDTH-TIC80_WIDTH)/2, OffsetTop = (TIC80_FULLHEIGHT-TIC80_HEIGHT)/2};

			tic_rect rect = {OffsetLeft, OffsetTop, TIC80_WIDTH, TIC80_HEIGHT};

			screen2buffer(buffer, tic->screen, rect);

			gif_write_animation(impl.studio.tic->cart.cover.data, &impl.studio.tic->cart.cover.size,
				TIC80_WIDTH, TIC80_HEIGHT, (const u8*)buffer, 1, TIC_FRAMERATE, 1);

			free(buffer);

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
	if(impl.video.buffer)
	{
		{
			s32 size = 0;
			u8* data = malloc(FRAME_SIZE * impl.video.frame);

			gif_write_animation(data, &size, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, (const u8*)impl.video.buffer, impl.video.frame, TIC_FRAMERATE, getConfig()->gifScale);

			fsGetFileData(onVideoExported, "screen.gif", data, size, DEFAULT_CHMOD, NULL);
		}

		free(impl.video.buffer);
		impl.video.buffer = NULL;
	}

	impl.video.record = false;
}

#if !defined(__EMSCRIPTEN__)

static void startVideoRecord()
{
	if(impl.video.record)
	{
		stopVideoRecord();
	}
	else
	{
		impl.video.frames = getConfig()->gifLength * TIC_FRAMERATE;
		impl.video.buffer = malloc(FRAME_SIZE * impl.video.frames);

		if(impl.video.buffer)
		{
			impl.video.record = true;
			impl.video.frame = 0;
		}
	}
}

#endif

static void takeScreenshot()
{
	impl.video.frames = 1;
	impl.video.buffer = malloc(FRAME_SIZE);

	if(impl.video.buffer)
	{
		impl.video.record = true;
		impl.video.frame = 0;
	}
}

static inline bool keyWasPressedOnce(s32 key)
{
	tic_mem* tic = impl.studio.tic;

	return tic->api.keyp(tic, key, -1, -1);
}

static void switchCrtMonitor()
{
	impl.config->data.crtMonitor = !impl.config->data.crtMonitor;
}

static void processShortcuts()
{
	tic_mem* tic = impl.studio.tic;

	if(impl.mode == TIC_START_MODE) return;
	if(impl.mode == TIC_CONSOLE_MODE && !impl.console->active) return;

	bool alt = tic->api.key(tic, tic_key_alt);
	bool ctrl = tic->api.key(tic, tic_key_ctrl);

	if(keyWasPressedOnce(tic_key_f6)) switchCrtMonitor();

	if(isGameMenu())
	{
		if(keyWasPressedOnce(tic_key_escape))
		{
			impl.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
		}
		else if(keyWasPressedOnce(tic_key_f11)) goFullscreen();
		else if(keyWasPressedOnce(tic_key_return))
		{
			if(alt) goFullscreen();
		}
		else if(keyWasPressedOnce(tic_key_f7)) setCoverImage();
		else if(keyWasPressedOnce(tic_key_f8)) takeScreenshot();
#if !defined(__EMSCRIPTEN__)
		else if(keyWasPressedOnce(tic_key_f9)) startVideoRecord();
#endif

		return;
	}

	if(alt)
	{
		if(keyWasPressedOnce(tic_key_grave)) setStudioMode(TIC_CONSOLE_MODE);
		else if(keyWasPressedOnce(tic_key_1)) setStudioMode(TIC_CODE_MODE);
		else if(keyWasPressedOnce(tic_key_2)) setStudioMode(TIC_SPRITE_MODE);
		else if(keyWasPressedOnce(tic_key_3)) setStudioMode(TIC_MAP_MODE);
		else if(keyWasPressedOnce(tic_key_4)) setStudioMode(TIC_SFX_MODE);
		else if(keyWasPressedOnce(tic_key_5)) setStudioMode(TIC_MUSIC_MODE);
		else if(keyWasPressedOnce(tic_key_return)) goFullscreen();
	}
	else if(ctrl)
	{
		if(keyWasPressedOnce(tic_key_pageup)) changeStudioMode(-1);
		else if(keyWasPressedOnce(tic_key_pagedown)) changeStudioMode(1);
		else if(keyWasPressedOnce(tic_key_q)) exitStudio();
		else if(keyWasPressedOnce(tic_key_r)) runProject();
		else if(keyWasPressedOnce(tic_key_return)) runProject();
		else if(keyWasPressedOnce(tic_key_s)) saveProject();
	}
	else
	{
		if(keyWasPressedOnce(tic_key_f1)) setStudioMode(TIC_CODE_MODE);
		else if(keyWasPressedOnce(tic_key_f2)) setStudioMode(TIC_SPRITE_MODE);
		else if(keyWasPressedOnce(tic_key_f3)) setStudioMode(TIC_MAP_MODE);
		else if(keyWasPressedOnce(tic_key_f4)) setStudioMode(TIC_SFX_MODE);
		else if(keyWasPressedOnce(tic_key_f5)) setStudioMode(TIC_MUSIC_MODE);
		else if(keyWasPressedOnce(tic_key_f7)) setCoverImage();
		else if(keyWasPressedOnce(tic_key_f8)) takeScreenshot();
#if !defined(__EMSCRIPTEN__)
		else if(keyWasPressedOnce(tic_key_f9)) startVideoRecord();
#endif
		else if(keyWasPressedOnce(tic_key_f11)) goFullscreen();
		else if(keyWasPressedOnce(tic_key_escape))
		{
			Code* code = impl.editor[impl.bank.index.code].code;

			if(impl.mode == TIC_CODE_MODE && code->mode != TEXT_EDIT_MODE)
			{
				code->escape(code);
				return;
			}

			if(impl.mode == TIC_DIALOG_MODE)
			{
				impl.dialog->escape(impl.dialog);
				return;
			}

			setStudioMode(impl.mode == TIC_CONSOLE_MODE ? impl.prevMode : TIC_CONSOLE_MODE);
		}
	}
}

#if defined(TIC80_PRO)

static void reloadConfirm(bool yes, void* data)
{
	if(yes)
		impl.console->updateProject(impl.console);
}

#endif

static void updateStudioProject()
{
#if defined(TIC80_PRO)

	if(impl.mode != TIC_START_MODE)
	{
		Console* console = impl.console;

		u64 mdate = fsMDate(console->fs, console->romName);

		if(impl.cart.mdate && mdate > impl.cart.mdate)
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
		Code* code = impl.editor[impl.bank.index.code].code;
		impl.console->codeLiveReload.reload(impl.console, code->src);
		if(impl.console->codeLiveReload.active && code->update)
			code->update(code);
	}

}

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
	if(getConfig()->showSync && impl.missedFrame)
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

		const u32* pal = tic_palette_blit(&impl.config->cart.bank0.palette);
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
	if(impl.video.record)
	{
		if(impl.video.frame < impl.video.frames)
		{
			tic_rect rect = {0, 0, TIC80_FULLWIDTH, TIC80_FULLHEIGHT};
			screen2buffer(impl.video.buffer + (TIC80_FULLWIDTH*TIC80_FULLHEIGHT) * impl.video.frame, pixels, rect);

			if(impl.video.frame % TIC_FRAMERATE < TIC_FRAMERATE / 2)
			{
				const u32* pal = tic_palette_blit(&impl.config->cart.bank0.palette);
				drawRecordLabel(pixels, TIC80_WIDTH-24, 8, &pal[tic_color_red]);
			}

			impl.video.frame++;

		}
		else
		{
			stopVideoRecord();
		}
	}
}

static void drawPopup()
{
	if(impl.popup.counter > 0)
	{
		impl.popup.counter--;

		s32 anim = 0;

		enum{Dur = TIC_FRAMERATE/2};

		if(impl.popup.counter < Dur)
			anim = -((Dur - impl.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);
		else if(impl.popup.counter >= (POPUP_DUR - Dur))
			anim = (((POPUP_DUR - Dur) - impl.popup.counter) * (TIC_FONT_HEIGHT+1) / Dur);

		impl.studio.tic->api.rect(impl.studio.tic, 0, anim, TIC80_WIDTH, TIC_FONT_HEIGHT+1, (tic_color_red));
		impl.studio.tic->api.text(impl.studio.tic, impl.popup.message, 
			(s32)(TIC80_WIDTH - strlen(impl.popup.message)*TIC_FONT_WIDTH)/2,
			anim + 1, (tic_color_white), false);
	}
}

static void renderStudio()
{
	tic_mem* tic = impl.studio.tic;

	showTooltip("");

	{
		const tic_sfx* sfx = NULL;
		const tic_music* music = NULL;

		switch(impl.mode)
		{
		case TIC_RUN_MODE:
			sfx = &impl.studio.tic->ram.sfx;
			music = &impl.studio.tic->ram.music;
			break;
		case TIC_START_MODE:
		case TIC_DIALOG_MODE:
		case TIC_MENU_MODE:
		case TIC_SURF_MODE:
			sfx = &impl.config->cart.bank0.sfx;
			music = &impl.config->cart.bank0.music;
			break;
		default:
			sfx = &impl.studio.tic->cart.banks[impl.bank.index.sfx].sfx;
			music = &impl.studio.tic->cart.banks[impl.bank.index.music].music;
		}

		impl.studio.tic->api.tick_start(impl.studio.tic, sfx, music);
	}

	switch(impl.mode)
	{
	case TIC_START_MODE:	impl.start->tick(impl.start); break;
	case TIC_CONSOLE_MODE: 	impl.console->tick(impl.console); break;
	case TIC_RUN_MODE: 		impl.run->tick(impl.run); break;
	case TIC_CODE_MODE: 	
		{
			Code* code = impl.editor[impl.bank.index.code].code;
			code->tick(code);
		}
		break;
	case TIC_SPRITE_MODE:	
		{
			Sprite* sprite = impl.editor[impl.bank.index.sprites].sprite;
			sprite->tick(sprite);		
		}
		break;
	case TIC_MAP_MODE:
		{
			Map* map = impl.editor[impl.bank.index.map].map;
			map->tick(map);
		}
		break;
	case TIC_SFX_MODE:
		{
			Sfx* sfx = impl.editor[impl.bank.index.sfx].sfx;
			sfx->tick(sfx);
		}
		break;
	case TIC_MUSIC_MODE:
		{
			Music* music = impl.editor[impl.bank.index.music].music;
			music->tick(music);
		}
		break;

	case TIC_WORLD_MODE:	impl.world->tick(impl.world); break;
	case TIC_DIALOG_MODE:	impl.dialog->tick(impl.dialog); break;
	case TIC_MENU_MODE:		impl.menu->tick(impl.menu); break;
	case TIC_SURF_MODE:		impl.surf->tick(impl.surf); break;
	default: break;
	}

	drawPopup();

	if(getConfig()->noSound)
		memset(tic->ram.registers, 0, sizeof tic->ram.registers);

	impl.studio.tic->api.tick_end(impl.studio.tic);
}

static void updateSystemFont()
{
	memset(impl.studio.tic->font.data, 0, sizeof(tic_font));

	for(s32 i = 0; i < TIC_FONT_CHARS; i++)
		for(s32 y = 0; y < TIC_SPRITESIZE; y++)
			for(s32 x = 0; x < TIC_SPRITESIZE; x++)
				if(tic_tool_peek4(&impl.config->cart.bank0.sprites.data[i], TIC_SPRITESIZE*(y+1) - x-1))
					impl.studio.tic->font.data[i*BITS_IN_BYTE+y] |= 1 << x;
}

void studioConfigChanged()
{
	Code* code = impl.editor[impl.bank.index.code].code;
	if(code->update)
		code->update(code);

	updateSystemFont();

	getSystem()->updateConfig();
}

u32 unzip(u8** dest, const u8* source, size_t size)
{
	// TODO: remove this size
	enum{DestSize = 10*1024*1024};

	unsigned long destSize = DestSize;

	u8* buffer = (u8*)malloc(destSize);

	if(buffer)
	{
		if(uncompress(buffer, &destSize, source, (unsigned long)size) == Z_OK)
		{
			*dest = (u8*)malloc(destSize+1);
			memcpy(*dest, buffer, destSize);
			(*dest)[destSize] = '\0';
		}

		free(buffer);

		return destSize;
	}

	return 0;
}

static void initKeymap()
{
	FileSystem* fs = impl.fs;

	s32 size = 0;
	u8* data = (u8*)fsLoadFile(fs, KEYMAP_DAT_PATH, &size);

	if(data)
	{
		if(size == KEYMAP_SIZE)
			memcpy(getKeymap(), data, KEYMAP_SIZE);

		free(data);
	}
}

static void processMouseStates()
{
	for(int i = 0; i < COUNT_OF(impl.mouse.state); i++)
		impl.mouse.state[i].click = false;

	tic_mem* tic = impl.studio.tic;

	tic->ram.vram.vars.cursor.sprite = tic_cursor_arrow;
	tic->ram.vram.vars.cursor.system = true;

	for(int i = 0; i < COUNT_OF(impl.mouse.state); i++)
	{
		MouseState* state = &impl.mouse.state[i];

		if(!state->down && (tic->ram.input.mouse.btns & (1 << i)))
		{
			state->down = true;

			state->start.x = tic->ram.input.mouse.x;
			state->start.y = tic->ram.input.mouse.y;
		}
		else if(state->down && !(tic->ram.input.mouse.btns & (1 << i)))
		{
			state->end.x = tic->ram.input.mouse.x;
			state->end.y = tic->ram.input.mouse.y;

			state->click = true;
			state->down = false;
		}
	}
}

static void studioTick()
{
	processShortcuts();
	processMouseStates();
	processGamepadMapping();

	renderStudio();

	tic_mem* tic = impl.studio.tic;
	
	{
		tic_scanline scanline = NULL;
		tic_overline overline = NULL;
		void* data = NULL;

		switch(impl.mode)
		{
		case TIC_RUN_MODE:
			scanline = tic->api.scanline;
			overline = tic->api.overline;
			break;
		case TIC_SPRITE_MODE:
			{
				Sprite* sprite = impl.editor[impl.bank.index.sprites].sprite;
				overline = sprite->overline;
				scanline = sprite->scanline;
				data = sprite;
				memcpy(&tic->ram.vram.palette, getBankPalette(), sizeof(tic_palette));
			}
			break;
		case TIC_MAP_MODE:
			{
				Map* map = impl.editor[impl.bank.index.map].map;
				overline = map->overline;
				scanline = map->scanline;
				data = map;
				memcpy(&tic->ram.vram.palette, getBankPalette(), sizeof(tic_palette));
			}
			break;
		default:
			memcpy(&tic->ram.vram.palette, &impl.config->cart.bank0.palette, sizeof(tic_palette));
			break;
		}

		tic->api.blit(tic, scanline, overline, data);

		recordFrame(tic->screen);
		drawDesyncLabel(tic->screen);
	
	}
}

static void studioClose()
{
	free((void*)getConfig()->crtShader);

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			free(impl.editor[i].code);
			free(impl.editor[i].sprite);
			free(impl.editor[i].map);
			free(impl.editor[i].sfx);
			free(impl.editor[i].music);
		}

		free(impl.start);
		free(impl.console);
		free(impl.run);
		free(impl.world);
		free(impl.config);
		free(impl.dialog);
		free(impl.menu);
		free(impl.surf);
	}

	if(impl.tic80local)
		tic80_delete((tic80*)impl.tic80local);
}

Studio* studioInit(s32 argc, char **argv, s32 samplerate, const char* folder, System* system)
{
	setbuf(stdout, NULL);
	impl.argc = argc;
	impl.argv = argv;
	impl.samplerate = samplerate;

	impl.system = system;

	if(argc > 1 && fsExists(argv[1]))
	{
		char name[FILENAME_MAX];
		fsBasename(argv[1], name);
		impl.fs = createFileSystem(name);
	}
	else impl.fs = createFileSystem(folder);

	impl.tic80local = (tic80_local*)tic80_create(impl.samplerate);
	impl.studio.tic = impl.tic80local->memory;

	{
		for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
		{
			impl.editor[i].code		= calloc(1, sizeof(Code));
			impl.editor[i].sprite	= calloc(1, sizeof(Sprite));
			impl.editor[i].map 		= calloc(1, sizeof(Map));
			impl.editor[i].sfx 		= calloc(1, sizeof(Sfx));
			impl.editor[i].music 	= calloc(1, sizeof(Music));
		}

		impl.start 		= calloc(1, sizeof(Start));
		impl.console 	= calloc(1, sizeof(Console));
		impl.run 		= calloc(1, sizeof(Run));
		impl.world 		= calloc(1, sizeof(World));
		impl.config 	= calloc(1, sizeof(Config));
		impl.dialog 	= calloc(1, sizeof(Dialog));
		impl.menu 		= calloc(1, sizeof(Menu));
		impl.surf 		= calloc(1, sizeof(Surf));
	}

	fsMakeDir(impl.fs, TIC_LOCAL);
	initConfig(impl.config, impl.studio.tic, impl.fs);

	initKeymap();

	initStart(impl.start, impl.studio.tic);
	initConsole(impl.console, impl.studio.tic, impl.fs, impl.config, impl.argc, impl.argv);
	initSurfMode();

	initRunMode();

	initModules();

	if(impl.console->skipStart)
	{
		setStudioMode(TIC_CONSOLE_MODE);
	}

	if(impl.console->goFullscreen)
	{
		goFullscreen();
	}

	if(impl.console->crtMonitor)
	{
		impl.config->data.crtMonitor = true;
	}

	impl.studio.tick = studioTick;
	impl.studio.close = studioClose;
	impl.studio.updateProject = updateStudioProject;
	impl.studio.exit = exitStudio;
	impl.studio.config = getConfig;
	impl.studio.isGamepadMode = isGamepadMode;

	return &impl.studio;
}

System* getSystem()
{
	return impl.system;
}
