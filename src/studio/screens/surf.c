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
#include "menu.h"
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
#define PAGE 5
#define COVER_WIDTH 140
#define COVER_HEIGHT 116
#define COVER_Y 5
#define COVER_X (TIC80_WIDTH - COVER_WIDTH - COVER_Y)
#define COVER_FADEIN 96
#define COVER_FADEOUT 256
#define CAN_OPEN_URL (__TIC_WINDOWS__ || __TIC_LINUX__ || __TIC_MACOSX__ || __TIC_ANDROID__)

static const char* PngExt = PNG_EXT;

typedef struct SurfItem SurfItem;

struct SurfItem
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
    SurfItem* items;
    s32 count;
    Surf* surf;
    fs_done_callback done;
    void* data;
} AddMenuItemData;

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
    tiles2ram(tic->ram, &getConfig(surf->studio)->cart->bank0.tiles);
    tic_api_spr(tic, 12, TipX, y+1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "SELECT";
        tic_api_print(tic, Label, TipX + Gap, y+3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap, y+2, tic_color_white, true, 1, false);
    }

    tic_api_spr(tic, 13, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "BACK";
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +2, tic_color_white, true, 1, false);
    }
}

static SurfItem* getMenuItem(Surf* surf)
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

        tiles2ram(tic->ram, &getConfig(surf->studio)->cart->bank0.tiles);
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

    tic_api_rect(tic, 0, y + (MENU_HEIGHT - surf->anim.val.menuHeight) / 2, TIC80_WIDTH, surf->anim.val.menuHeight, tic_color_red);

    s32 ym = y - surf->menu.pos * MENU_HEIGHT + (MENU_HEIGHT - TIC_FONT_HEIGHT) / 2 - surf->anim.val.pos;
    for(s32 i = 0; i < surf->menu.count; i++, ym += Height)
    {
        const char* name = surf->menu.items[i].label;

        if (ym > (-(TIC_FONT_HEIGHT + 1)) && ym <= TIC80_HEIGHT) 
        {
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
        data->items = realloc(data->items, sizeof(SurfItem) * ++data->count);
        SurfItem* item = &data->items[data->count-1];

        *item = (SurfItem)
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
    const SurfItem* item1 = a;
    const SurfItem* item2 = b;

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
            SurfItem* item = &surf->menu.items[i];

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
}

static void updateMenuItemCover(Surf* surf, s32 pos, const u8* cover, s32 size)
{
    SurfItem* item = &surf->menu.items[pos];

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

static void requestCover(Surf* surf, SurfItem* item)
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
    
    SurfItem* item = getMenuItem(surf);
    
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

static void initItemsAsync(Surf* surf, fs_done_callback callback, void* calldata)
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

static void onGoBackDirDone(void* data)
{
    GoBackDirDoneData* goBackDirDoneData = data;
    Surf* surf = goBackDirDoneData->surf;

    char current[TICNAME_MAX];
    tic_fs_dir(surf->fs, current);

    for(s32 i = 0; i < surf->menu.count; i++)
    {
        const SurfItem* item = &surf->menu.items[i];

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

    surf->anim.movie = resetMovie(&surf->anim.goback.show);
}

static void onGoBackDir(void* data)
{
    Surf* surf = data;
    char last[TICNAME_MAX];
    tic_fs_dir(surf->fs, last);

    tic_fs_dirback(surf->fs);

    GoBackDirDoneData goBackDirDoneData = {surf, strdup(last)};
    initItemsAsync(surf, onGoBackDirDone, MOVE(goBackDirDoneData));
}

static void onGoToDirDone(void* data)
{
    Surf* surf = data;
    surf->anim.movie = resetMovie(&surf->anim.gotodir.show);
}

static void onGoToDir(void* data)
{
    Surf* surf = data;
    SurfItem* item = getMenuItem(surf);

    tic_fs_changedir(surf->fs, item->name);
    initItemsAsync(surf, onGoToDirDone, surf);
}

static void goBackDir(Surf* surf)
{
    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    if(strcmp(dir, "") != 0)
    {
        playSystemSfx(surf->studio, 2);

        surf->anim.movie = resetMovie(&surf->anim.goback.hide);
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
        playSystemSfx(surf->studio, 2);
        surf->anim.movie = resetMovie(&surf->anim.gotodir.hide);
    }
}

static void onCartLoaded(void* data)
{
    Surf* surf = data;
    runGame(surf->studio);
}

static void onLoadCommandConfirmed(Studio* studio, bool yes, void* data)
{
    if(yes)
    {
        Surf* surf = data;
        SurfItem* item = getMenuItem(surf);

        if (item->hash)
        {
            surf->console->loadByHash(surf->console, item->name, item->hash, NULL, onCartLoaded, surf);
        }
        else
        {
            surf->console->load(surf->console, item->name);
            runGame(surf->studio);
        }
    }
}

static void onPlayCart(void* data)
{
    Surf* surf = data;
    SurfItem* item = getMenuItem(surf);

    studioCartChanged(surf->studio)
        ? confirmLoadCart(surf->studio, onLoadCommandConfirmed, surf)
        : onLoadCommandConfirmed(surf->studio, true, surf);
}

static void loadCart(Surf* surf)
{
    SurfItem* item = getMenuItem(surf);

    if(tic_tool_has_ext(item->name, PngExt))
    {
        s32 size = 0;
        void* data = tic_fs_load(surf->fs, item->name, &size);

        if(data)
        {
            tic_cartridge* cart = loadPngCart((png_buffer){data, size});

            if(cart)
            {
                surf->anim.movie = resetMovie(&surf->anim.play);
                free(cart);
            }
        }
    }
    else surf->anim.movie = resetMovie(&surf->anim.play);
}

static void move(Surf* surf, s32 dir)
{
    surf->menu.target = (surf->menu.pos + surf->menu.count + dir) % surf->menu.count;

    Anim* anim = surf->anim.move.items;
    anim->end = (surf->menu.target - surf->menu.pos) * MENU_HEIGHT;

    surf->anim.movie = resetMovie(&surf->anim.move);
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

        if(tic_api_btnp(tic, Up, Hold, Period)
            || tic_api_keyp(tic, tic_key_up, Hold, Period))
        {
            move(surf, -1);
            playSystemSfx(surf->studio, 2);
        }
        else if(tic_api_btnp(tic, Down, Hold, Period)
            || tic_api_keyp(tic, tic_key_down, Hold, Period))
        {
            move(surf, +1);
            playSystemSfx(surf->studio, 2);
        }
        else if(tic_api_btnp(tic, Left, Hold, Period)
            || tic_api_keyp(tic, tic_key_left, Hold, Period)
            || tic_api_keyp(tic, tic_key_pageup, Hold, Period))
        {
            s32 dir = -PAGE;

            if(surf->menu.pos == 0) dir = -1;
            else if(surf->menu.pos <= PAGE) dir = -surf->menu.pos;

            move(surf, dir);
        }
        else if(tic_api_btnp(tic, Right, Hold, Period)
            || tic_api_keyp(tic, tic_key_right, Hold, Period)
            || tic_api_keyp(tic, tic_key_pagedown, Hold, Period))
        {
            s32 dir = +PAGE, last = surf->menu.count - 1;

            if(surf->menu.pos == last) dir = +1;
            else if(surf->menu.pos + PAGE >= last) dir = last - surf->menu.pos;

            move(surf, dir);
        }

        if(tic_api_btnp(tic, A, -1, -1)
            || ticEnterWasPressed(tic, -1, -1))
        {
            SurfItem* item = getMenuItem(surf);
            item->dir 
                ? changeDirectory(surf, item->name) 
                : loadCart(surf);
        }

        if(tic_api_btnp(tic, B, -1, -1)
            || tic_api_keyp(tic, tic_key_backspace, -1, -1))
        {
            goBackDir(surf);
        }

#ifdef CAN_OPEN_URL

        if(tic_api_btnp(tic, Y, -1, -1))
        {
            SurfItem* item = getMenuItem(surf);

            if(!item->dir)
            {
                char url[TICNAME_MAX];
                sprintf(url, TIC_WEBSITE "/play?cart=%i", item->id);
                tic_sys_open_url(url);
            }
        }
#endif

    }

}

static inline bool isIdle(Surf* surf)
{
    return surf->anim.movie == &surf->anim.idle;
}

static void tick(Surf* surf)
{
    processAnim(surf->anim.movie, surf);

    if(!surf->init)
    {
        initItemsAsync(surf, NULL, NULL);
        surf->anim.movie = resetMovie(&surf->anim.show);
        surf->init = true;
    }

    tic_mem* tic = surf->tic;
    tic_api_cls(tic, TIC_COLOR_BG);

    studio_menu_anim(surf->tic, surf->ticks++);

    if (isIdle(surf) && surf->menu.count > 0)
    {
        processGamepad(surf);
        if(tic_api_keyp(tic, tic_key_escape, -1, -1))
            setStudioMode(surf->studio, TIC_CONSOLE_MODE);
    }

    if (getStudioMode(surf->studio) != TIC_SURF_MODE) return;

    if (surf->menu.count > 0)
    {
        loadCover(surf);

        tic_screen* cover = getMenuItem(surf)->cover;

        if(cover)
            memcpy(tic->ram->vram.screen.data, cover->data, sizeof(tic_screen));
    }

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram->vram.palette.data, getConfig(surf->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        if(surf->menu.count > 0)
        {
            drawMenu(surf, surf->anim.val.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2);
        }
        else if(!surf->loading)
        {
            static const char Label[] = "You don't have any files...";
            s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, tic_color_white, true, 1, false);
            tic_api_print(tic, Label, (TIC80_WIDTH - size) / 2, (TIC80_HEIGHT - TIC_FONT_HEIGHT)/2, tic_color_white, true, 1, false);
        }

        drawTopToolbar(surf, 0, surf->anim.val.topBarY - MENU_HEIGHT);
        drawBottomToolbar(surf, 0, TIC80_HEIGHT - surf->anim.val.bottomBarY);
    }
}

static void resume(Surf* surf)
{
    surf->anim.movie = resetMovie(&surf->anim.show);
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Surf* surf = (Surf*)data;

    if(surf->menu.count > 0)
    {
        const SurfItem* item = getMenuItem(surf);

        if(item->palette)
        {
            if(row == 0)
            {
                memcpy(&tic->ram->vram.palette, item->palette, sizeof(tic_palette));
                fadePalette(&tic->ram->vram.palette, surf->anim.val.coverFade);
            }

            return;
        }
    }

    studio_menu_anim_scanline(tic, row, NULL);
}

static void emptyDone(void* data) {}

static void setIdle(void* data)
{
    Surf* surf = data;
    surf->anim.movie = resetMovie(&surf->anim.idle);
}

static void setLeftShow(void* data)
{
    Surf* surf = data;
    surf->anim.movie = resetMovie(&surf->anim.gotodir.show);
}

static void freeAnim(Surf* surf)
{
    FREE(surf->anim.show.items);
    FREE(surf->anim.play.items);
    FREE(surf->anim.move.items);
    FREE(surf->anim.gotodir.show.items);
    FREE(surf->anim.gotodir.hide.items);
    FREE(surf->anim.goback.show.items);
    FREE(surf->anim.goback.hide.items);
}

static void moveDone(void* data)
{
    Surf* surf = data;
    surf->menu.pos = surf->menu.target;
    surf->anim.val.pos = 0;
    surf->anim.movie = resetMovie(&surf->anim.idle);
}

void initSurf(Surf* surf, Studio* studio, struct Console* console)
{
    freeAnim(surf);

    *surf = (Surf)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .console = console,
        .fs = console->fs,
        .net = console->net,
        .tick = tick,
        .ticks = 0,
        .init = false,
        .loading = true,
        .resume = resume,
        .menu = 
        {
            .pos = 0,
            .items = NULL,
            .count = 0,
        },
        .anim =
        {
            .idle = {.done = emptyDone,},

            .show = MOVIE_DEF(ANIM, setIdle,
            {
                {0, MENU_HEIGHT, ANIM, &surf->anim.val.topBarY, AnimEaseIn},
                {0, MENU_HEIGHT, ANIM, &surf->anim.val.bottomBarY, AnimEaseIn},
                {-TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                {COVER_FADEOUT, COVER_FADEIN, ANIM, &surf->anim.val.coverFade, AnimEaseIn},
            }),

            .play = MOVIE_DEF(ANIM, onPlayCart,
            {
                {MENU_HEIGHT, 0, ANIM, &surf->anim.val.topBarY, AnimEaseIn},
                {MENU_HEIGHT, 0, ANIM, &surf->anim.val.bottomBarY, AnimEaseIn},
                {0, -TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                {COVER_FADEIN, COVER_FADEOUT, ANIM, &surf->anim.val.coverFade, AnimEaseIn},
            }),

            .move = MOVIE_DEF(9, moveDone, {{0, 0, 9, &surf->anim.val.pos, AnimLinear}}),

            .gotodir = 
            {
                .show = MOVIE_DEF(ANIM, setIdle,
                {
                    {TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                    {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                }),

                .hide = MOVIE_DEF(ANIM, onGoToDir,
                {
                    {0, -TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                    {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                }),
            },

            .goback = 
            {
                .show = MOVIE_DEF(ANIM, setIdle,
                {
                    {-TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                    {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                }),

                .hide = MOVIE_DEF(ANIM, onGoBackDir,
                {
                    {0, TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                    {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                }),
            },
        },
        .scanline = scanline,
    };

    surf->anim.movie = resetMovie(&surf->anim.idle);

    tic_fs_makedir(surf->fs, TIC_CACHE);
}

void freeSurf(Surf* surf)
{
    freeAnim(surf);
    resetMenu(surf);
    free(surf);
}
