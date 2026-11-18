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
#include "tools.h"
#include "studio/config.h"
#include "studio/fs.h"
#include "studio/keyboard.h"
#include "studio/layout.h"
#include "studio/paths.h"
#include "studio/ram.h"
#include "studio/sound.h"
#include "menu.h"
#include "ext/gif.h"
#include "ext/png.h"

#if defined(TIC80_PRO)
#include "studio/project.h"
#else
#include "cart.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAIN_OFFSET   4
#define MENU_HEIGHT   10
#define ANIM          10
#define PAGE          5
#define COVER_WIDTH   140
#define COVER_HEIGHT  116
#define COVER_Y       5
#define COVER_X       (TIC80_WIDTH - COVER_WIDTH - COVER_Y)
#define COVER_FADEIN  96
#define COVER_FADEOUT 256

// How long a cover the mount did not give is left alone before it is asked for
// again, while its item stays under the cursor.
#define COVER_RETRY_TICKS (TIC80_FRAMERATE * 5)

static const char* PngExt = PNG_EXT;

typedef struct
{
    SurfItem* items;
    s32 count;
    Surf* surf;
    fs_done_callback done;
    void* data;
    char dir[TICNAME_MAX];
} AddMenuItemData;

static void drawTopToolbar(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum
    {
        Height = MENU_HEIGHT
    };

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);

    {
        static const char Label[] = "TIC-80 SURF";
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT) / 2;
        tic_api_print(tic, Label, xl, yl + 1, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, xl, yl, tic_color_white, true, 1, false);
    }

    enum
    {
        Gap = 10,
        TipX = 150,
        SelectWidth = 54
    };

    u8 colorkey = 0;
    tiles2ram(tic->ram, &surf->config->cart->bank0.tiles);
    tic_api_spr(tic, 12, TipX, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "SELECT";
        tic_api_print(tic, Label, TipX + Gap, y + 3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap, y + 2, tic_color_white, true, 1, false);
    }

    tic_api_spr(tic, 13, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "BACK";
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 2, tic_color_white, true, 1, false);
    }
}

static void surfSfx(Surf* surf)
{
    sound_play(surf->tic, &surf->config->cart->bank0.sfx, 2);
}

static SurfItem* getMenuItem(Surf* surf)
{
    return &surf->menu.items[surf->menu.pos];
}

static void drawBottomToolbar(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum
    {
        Height = MENU_HEIGHT
    };

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);
    {
        char label[TICNAME_MAX + 1];
        char dir[TICNAME_MAX];
        tic_fs_dir(surf->fs, dir);

        sprintf(label, "/%s", dir);
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT) / 2;
        tic_api_print(tic, label, xl, yl + 1, tic_color_black, true, 1, false);
        tic_api_print(tic, label, xl, yl, tic_color_white, true, 1, false);
    }
}

static void drawMenu(Surf* surf, s32 x, s32 y)
{
    tic_mem* tic = surf->tic;

    enum
    {
        Height = MENU_HEIGHT
    };

    tic_api_rect(tic, 0, y + (MENU_HEIGHT - surf->anim.val.menuHeight) / 2, TIC80_WIDTH, surf->anim.val.menuHeight, tic_color_red);

    s32 ym = y - surf->menu.pos * MENU_HEIGHT + (MENU_HEIGHT - TIC_FONT_HEIGHT) / 2 - surf->anim.val.pos;
    for(s32 i = 0; i < surf->menu.count; i++, ym += Height)
    {
        const char* name = surf->menu.items[i].label;

        if(ym > (-(TIC_FONT_HEIGHT + 1)) && ym <= TIC80_HEIGHT)
        {
            tic_api_print(tic, name, x + MAIN_OFFSET, ym + 1, tic_color_black, false, 1, false);
            tic_api_print(tic, name, x + MAIN_OFFSET, ym, tic_color_white, false, 1, false);
        }
    }
}

static inline void cutExt(char* name, const char* ext)
{
    name[strlen(name) - strlen(ext)] = '\0';
}

static bool addMenuItem(const fs_item* entry, void* ptr)
{
    AddMenuItemData* data = (AddMenuItemData*)ptr;
    const char* name = entry->name;

    static const char CartExt[] = CART_EXT;

    if(entry->dir || tic_tool_has_ext(name, CartExt) || tic_tool_has_ext(name, PngExt)
#if defined(TIC80_PRO)
       || tic_tool_is_project(name)
#endif
    )
    {
        data->items = realloc(data->items, sizeof(SurfItem) * ++data->count);
        SurfItem* item = &data->items[data->count - 1];

        *item = (SurfItem){
            .name = strdup(name),
            .dir = entry->dir,
        };

        if(entry->dir)
        {
            char folder[TICNAME_MAX];
            sprintf(folder, "[%s]", name);
            item->label = strdup(folder);
        }
        else
        {
            if(entry->title)
                item->label = strdup(entry->title);
            else
            {
                item->label = strdup(name);

                if(tic_tool_has_ext(name, CartExt))
                    cutExt(item->label, CartExt);
            }

            if(!tic_tool_has_ext(name, CartExt))
                item->project = true;
        }
    }

    return true;
}

static void freeMenuItems(SurfItem* items, s32 count)
{
    for(s32 i = 0; i < count; i++)
    {
        SurfItem* item = &items[i];

        free(item->name);

        FREE(item->cover);
        FREE(item->label);
        FREE(item->palette);
    }

    free(items);
}

static void addMenuItemsDone(const fs_usage* usage, fs_result result, void* data)
{
    (void)usage;

    AddMenuItemData* addMenuItemData = data;
    Surf* surf = addMenuItemData->surf;

    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    // A listing answers into the browser only while it is still the folder the
    // browser is in. An answer for a folder left behind would put its items
    // under the wrong path, and menu.pos would point into another list.
    if(strcmp(dir, addMenuItemData->dir) == 0)
    {
        surf->menu.items = addMenuItemData->items;
        surf->menu.count = addMenuItemData->count;

        if(surf->menu.pos >= surf->menu.count)
            surf->menu.pos = 0;

        surf->loading = false;

        if(result.code != FS_OK)
            snprintf(surf->message, sizeof surf->message, "%s",
                     result.message ? result.message : "could not list the folder");
    }
    else
        freeMenuItems(addMenuItemData->items, addMenuItemData->count);

    // The animation the answer was waited on belongs to the screen, not to the
    // listing, and is finished either way.
    if(addMenuItemData->done)
        addMenuItemData->done(addMenuItemData->data);

    free(addMenuItemData);
}

static void resetMenu(Surf* surf)
{
    if(surf->menu.items)
    {
        freeMenuItems(surf->menu.items, surf->menu.count);

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
        // zeroed: the cover is poked a nibble at a time, and a poke reads the
        // byte around the nibble it writes
        item->cover = calloc(1, sizeof(tic_screen));
        item->palette = malloc(sizeof(tic_palette));

        if(image->width == TIC80_WIDTH && image->height == TIC80_HEIGHT && image->colors <= TIC_PALETTE_SIZE)
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
    char name[TICNAME_MAX];
    char dir[TICNAME_MAX];
} CoverLoadingData;

// The item an answer was asked for, or NULL when the list it came from is
// gone: positions mean nothing once the browser lists something else.
static SurfItem* coverItem(Surf* surf, const CoverLoadingData* data)
{
    if(data->pos >= surf->menu.count)
        return NULL;

    SurfItem* item = &surf->menu.items[data->pos];

    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    return strcmp(dir, data->dir) == 0 && strcmp(item->name, data->name) == 0 ? item : NULL;
}

static void coverLoaded(const u8* bytes, s32 size, fs_result result, void* data)
{
    CoverLoadingData* cover = data;
    Surf* surf = cover->surf;
    SurfItem* item = coverItem(surf, cover);

    if(result.code == FS_OK)
    {
        if(item)
            updateMenuItemCover(surf, cover->pos, bytes, size);
    }
    else if(item)
    {
        // A cover the mount did not give is asked for again later, not every frame.
        item->coverLoading = false;
        item->coverRetry = surf->ticks + COVER_RETRY_TICKS;
    }

    free(cover);
}

static void requestCover(Surf* surf, SurfItem* item)
{
    item->coverLoading = true;

    CoverLoadingData* data = malloc(sizeof(CoverLoadingData));

    if(!data)
    {
        item->coverLoading = false;
        return;
    }

    *data = (CoverLoadingData){.surf = surf, .pos = surf->menu.pos};
    snprintf(data->name, sizeof data->name, "%s", item->name);
    tic_fs_dir(surf->fs, data->dir);

    tic_fs_cover(surf->fs, item->name, coverLoaded, data);
}

static void loadCover(Surf* surf)
{
    SurfItem* item = getMenuItem(surf);

    if(item->dir || item->coverLoading || surf->ticks < item->coverRetry || item->cover)
        return;

    requestCover(surf, item);
}

static void initItemsAsync(Surf* surf, fs_done_callback callback, void* calldata)
{
    resetMenu(surf);

    surf->loading = true;

    char dir[TICNAME_MAX];
    tic_fs_dir(surf->fs, dir);

    AddMenuItemData data = {.surf = surf, .done = callback, .data = calldata};

    // The answer says which folder it is for, see addMenuItemsDone.
    strcpy(data.dir, dir);

    if(strcmp(dir, "") != 0)
    {
        fs_item parent = {"..", NULL, true, -1, 0};
        addMenuItem(&parent, &data);
    }

    tic_fs_list(surf->fs, "", addMenuItem, addMenuItemsDone, MOVE(data));
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
            char path[TICNAME_MAX * 2];

            if(strlen(current))
                sprintf(path, "%s/%s", current, item->name);
            else
                strcpy(path, item->name);

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
        surfSfx(surf);

        surf->anim.movie = resetMovie(&surf->anim.goback.hide);
    }
}

static void changeDirectory(Surf* surf, const char* name)
{
    if(strcmp(name, "..") == 0)
    {
        goBackDir(surf);
    }
    else
    {
        surfSfx(surf);
        surf->anim.movie = resetMovie(&surf->anim.gotodir.hide);
    }
}

static void autoSave(Surf* surf)
{
    const char* save_directory = "/downloads";

    if(!tic_fs_isdir(surf->fs, save_directory))
        tic_fs_makedir(surf->fs, save_directory);

    surf->deps.autoSave(surf->deps.host);
}

static void onCartLoaded(fs_result result, void* data)
{
    Surf* surf = data;

    if(result.code != FS_OK)
    {
        snprintf(surf->message, sizeof surf->message, "%s",
                 result.message ? result.message : "cart not loaded");

        // The play movie is over without a cart: the browser comes back.
        surf->anim.movie = resetMovie(&surf->anim.show);
        return;
    }

    surf->message[0] = '\0';

    if(surf->config->data.options.autosave)
    {
        autoSave(surf);
    }

    surf->deps.run(surf->deps.host);
}

static void onLoadCommandConfirmed(void* host, bool yes, void* data)
{
    (void)host;
    if(yes)
    {
        Surf* surf = data;
        SurfItem* item = getMenuItem(surf);

        // One load entry for every mount; the cart runs when the host answers.
        surf->deps.loadCart(surf->deps.host, item->name, onCartLoaded, surf);
    }
}

static void onPlayCart(void* data)
{
    Surf* surf = data;

    surf->deps.cartChanged(surf->deps.host)
        ? surf->deps.confirmLoadCart(surf->deps.host, onLoadCommandConfirmed, surf)
        : onLoadCommandConfirmed(surf->deps.host, true, surf);
}

static void loadCart(Surf* surf)
{
    SurfItem* item = getMenuItem(surf);

    if(tic_tool_has_ext(item->name, PngExt))
    {
        s32 size = 0;
        void* data = fs_read(tic_fs_path(surf->fs, item->name), &size);

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
    else
        surf->anim.movie = resetMovie(&surf->anim.play);
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

    enum
    {
        Frames = MENU_HEIGHT
    };

    {
        enum
        {
            Hold = KEYBOARD_HOLD,
            Period = Frames
        };

        enum
        {
            Up,
            Down,
            Left,
            Right,
            A,
            B,
            X,
            Y
        };

        // The items are what the moving and the opening need; going back and
        // out is left outside, so a folder that failed to list still lets go.
        if(surf->menu.count > 0)
        {
            if(tic_api_btnp(tic, Up, Hold, Period) || tic_api_keyp(tic, tic_key_up, Hold, Period))
            {
                move(surf, -1);
                surfSfx(surf);
            }
            else if(tic_api_btnp(tic, Down, Hold, Period) || tic_api_keyp(tic, tic_key_down, Hold, Period))
            {
                move(surf, +1);
                surfSfx(surf);
            }
            else if(tic_api_btnp(tic, Left, Hold, Period) || tic_api_keyp(tic, tic_key_left, Hold, Period) || tic_api_keyp(tic, tic_key_pageup, Hold, Period))
            {
                s32 dir = -PAGE;

                if(surf->menu.pos == 0)
                    dir = -1;
                else if(surf->menu.pos <= PAGE)
                    dir = -surf->menu.pos;

                move(surf, dir);
            }
            else if(tic_api_btnp(tic, Right, Hold, Period) || tic_api_keyp(tic, tic_key_right, Hold, Period) || tic_api_keyp(tic, tic_key_pagedown, Hold, Period))
            {
                s32 dir = +PAGE, last = surf->menu.count - 1;

                if(surf->menu.pos == last)
                    dir = +1;
                else if(surf->menu.pos + PAGE >= last)
                    dir = last - surf->menu.pos;

                move(surf, dir);
            }

            if(tic_api_btnp(tic, A, -1, -1) || keyboard_enter(tic, -1, -1))
            {
                SurfItem* item = getMenuItem(surf);
                item->dir
                    ? changeDirectory(surf, item->name)
                    : loadCart(surf);
            }
        }

        if(tic_api_btnp(tic, B, -1, -1) || tic_api_keyp(tic, tic_key_backspace, -1, -1))
        {
            if(tic_fs_isroot(surf->fs))
                surf->deps.exit(surf->deps.host);
            else
                goBackDir(surf);
        }
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

    menu_anim(surf->tic, surf->ticks++);

    if(isIdle(surf))
    {
        processGamepad(surf);

        // Escape leaves the browser whatever the list holds: an empty folder
        // must not be a screen with no way out of it.
        if(tic_api_keyp(tic, tic_key_escape, -1, -1))
            surf->deps.exit(surf->deps.host);
    }

    if(!surf->deps.isActive(surf->deps.host)) return;

    if(surf->menu.count > 0)
    {
        loadCover(surf);

        tic_screen* cover = getMenuItem(surf)->cover;

        if(cover)
            memcpy(tic->ram->vram.screen.data, cover->data, sizeof(tic_screen));
    }

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram->vram.palette.data, surf->config->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        if(surf->menu.count > 0)
        {
            drawMenu(surf, surf->anim.val.menuX, (TIC80_HEIGHT - MENU_HEIGHT) / 2);
        }
        else if(!surf->loading)
        {
            static const char Label[] = "You don't have any files...";
            s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, tic_color_white, true, 1, false);
            tic_api_print(tic, Label, (TIC80_WIDTH - size) / 2, (TIC80_HEIGHT - TIC_FONT_HEIGHT) / 2, tic_color_white, true, 1, false);
        }

        drawTopToolbar(surf, 0, surf->anim.val.topBarY - MENU_HEIGHT);
        drawBottomToolbar(surf, 0, TIC80_HEIGHT - surf->anim.val.bottomBarY);

        if(surf->message[0])
        {
            s32 size = tic_api_print(tic, surf->message, 0, -TIC_FONT_HEIGHT, tic_color_red, true, 1, false);
            tic_api_print(tic, surf->message, (TIC80_WIDTH - size) / 2,
                          TIC80_HEIGHT - 2 * MENU_HEIGHT - TIC_FONT_HEIGHT, tic_color_red, true, 1, false);
        }
    }
}

static void resume(Surf* surf)
{
    surf->anim.movie = resetMovie(&surf->anim.show);
}

// The cover's palette is faded in as its cart is picked.
static void fadePalette(tic_palette* pal, s32 value)
{
    for(u8 *i = pal->data, *end = i + sizeof(tic_palette); i < end; i++)
        *i = (u8)(*i * value >> 8);
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

    menu_anim_scanline(tic, row, NULL);
}

static void emptyDone(void* data)
{
    (void)data;
}

static void setIdle(void* data)
{
    Surf* surf = data;
    surf->anim.movie = resetMovie(&surf->anim.idle);
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

void surf_init(Surf* surf, const SurfDeps* deps)
{
    freeAnim(surf);
    resetMenu(surf);

    *surf = (Surf){
        .deps = *deps,
        .tic = deps->tic,
        .config = deps->config,
        .fs = deps->fs,
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
                .idle = {
                    .done = emptyDone,
                },

                .show = MOVIE_DEF(ANIM, setIdle, {
                                                     {0, MENU_HEIGHT, ANIM, &surf->anim.val.topBarY, AnimEaseIn},
                                                     {0, MENU_HEIGHT, ANIM, &surf->anim.val.bottomBarY, AnimEaseIn},
                                                     {-TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                     {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                     {COVER_FADEOUT, COVER_FADEIN, ANIM, &surf->anim.val.coverFade, AnimEaseIn},
                                                 }),

                .play = MOVIE_DEF(ANIM, onPlayCart, {
                                                        {MENU_HEIGHT, 0, ANIM, &surf->anim.val.topBarY, AnimEaseIn},
                                                        {MENU_HEIGHT, 0, ANIM, &surf->anim.val.bottomBarY, AnimEaseIn},
                                                        {0, -TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                        {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                        {COVER_FADEIN, COVER_FADEOUT, ANIM, &surf->anim.val.coverFade, AnimEaseIn},
                                                    }),

                .move = MOVIE_DEF(9, moveDone, {{0, 0, 9, &surf->anim.val.pos, AnimLinear}}),

                .gotodir = {
                    .show = MOVIE_DEF(ANIM, setIdle, {
                                                         {TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                         {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                     }),

                    .hide = MOVIE_DEF(ANIM, onGoToDir, {
                                                           {0, -TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                           {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                       }),
                },

                .goback = {
                    .show = MOVIE_DEF(ANIM, setIdle, {
                                                         {-TIC80_WIDTH, 0, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                         {0, MENU_HEIGHT, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                     }),

                    .hide = MOVIE_DEF(ANIM, onGoBackDir, {
                                                             {0, TIC80_WIDTH, ANIM, &surf->anim.val.menuX, AnimEaseIn},
                                                             {MENU_HEIGHT, 0, ANIM, &surf->anim.val.menuHeight, AnimEaseIn},
                                                         }),
                },
            },
        .scanline = scanline,
    };

    surf->anim.movie = resetMovie(&surf->anim.idle);
}

void surf_free(Surf* surf)
{
    freeAnim(surf);
    resetMenu(surf);
    free(surf);
}
