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

// Loading a cart into the running machine and writing it back out: the console
// and the browser both work on the loaded cart, so neither owns it.

#include "rom.h"
#include "studio/paths.h"
#include "cart.h"
#include "tools.h"
#include "fs.h"
#include "config.h"
#include "defines.h"
#include "script.h"
#include "ext/png.h"

#include <stdio.h>

#if defined(BUILD_RENDER_CACHE)
#include "core/core.h"

// The cover is drawn inside the studio's frame: keep studio.h's redirects for
// the two calls it uses, so the draw cache sees them.
#define tic_api_cls(tic, ...)   (((tic_core*)(tic))->api.cls(tic, __VA_ARGS__))
#define tic_api_print(tic, ...) (((tic_core*)(tic))->api.print(tic, __VA_ARGS__))
#endif

#if defined(TIC80_PRO)
#include "project.h"
#endif

void rom_set_cart_name(const RomDeps* deps, const char* name, const char* path)
{
    CartName* rom = deps->cart;

    if(rom->name != name) strcpy(rom->name, name);
    if(rom->path != path) strcpy(rom->path, path);
}

// A launch or a drop names its own file; the name is the path's last part.
static const char* pathName(const char* path)
{
    const char* ptr = path + strlen(path);

    while(ptr > path && ptr[-1] != '/' && ptr[-1] != '\\')
        --ptr;

    return ptr;
}

// The one cart loader; a name that is not a cart or fails to parse loads nothing.
bool rom_load_bytes(const RomDeps* deps, const char* name, const void* bytes, s32 size)
{
    tic_mem* tic = deps->tic;
    bool loaded = false;

    if(tic_tool_has_ext(name, PNG_EXT))
    {
        tic_cartridge* cart = loadPngCart((png_buffer){(u8*)bytes, size});

        if(cart)
        {
            memcpy(&tic->cart, cart, sizeof(tic_cartridge));
            free(cart);
            loaded = true;
        }
    }
    else if(tic_tool_has_ext(name, CART_EXT))
    {
        tic_cart_load(&tic->cart, bytes, size);
        loaded = true;
    }
#if defined(TIC80_PRO)
    else if(tic_tool_is_project(name))
    {
        tic_cartridge* cart = malloc(sizeof(tic_cartridge));

        if(cart)
        {
            loaded = tic_project_load(name, bytes, size, cart);

            if(loaded)
                memcpy(&tic->cart, cart, sizeof(tic_cartridge));

            free(cart);
        }
    }
#endif

    if(!loaded)
        return false;

    tic_api_reset(tic);

    rom_set_cart_name(deps, name, tic_fs_path(deps->fs, name));

    deps->loaded(deps->host);

    return true;
}

typedef struct
{
    const RomDeps* deps;
    fs_done_cb done;
    void* data;
} LoadCartData;

static void asyncLoadDone(const char* name, const u8* bytes, s32 size, fs_result result, void* data)
{
    LoadCartData* load = data;

    if(result.code == FS_OK && !rom_load_bytes(load->deps, name, bytes, size))
        result = (fs_result){FS_UNSUPPORTED, "not a cart"};

    if(load->done)
        load->done(result, load->data);

    free(load);
}

void rom_load_cart_async(const RomDeps* deps, const char* path, fs_done_cb done, void* data)
{
    LoadCartData* load = malloc(sizeof(LoadCartData));

    if(!load)
    {
        if(done)
            done((fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    *load = (LoadCartData){deps, done, data};

    tic_fs_load(deps->fs, path, asyncLoadDone, load);
}

// The launch and a drop load a local path; a remote answer would come later.
typedef struct
{
    const RomDeps* deps;
    char path[TICNAME_MAX];
    bool loaded;
    bool finished;
    bool waited;
    bool cancelled;
} SyncLoadData;

static void syncLoadDone(const char* name, const u8* bytes, s32 size, fs_result result, void* data)
{
    SyncLoadData* load = data;

    load->finished = true;

    if(result.code == FS_OK && !load->cancelled && rom_load_bytes(load->deps, name, bytes, size))
    {
        // A launch named the file itself; its path stays the cart's.
        rom_set_cart_name(load->deps, name, load->path);

        load->loaded = true;
    }

    if(load->waited)
        free(load);
}

bool rom_load_cart(const RomDeps* deps, const char* path)
{
    SyncLoadData* load = calloc(1, sizeof(SyncLoadData));

    if(!load)
        return false;

    load->deps = deps;
    snprintf(load->path, sizeof load->path, "%s", path);

    tic_fs_load(deps->fs, path, syncLoadDone, load);

    bool loaded = load->loaded;

    if(!loaded)
    {
        // The process's own path, as a launch or a drop names it.
        s32 size = 0;
        void* data = fs_read(path, &size);

        if(data)
        {
            load->cancelled = true;

            const char* name = pathName(path);

            if(rom_load_bytes(deps, name, data, size))
            {
                rom_set_cart_name(deps, name, path);
                loaded = true;
            }

            free(data);
        }
    }

    load->waited = true;

    if(load->finished)
        free(load);

    return loaded;
}

// The cart's file changed under the studio — another editor, a build — and the
// machine takes it again: the reload the console used to do for the host.
void rom_reload_cart(const RomDeps* deps)
{
    tic_mem* tic = deps->tic;
    const char* path = deps->cart->path;

    if(*path)
    {
        s32 size = 0;
        void* data = fs_read(path, &size);

        if(data) SCOPE(free(data))
            {
#if defined(TIC80_PRO)
                if(tic_tool_is_project(path))
                    tic_project_load(deps->cart->name, data, size, &tic->cart);
                else
#endif
                    tic_cart_load(&tic->cart, data, size);

                deps->loaded(deps->host);
            }
    }
}

static void drawShadowText(tic_mem* tic, const char* text, s32 x, s32 y, tic_color color, s32 scale)
{
    tic_api_print(tic, text, x, y + scale, tic_color_black, false, scale, false);
    tic_api_print(tic, text, x, y, color, false, scale, false);
}

CartSaveResult rom_save_cart(const RomDeps* deps, const char* name)
{
    tic_mem* tic = deps->tic;

    bool success = false;

    if(name && strlen(name))
    {
        if(strcmp(name, CONFIG_TIC_PATH) == 0)
        {
            Config* config = deps->config;
            config->saveConfigCart(config);
            deps->saved(deps->host);
            return CART_SAVE_OK;
        }
        else
        {
            // what the cart is saved into: the saved cart, or the project
            // text, which is the cart written out and a few times its size
            u8* buffer = NULL;
            s32 size = 0;

            if(tic_tool_has_ext(name, PNG_EXT))
            {
                png_buffer cover;

                {
                    enum
                    {
                        CoverWidth = 256
                    };

                    static const u8 Cartridge[] =
                        {
#include "../build/assets/cart.png.dat"
                        };

                    png_buffer template = {(u8*)Cartridge, sizeof Cartridge};
                    png_img img = png_read(template, NULL);

                    // draw screen
                    {
                        enum
                        {
                            PaddingLeft = 8,
                            PaddingTop = 8
                        };

                        const tic_bank* bank = &tic->cart.bank0;
                        const tic_rgb* pal = bank->palette.vbank0.colors;
                        const u8* screen = bank->screen.data;
                        u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;

                        for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                            ptr[i / TIC80_WIDTH * CoverWidth + i % TIC80_WIDTH] = tic_rgba(pal + tic_tool_peek4(screen, i));
                    }

                    // draw title/author/desc
                    {
                        enum
                        {
                            Width = 224,
                            Height = 40,
                            PaddingTop = 162,
                            PaddingLeft = 16,
                            Scale = 2,
                            Row = TIC_FONT_HEIGHT * 2 * Scale
                        };

                        tic_api_cls(tic, tic_color_dark_grey);

                        const char* comment = tic_get_script(tic)->singleComment;

                        const char* title = tic_tool_metatag(tic->cart.code.data, "title", comment);
                        if(*title)
                        {
                            drawShadowText(tic, title, 0, 0, tic_color_white, Scale);
                        }

                        const char* author = tic_tool_metatag(tic->cart.code.data, "author", comment);
                        if(*author)
                        {
                            char buf[TICNAME_MAX];
                            snprintf(buf, sizeof buf, "by %s", author);
                            drawShadowText(tic, buf, 0, Row, tic_color_grey, Scale);
                        }

                        u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;
                        const u8* screen = tic->ram->vram.screen.data;
                        const tic_rgb Sweetie16[] = {
                            {0x1a, 0x1c, 0x2c}, {0x5d, 0x27, 0x5d}, {0xb1, 0x3e, 0x53}, {0xef, 0x7d, 0x57}, {0xff, 0xcd, 0x75}, {0xa7, 0xf0, 0x70}, {0x38, 0xb7, 0x64}, {0x25, 0x71, 0x79}, {0x29, 0x36, 0x6f}, {0x3b, 0x5d, 0xc9}, {0x41, 0xa6, 0xf6}, {0x73, 0xef, 0xf7}, {0xf4, 0xf4, 0xf4}, {0x94, 0xb0, 0xc2}, {0x56, 0x6c, 0x86}, {0x33, 0x3c, 0x57}};
                        const tic_rgb* pal = Sweetie16;

                        for(s32 y = 0; y < Height; y++)
                            for(s32 x = 0; x < Width; x++)
                                ptr[CoverWidth * y + x] = tic_rgba(pal + tic_tool_peek4(screen, y * TIC80_WIDTH + x));
                    }

                    cover = png_write(img, (png_buffer){NULL, 0});

                    free(img.data);
                }

                png_buffer zip = png_create(TIC_CART_ZIP_SIZE);

                {
                    png_buffer cart = png_create(TIC_CART_SAVE_SIZE);
                    cart.size = tic_cart_save(&tic->cart, cart.data);
                    zip.size = tic_tool_zip(zip.data, zip.size, cart.data, cart.size);
                    free(cart.data);
                }

                png_buffer result = png_encode(cover, zip);
                free(zip.data);
                free(cover.data);

                buffer = result.data;
                size = result.size;
            }
#if defined(TIC80_PRO)
            else if(tic_tool_is_project(name))
            {
                buffer = malloc(TIC_CART_SAVE_SIZE * 3);

                if(buffer)
                    size = tic_project_save(name, buffer, &tic->cart);
            }
#endif
            else
            {
                buffer = malloc(TIC_CART_SAVE_SIZE);

                if(buffer)
                {
                    name = tic_tool_cart_name(name);
                    size = tic_cart_save(&tic->cart, buffer);
                }
            }

            if(size && fs_write(tic_fs_path(deps->fs, name), buffer, size))
            {
                rom_set_cart_name(deps, name, tic_fs_path(deps->fs, name));
                success = true;
                deps->saved(deps->host);
            }

            free(buffer);
        }
    }
    else if(strlen(deps->cart->name))
    {
        return rom_save_cart(deps, deps->cart->name);
    }
    else
        return CART_SAVE_MISSING_NAME;

    return success ? CART_SAVE_OK : CART_SAVE_ERROR;
}

CartSaveResult rom_autosave(const RomDeps* deps)
{
    char namepath[TICNAME_MAX];
    strcpy(namepath, "/downloads/");
    strcat(namepath, deps->cart->name);

    return rom_save_cart(deps, namepath);
}
