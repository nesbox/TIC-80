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
#include "studio/fs.h"

static void resumeGame(Menu* menu)
{
    hideGameMenu();
}

static void resetGame(Menu* menu)
{
    tic_mem* tic = menu->tic;

    tic_api_reset(tic);

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
    exitGameMenu();
}

static void exitStudioLocal(Menu* menu)
{
    exitStudio();
}

#if defined(CRT_SHADER_SUPPORT)
static void crtMonitor(Menu* menu)
{
    switchCrtMonitor();
}
#endif

static const struct {const char* label; void(*const handler)(Menu*);} MenuItems[] = 
{
    {"RESUME GAME",     resumeGame},
    {"RESET GAME",      resetGame},
    {"GAMEPAD CONFIG",  gamepadConfig},
#if defined(CRT_SHADER_SUPPORT)
    {"CRT MONITOR",     crtMonitor},
#endif
    {"", NULL},
    {"CLOSE GAME",      closeGame},
    {"QUIT TIC-80",     exitStudioLocal},
};

#define MENU_ITEMS_COUNT (COUNT_OF(MenuItems))

#define DIALOG_WIDTH (TIC80_WIDTH / 2)
#define DIALOG_HEIGHT (TIC80_HEIGHT / 2 + (MENU_ITEMS_COUNT - 6) * TIC_FONT_HEIGHT)

static tic_rect getRect(Menu* menu)
{
    tic_rect rect = {(TIC80_WIDTH - DIALOG_WIDTH) / 2, (TIC80_HEIGHT - DIALOG_HEIGHT) / 2, DIALOG_WIDTH, DIALOG_HEIGHT};

    rect.x -= menu->pos.x;
    rect.y -= menu->pos.y;

    return rect;
}
static void drawDialog(Menu* menu)
{
    tic_rect rect = getRect(menu);

    tic_mem* tic = menu->tic;

    tic_rect header = {rect.x, rect.y - (TOOLBAR_SIZE - 1), rect.w, TOOLBAR_SIZE};

    if(checkMousePos(&header))
    {
        setCursor(tic_cursor_hand);

        if(checkMouseDown(&header, tic_mouse_left))
        {
            if(!menu->drag.active)
            {
                menu->drag.start.x = tic_api_mouse(tic).x + menu->pos.x;
                menu->drag.start.y = tic_api_mouse(tic).y + menu->pos.y;

                menu->drag.active = true;
            }
        }
    }

    if(menu->drag.active)
    {
        setCursor(tic_cursor_hand);

        menu->pos.x = menu->drag.start.x - tic_api_mouse(tic).x;
        menu->pos.y = menu->drag.start.y - tic_api_mouse(tic).y;

        tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};
        if(!checkMouseDown(&rect, tic_mouse_left))
            menu->drag.active = false;
    }

    rect = getRect(menu);

    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_dark_grey);
    tic_api_rectb(tic, rect.x, rect.y, rect.w, rect.h, tic_color_white);
    tic_api_line(tic, rect.x, rect.y+DIALOG_HEIGHT, rect.x+DIALOG_WIDTH-1, rect.y+DIALOG_HEIGHT, tic_color_black);
    tic_api_rect(tic, rect.x, rect.y-(TOOLBAR_SIZE-2), rect.w, TOOLBAR_SIZE-2, tic_color_white);
    tic_api_line(tic, rect.x+1, rect.y-(TOOLBAR_SIZE-1), rect.x+DIALOG_WIDTH-2, rect.y-(TOOLBAR_SIZE-1), tic_color_white);

    {
        static const char Label[] = "GAME MENU";
        s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, 0, false, 1, false);
        tic_api_print(tic, Label, rect.x + (DIALOG_WIDTH - size)/2, rect.y-(TOOLBAR_SIZE-2), tic_color_dark_grey, false, 1, false);
    }

    {
        u8 chromakey = 14;
        tiles2ram(&tic->ram, &getConfig()->cart->bank0.tiles);
        tic_api_spr(tic, 0, rect.x+6, rect.y-4, 2, 2, &chromakey, 1, 1, tic_no_flip, tic_no_rotate);
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

    tic_api_rect(tic, x, y-1, Width, Height+1, tic_color_grey);
    tic_api_pix(tic, x, y+Height-1, tic_color_dark_grey, false);
    tic_api_pix(tic, x+Width-1, y+Height-1, tic_color_dark_grey, false);

    {
        char buf[] = "#1";
        sprintf(buf, "#%i", id+1);
        tic_api_print(tic, buf, x+2, y, (over ? tic_color_white : tic_color_light_grey), false, 1, false);
    }
}

static void drawTab(Menu* menu, s32 x, s32 y, s32 id)
{
    enum{Width = 15, Height = 7};
    tic_mem* tic = menu->tic;

    tic_api_rect(tic, x, y-2, Width, Height+2, tic_color_white);
    tic_api_pix(tic, x, y+Height-1, tic_color_black, false);
    tic_api_pix(tic, x+Width-1, y+Height-1, tic_color_black, false);
    tic_api_rect(tic, x+1, y+Height, Width-2 , 1, tic_color_black);

    {
        char buf[] = "#1";
        sprintf(buf, "#%i", id+1);
        tic_api_print(tic, buf, x+2, y, tic_color_dark_grey, false, 1, false);
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

        if(menu->gamepad.selected == index && menu->ticks % TIC80_FRAMERATE < TIC80_FRAMERATE / 2)
            continue;

        tic_api_spr(tic, 8+i, rect.x, rect.y, 1, 1, &chromakey, 1, 1, tic_no_flip, tic_no_rotate);//&getConfig()->cart->bank0.tiles

        static const char* const Names[tic_keys_count] = {"...", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "minus", "equals", "leftbracket", "rightbracket", "backslash", "semicolon", "apostrophe", "grave", "comma", "period", "slash", "space", "tab", "return", "backspace", "delete", "insert", "pageup", "pagedown", "home", "end", "up", "down", "left", "right", "capslock", "ctrl", "shift", "alt", "escape", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"};

        s32 code = codes[index];
        char label[32];
        strcpy(label, Names[code]);

        if(strlen(label) > MaxChars)
            label[MaxChars] = '\0';

        tic_api_print(tic, label, rect.x+10, rect.y+2, (over ? tic_color_grey : tic_color_dark_grey), false, 1, false);
    }
}

static void drawGamepadSetupTabs(Menu* menu, s32 x, s32 y)
{
    enum{Width = 90, Height = 41, Tabs = TIC_GAMEPADS, TabWidth = 16};
    tic_mem* tic = menu->tic;

    tic_api_rect(tic, x, y, Width, Height, tic_color_white);
    tic_api_pix(tic, x, y, tic_color_dark_blue, false);
    tic_api_pix(tic, x+Width-1, y, tic_color_dark_blue, false);
    tic_api_pix(tic, x, y+Height-1, tic_color_black, false);
    tic_api_pix(tic, x+Width-1, y+Height-1, tic_color_black, false);
    tic_api_rect(tic, x+1, y+Height, Width-2 , 1, tic_color_black);

    for(s32 i = 0; i < Tabs; i++)
        (menu->gamepad.tab == i ? drawTab : drawTabDisabled)(menu, x + 73 - i*TabWidth, y + 43, i);

    drawPlayerButtons(menu, x + 3, y + 3);
}

static void drawGamepadMenu(Menu* menu)
{
    if (getStudioMode() != TIC_MENU_MODE)
        return;

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
        tic_api_print(tic, Label, rect.x, rect.y+1, tic_color_light_grey, false, 1, false);
    }
    else
    {
        tic_api_print(tic, Label, rect.x, rect.y+1, tic_color_black, false, 1, false);
        tic_api_print(tic, Label, rect.x, rect.y, (over ? tic_color_light_grey : tic_color_white), false, 1, false);
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

        drawBitIcon(rect.x-7, rect.y+1, Icon, tic_color_black);
        drawBitIcon(rect.x-7, rect.y, Icon, tic_color_white);
    }

    drawGamepadSetupTabs(menu, dlgRect.x+25, dlgRect.y+4);
}

static void drawMainMenu(Menu* menu)
{
    if (getStudioMode() != TIC_MENU_MODE)
        return;

    tic_mem* tic = menu->tic;

    drawDialog(menu);

    tic_rect rect = getRect(menu);

    {
        for(s32 i = 0; i < MENU_ITEMS_COUNT; i++)
        {
            if(!*MenuItems[i].label)continue;

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
                    MenuItems[i].handler(menu);
                    return;
                }
            }

            const char* text = MenuItems[i].label;
            if(down)
            {
                tic_api_print(tic, text, label.x, label.y+1, tic_color_light_grey, false, 1, false);
            }
            else
            {
                tic_api_print(tic, text, label.x, label.y+1, tic_color_black, false, 1, false);
                tic_api_print(tic, text, label.x, label.y, (over ? tic_color_light_grey : tic_color_white), false, 1, false);
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

                drawBitIcon(label.x-7, label.y+1, Icon, tic_color_black);
                drawBitIcon(label.x-7, label.y, Icon, tic_color_white);
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

        if(tic_api_btnp(menu->tic, A, -1, -1))
        {
            menu->mode = MAIN_MENU_MODE;
            playSystemSfx(2);
        }
    }
}

static void processMainMenuGamepad(Menu* menu)
{
    enum{Count = MENU_ITEMS_COUNT, Hold = 30, Period = 5};

    enum
    {
        Up, Down, Left, Right, A, B, X, Y
    };

    if(tic_api_btnp(menu->tic, Up, Hold, Period))
    {
        do
        {
            if(--menu->main.focus < 0)
                menu->main.focus = Count - 1;

            menu->main.focus = menu->main.focus % Count;
        } while(!*MenuItems[menu->main.focus].label);

        playSystemSfx(2);
    }

    if(tic_api_btnp(menu->tic, Down, Hold, Period))
    {
        do
        {
            menu->main.focus = (menu->main.focus+1) % Count;
        } while(!*MenuItems[menu->main.focus].label);

        playSystemSfx(2);
    }

    if(tic_api_btnp(menu->tic, A, -1, -1))
    {
        MenuItems[menu->main.focus].handler(menu);
    }
}

static void saveMapping(Menu* menu)
{
    tic_fs_saveroot(menu->fs, KEYMAP_DAT_PATH, getKeymap(), KEYMAP_SIZE, true);
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

            if(tic_api_keyp(tic, key, -1, -1))
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
    if(getStudioMode() != TIC_MENU_MODE)
        return;

    tic_mem* tic = menu->tic;

    menu->ticks++;

    processKeyboard(menu);

    if(!menu->init)
    {
        playSystemSfx(0);

        menu->init = true;
    }

    switch(menu->mode)
    {
    case MAIN_MENU_MODE:
        processMainMenuGamepad(menu);
        break;
    case GAMEPAD_MENU_MODE:
        processGamedMenuGamepad(menu);
        break;
    }

    drawBGAnimation(tic, menu->ticks);
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Menu* menu = (Menu*)data;

    drawBGAnimationScanline(tic, row);
}

static void overline(tic_mem* tic, void* data)
{
    Menu* menu = (Menu*)data;

    switch(menu->mode)
    {
    case MAIN_MENU_MODE:
        drawMainMenu(menu);
        break;
    case GAMEPAD_MENU_MODE:
        drawGamepadMenu(menu);
        break;
    }
}

void initMenu(Menu* menu, tic_mem* tic, tic_fs* fs)
{
    *menu = (Menu)
    {
        .init = false,
        .fs = fs,
        .tic = tic,
        .tick = tick,
        .scanline = scanline,
        .overline = overline,
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
}

void freeMenu(Menu* menu)
{
    free(menu);
}