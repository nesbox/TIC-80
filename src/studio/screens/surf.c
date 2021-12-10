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

#include "surf.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "console.h"
#include "ext/gif.h"
#include "ext/png.h"

#if defined(TIC80_PRO)
#include "studio/project.h"
#else
#include "cart.h"
#endif

#include <string.h>

#define MAIN_OFFSET 4
#define MENU_HEIGHT 10
#define ANIM 10
#define COVER_WIDTH 140
#define COVER_HEIGHT 116
#define COVER_Y 5
#define COVER_X (TIC80_WIDTH - COVER_WIDTH - COVER_Y)
#define COVER_FADEIN 96
#define COVER_FADEOUT 256

#if defined(__TIC_WINDOWS__) || defined(__TIC_LINUX__) || defined(__TIC_MACOSX__)
#define CAN_OPEN_URL 1
#endif

static const char* PngExt = PNG_EXT;

typedef struct
{
    s32 start;
    s32 end;
    s32 duration;

    s32* val;
} Anim;

typedef struct Movie Movie;

struct Movie
{
    Anim** items;

    s32 time;
    s32 duration;
    s32 count;

    Movie* next;
    void (*done)(Surf* surf);
};

static struct
{
    s32 topBarY;
    s32 bottomBarY;
    s32 menuX;
    s32 menuHeight;
    s32 coverFade;
} AnimVar;

static Anim topBarShowAnim      = {0, MENU_HEIGHT, ANIM, &AnimVar.topBarY};
static Anim bottomBarShowAnim   = {0, MENU_HEIGHT, ANIM, &AnimVar.bottomBarY};
static Anim topBarHideAnim      = {MENU_HEIGHT, 0, ANIM, &AnimVar.topBarY};
static Anim bottomBarHideAnim   = {MENU_HEIGHT, 0, ANIM, &AnimVar.bottomBarY};
static Anim menuLeftHideAnim    = {0, -TIC80_WIDTH, ANIM, &AnimVar.menuX};
static Anim menuRightHideAnim   = {0, TIC80_WIDTH, ANIM, &AnimVar.menuX};
static Anim menuHideAnim        = {MENU_HEIGHT, 0, ANIM, &AnimVar.menuHeight};
static Anim menuLeftShowAnim    = {TIC80_WIDTH, 0, ANIM, &AnimVar.menuX};
static Anim menuRightShowAnim   = {-TIC80_WIDTH, 0, ANIM, &AnimVar.menuX};
static Anim menuShowAnim        = {0, MENU_HEIGHT, ANIM, &AnimVar.menuHeight};
static Anim coverFadeInAnim     = {COVER_FADEOUT, COVER_FADEIN, ANIM, &AnimVar.coverFade};
static Anim coverFadeOutAnim    = {COVER_FADEIN, COVER_FADEOUT, ANIM, &AnimVar.coverFade};

static Anim* MenuModeShowMovieItems[] = 
{
    &topBarShowAnim,
    &bottomBarShowAnim,
    &menuRightShowAnim,
    &menuShowAnim,
    &coverFadeInAnim,
};

static Anim* MenuModeHideMovieItems[] = 
{
    &topBarHideAnim,
    &bottomBarHideAnim,
    &menuLeftHideAnim,
    &menuHideAnim,
    &coverFadeOutAnim,
};

static Anim* MenuLeftHideMovieItems[] = 
{
    &menuLeftHideAnim,
    &menuHideAnim,
};

static Anim* MenuRightHideMovieItems[] = 
{
    &menuRightHideAnim,
    &menuHideAnim,
};

static Anim* MenuLeftShowMovieItems[] = 
{
    &menuLeftShowAnim,
    &menuShowAnim,
};

static Anim* MenuRightShowMovieItems[] = 
{
    &menuRightShowAnim,
    &menuShowAnim,
};

static Movie EmptyState;
static Movie MenuModeState;

#define DECLARE_MOVIE(NAME, NEXT) static Movie NAME ## State =  \
{                                                               \
    .items = NAME ## MovieItems,                                \
    .count = COUNT_OF(NAME ## MovieItems),                      \
    .duration = ANIM,                                           \
    .next = & NEXT ## State,                                    \
}

DECLARE_MOVIE(MenuModeShow,     MenuMode);
DECLARE_MOVIE(MenuModeHide,     Empty);
DECLARE_MOVIE(MenuLeftShow,     MenuMode);
DECLARE_MOVIE(MenuRightShow,    MenuMode);
DECLARE_MOVIE(MenuLeftHide,     MenuLeftShow);
DECLARE_MOVIE(MenuRightHide,    MenuRightShow);

typedef struct MenuItem MenuItem;

struct MenuItem
{
    char* label;
    char* name;
    char* hash;
    s32 id;
    tic_screen* cover;

    tic_palette* palette;

    bool coverLoading;
    bool dir;
    bool project;
};

typedef struct
{
    MenuItem* items;
    s32 count;
    Surf* surf;
    fs_done_callback done;
    void* data;
} AddMenuItemData;

static void resetMovie(Surf* surf, Movie* movie, void (*done)(Surf* surf))
{
    surf->state = movie;

    for(s32 i = 0; i < movie->count; i++)
    {
        Anim* anim = movie->items[i];
        *anim->val = anim->start;
    }

    movie->time = 0;
    movie->done = done;
}

static void drawTopToolbar(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum{Height = MENU_HEIGHT};

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);

    {
        static const char Label[] = "TIC-80 SURF";
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
        tic_api_print(tic, Label, xl, yl+1, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, xl, yl, tic_color_white, true, 1, false);
    }

    enum{Gap = 10, TipX = 150, SelectWidth = 54};

    u8 colorkey = 0;
    tiles2ram(&tic->ram, &getConfig()->cart->bank0.tiles);
    tic_api_spr(tic, 12, TipX, y+1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "SELECT";
        tic_api_print(tic, Label, TipX + Gap, y+3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap, y+2, tic_color_white, true, 1, false);
    }

    tic_api_spr(tic, 13, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);//&getConfig()->cart->bank0.tiles, 
    {
        static const char Label[] = "BACK";
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +2, tic_color_white, true, 1, false);
    }
}

static MenuItem* getMenuItem(Surf* surf)
{
    return &surf->menu.items[surf->menu.pos];
}

static void drawBottomToolbar(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum{Height = MENU_HEIGHT};

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);
    {
        char label[TICNAME_MAX + 1];
        char dir[TICNAME_MAX];
        tic_fs_dir(surf->fs, dir);

        sprintf(label, "/%s", dir);
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
        tic_api_print(tic, label, xl, yl+1, tic_color_black, true, 1, false);
        tic_api_print(tic, label, xl, yl, tic_color_white, true, 1, false);
    }

#ifdef CAN_OPEN_URL 

    if(surf->menu.count > 0 && getMenuItem(surf)->hash)
    {
        enum{Gap = 10, TipX = 134, SelectWidth = 54};

        u8 colorkey = 0;

        tiles2ram(&tic->ram, &getConfig()->cart->bank0.tiles);
        tic_api_spr(tic, 15, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
        {
            static const char Label[] = "WEBSITE";
            tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 3, tic_color_black, true, 1, false);
            tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 2, tic_color_white, true, 1, false);
        }
    }
#endif

}

static void drawMenu(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum {Height = MENU_HEIGHT};

    tic_api_rect(tic, 0, y + (MENU_HEIGHT - AnimVar.menuHeight)/2, TIC80_WIDTH, AnimVar.menuHeight, tic_color_red);

    for(s32 i = 0; i < surf->menu.count; i++)
    {
        const char* name = surf->menu.items[i].label;

        s32 ym = Height * i + y - surf->menu.pos*MENU_HEIGHT - (surf->menu.anim * surf->menu.anim_target) + (MENU_HEIGHT - TIC_FONT_HEIGHT)/2;

        if (ym > (-(TIC_FONT_HEIGHT + 1)) && ym <= TIC80_HEIGHT) {
            tic_api_print(tic, name, x + MAIN_OFFSET, ym + 1, tic_color_black, false, 1, false);
            tic_api_print(tic, name, x + MAIN_OFFSET, ym, tic_color_white, false, 1, false);
        }
    }
}

static inline void cutExt(char* name, const char* ext)
{
    name[strlen(name)-strlen(ext)] = '\0';
}

static bool addMenuItem(const char* name, const char* title, const char* hash, s32 id, void* ptr, bool dir)
{
    AddMenuItemData* data = (AddMenuItemData*)ptr;

    static const char CartExt[] = CART_EXT;

    if(dir 
        || tic_tool_has_ext(name, CartExt)
        || tic_tool_has_ext(name, PngExt)
#if defined(TIC80_PRO)
        || tic_project_ext(name)
#endif
        )
    {
        data->items = realloc(data->items, sizeof(MenuItem) * ++data->count);
        MenuItem* item = &data->items[data->count-1];

        *item = (MenuItem)
        {
            .name = strdup(name),
            .hash = hash ? strdup(hash) : NULL,
            .id = id,
            .dir = dir,
        };

        if(dir)
        {
            char folder[TICNAME_MAX];
            sprintf(folder, "[%s]", name);
            item->label = strdup(folder);
        }
        else
        {
            item->label = title ? strdup(title) : strdup(name);

            if(tic_tool_has_ext(name, CartExt))
                cutExt(item->label, CartExt);
            else
                item->project = true;
        }
    }

    return true;
}

static s32 itemcmp(const void* a, const void* b)
{
    const MenuItem* item1 = a;
    const MenuItem* item2 = b;

    if(item1->dir != item2->dir)
        return item1->dir ? -1 : 1;
    else if(item1->dir && item2->dir)
        return strcmp(item1->name, item2->name);

    return 0;
}

static void addMenuItemsDone(void* data)
{
    AddMenuItemData* addMenuItemData = data;
    Surf* surf = addMenuItemData->surf;

    surf->menu.items = addMenuItemData->items;
    surf->menu.count = addMenuItemData->count;

    if(!tic_fs_ispubdir(surf->fs))
        qsort(surf->menu.items, surf->menu.count, sizeof *surf->menu.items, itemcmp);

    if (addMenuItemData->done)
        addMenuItemData->done(addMenuItemData->data);

    free(addMenuItemData);

    surf->loading = false;
}

static void resetMenu(Surf* surf)
{
    if(surf->menu.items)
    {
        for(s32 i = 0; i < surf->menu.count; i++)
        {
            MenuItem* item = &surf->menu.items[i];

            free(item->name);

            FREE(item->hash);
            FREE(item->cover);
            FREE(item->label);
            FREE(item->palette);
        }

        free(surf->menu.items);

        surf->menu.items = NULL;
        surf->menu.count = 0;
    }

    surf->menu.pos = 0;
    surf->menu.anim = 0;
}

static void updateMenuItemCover(Surf* surf, s32 pos, const u8* cover, s32 size)
{
    MenuItem* item = &surf->menu.items[pos];

    gif_image* image = gif_read_data(cover, size);

    if(image)
    {
        item->cover = malloc(sizeof(tic_screen));
        item->palette = malloc(sizeof(tic_palette));

        if (image->width == TIC80_WIDTH 
            && image->height == TIC80_HEIGHT 
            && image->colors <= TIC_PALETTE_SIZE)
        {
            memcpy(item->palette, image->palette, image->colors * sizeof(tic_rgb));

            for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                tic_tool_poke4(item->cover->data, i, image->buffer[i]);
        }
        else
        {
            memset(item->cover, 0, sizeof(tic_screen));
            memset(item->palette, 0, sizeof(tic_palette));
        }

        gif_close(image);
    }
}

typedef struct
{
    Surf* surf;
    s32 pos;
    char cachePath[TICNAME_MAX];
    char dir[TICNAME_MAX];
} CoverLoadingData;

static void coverLoaded(const net_get_data* netData)
{
    CoverLoadingData* coverLoadingData = netData->calldata;
    Surf* surf = coverLoadingData->surf;

    if (netData->type == net_get_done)
    {
        tic_fs_saveroot(surf->fs, coverLoadingData->cachePath, netData->done.data, netData->done.size, false);

        char dir[TICNAME_MAX];
        tic_fs_dir(surf->fs, dir);

        if(strcmp(dir, coverLoadingData->dir) == 0)
            updateMenuItemCover(surf, coverLoadingData->pos, netData->done.data, netData->done.size);
    }

    switch (netData->type)
    {
    case net_get_done:
    case net_get_error:
        free(coverLoadingData);
        break;
    default: break;
    }
}

static void requestCover(Surf* surf, MenuItem* item)
{
    CoverLoadingData coverLoadingData = {surf, surf->menu.pos};
    tic_fs_dir(surf->fs, coverLoadingData.dir);

    const char* hash = item->hash;
    sprintf(coverLoadingData.cachePath, TIC_CACHE "%s.gif", hash);

    {
        s32 size = 0;
        void* data = tic_fs_loadroot(surf->fs, coverLoadingData.cachePath, &size);

        if (data)
        {
            updateMenuItemCover(surf, surf->menu.pos, data, size);
            free(data);
        }
    }

    char path[TICNAME_MAX];
    sprintf(path, "/cart/%s/cover.gif", hash);

    tic_net_get(surf->net, path, coverLoaded, MOVE(coverLoadingData));
}

static void loadCover(Surf* surf)
{
    tic_mem* tic = surf->tic;
    
    MenuItem* item = getMenuItem(surf);
    
    if(item->coverLoading)
        return;

    item->coverLoading = true;

    if(!tic_fs_ispubdir(surf->fs))
    {

        s32 size = 0;
        void* data = tic_fs_load(surf->fs, item->name, &size);

        if(data)
        {
            tic_cartridge* cart = (tic_cartridge*)malloc(sizeof(tic_cartridge));

            if(cart)
            {

                if(tic_tool_has_ext(item->name, PngExt))
                {
                    tic_cartridge* pngcart = loadPngCart((png_buffer){data, size});

                    if(pngcart)
                    {
                        memcpy(cart, pngcart, sizeof(tic_cartridge));
                        free(pngcart);
                    }
                    else memset(cart, 0, sizeof(tic_cartridge));
                }
#if defined(TIC80_PRO)
                else if(tic_project_ext(item->name))
                    tic_project_load(item->name, data, size, cart);
#endif
                else
                    tic_cart_load(cart, data, size);

                if(!EMPTY(cart->bank0.screen.data) && !EMPTY(cart->bank0.palette.vbank0.data))
                {
                    memcpy((item->palette = malloc(sizeof(tic_palette))), &cart->bank0.palette.vbank0, sizeof(tic_palette));
                    memcpy((item->cover = malloc(sizeof(tic_screen))), &cart->bank0.screen, sizeof(tic_screen));
                }

                free(cart);
            }

            free(data);
        }
    }
    else if(item->hash && !item->cover)
    {
        requestCover(surf, item);    
    }
}

static void initMenuAsync(Surf* surf, fs_done_callback callback, void* calldata)
{
    resetMenu(surf);

    surf->loading = true;

    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    AddMenuItemData data = { NULL, 0, surf, callback, calldata};

    if(strcmp(dir, "") != 0)
        addMenuItem("..", NULL, NULL, 0, &data, true);

    tic_fs_enum(surf->fs, addMenuItem, addMenuItemsDone, MOVE(data));
}

typedef struct
{
    Surf* surf;
    char* last;
} GoBackDirDoneData;

static void initMenu(Surf* surf)
{
    initMenuAsync(surf, NULL, NULL);
}

static void onGoBackDirDone(void* data)
{
    GoBackDirDoneData* goBackDirDoneData = data;
    Surf* surf = goBackDirDoneData->surf;

    char current[TICNAME_MAX];
    tic_fs_dir(surf->fs, current);

    for(s32 i = 0; i < surf->menu.count; i++)
    {
        const MenuItem* item = &surf->menu.items[i];

        if(item->dir)
        {
            char path[TICNAME_MAX];

            if(strlen(current))
                sprintf(path, "%s/%s", current, item->name);
            else strcpy(path, item->name);

            if(strcmp(path, goBackDirDoneData->last) == 0)
            {
                surf->menu.pos = i;
                break;
            }
        }
    }

    free(goBackDirDoneData->last);
    free(goBackDirDoneData);
}

static void onGoBackDir(Surf* surf)
{
    char last[TICNAME_MAX];
    tic_fs_dir(surf->fs, last);

    tic_fs_dirback(surf->fs);

    GoBackDirDoneData goBackDirDoneData = {surf, strdup(last)};
    initMenuAsync(surf, onGoBackDirDone, MOVE(goBackDirDoneData));
}

static void onGoToDir(Surf* surf)
{
    MenuItem* item = getMenuItem(surf);

    tic_fs_changedir(surf->fs, item->name);
    initMenu(surf);
}

static void goBackDir(Surf* surf)
{
    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    if(strcmp(dir, "") != 0)
    {
        playSystemSfx(2);
        resetMovie(surf, &MenuRightHideState, onGoBackDir);
    }
}

static void changeDirectory(Surf* surf, const char* name)
{
    if (strcmp(name, "..") == 0)
    {
        goBackDir(surf);
    }
    else
    {
        playSystemSfx(2);
        resetMovie(surf, &MenuLeftHideState, onGoToDir);
    }
}

static void onCartLoaded(void* data)
{
    runGameFromSurf();
}

static void onPlayCart(Surf* surf)
{
    MenuItem* item = getMenuItem(surf);

    if (item->hash)
    {
        surf->console->loadByHash(surf->console, item->name, item->hash, NULL, onCartLoaded, NULL);
    }
    else
    {
        surf->console->load(surf->console, item->name);
        runGameFromSurf();
    }
}

static void loadCart(Surf* surf)
{
    MenuItem* item = getMenuItem(surf);

    if(tic_tool_has_ext(item->name, PngExt))
    {
        s32 size = 0;
        void* data = tic_fs_load(surf->fs, item->name, &size);

        if(data)
        {
            tic_cartridge* cart = loadPngCart((png_buffer){data, size});

            if(cart)
            {
                resetMovie(surf, &MenuModeHideState, onPlayCart);
                free(cart);
            }
        }
    }
    else resetMovie(surf, &MenuModeHideState, onPlayCart);
}

static void processAnim(Surf* surf)
{
    enum{Frames = MENU_HEIGHT};

    {
        if(surf->state->time > surf->state->duration)
        {
            if(surf->state->done)
                surf->state->done(surf);

            if(surf->state->next)
                resetMovie(surf, surf->state->next, NULL);
        }

        for(s32 i = 0; i < surf->state->count; i++)
        {
            Anim* anim = surf->state->items[i];

            if(surf->state->time < anim->duration)
            {
                *anim->val = anim->start + (anim->end - anim->start) * surf->state->time / anim->duration;
            }
            else
            {
                *anim->val = anim->end;
            }
        }

        surf->state->time++;

    }

    if(surf->menu.anim > 0)
    {
        surf->menu.anim++;

        if(surf->menu.anim >= Frames)
        {
            s32 old_pos = surf->menu.pos;

            surf->menu.anim = 0;
            surf->menu.pos += surf->menu.anim_target;

            if(surf->menu.pos < 0)
            {
                if(old_pos == 0)
                    surf->menu.pos = surf->menu.count - 1;
                else
                    surf->menu.pos = 0;
            }
            else if(surf->menu.pos >= surf->menu.count)
            {
                if(old_pos == surf->menu.count - 1)
                    surf->menu.pos = 0;
                else
                    surf->menu.pos = surf->menu.count - 1;

            }
        }
    }
}

static void processGamepad(Surf* surf)
{
    tic_mem* tic = surf->tic;

    enum{Frames = MENU_HEIGHT};

    {
        enum{Hold = KEYBOARD_HOLD, Period = Frames};

        enum
        {
            Up, Down, Left, Right, A, B, X, Y
        };

        if(tic_api_btnp(tic, Up, Hold, Period))
        {
            surf->menu.anim = 1;
            surf->menu.anim_target = -1;

            playSystemSfx(2);
        }
        else if(tic_api_btnp(tic, Down, Hold, Period))
        {
            surf->menu.anim = 1;
            surf->menu.anim_target = 1;

            playSystemSfx(2);
        }
        else if(
            tic_api_btnp(tic, Left, Hold, Period)
            || tic_api_keyp(tic, tic_key_pageup, Hold, Period))
        {
            surf->menu.anim = 1;
            surf->menu.anim_target = -5;
        }
        else if(
            tic_api_btnp(tic, Right, Hold, Period)
            || tic_api_keyp(tic, tic_key_pagedown, Hold, Period))
        {
            surf->menu.anim = 1;
            surf->menu.anim_target = 5;
        }

        if(tic_api_btnp(tic, A, -1, -1))
        {
            MenuItem* item = getMenuItem(surf);
            item->dir 
                ? changeDirectory(surf, item->name) 
                : loadCart(surf);
        }

        if(tic_api_btnp(tic, B, -1, -1))
        {
            goBackDir(surf);
        }

#ifdef CAN_OPEN_URL

        if(tic_api_btnp(tic, Y, -1, -1))
        {
            MenuItem* item = getMenuItem(surf);

            if(!item->dir)
            {
                char url[TICNAME_MAX];
                sprintf(url, TIC_WEBSITE "/play?cart=%i", item->id);
                tic_sys_open_path(url);
            }
        }
#endif

    }

}

static void tick(Surf* surf)
{
    if(!surf->init)
    {
        initMenu(surf);

        resetMovie(surf, &MenuModeShowState, NULL);

        surf->init = true;
    }

    surf->ticks++;

    tic_mem* tic = surf->tic;
    tic_api_cls(tic, TIC_COLOR_BG);

    drawBGAnimation(surf->tic, surf->ticks);

    if (surf->menu.count > 0)
    {
        processAnim(surf);

        if (surf->state == &MenuModeState)
        {
            processGamepad(surf);
        }
    }

    if (getStudioMode() != TIC_SURF_MODE) return;

    if (surf->menu.count > 0)
    {
        loadCover(surf);

        tic_screen* cover = getMenuItem(surf)->cover;

        if(cover)
            memcpy(tic->ram.vram.screen.data, cover->data, sizeof(tic_screen));
    }

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram.vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram.vram.palette.data, getConfig()->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        if(surf->menu.count > 0)
        {
            drawMenu(surf, AnimVar.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2);
        }
        else if(!surf->loading)
        {
            static const char Label[] = "You don't have any files...";
            s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, tic_color_white, true, 1, false);
            tic_api_print(tic, Label, (TIC80_WIDTH - size) / 2, (TIC80_HEIGHT - TIC_FONT_HEIGHT)/2, tic_color_white, true, 1, false);
        }

        drawTopToolbar(surf, 0, AnimVar.topBarY - MENU_HEIGHT);
        drawBottomToolbar(surf, 0, TIC80_HEIGHT - AnimVar.bottomBarY);
    }
}

static void resume(Surf* surf)
{
    resetMovie(surf, &MenuModeShowState, NULL);
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Surf* surf = (Surf*)data;

    if(surf->menu.count > 0)
    {
        const MenuItem* item = getMenuItem(surf);

        if(item->palette)
        {
            if(row == 0)
            {
                memcpy(&tic->ram.vram.palette, item->palette, sizeof(tic_palette));
                fadePalette(&tic->ram.vram.palette, AnimVar.coverFade);
            }

            return;
        }
    }

    drawBGAnimationScanline(tic, row);
}

void initSurf(Surf* surf, tic_mem* tic, struct Console* console)
{
    *surf = (Surf)
    {
        .tic = tic,
        .console = console,
        .fs = console->fs,
        .net = console->net,
        .tick = tick,
        .ticks = 0,
        .state = &EmptyState,
        .init = false,
        .loading = true,
        .resume = resume,
        .menu = 
        {
            .pos = 0,
            .anim = 0,
            .items = NULL,
            .count = 0,
        },
        .scanline = scanline,
    };

    tic_fs_makedir(surf->fs, TIC_CACHE);
}

void freeSurf(Surf* surf)
{
    resetMenu(surf);
    free(surf);
}