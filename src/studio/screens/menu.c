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

typedef struct
{
    s32 start;
    s32 end;
    s32 time;

    s32 *value;

    s32(*factor)(s32 d);
} Anim;

typedef struct
{
    void(*done)(Menu *menu);

    s32 time;
    s32 tick;

    s32 count;
    Anim* items;
} Movie;

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
    tic_mem* tic;

    s32 ticks;

    MenuItem* items;
    s32 count;
    s32 pos;
    s32 backPos;

    void* data;
    void(*back)(void*);

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
};

#define BG_ANIM_COLOR tic_color_dark_grey

enum
{
    Up, Down, Left, Right, A, B, X, Y
};

enum{TextMargin = 2, ItemHeight = TIC_FONT_HEIGHT + TextMargin * 2};
enum{Hold = KEYBOARD_HOLD, Period = ItemHeight};

static inline s32 linear(s32 d)
{
    return d;
}

static inline s32 easein(s32 d)
{
    return d * d;
}

static inline s32 lerp(s32 a, s32 b, s32 d0, s32 d1)
{
    return (b - a) * d0 / d1 + a;
}

static void animTick(Menu *menu)
{
    Movie* movie = menu->anim.movie;

    for(Anim* it = movie->items, *end = it + movie->count; it != end; ++it)
        *it->value = lerp(it->start, it->end, it->factor(movie->tick), it->factor(it->time));
}

static void processAnim(Menu *menu)
{
    animTick(menu);

    Movie* movie = menu->anim.movie;

    if(movie->tick == movie->time)
        movie->done(menu);

    movie->tick++;
}

static void resetMovie(Menu *menu, Movie* movie)
{
    (menu->anim.movie = movie)->tick = 0;
    animTick(menu);
}

static void emptyDone() {}

static void menuUpDone(Menu *menu)
{
    menu->pos = (menu->pos + (menu->count - 1)) % menu->count;
    menu->anim.pos = 0;
    resetMovie(menu, &menu->anim.idle);
}

static void menuDownDone(Menu *menu)
{
    menu->pos = (menu->pos + (menu->count + 1)) % menu->count;
    menu->anim.pos = 0;
    resetMovie(menu, &menu->anim.idle);
}

static void startDone(Menu *menu)
{
    resetMovie(menu, &menu->anim.idle);
}

static void closeDone(Menu *menu)
{
    resetMovie(menu, &menu->anim.idle);
    menu->items[menu->pos].handler(menu->data);
}

static void backDone(Menu *menu)
{
    resetMovie(menu, &menu->anim.idle);
    s32 pos = menu->backPos;
    menu->back(menu->data);
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

static void updateOption(MenuOption* option, s32 val)
{
    option->pos = (option->pos + option->count + val) % option->count;
    option->set(option->pos);
    option->pos = option->get();
}

static void drawMenu(Menu* menu, s32 x, s32 y)
{
    if (getStudioMode() != TIC_MENU_MODE)
        return;

    tic_mem* tic = menu->tic;

    if(animIdle(menu))
    {
        if(tic_api_btnp(menu->tic, Up, Hold, Period))
        {
            playSystemSfx(2);
            resetMovie(menu, &menu->anim.up);
        }

        if(tic_api_btnp(menu->tic, Down, Hold, Period))
        {
            playSystemSfx(2);
            resetMovie(menu, &menu->anim.down);
        }

        MenuOption* option = menu->items[menu->pos].option;
        if(option)
        {
            if(tic_api_btnp(menu->tic, Left, Hold, Period))
            {
                playSystemSfx(2);
                updateOption(option, -1);
            }

            if(tic_api_btnp(menu->tic, Right, Hold, Period))
            {
                playSystemSfx(2);
                updateOption(option, +1);
            }            
        }

        if(tic_api_btnp(menu->tic, A, -1, -1) || tic_api_keyp(tic, tic_key_return, -1, -1))
        {
            if(option)
            {
                playSystemSfx(2);
                updateOption(option, +1);
            }
            else if(menu->items[menu->pos].handler)
            {
                playSystemSfx(2);
                resetMovie(menu, &menu->anim.close);
            }
        }

        if(tic_api_btnp(menu->tic, B, -1, -1) && menu->back)
        {
            playSystemSfx(2);
            resetMovie(menu, &menu->anim.back);
        }
    }

    s32 i = 0;
    char buf[TICNAME_MAX];
    for(const MenuItem *it = menu->items, *end = it + menu->count; it != end; ++it, ++i)
    {
        s32 len = it->option
            ? sprintf(buf, "%s%s", it->label, it->option->values[it->option->pos])
            : sprintf(buf, "%s", it->label);

        printShadow(tic, buf, x + (TIC80_WIDTH - len * TIC_FONT_WIDTH) / 2 + menu->anim.offset, 
            y + TextMargin + ItemHeight * (i - menu->pos) - menu->anim.pos, tic_color_white);
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
    tic_rgb* dst = tic->ram.vram.palette.colors + BG_ANIM_COLOR;

    memcpy(dst, &(tic_rgb){val * 3 / 4, val * 4 / 5, val}, sizeof(tic_rgb));
}

static void drawCursor(Menu* menu, s32 x, s32 y)
{
    tic_mem* tic = menu->tic;

    tic_api_rect(tic, x, y - (menu->anim.cursor - ItemHeight) / 2, TIC80_WIDTH, menu->anim.cursor, tic_color_red);
}

#define MOVIE_DEF(TIME, DONE, ...)              \
{                                               \
    .time = TIME,                               \
    .done = DONE,                               \
    .count = COUNT_OF(((Anim[])__VA_ARGS__)),   \
    .items = MOVE((Anim[])__VA_ARGS__),         \
}

Menu* studio_menu_create(struct tic_mem* tic)
{
    Menu* menu = malloc(sizeof(Menu));
    *menu = (Menu)
    {
        .tic = tic,
        .anim =
        {
            .idle = {.done = emptyDone,},

            .start = MOVIE_DEF(10, startDone,
            {
                {-10, 0, 10, &menu->anim.top, linear},
                {10, 0, 10, &menu->anim.bottom, linear},
                {0, 10, 10, &menu->anim.cursor, linear},
                {-TIC80_WIDTH, 0, 10, &menu->anim.offset, linear},
            }),

            .up = MOVIE_DEF(9, menuUpDone, {{0, -9, 9, &menu->anim.pos, easein}}),
            .down = MOVIE_DEF(9, menuDownDone, {{0, 9, 9, &menu->anim.pos, easein}}),

            .close = MOVIE_DEF(10, closeDone,
            {
                {0, -10, 10, &menu->anim.top, linear},
                {0, 10, 10, &menu->anim.bottom, linear},
                {10, 0, 10, &menu->anim.cursor, linear},
                {0, TIC80_WIDTH, 10, &menu->anim.offset, linear},
            }),

            .back = MOVIE_DEF(10, backDone,
            {
                {0, -10, 10, &menu->anim.top, linear},
                {0, 10, 10, &menu->anim.bottom, linear},
                {10, 0, 10, &menu->anim.cursor, linear},
                {0, TIC80_WIDTH, 10, &menu->anim.offset, linear},
            }),
        },
    };

    return menu;
}

#undef MOVIE_DEF

void studio_menu_tick(Menu* menu)
{
    tic_mem* tic = menu->tic;

    if(menu->ticks == 0)
        playSystemSfx(0);

    processAnim(menu);

    if(getStudioMode() != TIC_MENU_MODE)
        return;

    studio_menu_anim(tic, menu->ticks);

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram.vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram.vram.palette.data, getConfig()->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        drawCursor(menu, 0, (TIC80_HEIGHT - ItemHeight) / 2);
        drawMenu(menu, 0, (TIC80_HEIGHT - ItemHeight) / 2);
        drawTopBar(menu, 0, 0);
        drawBottomBar(menu, 0, TIC80_HEIGHT - ItemHeight);
    }

    menu->ticks++;
}

void studio_menu_init(Menu* menu, const MenuItem* items, s32 rows, s32 pos, s32 backPos, void(*back)(void*), void* data)
{
    const s32 size = sizeof menu->items[0] * rows;

    *menu = (Menu)
    {
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
        if(it->option)
            it->option->pos = it->option->get();

    resetMovie(menu, &menu->anim.start);
}

bool studio_menu_back(Menu* menu)
{
    if(menu->back)
    {
        playSystemSfx(2);
        resetMovie(menu, &menu->anim.back);        
    }

    return menu->back != NULL;
}

void studio_menu_free(Menu* menu)
{
    ANIM_STATES(ANIM_FREE);

    FREE(menu->items);
    free(menu);
}
