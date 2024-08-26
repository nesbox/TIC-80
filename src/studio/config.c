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

#define JSON(...) #__VA_ARGS__

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

static const char OptionsJsonPath[] = TIC_LOCAL "options.json";

typedef struct
{
    char data[1024];
} string;

static void loadOptions(Config* config)
{
    s32 size;
    u8* data = (u8*)tic_fs_loadroot(config->fs, OptionsJsonPath, &size);

    if(data) SCOPE(free(data))
    {
        string json;
        snprintf(json.data, sizeof json, "%.*s", size, data);

        if(json_parse(json.data, strlen(json.data)))
        {
            struct StudioOptions* options = &config->data.options;

#if defined(CRT_SHADER_SUPPORT)
            options->crt = json_bool("crt", 0);
#endif
            options->fullscreen = json_bool("fullscreen", 0);
            options->vsync = json_bool("vsync", 0);
            options->integerScale = json_bool("integerScale", 0);
            options->volume = json_int("volume", 0);
            options->autosave = json_bool("autosave", 0);

            string mapping;
            json_string("mapping", 0, mapping.data, sizeof mapping);
            tic_tool_str2buf(mapping.data, strlen(mapping.data), &options->mapping, false);

#if defined(BUILD_EDITORS)
            options->keybindMode = json_int("keybindMode", 0);
            options->tabMode = json_int("tabMode", 0);
            options->tabSize = json_int("tabSize", 0);
#endif
        }
    }
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

    loadOptions(config);

#if defined(__TIC_LINUX__)
    // do not load fullscreen option on Linux
    config->data.options.fullscreen = false;
#endif

    tic_api_reset(config->tic);
}

static string data2str(const void* data, s32 size)
{
    string res;
    tic_tool_buf2str(data, size, res.data, false);
    return res;
}

static const char* bool2str(bool value)
{
    return value ? "true" : "false";
}

static void saveOptions(Config* config)
{
    const struct StudioOptions* options = &config->data.options;

    string buf;
    sprintf(buf.data, JSON(
        {
#if defined(CRT_SHADER_SUPPORT)
            "crt":%s,
#endif
            "fullscreen":%s,
            "vsync":%s,
            "integerScale":%s,
            "volume":%i,
            "autosave":%s,
            "mapping":"%s"
#if defined(BUILD_EDITORS)
            ,
            "keybindMode":%i,
            "tabMode":%i,
            "tabSize":%i
#endif
        })
        ,
#if defined(CRT_SHADER_SUPPORT)
        bool2str(options->crt),
#endif
        bool2str(options->fullscreen),
        bool2str(options->vsync),
        bool2str(options->integerScale),
        options->volume,
        bool2str(options->autosave),
        data2str(&options->mapping, sizeof options->mapping).data

#if defined(BUILD_EDITORS)
        ,
        options->keybindMode,
        options->tabMode,
        options->tabSize
#endif
        );

    tic_fs_saveroot(config->fs, OptionsJsonPath, buf.data, strlen(buf.data), true);
}

void freeConfig(Config* config)
{
    saveOptions(config);

    free(config->cart);
    free(config);
}
