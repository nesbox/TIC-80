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

#include "studio/studio.h"
#include "studio/studio_impl.h"
#include "menu.h"
#include "main_menu.h"

static struct MenuImpl 
{
    Menu*       menu;

    struct 
    {
        MenuItem* items;
        s32 count;
    }* gameMenu;

    Gamepads*   gamepads;
    Config*     config;
    tic_mem*    mem;
} menu_impl;

#define OPTION_VALUES(...)                          \
    .values = (const char*[])__VA_ARGS__,           \
    .count = COUNT_OF(((const char*[])__VA_ARGS__))

// TODO: do we need to lock this down further?
static StudioConfig* rwConfig() 
{
    return &menu_impl.config->data;
}

void initMainMenu(Menu *menu, void *gameMenu, Config *config, tic_mem *mem, Gamepads *gamepads) 
{
    menu_impl = (struct MenuImpl)
    {
        .menu = menu,
        .gameMenu = gameMenu,
        .config = config,
        .mem = mem,
        .gamepads = gamepads
    };
}

static s32 optionFullscreenGet()
{
    return tic_sys_fullscreen_get() ? 1 : 0;
}

static void optionFullscreenSet(s32 pos)
{
    tic_sys_fullscreen_set(rwConfig()->options.fullscreen = (pos == 1));
}

static const char OffValue[] =  "OFF";
static const char OnValue[] =   "ON";

static MenuOption FullscreenOption = 
{
    OPTION_VALUES({OffValue, OnValue}),
    optionFullscreenGet,
    optionFullscreenSet,
};

#if defined(CRT_SHADER_SUPPORT)
static s32 optionCrtMonitorGet()
{
    return getConfig()->options.crt ? 1 : 0;
}

static void optionCrtMonitorSet(s32 pos)
{
    rwConfig()->options.crt = pos == 1;
}

static MenuOption CrtMonitorOption = 
{
    OPTION_VALUES({OffValue, OnValue}),
    optionCrtMonitorGet,
    optionCrtMonitorSet,
};

#endif

static s32 optionVSyncGet()
{
    return getConfig()->options.vsync ? 1 : 0;
}

static void optionVSyncSet(s32 pos)
{
    rwConfig()->options.vsync = pos == 1;
}

static MenuOption VSyncOption = 
{
    OPTION_VALUES({OffValue, OnValue}),
    optionVSyncGet,
    optionVSyncSet,
};

static s32 optionVolumeGet()
{
    return getConfig()->options.volume;
}

static void optionVolumeSet(s32 pos)
{
    rwConfig()->options.volume = pos;
}

static MenuOption VolumeOption = 
{
    OPTION_VALUES(
    {
        "00", "01", "02", "03", 
        "04", "05", "06", "07", 
        "08", "09", "10", "11", 
        "12", "13", "14", "15", 
    }),
    optionVolumeGet,
    optionVolumeSet,
};

#if defined(BUILD_EDITORS)

static s32 optionDevModeGet()
{
    return getConfig()->options.devmode ? 1 : 0;
}

static void optionDevModeSet(s32 pos)
{
    rwConfig()->options.devmode = pos == 1;
}

static MenuOption DevModeOption = 
{
    OPTION_VALUES({OffValue, OnValue}),
    optionDevModeGet,
    optionDevModeSet,
};

#endif

static const MenuItem OptionMenu[] =
{
#if defined(CRT_SHADER_SUPPORT)
    {"CRT MONITOR",     NULL,   &CrtMonitorOption},
#endif
#if defined(BUILD_EDITORS)
    {"DEV MODE",        NULL,   &DevModeOption, "The game menu is disabled in dev mode."},
#endif
    {"VSYNC",           NULL,   &VSyncOption, "VSYNC needs restart!"},
    {"FULLSCREEN",      NULL,   &FullscreenOption},
    {"VOLUME",          NULL,   &VolumeOption},
    {"SETUP GAMEPAD",   showGamepadMenu},
    {""},
    {"BACK",            showMainMenu, .back = true},
};

static void showOptionsMenu();
static void gameMenuHandler(void* data)
{
    tic_mem* tic = menu_impl.mem;
    tic_core_script_config(tic)->callback.gamemenu(tic, *(s32*)data, NULL);
    resumeGame();
}

void freeGameMenu()
{
    // we may never have opened the menu
    if (!menu_impl.gameMenu) return;

    if(menu_impl.gameMenu->items)
    {
        for(MenuItem *it = menu_impl.gameMenu->items, *end = it + menu_impl.gameMenu->count; it != end; ++it)
            free((void*)it->label);

        free(menu_impl.gameMenu->items);
        menu_impl.gameMenu->count = 0;
        menu_impl.gameMenu->items = NULL;
    }
}

void initGameMenu()
{
    tic_mem* tic = menu_impl.mem;

    freeGameMenu();

    char* menu = tic_tool_metatag(tic->cart.code.data, "menu", tic_core_script_config(tic)->singleComment);

    if(menu) SCOPE(free(menu))
    {
        MenuItem *items = NULL;
        s32 count = 0;

        char* label = strtok(menu, " ");
        while(label)
        {
            items = realloc(items, sizeof(MenuItem) * ++count);
            items[count - 1] = (MenuItem){strdup(label), gameMenuHandler};

            label = strtok(NULL, " ");
        }

        count += 2;
        items = realloc(items, sizeof(MenuItem) * count);
        items[count - 2] = (MenuItem){strdup("")};
        items[count - 1] = (MenuItem){strdup("BACK"), showMainMenu, .back = true};

        menu_impl.gameMenu->items = items;
        menu_impl.gameMenu->count = count;
    }
}

void showGameMenu()
{
    studio_menu_init(menu_impl.menu, menu_impl.gameMenu->items, menu_impl.gameMenu->count, 0, 0, showMainMenu, NULL);
}

static inline s32 mainMenuStart()
{
    return menu_impl.gameMenu->count ? 0 : 1;
}

static const MenuItem MainMenu[] =
{
    {"GAME MENU",   showGameMenu},
    {"RESUME GAME", resumeGame},
    {"RESET GAME",  resetGame},
#if defined(BUILD_EDITORS)
    {"CLOSE GAME",  exitGame},
#endif
    {"OPTIONS",     showOptionsMenu},
    {""},
    {"QUIT TIC-80", exitStudio},
};

void showMainMenu()
{
    initGameMenu();

    s32 start = mainMenuStart();

    studio_menu_init(menu_impl.menu, MainMenu + start, COUNT_OF(MainMenu) - start, 0, 0, resumeGame, NULL);
}

static void showOptionsMenuPos(s32 pos)
{
    studio_menu_init(menu_impl.menu, OptionMenu, 
        COUNT_OF(OptionMenu), pos, COUNT_OF(MainMenu) - 3 - mainMenuStart(), showMainMenu, NULL);
}

static void showOptionsMenu()
{
    showOptionsMenuPos(COUNT_OF(OptionMenu) - 4);
}

static void saveGamepadMenu()
{
    rwConfig()->options.mapping = menu_impl.gamepads->mapping;
    showOptionsMenuPos(COUNT_OF(OptionMenu) - 3);
}

static void resetGamepadMenu();

static char MappingItems[TIC_BUTTONS][sizeof "RIGHT - RIGHT"];

static const char* const ButtonLabels[] = 
{
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
    "A",
    "B",
    "X",
    "Y",
};

enum{KeyMappingStart = 2};

static void assignMapping(void* data)
{
    menu_impl.gamepads->key = *(s32*)data - KeyMappingStart;

    static const char Fmt[] = "to assign to (%s) button...";
    static char str[sizeof Fmt + STRLEN("RIGHT")];

    static const MenuItem AssignKeyMenu[] =
    {
        {"Please, press a key you want"},
        {str},
    };

    sprintf(str, Fmt, ButtonLabels[menu_impl.gamepads->key]);

    studio_menu_init(menu_impl.menu, AssignKeyMenu, COUNT_OF(AssignKeyMenu), 1, 0, NULL, NULL);
}

static void initGamepadButtons()
{
    static const char* const KeysList[] =
    {
        "...",
        "A",    "B",    "C",    "D",    "E",    "F",    "G",    "H", 
        "I",    "J",    "K",    "L",    "M",    "N",    "O",    "P", 
        "Q",    "R",    "S",    "T",    "U",    "V",    "W",    "X", 
        "Y",    "Z",    "0",    "1",    "2",    "3",    "4",    "5", 
        "6",    "7",    "8",    "9",    "-",    "=",    "[",    "]", 
        "\\",   ";",    "'",    "`",    ",",    ".",    "/",    "SPCE", 
        "TAB",  "RET",  "BACKS","DEL",  "INS",  "PGUP", "PGDN", "HOME", 
        "END",  "UP",   "DOWN", "LEFT", "RIGHT","CAPS", "CTRL", "SHIFT", 
        "ALT",  "ESC",  "F1",   "F2",   "F3",   "F4",   "F5",   "F6", 
        "F7",   "F8",   "F9",   "F10",  "F11",  "F12",
    };

    for(s32 i = 0, index = menu_impl.gamepads->index * TIC_BUTTONS; i != TIC_BUTTONS; ++i)
        sprintf(MappingItems[i], "%-5s - %-5s", ButtonLabels[i], KeysList[menu_impl.gamepads->mapping.data[index++]]);
}

static s32 optionGamepadGet()
{
    return menu_impl.gamepads->index;
}

static void optionGamepadSet(s32 pos)
{
    menu_impl.gamepads->index = pos;
    initGamepadButtons();
}

static MenuOption GamepadOption = 
{
    OPTION_VALUES({"1", "2", "3", "4"}),
    optionGamepadGet,
    optionGamepadSet,
};

void initGamepadMenu()
{
    static const MenuItem GamepadMenu[] =
    {
        {"GAMEPAD", NULL, &GamepadOption},
        {""},

        {MappingItems[0], assignMapping},
        {MappingItems[1], assignMapping},
        {MappingItems[2], assignMapping},
        {MappingItems[3], assignMapping},
        {MappingItems[4], assignMapping},
        {MappingItems[5], assignMapping},
        {MappingItems[6], assignMapping},
        {MappingItems[7], assignMapping},

        {""},
        {"SAVE MAPPING",        saveGamepadMenu},
        {"RESET TO DEFAULTS",   resetGamepadMenu},
        {"BACK",                showOptionsMenu, .back = true},
    };

    initGamepadButtons();

    studio_menu_init(menu_impl.menu, GamepadMenu, COUNT_OF(GamepadMenu), 
        menu_impl.gamepads->key < 0 ? KeyMappingStart : menu_impl.gamepads->key + KeyMappingStart, 
        COUNT_OF(OptionMenu) - 3, showOptionsMenu, NULL);

    menu_impl.gamepads->key = -1;
}

static void resetGamepadMenu()
{
    menu_impl.gamepads->index = 0;
    ZEROMEM(menu_impl.gamepads->mapping);
    tic_sys_default_mapping(&menu_impl.gamepads->mapping);
    initGamepadMenu();
}

void showGamepadMenu()
{
    menu_impl.gamepads->index = 0;
    menu_impl.gamepads->mapping = getConfig()->options.mapping;

    initGamepadMenu();
}