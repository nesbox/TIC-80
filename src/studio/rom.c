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

#include "studio.h"
#include "rom.h"
#include "cart.h"
#include "fs.h"
#include "config.h"

#if defined(TIC80_PRO)
#include "project.h"
#endif


static const char* getName(const char* name, const char* ext)
{
    static char path[TICNAME_MAX];

    strcpy(path, name);

    size_t ps = strlen(path);
    size_t es = strlen(ext);

    if(!(ps > es && strstr(path, ext) + es == path + ps))
        strcat(path, ext);

    return path;
}

const char* getCartName(const char* name)
{
    return getName(name, CART_EXT);
}

static inline tic_cartridge* newCart()
{
    return malloc(sizeof(tic_cartridge));
}

void loadCartSection(Studio* studio, const tic_cartridge* cart, const char* section)
{
    tic_mem* tic = getMemory(studio);

    static const struct Section
    {
        const char* name;
        s32 offset;
        s32 size;
    } Sections[] =
    {
#define SECTION_DEF(name, ...) {#name, offsetof(tic_bank, name), sizeof(tic_ ## name)},
        TIC_SYNC_LIST(SECTION_DEF)
#undef  SECTION_DEF
    };

    if(section)
    {
        if(strcmp(section, "code") == 0)
            memcpy(&tic->cart.code, &cart->code, sizeof(tic_code));
        else
            FOR(const struct Section*, it, Sections)
                if(strcmp(section, it->name) == 0)
                {
                    memcpy((u8*)&tic->cart.bank0 + it->offset, (const u8*)&cart->bank0 + it->offset, it->size);
                    break;
                }
    }
    else
        memcpy(&tic->cart, cart, sizeof(tic_cartridge));
}

void studioSetCartName(Studio* studio, const char* name, const char* path)
{
    CartName* rom = studioCart(studio);

    if(rom->name != name) strcpy(rom->name, name);
    if(rom->path != path) strcpy(rom->path, path);
}

typedef struct
{
    Studio* studio;
    char* name;
    char* section;
    fs_done_callback callback;
    void* calldata;
} LoadByHashData;

static void hashLoadDone(const u8* buffer, s32 size, void* data)
{
    LoadByHashData* loadByHashData = data;
    Studio* studio = loadByHashData->studio;

    tic_cartridge* cart = newCart();

    SCOPE(free(cart))
    {
        tic_cart_load(cart, buffer, size);
        loadCartSection(studio, cart, loadByHashData->section);

        tic_api_reset(getMemory(studio));

        if(!loadByHashData->section)
            studioSetCartName(studio, loadByHashData->name, tic_fs_path(studio_fs(studio), loadByHashData->name));

        studioRomLoaded(studio);
    }

    if (loadByHashData->callback)
        loadByHashData->callback(loadByHashData->calldata);

    FREE(loadByHashData->name);
    FREE(loadByHashData->section);
    FREE(loadByHashData);
}

void studioLoadByHash(Studio* studio, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data)
{
    LoadByHashData loadByHashData = { studio, strdup(name), section ? strdup(section) : NULL, callback, data};
    tic_fs_hashload(studio_fs(studio), name, hash, hashLoadDone, MOVE(loadByHashData));
}

static void drawShadowText(tic_mem* tic, const char* text, s32 x, s32 y, tic_color color, s32 scale)
{
    tic_api_print(tic, text, x, y + scale, tic_color_black, false, scale, false);
    tic_api_print(tic, text, x, y, color, false, scale, false);
}

CartSaveResult studioSaveCart(Studio* studio, const char* name)
{
    tic_mem* tic = getMemory(studio);

    bool success = false;

    if(name && strlen(name))
    {
        u8* buffer = (u8*)malloc(sizeof(tic_cartridge) * 3);

        if(buffer)
        {
            if(strcmp(name, CONFIG_TIC_PATH) == 0)
            {
                Config* config = studio_config_get(studio);
                config->saveConfigCart(config);
                studioRomSaved(studio);
                free(buffer);
                return CART_SAVE_OK;
            }
            else
            {
                s32 size = 0;

                if(tic_tool_has_ext(name, PNG_EXT))
                {
                    png_buffer cover;

                    {
                        enum{CoverWidth = 256};

                        static const u8 Cartridge[] =
                        {
                            #include "../build/assets/cart.png.dat"
                        };

                        png_buffer template = {(u8*)Cartridge, sizeof Cartridge};
                        png_img img = png_read(template, NULL);

                        // draw screen
                        {
                            enum{PaddingLeft = 8, PaddingTop = 8};

                            const tic_bank* bank = &tic->cart.bank0;
                            const tic_rgb* pal = bank->palette.vbank0.colors;
                            const u8* screen = bank->screen.data;
                            u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;

                            for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                                ptr[i / TIC80_WIDTH * CoverWidth + i % TIC80_WIDTH] = tic_rgba(pal + tic_tool_peek4(screen, i));
                        }

                        // draw title/author/desc
                        {
                            enum{Width = 224, Height = 40, PaddingTop = 162, PaddingLeft = 16, Scale = 2, Row = TIC_FONT_HEIGHT * 2 * Scale};

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
								{0x1a, 0x1c, 0x2c}, {0x5d, 0x27, 0x5d}, {0xb1, 0x3e, 0x53}, {0xef, 0x7d, 0x57},
								{0xff, 0xcd, 0x75}, {0xa7, 0xf0, 0x70}, {0x38, 0xb7, 0x64}, {0x25, 0x71, 0x79},
								{0x29, 0x36, 0x6f}, {0x3b, 0x5d, 0xc9}, {0x41, 0xa6, 0xf6}, {0x73, 0xef, 0xf7},
								{0xf4, 0xf4, 0xf4}, {0x94, 0xb0, 0xc2}, {0x56, 0x6c, 0x86}, {0x33, 0x3c, 0x57}
							};
							const tic_rgb* pal = Sweetie16;

                            for(s32 y = 0; y < Height; y++)
                                for(s32 x = 0; x < Width; x++)
                                    ptr[CoverWidth * y + x] = tic_rgba(pal + tic_tool_peek4(screen, y * TIC80_WIDTH + x));
                        }

                        cover = png_write(img, (png_buffer){NULL, 0});

                        free(img.data);
                    }

                    png_buffer zip = png_create(sizeof(tic_cartridge));

                    {
                        png_buffer cart = png_create(sizeof(tic_cartridge));
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
                else if(project_ext(name))
                {
                    size = tic_project_save(name, buffer, &tic->cart);
                }
#endif
                else
                {
                    name = getCartName(name);
                    size = tic_cart_save(&tic->cart, buffer);
                }

                if(size && tic_fs_save(studio_fs(studio), name, buffer, size, true))
                {
                    studioSetCartName(studio, name, tic_fs_path(studio_fs(studio), name));
                    success = true;
                    studioRomSaved(studio);
                }
            }

            free(buffer);
        }
    }
    else if (strlen(studioCart(studio)->name))
    {
        return studioSaveCart(studio, studioCart(studio)->name);
    }
    else return CART_SAVE_MISSING_NAME;

    return success ? CART_SAVE_OK : CART_SAVE_ERROR;
}

static inline bool isslash(char c)
{
    return c == '/' || c == '\\';
}

bool studioLoadCart(Studio* studio, const char* path)
{
    bool done = false;

    s32 size = 0;
    // A name from the browser is relative to the cart folder, a path from the
    // command line or a drop is the process's own.
    void* data = tic_fs_load(studio_fs(studio), path, &size);

    if(!data)
        data = fs_read(path, &size);

    if(data)
    {
        const char* cartName = NULL;

        {
            const char* ptr = path + strlen(path);
            while(ptr > path && !isslash(*ptr))--ptr;
            cartName = ptr + isslash(*ptr);
        }

        studioSetCartName(studio, cartName, path);
        tic_mem* tic = getMemory(studio);

        if(tic_tool_has_ext(cartName, PNG_EXT))
        {
            tic_cartridge* cart = loadPngCart((png_buffer){data, size});

            if(cart)
            {
                memcpy(&tic->cart, cart, sizeof(tic_cartridge));
                free(cart);
                done = true;
            }
        }
        else if(tic_tool_has_ext(cartName, CART_EXT))
        {
            tic_cart_load(&tic->cart, data, size);
            done = true;
        }
#if defined(TIC80_PRO)
        else if(project_ext(cartName))
        {
            if(tic_project_load(cartName, data, size, &tic->cart))
                done = true;
        }
#endif

        free(data);
    }

    if(done)
        studioRomLoaded(studio);

    return done;
}

CartSaveResult studioAutoSave(Studio* studio)
{
    char namepath[TICNAME_MAX];
    strcpy(namepath, "/downloads/");
    strcat(namepath, studioCart(studio)->name);

    return studioSaveCart(studio, namepath);
}
