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
#include "studio/studio.h"
#include "studio/fs.h"

#include <math.h>
#include <string.h>

#define ANIM_STATES(macro)  \
    macro(idle)             \
    macro(start)            \
    macro(up)               \
    macro(down)             \
    macro(close)            \
    macro(back)

#define ANIM_MOVIE(name)    Movie name;
#define ANIM_FREE(name)     FREE(menu->anim.name.items);

struct Menu
{
    Studio* studio;
    tic_mem* tic;

    s32 ticks;

    MenuItem* items;
    s32 count;
    s32 pos;
    s32 backPos;

    void* data;
    MenuItemHandler back;

    struct
    {
        s32 pos;
        s32 top;
        s32 bottom;
        s32 cursor;
        s32 offset;

        Movie* movie;

        ANIM_STATES(ANIM_MOVIE)

    } anim;

    struct
    {
        s32 item;
        s32 option;
    } maxwidth;
};

#define BG_ANIM_COLOR tic_color_dark_grey

enum
{
    Up, Down, Left, Right, A, B, X, Y
};

enum{TextMargin = 2, ItemHeight = TIC_FONT_HEIGHT + TextMargin * 2};
enum{Hold = KEYBOARD_HOLD, Period = ItemHeight};

static void emptyDone(void* data) {}

static void menuUpDone(void* data)
{
    Menu *menu = data;
    menu->pos = (menu->pos + (menu->count - 1)) % menu->count;
    if (strcmp("", menu->items[menu->pos].label) == 0) return menuUpDone(data);
    menu->anim.pos = 0;
    menu->anim.movie = resetMovie(&menu->anim.idle);
}

static void menuDownDone(void* data)
{
    Menu *menu = data;
    menu->pos = (menu->pos + (menu->count + 1)) % menu->count;
    if (strcmp("", menu->items[menu->pos].label) == 0) return menuDownDone(data);
    menu->anim.pos = 0;
    menu->anim.movie = resetMovie(&menu->anim.idle);
}

static void startDone(void* data)
{
    Menu *menu = data;
    menu->anim.movie = resetMovie(&menu->anim.idle);
}

static void closeDone(void* data)
{
    Menu *menu = data;
    menu->anim.movie = resetMovie(&menu->anim.idle);
    menu->items[menu->pos].handler(menu->data, menu->pos);
}

static void backDone(void* data)
{
    Menu *menu = data;
    menu->anim.movie = resetMovie(&menu->anim.idle);
    s32 pos = menu->backPos;
    menu->back(menu->data, 0);
    menu->pos = pos;
}

static void printShadow(tic_mem* tic, const char* text, s32 x, s32 y, tic_color color)
{
    tic_api_print(tic, text, x, y + 1, tic_color_black, true, 1, false);
    tic_api_print(tic, text, x, y, color, true, 1, false);
}

static inline bool animIdle(Menu* menu)
{
    return menu->anim.movie == &menu->anim.idle;
}

static void drawTopBar(Menu* menu, s32 x, s32 y)
{
    tic_mem* tic = menu->tic;

    y += menu->anim.top;

    tic_api_rect(tic, x, y, TIC80_WIDTH, ItemHeight, tic_color_grey);
    tic_api_rect(tic, x, y + ItemHeight, TIC80_WIDTH, 1, tic_color_black);

    static const char Text[] = TIC_NAME_FULL;
    printShadow(tic, Text, (TIC80_WIDTH - STRLEN(Text) * TIC_FONT_WIDTH) / 2, y + TextMargin, tic_color_white);
}

static void drawBottomBar(Menu* menu, s32 x, s32 y)
{
    tic_mem* tic = menu->tic;

    y += menu->anim.bottom;

    tic_api_rect(tic, x, y, TIC80_WIDTH, ItemHeight, tic_color_grey);
    tic_api_rect(tic, x, y - 1, TIC80_WIDTH, 1, tic_color_black);

    const char* help = menu->items[menu->pos].help;
    if(help)
    {
        if(menu->ticks % TIC80_FRAMERATE < TIC80_FRAMERATE / 2)
            printShadow(tic, help, x + (TIC80_WIDTH - strlen(help) * TIC_FONT_WIDTH) / 2 + menu->anim.offset,
                y + TextMargin, tic_color_white);
    }
    else
    {
        static const char Text[] = TIC_COPYRIGHT;
        printShadow(tic, Text, (TIC80_WIDTH - STRLEN(Text) * TIC_FONT_WIDTH) / 2, y + TextMargin, tic_color_white);
    }
}

static void updateOption(MenuOption* option, s32 val, void* data)
{
    option->pos = (option->pos + option->count + val) % option->count;
    option->set(data, option->pos);
    option->pos = option->get(data);
}

static void onMenuItem(Menu* menu, const MenuItem* item)
{
    playSystemSfx(menu->studio, 2);
    menu->anim.movie = resetMovie(item->back ? &menu->anim.back : &menu->anim.close);
}

static void drawOptionArrow(Menu* menu, MenuOption* option, s32 x, s32 y, s32 icon, s32 delta)
{
    tic_rect left = {x - 1, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};
    bool down = false;
    bool over = false;
    if(checkMousePos(menu->studio, &left))
    {
        over = true;
        setCursor(menu->studio, tic_cursor_hand);
        down = checkMouseDown(menu->studio, &left, tic_mouse_left);

        if(checkMouseClick(menu->studio, &left, tic_mouse_left))
        {
            playSystemSfx(menu->studio, 2);
            updateOption(option, delta, menu->data);
        }
    }

    if(down)
    {
        drawBitIcon(menu->studio, icon, left.x, left.y, tic_color_white);
    }
    else
    {
        drawBitIcon(menu->studio, icon, left.x, left.y, tic_color_black);
        drawBitIcon(menu->studio, icon, left.x, left.y - 1, over ? tic_color_white : tic_color_light_grey);
    }
}

static void drawMenu(Menu* menu, s32 x, s32 y)
{
    if (getStudioMode(menu->studio) != TIC_MENU_MODE)
        return;

    tic_mem* tic = menu->tic;

    if(animIdle(menu))
    {
        if(tic_api_btnp(menu->tic, Up, Hold, Period)
            || tic_api_keyp(tic, tic_key_up, Hold, Period))
        {
            playSystemSfx(menu->studio, 2);
            menu->anim.movie = resetMovie(&menu->anim.up);
        }

        if(tic_api_btnp(menu->tic, Down, Hold, Period)
            || tic_api_keyp(tic, tic_key_down, Hold, Period))
        {
            playSystemSfx(menu->studio, 2);
            menu->anim.movie = resetMovie(&menu->anim.down);
        }

        MenuItem* item = &menu->items[menu->pos];
        MenuOption* option = item->option;
        if(option)
        {
            if(tic_api_btnp(menu->tic, Left, Hold, Period)
                || tic_api_keyp(tic, tic_key_left, Hold, Period))
            {
                playSystemSfx(menu->studio, 2);
                updateOption(option, -1, menu->data);
            }

            if(tic_api_btnp(menu->tic, Right, Hold, Period)
                || tic_api_keyp(tic, tic_key_right, Hold, Period))
            {
                playSystemSfx(menu->studio, 2);
                updateOption(option, +1, menu->data);
            }            
        }

        if(tic_api_btnp(menu->tic, A, -1, -1) 
           || tic_api_keyp(tic, tic_key_return, Hold, Period))
        {
            if(option)
            {
                playSystemSfx(menu->studio, 2);
                updateOption(option, +1, menu->data);
            }
            else if(menu->items[menu->pos].handler)
                onMenuItem(menu, item);
        }

        if((tic_api_btnp(menu->tic, B, -1, -1) 
            || tic_api_keyp(tic, tic_key_backspace, Hold, Period)) 
                && menu->back)
        {
            playSystemSfx(menu->studio, 2);
            menu->anim.movie = resetMovie(&menu->anim.back);
        }
    }

    s32 i = 0;
    for(const MenuItem *it = menu->items, *end = it + menu->count; it != end; ++it, ++i)
    {
        s32 width = it->option ? menu->maxwidth.item + menu->maxwidth.option + 3 * TIC_FONT_WIDTH : it->width;

        tic_rect rect = {x + (TIC80_WIDTH - width) / 2 + menu->anim.offset, 
            y + TextMargin + ItemHeight * (i - menu->pos) - menu->anim.pos, it->width, TIC_FONT_HEIGHT};

        if (it->hotkey != tic_key_unknown && tic_api_keyp(tic, it->hotkey, Hold, Period))
        {
            // hotkeys not supported on options for simplicity
            if(it->option == NULL && it->handler)
                onMenuItem(menu, it);

            menu->pos = it - menu->items; // set pos so that close will call this handler
        }

        bool down = false;
        if(animIdle(menu) && checkMousePos(menu->studio, &rect) && it->handler)
        {
            setCursor(menu->studio, tic_cursor_hand);

            if(checkMouseDown(menu->studio, &rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(menu->studio, &rect, tic_mouse_left))
            {
                if(it->handler)
                {
                    menu->pos = i;
                    onMenuItem(menu, it);
                }
            }
        }

        if(down)
            tic_api_print(tic, it->label, rect.x, rect.y + 1, tic_color_white, true, 1, false);
        else
            printShadow(tic, it->label, rect.x, rect.y, tic_color_white);

        if(it->option)
        {
            drawOptionArrow(menu, it->option, rect.x + menu->maxwidth.item + TIC_FONT_WIDTH, rect.y, tic_icon_left, -1);
            drawOptionArrow(menu, it->option, 
                rect.x + menu->maxwidth.item + it->option->width + 2 * TIC_FONT_WIDTH, rect.y, tic_icon_right, +1);

            printShadow(tic, it->option->values[it->option->pos], 
                rect.x + menu->maxwidth.item + 2 * TIC_FONT_WIDTH, rect.y, tic_color_yellow);
        }
    }
}

// BG animation based on DevEd code
void studio_menu_anim(tic_mem* tic, s32 ticks)
{
    tic_api_cls(tic, TIC_COLOR_BG);

    enum{Gap = 72};

    for(s32 x = 16; x <= 16 * 16; x += 16)
    {
        s32 ly = Gap - 8 * 32 * 16 / (x - ticks % 16);

        tic_api_line(tic, 0, ly, TIC80_WIDTH, ly, BG_ANIM_COLOR);
        tic_api_line(tic, 0, TIC80_HEIGHT - ly, 
            TIC80_WIDTH, TIC80_HEIGHT - ly, BG_ANIM_COLOR);
    }

    for(s32 x = -32; x <= 32; x++)
    {
        tic_api_line(tic, TIC80_WIDTH / 2 - x * 4, Gap - 16, 
            TIC80_WIDTH / 2 - x * 24, -16, BG_ANIM_COLOR);

        tic_api_line(tic, TIC80_WIDTH / 2 - x * 4, TIC80_HEIGHT - Gap + 16, 
            TIC80_WIDTH / 2 - x * 24, TIC80_HEIGHT + 16, BG_ANIM_COLOR);
    }
}

void studio_menu_anim_scanline(tic_mem* tic, s32 row, void* data)
{
    s32 dir = row < TIC80_HEIGHT / 2 ? 1 : -1;
    s32 val = dir * (TIC80_WIDTH - row * 7 / 2);
    tic_rgb* dst = tic->ram->vram.palette.colors + BG_ANIM_COLOR;

    memcpy(dst, &(tic_rgb){val * 3 / 4, val * 4 / 5, val}, sizeof(tic_rgb));
}

static void drawCursor(Menu* menu, s32 x, s32 y)
{
    tic_mem* tic = menu->tic;

    tic_api_rect(tic, x, y - (menu->anim.cursor - ItemHeight) / 2, TIC80_WIDTH, menu->anim.cursor, tic_color_red);
}

Menu* studio_menu_create(Studio* studio)
{
    Menu* menu = malloc(sizeof(Menu));
    *menu = (Menu)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .anim =
        {
            .idle = {.done = emptyDone,},

            .start = MOVIE_DEF(10, startDone,
            {
                {-10, 0, 10, &menu->anim.top, AnimLinear},
                {10, 0, 10, &menu->anim.bottom, AnimLinear},
                {0, 10, 10, &menu->anim.cursor, AnimLinear},
                {-TIC80_WIDTH, 0, 10, &menu->anim.offset, AnimLinear},
            }),

            .up = MOVIE_DEF(9, menuUpDone, {{0, -9, 9, &menu->anim.pos, AnimEaseIn}}),
            .down = MOVIE_DEF(9, menuDownDone, {{0, 9, 9, &menu->anim.pos, AnimEaseIn}}),

            .close = MOVIE_DEF(10, closeDone,
            {
                {0, -10, 10, &menu->anim.top, AnimLinear},
                {0, 10, 10, &menu->anim.bottom, AnimLinear},
                {10, 0, 10, &menu->anim.cursor, AnimLinear},
                {0, TIC80_WIDTH, 10, &menu->anim.offset, AnimLinear},
            }),

            .back = MOVIE_DEF(10, backDone,
            {
                {0, -10, 10, &menu->anim.top, AnimLinear},
                {0, 10, 10, &menu->anim.bottom, AnimLinear},
                {10, 0, 10, &menu->anim.cursor, AnimLinear},
                {0, TIC80_WIDTH, 10, &menu->anim.offset, AnimLinear},
            }),
        },
    };

    return menu;
}

#undef MOVIE_DEF

void studio_menu_tick(Menu* menu)
{
    tic_mem* tic = menu->tic;

    processAnim(menu->anim.movie, menu);

    // process scroll
    if(animIdle(menu))
    {
        tic80_input* input = &tic->ram->input;

        if(input->mouse.scrolly)
        {
            if(tic_api_key(tic, tic_key_ctrl) || tic_api_key(tic, tic_key_shift))
            {
                MenuOption* option = menu->items[menu->pos].option;
                if(option)
                    updateOption(option, input->mouse.scrolly < 0 ? -1 : +1, menu->data);
            }
            else
            {
                s32 pos = menu->pos + (input->mouse.scrolly < 0 ? +1 : -1);
                menu->pos = CLAMP(pos, 0, menu->count - 1);                
            }
        }
    }

    if(getStudioMode(menu->studio) != TIC_MENU_MODE)
        return;

    studio_menu_anim(tic, menu->ticks);

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_blue);
        memcpy(tic->ram->vram.palette.data, getConfig(menu->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        drawCursor(menu, 0, (TIC80_HEIGHT - ItemHeight) / 2);
        drawMenu(menu, 0, (TIC80_HEIGHT - ItemHeight) / 2);
        drawTopBar(menu, 0, 0);
        drawBottomBar(menu, 0, TIC80_HEIGHT - ItemHeight);
    }

    menu->ticks++;
}

void studio_menu_init(Menu* menu, const MenuItem* items, s32 rows, s32 pos, s32 backPos, MenuItemHandler back, void* data)
{
    const s32 size = sizeof menu->items[0] * rows;

    *menu = (Menu)
    {
        .studio = menu->studio,
        .tic = menu->tic,
        .anim = menu->anim,
        .items = realloc(menu->items, size),
        .data = data,
        .count = rows,
        .pos = pos,
        .backPos = backPos,
        .back = back,
    };

    memcpy(menu->items, items, size);
    for(MenuItem *it = menu->items, *end = it + menu->count; it != end; ++it)
    {
        it->width = strlen(it->label) * TIC_FONT_WIDTH;

        if(it->option)
        {
            if(menu->maxwidth.item < it->width)
                menu->maxwidth.item = it->width;

            for(const char **opt = it->option->values, **end = opt + it->option->count; opt != end; ++opt)
            {
                s32 len = strlen(*opt) * TIC_FONT_WIDTH;

                if(it->option->width < len)
                    it->option->width = len;

                if(menu->maxwidth.option < len)
                    menu->maxwidth.option = len;
            }

            it->option->pos = it->option->get(menu->data);
        }
    }

    menu->anim.movie = resetMovie(&menu->anim.start);
}

bool studio_menu_back(Menu* menu)
{
    if(menu->back)
    {
        playSystemSfx(menu->studio, 2);
        menu->anim.movie = resetMovie(&menu->anim.back);
    }

    return menu->back != NULL;
}

void studio_menu_free(Menu* menu)
{
    ANIM_STATES(ANIM_FREE);

    FREE(menu->items);
    free(menu);
}
