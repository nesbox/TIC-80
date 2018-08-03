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

#include "menu.h"
#include "fs.h"

#define DIALOG_WIDTH (TIC80_WIDTH/2)
#define DIALOG_HEIGHT (TIC80_HEIGHT/2)

static const char* Rows[] = 
{
	"RESUME GAME",
	"RESET GAME",
	"GAMEPAD CONFIG",
	"",
	"CLOSE GAME",
	"QUIT TIC-80",
};

static void resumeGame(Menu* menu)
{
	hideGameMenu();
}

static void resetGame(Menu* menu)
{
	tic_mem* tic = menu->tic;

	tic->api.reset(tic);

	setStudioMode(TIC_RUN_MODE);
}

static void gamepadConfig(Menu* menu)
{
	playSystemSfx(2);
	menu->mode = GAMEPAD_MENU_MODE;
	menu->gamepad.tab = 0;
}

static void closeGame(Menu* menu)
{
	exitFromGameMenu();
}

static void(*const MenuHandlers[])(Menu*) = {resumeGame, resetGame, gamepadConfig, NULL, closeGame, exitStudio};

static tic_rect getRect(Menu* menu)
{
	tic_rect rect = {(TIC80_WIDTH - DIALOG_WIDTH)/2, (TIC80_HEIGHT - DIALOG_HEIGHT)/2, DIALOG_WIDTH, DIALOG_HEIGHT};

	rect.x -= menu->pos.x;
	rect.y -= menu->pos.y;

	return rect;
}
static void drawDialog(Menu* menu)
{
	tic_rect rect = getRect(menu);

	tic_mem* tic = menu->tic;

	tic_rect header = {rect.x, rect.y-(TOOLBAR_SIZE-1), rect.w, TOOLBAR_SIZE};

	if(checkMousePos(&header))
	{
		setCursor(tic_cursor_hand);

		if(checkMouseDown(&header, tic_mouse_left))
		{
			if(!menu->drag.active)
			{
				menu->drag.start.x = getMouseX() + menu->pos.x;
				menu->drag.start.y = getMouseY() + menu->pos.y;

				menu->drag.active = true;
			}
		}
	}

	if(menu->drag.active)
	{
		setCursor(tic_cursor_hand);

		menu->pos.x = menu->drag.start.x - getMouseX();
		menu->pos.y = menu->drag.start.y - getMouseY();

		tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};
		if(!checkMouseDown(&rect, tic_mouse_left))
			menu->drag.active = false;
	}

	rect = getRect(menu);

	tic->api.rect(tic, rect.x, rect.y, rect.w, rect.h, (tic_color_blue));
	tic->api.rect_border(tic, rect.x, rect.y, rect.w, rect.h, (tic_color_white));
	tic->api.line(tic, rect.x, rect.y+DIALOG_HEIGHT, rect.x+DIALOG_WIDTH-1, rect.y+DIALOG_HEIGHT, (tic_color_black));
	tic->api.rect(tic, rect.x, rect.y-(TOOLBAR_SIZE-2), rect.w, TOOLBAR_SIZE-2, (tic_color_white));
	tic->api.line(tic, rect.x+1, rect.y-(TOOLBAR_SIZE-1), rect.x+DIALOG_WIDTH-2, rect.y-(TOOLBAR_SIZE-1), (tic_color_white));

	{
		static const char Label[] = "GAME MENU";
		s32 size = tic->api.text(tic, Label, 0, -TIC_FONT_HEIGHT, 0, false);
		tic->api.text(tic, Label, rect.x + (DIALOG_WIDTH - size)/2, rect.y-(TOOLBAR_SIZE-2), (tic_color_gray), false);
	}

	{
		u8 chromakey = 14;
		tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 0, rect.x+6, rect.y-4, 2, 2, &chromakey, 1, 1, tic_no_flip, tic_no_rotate);
	}	
}

static void drawTabDisabled(Menu* menu, s32 x, s32 y, s32 id)
{
	enum{Width = 15, Height = 7};
	tic_mem* tic = menu->tic;

	tic_rect rect = {x, y, Width, Height};
	bool over = false;

	if(menu->gamepad.tab != id && checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);
		over = true;

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			menu->gamepad.tab = id;
			menu->gamepad.selected = -1;
			return;
		}
	}

	tic->api.rect(tic, x, y-1, Width, Height+1, (tic_color_dark_gray));
	tic->api.pixel(tic, x, y+Height-1, (tic_color_blue));
	tic->api.pixel(tic, x+Width-1, y+Height-1, (tic_color_blue));

	{
		char buf[] = "#1";
		sprintf(buf, "#%i", id+1);
		tic->api.fixed_text(tic, buf, x+2, y, (over ? tic_color_light_blue : tic_color_gray), false);
	}
}

static void drawTab(Menu* menu, s32 x, s32 y, s32 id)
{
	enum{Width = 15, Height = 7};
	tic_mem* tic = menu->tic;

	tic->api.rect(tic, x, y-2, Width, Height+2, (tic_color_white));
	tic->api.pixel(tic, x, y+Height-1, (tic_color_black));
	tic->api.pixel(tic, x+Width-1, y+Height-1, (tic_color_black));
	tic->api.rect(tic, x+1, y+Height, Width-2 , 1, (tic_color_black));

	{
		char buf[] = "#1";
		sprintf(buf, "#%i", id+1);
		tic->api.fixed_text(tic, buf, x+2, y, (tic_color_gray), false);
	}
}

static void drawPlayerButtons(Menu* menu, s32 x, s32 y)
{
	tic_mem* tic = menu->tic;

	u8 chromakey = 0;

	tic_key* codes = getKeymap();

	enum {Width = 41, Height = TIC_SPRITESIZE, Rows = 4, Cols = 2, MaxChars = 5, Buttons = 8};

	for(s32 i = 0; i < Buttons; i++)
	{
		tic_rect rect = {x + i / Rows * (Width+2), y + (i%Rows)*(Height+1), Width, TIC_SPRITESIZE};
		bool over = false;

		s32 index = i+menu->gamepad.tab * Buttons;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			over = true;

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				menu->gamepad.selected = menu->gamepad.selected != index ? index : -1;
			}
		}

		if(menu->gamepad.selected == index && menu->ticks % TIC_FRAMERATE < TIC_FRAMERATE / 2)
			continue;

		tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 8+i, rect.x, rect.y, 1, 1, &chromakey, 1, 1, tic_no_flip, tic_no_rotate);

		static const char* const Names[tic_keys_count] = {"...", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "minus", "equals", "leftbracket", "rightbracket", "backslash", "semicolon", "apostrophe", "grave", "comma", "period", "slash", "space", "tab", "return", "backspace", "delete", "insert", "pageup", "pagedown", "home", "end", "up", "down", "left", "right", "capslock", "ctrl", "shift", "alt", "escape", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"};

		s32 code = codes[index];
		char label[32];
		strcpy(label, Names[code]);

		if(strlen(label) > MaxChars)
			label[MaxChars] = '\0';

		tic->api.text(tic, label, rect.x+10, rect.y+2, (over ? tic_color_gray : tic_color_black), false);
	}
}

static void drawGamepadSetupTabs(Menu* menu, s32 x, s32 y)
{
	enum{Width = 90, Height = 41, Tabs = TIC_GAMEPADS, TabWidth = 16};
	tic_mem* tic = menu->tic;

	tic->api.rect(tic, x, y, Width, Height, (tic_color_white));
	tic->api.pixel(tic, x, y, (tic_color_blue));
	tic->api.pixel(tic, x+Width-1, y, (tic_color_blue));
	tic->api.pixel(tic, x, y+Height-1, (tic_color_black));
	tic->api.pixel(tic, x+Width-1, y+Height-1, (tic_color_black));
	tic->api.rect(tic, x+1, y+Height, Width-2 , 1, (tic_color_black));

	for(s32 i = 0; i < Tabs; i++)
		(menu->gamepad.tab == i ? drawTab : drawTabDisabled)(menu, x + 73 - i*TabWidth, y + 43, i);

	drawPlayerButtons(menu, x + 3, y + 3);
}

static void drawGamepadMenu(Menu* menu)
{
	drawDialog(menu);

	tic_mem* tic = menu->tic;

	tic_rect dlgRect = getRect(menu);

	static const char Label[] = "BACK";

	tic_rect rect = {dlgRect.x + 25, dlgRect.y + 56, (sizeof(Label)-1)*TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

	bool over = false;
	bool down = false;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		over = true;

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			down = true;
		}

		if(checkMouseClick(&rect, tic_mouse_left))
		{
			menu->gamepad.selected = -1;
			menu->mode = MAIN_MENU_MODE;
			playSystemSfx(2);
			return;
		}
	}

	if(down)
	{
		tic->api.text(tic, Label, rect.x, rect.y+1, (tic_color_light_blue), false);
	}
	else
	{
		tic->api.text(tic, Label, rect.x, rect.y+1, (tic_color_black), false);
		tic->api.text(tic, Label, rect.x, rect.y, (over ? tic_color_light_blue : tic_color_white), false);
	}

	{
		static const u8 Icon[] =
		{
			0b10000000,
			0b11000000,
			0b11100000,
			0b11000000,
			0b10000000,
			0b00000000,
			0b00000000,
			0b00000000,
		};

		drawBitIcon(rect.x-7, rect.y+1, Icon, (tic_color_black));
		drawBitIcon(rect.x-7, rect.y, Icon, (tic_color_white));
	}

	drawGamepadSetupTabs(menu, dlgRect.x+25, dlgRect.y+4);
}

static void drawMainMenu(Menu* menu)
{
	tic_mem* tic = menu->tic;

	drawDialog(menu);

	tic_rect rect = getRect(menu);

	{
		for(s32 i = 0; i < COUNT_OF(Rows); i++)
		{
			if(!*Rows[i])continue;

			tic_rect label = {rect.x + 22, rect.y + (TIC_FONT_HEIGHT+1)*i + 16, 86, TIC_FONT_HEIGHT+1};
			bool over = false;
			bool down = false;

			if(checkMousePos(&label))
			{
				setCursor(tic_cursor_hand);

				over = true;

				if(checkMouseDown(&label, tic_mouse_left))
				{
					down = true;
					menu->main.focus = i;
				}

				if(checkMouseClick(&label, tic_mouse_left))
				{
					MenuHandlers[i](menu);
					return;
				}
			}

			if(down)
			{
				tic->api.text(tic, Rows[i], label.x, label.y+1, (tic_color_light_blue), false);
			}
			else
			{
				tic->api.text(tic, Rows[i], label.x, label.y+1, (tic_color_black), false);
				tic->api.text(tic, Rows[i], label.x, label.y, (over ? tic_color_light_blue : tic_color_white), false);
			}

			if(i == menu->main.focus)
			{
				static const u8 Icon[] =
				{
					0b10000000,
					0b11000000,
					0b11100000,
					0b11000000,
					0b10000000,
					0b00000000,
					0b00000000,
					0b00000000,
				};

				drawBitIcon(label.x-7, label.y+1, Icon, (tic_color_black));
				drawBitIcon(label.x-7, label.y, Icon, (tic_color_white));
			}
		}
	}
}

static void processGamedMenuGamepad(Menu* menu)
{
	if(menu->gamepad.selected < 0)
	{
		enum
		{
			Up, Down, Left, Right, A, B, X, Y
		};

		if(menu->tic->api.btnp(menu->tic, A, -1, -1))
		{
			menu->mode = MAIN_MENU_MODE;
			playSystemSfx(2);
		}
	}
}

static void processMainMenuGamepad(Menu* menu)
{
	enum{Count = COUNT_OF(Rows), Hold = 30, Period = 5};

	enum
	{
		Up, Down, Left, Right, A, B, X, Y
	};

	if(menu->tic->api.btnp(menu->tic, Up, Hold, Period))
	{
		do
		{
			if(--menu->main.focus < 0)
				menu->main.focus = Count - 1;

			menu->main.focus = menu->main.focus % Count;
		} while(!*Rows[menu->main.focus]);

		playSystemSfx(2);
	}

	if(menu->tic->api.btnp(menu->tic, Down, Hold, Period))
	{
		do
		{
			menu->main.focus = (menu->main.focus+1) % Count;
		} while(!*Rows[menu->main.focus]);

		playSystemSfx(2);
	}

	if(menu->tic->api.btnp(menu->tic, A, -1, -1))
	{
		MenuHandlers[menu->main.focus](menu);
	}
}

static void saveMapping(Menu* menu)
{
	fsSaveRootFile(menu->fs, KEYMAP_DAT_PATH, getKeymap(), KEYMAP_SIZE, true);
}

static void processKeyboard(Menu* menu)
{
	tic_mem* tic = menu->tic;

	if(tic->ram.input.keyboard.data == 0) return;

	if(menu->gamepad.selected < 0)
		return;

	if(keyWasPressed(tic_key_escape));
	else if(anyKeyWasPressed())
	{
		for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
		{
			tic_key key = tic->ram.input.keyboard.keys[i];

			if(tic->api.keyp(tic, key, -1, -1))
			{
				tic_key* codes = getKeymap();
				codes[menu->gamepad.selected] = key;
				saveMapping(menu);
				break;
			}
		}
	}

	menu->gamepad.selected = -1;
}

static void tick(Menu* menu)
{
	menu->ticks++;

	processKeyboard(menu);

	if(getStudioMode() != TIC_MENU_MODE)
		return;

	if(!menu->init)
	{
		playSystemSfx(0);

		menu->init = true;
	}

	memcpy(menu->tic->ram.vram.screen.data, menu->bg, sizeof menu->tic->ram.vram.screen.data);

	switch(menu->mode)
	{
	case MAIN_MENU_MODE:
		processMainMenuGamepad(menu);
		drawMainMenu(menu);
		break;
	case GAMEPAD_MENU_MODE:
		processGamedMenuGamepad(menu);
		drawGamepadMenu(menu);
		break;
	}
}

void initMenu(Menu* menu, tic_mem* tic, FileSystem* fs)
{
	*menu = (Menu)
	{
		.init = false,
		.fs = fs,
		.tic = tic,
		.tick = tick,
		.bg = menu->bg,
		.ticks = 0,
		.pos = {0, 0},
		.main =
		{
			.focus = 0,
		},
		.gamepad =
		{
			.tab = 0,
			.selected = -1,
		},
		.drag = 
		{
			.start = {0, 0},
			.active = 0,
		},
		.mode = MAIN_MENU_MODE,
	};

	enum{Size = sizeof tic->ram.vram.screen.data};

	if(!menu->bg)
		menu->bg = malloc(Size);

	if(menu->bg)
		memcpy(menu->bg, tic->ram.vram.screen.data, Size);
}