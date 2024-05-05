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

#include "config.h"
#include "fs.h"
#include "cart.h"
#include "ext/json.h"

#if defined(__EMSCRIPTEN__)
#define DEFAULT_VSYNC 0
#else
#define DEFAULT_VSYNC 1
#endif

#if defined(__TIC_ANDROID__)
#define INTEGER_SCALE_DEFAULT false
#else
#define INTEGER_SCALE_DEFAULT true
#endif

static void readConfig(Config* config) 
{
    const char* json = config->cart->code.data;

    if(json_parse(json, strlen(json)))
    {
        config->data.checkNewVersion = json_bool("CHECK_NEW_VERSION", 0);
        config->data.uiScale = json_int("UI_SCALE", 0);
        config->data.soft = json_bool("SOFTWARE_RENDERING", 0);
        config->data.trim = json_bool("TRIM_ON_SAVE", 0);

        if(config->data.uiScale <= 0)
            config->data.uiScale = 1;

        config->data.theme.gamepad.touch.alpha = json_int("GAMEPAD_TOUCH_ALPHA", 0);

        s32 theme = json_object("CODE_THEME", 0);

#define CODE_COLOR_DEF(VAR) config->data.theme.code.VAR = json_int(#VAR, theme);
        CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef  CODE_COLOR_DEF

        config->data.theme.code.select = json_int("SELECT", theme);
        config->data.theme.code.cursor = json_int("CURSOR", theme);

        config->data.theme.code.shadow = json_bool("SHADOW", theme);
        config->data.theme.code.altFont = json_bool("ALT_FONT", theme);
        config->data.theme.code.altCaret = json_bool("ALT_CARET", theme);
        config->data.theme.code.matchDelimiters = json_bool("MATCH_DELIMITERS", theme);
        config->data.theme.code.autoDelimiters = json_bool("AUTO_DELIMITERS", theme);

    }
}

static void update(Config* config, const u8* buffer, s32 size)
{
    tic_cart_load(config->cart, buffer, size);

    readConfig(config);
    studioConfigChanged(config->studio);
}

static void setDefault(Config* config)
{
    config->data = (StudioConfig)
    {
        .cart = config->cart,
        .uiScale = 4,
        .options = 
        {
#if defined(CRT_SHADER_SUPPORT)
            .crt            = false,
#endif
            .volume         = MAX_VOLUME,
            .vsync          = DEFAULT_VSYNC,
            .fullscreen     = false,
            .integerScale   = INTEGER_SCALE_DEFAULT,
            .autosave       = false,
#if defined(BUILD_EDITORS)
            .keybindMode    = KEYBIND_STANDARD,
            .devmode        = false,
            .tabMode        = TAB_AUTO,
            .tabSize        = 1,
#endif
        },
    };

    tic_sys_default_mapping(&config->data.options.mapping);

    {
        static const u8 ConfigZip[] =
        {
            #include "../build/assets/config.tic.dat"
        };

        u8* data = malloc(sizeof(tic_cartridge));

        SCOPE(free(data))
        {
            update(config, data, tic_tool_unzip(data, sizeof(tic_cartridge), ConfigZip, sizeof ConfigZip));
        }
    }
}

static void saveConfig(Config* config, bool overwrite)
{
    u8* buffer = malloc(sizeof(tic_cartridge));

    if(buffer)
    {
        s32 size = tic_cart_save(config->data.cart, buffer);

        tic_fs_saveroot(config->fs, CONFIG_TIC_PATH, buffer, size, overwrite);

        free(buffer);
    }
}

static void reset(Config* config)
{
    setDefault(config);
    saveConfig(config, true);
}

static void save(Config* config)
{
    *config->cart = config->tic->cart;
    readConfig(config);
    saveConfig(config, true);

    studioConfigChanged(config->studio);
}

static const char OptionsDatPath[] = TIC_LOCAL_VERSION "options.dat";

static void loadConfigData(tic_fs* fs, const char* path, void* dst, s32 size)
{
    s32 dataSize = 0;
    u8* data = (u8*)tic_fs_loadroot(fs, path, &dataSize);

    if(data) SCOPE(free(data))
        if(dataSize == size)
            memcpy(dst, data, size);
}

void initConfig(Config* config, Studio* studio, tic_fs* fs)
{
    *config = (Config)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .cart = realloc(config->cart, sizeof(tic_cartridge)),
        .save = save,
        .reset = reset,
        .fs = fs,
    };

    setDefault(config);

    // read config.tic
    {
        s32 size = 0;
        u8* data = (u8*)tic_fs_loadroot(fs, CONFIG_TIC_PATH, &size);

        if(data)
        {
            update(config, data, size);

            free(data);
        }
        else saveConfig(config, false);        
    }

    loadConfigData(fs, OptionsDatPath, &config->data.options, sizeof config->data.options);

#if defined(__TIC_LINUX__)
    // do not load fullscreen option on Linux
    config->data.options.fullscreen = false;
#endif

    tic_api_reset(config->tic);
}

void freeConfig(Config* config)
{
    tic_fs_saveroot(config->fs, OptionsDatPath, &config->data.options, sizeof config->data.options, true);

    free(config->cart);
    free(config);
}
