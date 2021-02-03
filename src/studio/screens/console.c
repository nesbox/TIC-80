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

#include "console.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/config.h"
#include "ext/gif.h"
#include "studio/project.h"
#include "zip.h"

#include <ctype.h>
#include <string.h>

#if !defined(__TIC_MACOSX__)
#include <malloc.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if defined(__TIC_WINDOWS__)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <sys/stat.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#define CONSOLE_CURSOR_COLOR tic_color_red
#define CONSOLE_BACK_TEXT_COLOR tic_color_grey
#define CONSOLE_FRONT_TEXT_COLOR tic_color_white
#define CONSOLE_ERROR_TEXT_COLOR tic_color_red
#define CONSOLE_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define CONSOLE_CURSOR_DELAY (TIC80_FRAMERATE / 2)
#define CONSOLE_BUFFER_WIDTH (STUDIO_TEXT_BUFFER_WIDTH)
#define CONSOLE_BUFFER_HEIGHT (STUDIO_TEXT_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SCREENS 64
#define CONSOLE_BUFFER_SIZE (CONSOLE_BUFFER_WIDTH * CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)

typedef enum
{
#if defined(TIC_BUILD_WITH_LUA)
    LuaScript,  

#   if defined(TIC_BUILD_WITH_MOON)
    MoonScript, 
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    Fennel,
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    JavaScript, 
#endif

#if defined(TIC_BUILD_WITH_WREN)
    WrenScript, 
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    SquirrelScript,
#endif

} ScriptLang;

#if defined(__EMSCRIPTEN__)
#define CAN_ADDGET_FILE 1
#endif

#if defined(TIC_BUILD_WITH_LUA)
static const char DefaultLuaTicPath[] = TIC_LOCAL_VERSION "default.tic";

#   if defined(TIC_BUILD_WITH_MOON)
static const char DefaultMoonTicPath[] = TIC_LOCAL_VERSION "default_moon.tic";
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
static const char DefaultFennelTicPath[] = TIC_LOCAL_VERSION "default_fennel.tic";
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
static const char DefaultJSTicPath[] = TIC_LOCAL_VERSION "default_js.tic";
#endif

#if defined(TIC_BUILD_WITH_WREN)
static const char DefaultWrenTicPath[] = TIC_LOCAL_VERSION "default_wren.tic";
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
static const char DefaultSquirrelTicPath[] = TIC_LOCAL_VERSION "default_squirrel.tic";
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

static const char* getCartName(const char* name)
{
    return getName(name, CART_EXT);
}

static void scrollBuffer(char* buffer)
{
    memmove(buffer, buffer + CONSOLE_BUFFER_WIDTH, CONSOLE_BUFFER_SIZE - CONSOLE_BUFFER_WIDTH);
    memset(buffer + CONSOLE_BUFFER_SIZE - CONSOLE_BUFFER_WIDTH, 0, CONSOLE_BUFFER_WIDTH);
}

static void scrollConsole(Console* console)
{
    while(console->cursor.y >= CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)
    {
        scrollBuffer(console->buffer);
        scrollBuffer((char*)console->colorBuffer);

        console->cursor.y--;
    }

    s32 minScroll = console->cursor.y - CONSOLE_BUFFER_HEIGHT + 1;
    if(console->scroll.pos < minScroll)
        console->scroll.pos = minScroll;
}

static void consolePrint(Console* console, const char* text, u8 color)
{
#ifndef BAREMETALPI
    printf("%s", text);
#endif
    const char* textPointer = text;
    const char* endText = textPointer + strlen(text);

    while(textPointer != endText)
    {
        char symbol = *textPointer++;

        scrollConsole(console);

        if(symbol == '\n')
        {
            console->cursor.x = 0;
            console->cursor.y++;
        }
        else
        {
            s32 offset = console->cursor.x + console->cursor.y * CONSOLE_BUFFER_WIDTH;
            *(console->buffer + offset) = symbol;
            *(console->colorBuffer + offset) = color;

            console->cursor.x++;

            if(console->cursor.x >= CONSOLE_BUFFER_WIDTH)
            {
                console->cursor.x = 0;
                console->cursor.y++;
            }
        }

    }
}

static void printBack(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_BACK_TEXT_COLOR);
}

static void printFront(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_FRONT_TEXT_COLOR);
}

static void printError(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_ERROR_TEXT_COLOR);
}

static void printLine(Console* console)
{
    consolePrint(console, "\n", 0);
}

static void commandDoneLine(Console* console, bool newLine)
{
    if(newLine)
        printLine(console);

    char dir[TICNAME_MAX];
    tic_fs_dir(console->fs, dir);
    if(strlen(dir))
        printBack(console, dir);

    printFront(console, ">");
    console->active = true;
}

static void commandDone(Console* console)
{
    commandDoneLine(console, true);
}

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color, bool alt)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, true, 1, alt);
}

static void drawCursor(Console* console, s32 x, s32 y, u8 symbol)
{
    if(!console->active)
        return;

    bool inverse = console->cursor.delay || console->tickCounter % CONSOLE_CURSOR_BLINK_PERIOD < CONSOLE_CURSOR_BLINK_PERIOD / 2;

    if(inverse)
        tic_api_rect(console->tic, x-1, y-1, TIC_FONT_WIDTH+1, TIC_FONT_HEIGHT+1, CONSOLE_CURSOR_COLOR);

    drawChar(console->tic, symbol, x, y, inverse ? TIC_COLOR_BG : CONSOLE_FRONT_TEXT_COLOR, false);
}

static void drawConsoleText(Console* console)
{
    char* pointer = console->buffer + console->scroll.pos * CONSOLE_BUFFER_WIDTH;
    u8* colorPointer = console->colorBuffer + console->scroll.pos * CONSOLE_BUFFER_WIDTH;

    const char* end = console->buffer + CONSOLE_BUFFER_SIZE;
    s32 x = 0;
    s32 y = 0;

    while(pointer < end)
    {
        char symbol = *pointer++;
        u8 color = *colorPointer++;

        if(symbol)
            drawChar(console->tic, symbol, x * STUDIO_TEXT_WIDTH, y * STUDIO_TEXT_HEIGHT, color, false);

        if(++x == CONSOLE_BUFFER_WIDTH)
        {
            y++;
            x = 0;
        }
    }
}

static void drawConsoleInputText(Console* console)
{
    s32 x = console->cursor.x * STUDIO_TEXT_WIDTH;
    s32 y = (console->cursor.y - console->scroll.pos) * STUDIO_TEXT_HEIGHT;

    const char* pointer = console->inputBuffer;
    const char* end = pointer + strlen(console->inputBuffer);
    s32 index = 0;

    while(pointer != end)
    {
        char symbol = *pointer++;

        if(console->inputPosition == index)
            drawCursor(console, x, y, symbol);
        else
            drawChar(console->tic, symbol, x, y, CONSOLE_FRONT_TEXT_COLOR, false);

        index++;

        x += STUDIO_TEXT_WIDTH;
        if(x == (CONSOLE_BUFFER_WIDTH * STUDIO_TEXT_WIDTH))
        {
            y += STUDIO_TEXT_HEIGHT;
            x = 0;
        }

    }

    if(console->inputPosition == index)
        drawCursor(console, x, y, ' ');


}

static void processConsoleHome(Console* console)
{
    console->inputPosition = 0;
}

static void processConsoleEnd(Console* console)
{
    console->inputPosition = strlen(console->inputBuffer);
}

static void processConsoleDel(Console* console)
{
    char* pos = console->inputBuffer + console->inputPosition;
    size_t size = strlen(pos);
    memmove(pos, pos + 1, size);
}

static void processConsoleBackspace(Console* console)
{
    if(console->inputPosition > 0)
    {
        console->inputPosition--;

        processConsoleDel(console);
    }
}

static void onConsoleHelpCommand(Console* console, const char* param);

static void onConsoleExitCommand(Console* console, const char* param)
{
    exitStudio();
    commandDone(console);
}

static s32 writeGifData(const tic_mem* tic, u8* dst, const u8* src, s32 width, s32 height)
{
    s32 size = 0;
    gif_color* palette = (gif_color*)malloc(sizeof(gif_color) * TIC_PALETTE_SIZE);

    if(palette)
    {
        const tic_rgb* pal = getBankPalette(false)->colors;
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++, pal++)
            palette[i].r = pal->r, palette[i].g = pal->g, palette[i].b = pal->b;

        gif_write_data(dst, &size, width, height, src, palette, TIC_PALETTE_BPP);

        free(palette);
    }

    return size;
}

static bool loadRom(tic_mem* tic, const void* data, s32 size)
{
    tic_cart_load(&tic->cart, data, size);
    tic_api_reset(tic);

    return true;
}

static bool onConsoleLoadSectionCommand(Console* console, const char* param)
{
    bool result = false;

    if(param)
    {
        static const char* Sections[] =
        {
            "cover",
            "sprites",
            "map",
            "code",
            "sfx",
            "music",
            "palette",
        };

        char buf[64];

        for(s32 i = 0; i < COUNT_OF(Sections); i++)
        {
            sprintf(buf, "%s %s", CART_EXT, Sections[i]);
            char* pos = strstr(param, buf);

            if(pos)
            {
                pos[sizeof(CART_EXT) - 1] = 0;
                const char* name = getCartName(param);
                s32 size = 0;
                void* data = tic_fs_load(console->fs, name, &size);

                if(data)
                {
                    tic_cartridge* cart = (tic_cartridge*)malloc(sizeof(tic_cartridge));

                    if(cart)
                    {
                        tic_mem* tic = console->tic;
                        tic_cart_load(cart, data, size);

                        switch(i)
                        {
                        case 0: memcpy(&tic->cart.cover,            &cart->cover,           sizeof cart->cover); break;
                        case 1: memcpy(&tic->cart.bank0.tiles,      &cart->bank0.tiles,     sizeof(tic_tiles)*2); break;
                        case 2: memcpy(&tic->cart.bank0.map,        &cart->bank0.map,       sizeof(tic_map)); break;
                        case 3: memcpy(&tic->cart.code,             &cart->code,            sizeof(tic_code)); break;
                        case 4: memcpy(&tic->cart.bank0.sfx,        &cart->bank0.sfx,       sizeof(tic_sfx)); break;
                        case 5: memcpy(&tic->cart.bank0.music,      &cart->bank0.music,     sizeof(tic_music)); break;
                        case 6: memcpy(&tic->cart.bank0.palette,    &cart->bank0.palette,   sizeof(tic_palette)); break;
                        }

                        studioRomLoaded();

                        printLine(console);
                        printFront(console, Sections[i]);
                        printBack(console, " loaded from ");
                        printFront(console, name);
                        printLine(console);

                        free(cart);

                        result = true;
                    }

                    free(data);
                }
                else printBack(console, "\ncart loading error");

                commandDone(console);
            }
        }
    }

    return result;
}

static void* getDemoCart(Console* console, ScriptLang script, s32* size)
{
    char path[TICNAME_MAX];

    {
        switch(script)
        {
#if defined(TIC_BUILD_WITH_LUA)
        case LuaScript: strcpy(path, DefaultLuaTicPath); break;

#   if defined(TIC_BUILD_WITH_MOON)
        case MoonScript: strcpy(path, DefaultMoonTicPath); break;
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
        case Fennel: strcpy(path, DefaultFennelTicPath); break;
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
        case JavaScript: strcpy(path, DefaultJSTicPath); break;
#endif

#if defined(TIC_BUILD_WITH_WREN)
        case WrenScript: strcpy(path, DefaultWrenTicPath); break;
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
        case SquirrelScript: strcpy(path, DefaultSquirrelTicPath); break;
#endif          
        }

        void* data = tic_fs_loadroot(console->fs, path, size);

        if(data && *size)
            return data;
    }

    const u8* demo = NULL;
    s32 romSize = 0;

    switch(script)
    {
#if defined(TIC_BUILD_WITH_LUA)
    case LuaScript:
        {
            static const u8 LuaDemoRom[] =
            {
                #include "../build/assets/luademo.tic.dat"
            };

            demo = LuaDemoRom;
            romSize = sizeof LuaDemoRom;
        }
        break;

#   if defined(TIC_BUILD_WITH_MOON)
    case MoonScript:
        {
            static const u8 MoonDemoRom[] =
            {
                #include "../build/assets/moondemo.tic.dat"
            };

            demo = MoonDemoRom;
            romSize = sizeof MoonDemoRom;
        }
        break;
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    case Fennel:
        {
            static const u8 FennelDemoRom[] =
            {
                #include "../build/assets/fenneldemo.tic.dat"
            };

            demo = FennelDemoRom;
            romSize = sizeof FennelDemoRom;
        }
        break;
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */


#if defined(TIC_BUILD_WITH_JS)
    case JavaScript:
        {
            static const u8 JsDemoRom[] =
            {
                #include "../build/assets/jsdemo.tic.dat"
            };

            demo = JsDemoRom;
            romSize = sizeof JsDemoRom;
        }
        break;
#endif /* defined(TIC_BUILD_WITH_JS) */

#if defined(TIC_BUILD_WITH_WREN)
    case WrenScript:
        {
            static const u8 WrenDemoRom[] =
            {
                #include "../build/assets/wrendemo.tic.dat"
            };

            demo = WrenDemoRom;
            romSize = sizeof WrenDemoRom;
        }
        break;
#endif /* defined(TIC_BUILD_WITH_WREN) */

#if defined(TIC_BUILD_WITH_SQUIRREL)
    case SquirrelScript:
        {
            static const u8 SquirrelDemoRom[] =
            {
                #include "../build/assets/squirreldemo.tic.dat"
            };

            demo = SquirrelDemoRom;
            romSize = sizeof SquirrelDemoRom;
        }
        break;
#endif /* defined(TIC_BUILD_WITH_SQUIRREL) */
    }

    u8* data = calloc(1, sizeof(tic_cartridge));

    if(data)
    {
        *size = tic_tool_unzip(data, sizeof(tic_cartridge), demo, romSize);

        if(*size)
            tic_fs_saveroot(console->fs, path, data, *size, false);
    }

    return data;
}

static void setCartName(Console* console, const char* name, const char* path)
{
    strcpy(console->rom.name, name);
    strcpy(console->rom.path, path);
}

static void onConsoleLoadDemoCommandConfirmed(Console* console, const char* param)
{
    void* data = NULL;
    s32 size = 0;

    console->showGameMenu = false;

#if defined(TIC_BUILD_WITH_LUA)
    if(strcmp(param, DefaultLuaTicPath) == 0)
        data = getDemoCart(console, LuaScript, &size);

#   if defined(TIC_BUILD_WITH_MOON)
    if(strcmp(param, DefaultMoonTicPath) == 0)
        data = getDemoCart(console, MoonScript, &size);
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    if(strcmp(param, DefaultFennelTicPath) == 0)
        data = getDemoCart(console, Fennel, &size);
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    if(strcmp(param, DefaultJSTicPath) == 0)
        data = getDemoCart(console, JavaScript, &size);
#endif

#if defined(TIC_BUILD_WITH_WREN)
    if(strcmp(param, DefaultWrenTicPath) == 0)
        data = getDemoCart(console, WrenScript, &size);
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    if(strcmp(param, DefaultSquirrelTicPath) == 0)
        data = getDemoCart(console, SquirrelScript, &size);
#endif

    const char* name = getCartName(param);

    setCartName(console, name, tic_fs_path(console->fs, name));

    loadRom(console->tic, data, size);

    studioRomLoaded();

    printBack(console, "\ncart ");
    printFront(console, console->rom.name);
    printBack(console, " loaded!\n");

    free(data);
}

static void onCartLoaded(Console* console, const char* name)
{
    setCartName(console, name, tic_fs_path(console->fs, name));

    studioRomLoaded();

    printBack(console, "\ncart ");
    printFront(console, console->rom.name);
    printBack(console, " loaded!\nuse ");
    printFront(console, "RUN");
    printBack(console, " command to run it\n");

}

static void updateProject(Console* console)
{
    tic_mem* tic = console->tic;
    const char* path = console->rom.path;

    if(strlen(path) && hasProjectExt(path))
    {
        s32 size = 0;
        void* data = fs_read(path, &size);

        if(data)
        {
            tic_cartridge* cart = malloc(sizeof(tic_cartridge));

            if(cart)
            {
                if(tic_project_load(console->rom.name, data, size, cart))
                {
                    memcpy(&tic->cart, cart, sizeof(tic_cartridge));

                    studioRomLoaded();
                }
                else printError(console, "\nproject updating error :(");
                
                free(cart);
            }
            free(data);

        }       
    }
}

typedef struct
{
    Console* console;
    char* name;
    fs_done_callback callback;
    void* calldata;
}LoadByHashData;

static void loadByHashDone(const u8* buffer, s32 size, void* data)
{
    LoadByHashData* loadByHashData = data;
    Console* console = loadByHashData->console;

    loadRom(console->tic, buffer, size);
    onCartLoaded(console, loadByHashData->name);

    if (loadByHashData->callback)
        loadByHashData->callback(loadByHashData->calldata);

    free(loadByHashData->name);
    free(loadByHashData);

    console->showGameMenu = true;

    commandDone(console);
}

static void loadByHash(Console* console, const char* name, const char* hash, fs_done_callback callback, void* data)
{
    console->active = false;

    LoadByHashData loadByHashData = { console, strdup(name), callback, data};
    tic_fs_hashload(console->fs, hash, loadByHashDone, OBJCOPY(loadByHashData));
}

typedef struct
{
    Console* console;
    char* name;
    char* hash;
} LoadPublicCartData;

static bool compareFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
    LoadPublicCartData* loadPublicCartData = data;
    Console* console = loadPublicCartData->console;

    if (strcmp(name, loadPublicCartData->name) == 0 && info && strlen(info))
    {
        loadPublicCartData->hash = strdup(info);
        return false;
    }

    return true;
}

static void fileFound(void* data)
{
    LoadPublicCartData* loadPublicCartData = data;
    Console* console = loadPublicCartData->console;

    if (loadPublicCartData->hash)
    {
        loadByHash(console, loadPublicCartData->name, loadPublicCartData->hash, NULL, NULL);
        free(loadPublicCartData->hash);
    }
    else
    {
        char msg[TICNAME_MAX];
        sprintf(msg, "\nerror: `%s` file not loaded", loadPublicCartData->name);
        printError(console, msg);
        commandDone(console);
    }

    free(loadPublicCartData->name);
    free(loadPublicCartData);    
}

static void onConsoleLoadCommandConfirmed(Console* console, const char* param)
{
    if(onConsoleLoadSectionCommand(console, param)) return;

    if(param)
    {
        const char* name = getCartName(param);

        if (tic_fs_ispubdir(console->fs))
        {
            LoadPublicCartData loadPublicCartData = { console, strdup(name) };
            tic_fs_enum(console->fs, compareFilename, fileFound, OBJCOPY(loadPublicCartData));

            return;
        }
        else
        {
            console->showGameMenu = false;
            s32 size = 0;
            void* data = strcmp(name, CONFIG_TIC_PATH) == 0
                ? tic_fs_loadroot(console->fs, name, &size)
                : tic_fs_load(console->fs, name, &size);

            if(data)
            {
                loadRom(console->tic, data, size);
                onCartLoaded(console, name);
            }
            else
            {
                const char* name = getName(param, PROJECT_LUA_EXT);

                if(!tic_fs_exists(console->fs, name))
                    name = getName(param, PROJECT_MOON_EXT);

                if(!tic_fs_exists(console->fs, name))
                    name = getName(param, PROJECT_JS_EXT);

                if(!tic_fs_exists(console->fs, name))
                    name = getName(param, PROJECT_WREN_EXT);

                if(!tic_fs_exists(console->fs, name))
                    name = getName(param, PROJECT_FENNEL_EXT);

                if(!tic_fs_exists(console->fs, name))
                    name = getName(param, PROJECT_SQUIRREL_EXT);

                void* data = tic_fs_load(console->fs, name, &size);

                if(data && tic_project_load(name, data, size, &console->tic->cart))
                    onCartLoaded(console, name);
                else printBack(console, "\ncart loading error");
            }

            free(data);
        }
    }
    else printBack(console, "\ncart name is missing");

    commandDone(console);
}

typedef void(*ConfirmCallback)(Console* console, const char* param);

typedef struct
{
    Console* console;
    char* param;
    ConfirmCallback callback;

} CommandConfirmData;

static void onConfirm(bool yes, void* data)
{
    CommandConfirmData* confirmData = (CommandConfirmData*)data;

    if(yes)
    {
        confirmData->callback(confirmData->console, confirmData->param);
    }
    else commandDone(confirmData->console);

    if(confirmData->param)
        free(confirmData->param);

    free(confirmData);
}

static void confirmCommand(Console* console, const char** text, s32 rows, const char* param, ConfirmCallback callback)
{
    CommandConfirmData* data = malloc(sizeof(CommandConfirmData));

    data->console = console;
    data->param = param ? strdup(param) : NULL;
    data->callback = callback;

    showDialog(text, rows, onConfirm, data);
}

static void onConsoleLoadDemoCommand(Console* console, const char* param)
{
    if(studioCartChanged())
    {
        static const char* Rows[] =
        {
            "YOU HAVE",
            "UNSAVED CHANGES",
            "",
            "DO YOU REALLY WANT",
            "TO LOAD CART?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleLoadDemoCommandConfirmed);
    }
    else
    {
        onConsoleLoadDemoCommandConfirmed(console, param);
    }
}

static void onConsoleLoadCommand(Console* console, const char* param)
{
    if(studioCartChanged())
    {
        static const char* Rows[] =
        {
            "YOU HAVE",
            "UNSAVED CHANGES",
            "",
            "DO YOU REALLY WANT",
            "TO LOAD CART?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleLoadCommandConfirmed);
    }
    else
    {
        onConsoleLoadCommandConfirmed(console, param);
    }
}

static void loadDemo(Console* console, ScriptLang script)
{
    s32 size = 0;
    u8* data = getDemoCart(console, script, &size);

    if(data)
    {
        loadRom(console->tic, data, size);
        free(data);
    }

    memset(console->rom.name, 0, sizeof console->rom.name);

    studioRomLoaded();
}

static void onConsoleNewCommandConfirmed(Console* console, const char* param)
{
    bool done = false;

    if(param && strlen(param))
    {
#if defined(TIC_BUILD_WITH_LUA)
        if(strcmp(param, "lua") == 0)
        {
            loadDemo(console, LuaScript);
            done = true;
        }

#   if defined(TIC_BUILD_WITH_MOON)
        if(strcmp(param, "moon") == 0 || strcmp(param, "moonscript") == 0)
        {
            loadDemo(console, MoonScript);
            done = true;
        }
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
        if(strcmp(param, "fennel") == 0)
        {
            loadDemo(console, Fennel);
            done = true;
        }
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
        if(strcmp(param, "js") == 0 || strcmp(param, "javascript") == 0)
        {
            loadDemo(console, JavaScript);
            done = true;
        }
#endif

#if defined(TIC_BUILD_WITH_WREN)
        if(strcmp(param, "wren") == 0)
        {
            loadDemo(console, WrenScript);
            done = true;
        }
#endif          

#if defined(TIC_BUILD_WITH_SQUIRREL)
        if(strcmp(param, "squirrel") == 0)
        {
            loadDemo(console, SquirrelScript);
            done = true;
        }
#endif          

        if(!done)
        {
            printError(console, "\nunknown parameter: ");
            printError(console, param);
            commandDone(console);
            return;
        }
    }

#if defined(TIC_BUILD_WITH_LUA)
    else
    {
        loadDemo(console, LuaScript);
        done = true;
    }
#endif

    if(done) printBack(console, "\nnew cart is created");
    else printError(console, "\ncart not created");

    commandDone(console);
}

static void onConsoleNewCommand(Console* console, const char* param)
{
    if(studioCartChanged())
    {
        static const char* Rows[] =
        {
            "YOU HAVE",
            "UNSAVED CHANGES",
            "",
            "DO YOU REALLY WANT",
            "TO CREATE NEW CART?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleNewCommandConfirmed);
    }
    else
    {
        onConsoleNewCommandConfirmed(console, param);
    }
}

typedef struct
{
    s32 count;
    Console* console;
} PrintFileNameData;

static bool printFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
    PrintFileNameData* printData = data;
    Console* console = printData->console;

    printLine(console);

    if(dir)
    {
        printBack(console, "[");
        printBack(console, name);
        printBack(console, "]");
    }
    else printFront(console, name);

    printData->count++;

    return true;
}

static void onDirDone(void* data)
{
    PrintFileNameData* printData = data;
    Console* console = printData->console;

    if (printData->count == 0)
    {
        printBack(console, "\n\nuse ");
        printFront(console, "DEMO");
        printBack(console, " command to install demo carts");
    }

    printLine(console);
    commandDone(console);

    free(data);
}

typedef struct
{
    Console* console;
    char* name;
} ChangeDirData;

static void onConsoleChangeDirectoryDone(bool dir, void* data)
{
    ChangeDirData* changeDirData = data;
    Console* console = changeDirData->console;

    if (dir)
    {
        tic_fs_changedir(console->fs, changeDirData->name);
    }
    else printBack(console, "\ndir doesn't exist");

    free(changeDirData->name);
    free(changeDirData);

    commandDone(console);
}

static void onConsoleChangeDirectory(Console* console, const char* param)
{
    if(param && strlen(param))
    {
        if(strcmp(param, "/") == 0)
        {
            tic_fs_homedir(console->fs);
        }
        else if(strcmp(param, "..") == 0)
        {
            tic_fs_dirback(console->fs);
        }
        else
        {
            ChangeDirData data = { console, strdup(param) };
            tic_fs_isdir_async(console->fs, param, onConsoleChangeDirectoryDone, OBJCOPY(data));
            return;
        }
    }
    else printBack(console, "\ninvalid dir name");

    commandDone(console);
}

static void onConsoleMakeDirectory(Console* console, const char* param)
{
    if(param && strlen(param))
        tic_fs_makedir(console->fs, param);
    else printBack(console, "\ninvalid dir name");

    commandDone(console);
}

static void onConsoleDirCommand(Console* console, const char* param)
{
    printLine(console);

    PrintFileNameData data = {0, console};
    tic_fs_enum(console->fs, printFilename, onDirDone, OBJCOPY(data));
}

static void onConsoleFolderCommand(Console* console, const char* param)
{

    printBack(console, "\nStorage path:\n");
    printFront(console, tic_fs_pathroot(console->fs, ""));

    tic_fs_openfolder(console->fs);

    commandDone(console);
}

static void onConsoleClsCommand(Console* console, const char* param)
{
    memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
    memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);
    console->scroll.pos = 0;
    console->cursor.x = console->cursor.y = 0;
    printf("\r");

    commandDoneLine(console, false);
}

static void installDemoCart(tic_fs* fs, const char* name, const void* cart, s32 size)
{
    u8* data = calloc(1, sizeof(tic_cartridge));

    if(data)
    {
        s32 dataSize = tic_tool_unzip(data, sizeof(tic_cartridge), cart, size);

        if(dataSize)
            tic_fs_save(fs, name, data, dataSize, true);

        free(data);
    }
}

static void onConsoleInstallDemosCommand(Console* console, const char* param)
{
    static const u8 DemoFire[] =
    {
        #include "../build/assets/fire.tic.dat"
    };

    static const u8 DemoP3D[] =
    {
        #include "../build/assets/p3d.tic.dat"
    };

    static const u8 DemoSFX[] =
    {
        #include "../build/assets/sfx.tic.dat"
    };

    static const u8 DemoPalette[] =
    {
        #include "../build/assets/palette.tic.dat"
    };

    static const u8 DemoFont[] =
    {
        #include "../build/assets/font.tic.dat"
    };

    static const u8 DemoMusic[] =
    {
        #include "../build/assets/music.tic.dat"
    };

    static const u8 GameQuest[] =
    {
        #include "../build/assets/quest.tic.dat"
    };

    static const u8 GameTetris[] =
    {
        #include "../build/assets/tetris.tic.dat"
    };

    static const u8 Benchmark[] =
    {
        #include "../build/assets/benchmark.tic.dat"
    };

    static const u8 Bpp[] =
    {
        #include "../build/assets/bpp.tic.dat"
    };

    tic_fs* fs = console->fs;

    static const struct {const char* name; const u8* data; s32 size;} Demos[] =
    {
        {"fire.tic",        DemoFire,       sizeof DemoFire},
        {"font.tic",        DemoFont,       sizeof DemoFont},
        {"music.tic",       DemoMusic,      sizeof DemoMusic},
        {"p3d.tic",         DemoP3D,        sizeof DemoP3D},
        {"palette.tic",     DemoPalette,    sizeof DemoPalette},
        {"quest.tic",       GameQuest,      sizeof GameQuest},
        {"sfx.tic",         DemoSFX,        sizeof DemoSFX},
        {"tetris.tic",      GameTetris,     sizeof GameTetris},
        {"benchmark.tic",   Benchmark,      sizeof Benchmark},
        {"bpp.tic",         Bpp,            sizeof Bpp},
    };

    printBack(console, "\nadded carts:\n\n");

    for(s32 i = 0; i < COUNT_OF(Demos); i++)
    {
        installDemoCart(fs, Demos[i].name, Demos[i].data, Demos[i].size);
        printFront(console, Demos[i].name);
        printFront(console, "\n");
    }

    commandDone(console);
}

static void onConsoleGameMenuCommand(Console* console, const char* param)
{
    console->showGameMenu = false;
    showGameMenu();
    commandDone(console);
}

static void onConsoleSurfCommand(Console* console, const char* param)
{
    gotoSurf();
    commandDone(console);
}

static void onConsoleVersionCommand(Console* console, const char* param)
{
    printBack(console, "\n");
    consolePrint(console, TIC_VERSION_LABEL, CONSOLE_BACK_TEXT_COLOR);
    commandDone(console);
}

static void onConsoleConfigCommand(Console* console, const char* param)
{
    if(param == NULL)
    {
        onConsoleLoadCommand(console, CONFIG_TIC_PATH);
        return;
    }
    else if(strcmp(param, "reset") == 0)
    {
        console->config->reset(console->config);
        printBack(console, "\nconfiguration reset :)");
    }

#if defined(TIC_BUILD_WITH_LUA)
    else if(strcmp(param, "default") == 0 || strcmp(param, "default lua") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultLuaTicPath);
    }

#   if defined(TIC_BUILD_WITH_MOON)
    else if(strcmp(param, "default moon") == 0 || strcmp(param, "default moonscript") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultMoonTicPath);
    }
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    else if(strcmp(param, "default fennel") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultFennelTicPath);
    }
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    else if(strcmp(param, "default js") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultJSTicPath);
    }
#endif

#if defined(TIC_BUILD_WITH_WREN)
    else if(strcmp(param, "default wren") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultWrenTicPath);
    }
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    else if(strcmp(param, "default squirrel") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultSquirrelTicPath);
    }
#endif
    
    else
    {
        printError(console, "\nunknown parameter: ");
        printError(console, param);
    }

    commandDone(console);
}

static void onImportCover(const char* name, const void* buffer, s32 size, void* data)
{
    Console* console = (Console*)data;

    if(name)
    {
        static const char GifExt[] = ".gif";

        const char* pos = strstr(name, GifExt);

        if(pos && strcmp(pos, GifExt) == 0)
        {
            gif_image* image = gif_read_data(buffer, (s32)size);

            if (image)
            {
                enum
                {
                    Width = TIC80_WIDTH,
                    Height = TIC80_HEIGHT,
                    Size = Width * Height,
                };

                if(image->width == Width && image->height == Height)
                {
                    if(size <= sizeof console->tic->cart.cover.data)
                    {
                        console->tic->cart.cover.size = size;
                        memcpy(console->tic->cart.cover.data, buffer, size);

                        printLine(console);
                        printBack(console, name);
                        printBack(console, " successfully imported");
                    }
                    else printError(console, "\ncover image too big :(");
                }
                else printError(console, "\ncover image must be 240x136 :(");

                gif_close(image);
            }
            else printError(console, "\nfile importing error :(");
        }
        else printBack(console, "\nonly .gif files can be imported :|");
    }
    else printBack(console, "\nfile not imported :|");

    commandDone(console);
}

static void onImportSprites(const char* name, const void* buffer, s32 size, void* data)
{
    Console* console = (Console*)data;

    if(name)
    {
        static const char GifExt[] = ".gif";

        const char* pos = strstr(name, GifExt);

        if(pos && strcmp(pos, GifExt) == 0)
        {
            gif_image* image = gif_read_data(buffer, (s32)size);

            if (image)
            {
                enum
                {
                    Width = TIC_SPRITESHEET_SIZE,
                    Height = TIC_SPRITESHEET_SIZE*2,
                };

                s32 w = MIN(Width, image->width);
                s32 h = MIN(Height, image->height);

                for (s32 y = 0; y < h; y++)
                    for (s32 x = 0; x < w; x++)
                    {
                        u8 src = image->buffer[x + y * image->width];
                        const gif_color* c = &image->palette[src];
                        u8 color = tic_tool_find_closest_color(getBankPalette(false)->colors, c);

                        setSpritePixel(getBankTiles()->data, x, y, color);
                    }

                gif_close(image);

                printLine(console);
                printBack(console, name);
                printBack(console, " successfully imported");
            }
            else printError(console, "\nfile importing error :(");
        }
        else printBack(console, "\nonly .gif files can be imported :|");
    }
    else printBack(console, "\nfile not imported :|");

    commandDone(console);
}

static void injectMap(Console* console, const void* buffer, s32 size)
{
    enum {Size = sizeof(tic_map)};

    memset(getBankMap(), 0, Size);
    memcpy(getBankMap(), buffer, MIN(size, Size));
}

static void onImportMap(const char* name, const void* buffer, s32 size, void* data)
{
    Console* console = (Console*)data;

    if(name && buffer && size <= sizeof(tic_map))
    {
        injectMap(console, buffer, size);

        printLine(console);
        printBack(console, "map successfully imported");
    }
    else printBack(console, "\nfile not imported :|");

    commandDone(console);
}

static void onImportCode(const char* name, const void* buffer, s32 size, void* data)
{
    Console* console = (Console*)data;
    tic_mem* tic = console->tic;

    if(name && buffer && size <= sizeof(tic_code))
    {
        enum {Size = sizeof(tic_code)};

        memset(tic->cart.code.data, 0, Size);
        memcpy(tic->cart.code.data, buffer, MIN(size, Size));
        printLine(console);
        printBack(console, "code successfully imported");

        studioRomLoaded();
    }
    else printBack(console, "\ncode not imported :|");

    commandDone(console);
}

static void onConsoleImportCommand(Console* console, const char* param)
{
    bool error = true;

    char* filename = NULL;
    if(param)
    {
        filename = strchr(param, ' ');

        if(filename && strlen(filename + 1))
            *filename++ = '\0';
    }

    if(param && filename)
    {
        s32 size = 0;
        const void* data = tic_fs_load(console->fs, filename, &size);

        if(data)
        {
            if(strcmp(param, "sprites") == 0)
            {
                onImportSprites(filename, data, size, console);
                error = false;
            }
            else if(strcmp(param, "cover") == 0)
            {
                onImportCover(filename, data, size, console);
                error = false;
            }
            else if(strcmp(param, "map") == 0)
            {
                onImportMap(filename, data, size, console);
                error = false;
            }
            else if(strcmp(param, "code") == 0)
            {
                onImportCode(filename, data, size, console);
                error = false;
            }
        }
        else
        {
            char msg[TICNAME_MAX];
            sprintf(msg, "\nerror: `%s` file not loaded", filename);
            printError(console, msg);
            commandDone(console);
            return;
        }
    }

    if(error)
    {
        printBack(console, "\nusage: import (sprites cover map) file");
        commandDone(console);
    }
}

static void exportCover(Console* console, const char* filename)
{
    tic_cover_image* cover = &console->tic->cart.cover;

    if(cover->size)
    {
        void* data = malloc(cover->size);
        memcpy(data, cover->data, cover->size);
        if(tic_fs_save(console->fs, filename, data, cover->size, true))
        {
            printLine(console);
            printBack(console, filename);
            printBack(console, " exported :)");
        }
        else printError(console, "\nerror: cover not exported :(");

        free(data);
    }
    else printBack(console, "\ncover image is empty, run game and\npress [F7] to assign cover image");

    commandDone(console);
}

static void exportSfx(Console* console, s32 sfx, const char* filename)
{
    const char* path = studioExportSfx(sfx, filename);

    if(path)
    {
        printLine(console);
        printBack(console, filename);
        printBack(console, " exported :)");
        commandDone(console);
    }
    else
    {
        printError(console, "\nsfx exporting error :(");
        commandDone(console);
    }
}

static void exportMusic(Console* console, s32 track, const char* filename)
{
    const char* path = studioExportMusic(track, filename);

    if(path)
    {
        printLine(console);
        printBack(console, filename);
        printBack(console, " exported :)");

        commandDone(console);
    }
    else
    {
        printError(console, "\nmusic exporting error :(");
        commandDone(console);
    }
}

static void exportSprites(Console* console, const char* filename)
{
    enum
    {
        Width = TIC_SPRITESHEET_SIZE,
        Height = TIC_SPRITESHEET_SIZE*2,
    };

    enum{ Size = Width * Height * sizeof(gif_color)};
    u8* buffer = (u8*)malloc(Size);

    if(buffer)
    {
        u8* data = (u8*)malloc(Width * Height);

        if(data)
        {
            for (s32 y = 0; y < Height; y++)
                for (s32 x = 0; x < Width; x++)
                    data[x + y * Width] = getSpritePixel(getBankTiles()->data, x, y);

            s32 size = 0;
            if((size = writeGifData(console->tic, buffer, data, Width, Height)) 
                && tic_fs_save(console->fs, filename, buffer, size, true))
            {
                printLine(console);
                printBack(console, filename);
                printBack(console, " exported :)");
            }
            else printError(console, "\nerror: sprite not exported :(");

            commandDone(console);

            free(buffer);
            free(data);
        }
    }
}

static void exportMap(Console* console, const char* filename)
{
    enum{Size = sizeof(tic_map)};

    void* buffer = malloc(Size);

    if(buffer)
    {
        memcpy(buffer, getBankMap()->data, Size);

        if(tic_fs_save(console->fs, filename, buffer, Size, true))
        {
            printLine(console);
            printBack(console, filename);
            printBack(console, " exported :)");
        }
        else printError(console, "\nerror: map not exported :(");

        commandDone(console);
        free(buffer);
    }
}

static void *ticMemmem(const void* haystack, size_t hlen, const void* needle, size_t nlen)
{
    const u8* p = haystack;
    size_t plen = hlen;

    if(!nlen) return NULL;

    s32 needle_first = *(u8*)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
        return (void *)p;

        p++;
        plen = hlen - (p - (const u8*)haystack);
    }

    return NULL;
}

static const char TicCartSig[] = "TIC.CART";
#define SIG_SIZE (sizeof TicCartSig-1)

typedef struct
{
    u8 sig[SIG_SIZE];
    s32 appSize;
    s32 cartSize;
} EmbedHeader;

static void* embedCart(Console* console, u8* app, s32* size)
{
    tic_mem* tic = console->tic;
    u8* data = NULL;
    void* cart = malloc(sizeof(tic_cartridge));

    if(cart)
    {
        s32 cartSize = tic_cart_save(&tic->cart, cart);

        s32 zipSize = sizeof(tic_cartridge);
        u8* zipData = (u8*)malloc(zipSize);

        if(zipData)
        {
            if((zipSize = tic_tool_zip(zipData, zipSize, cart, cartSize)))
            {
                s32 appSize = *size;

                EmbedHeader header = 
                {
                    .appSize = appSize,
                    .cartSize = zipSize,
                };

                memcpy(header.sig, TicCartSig, SIG_SIZE);

                s32 finalSize = appSize + sizeof header + header.cartSize;
                data = malloc(finalSize);

                if (data)
                {
                    memcpy(data, app, appSize);
                    memcpy(data + appSize, &header, sizeof header);
                    memcpy(data + appSize + sizeof header, zipData, header.cartSize);

                    *size = finalSize;
                }
            }

            free(zipData);
        }

        free(cart);
    }
    
    return data;
}

typedef struct 
{
    Console* console;
    char filename[TICNAME_MAX];
} GameExportData;

static void onExportGet(const net_get_data* data)
{
    GameExportData* exportData = (GameExportData*)data->calldata;
    Console* console = exportData->console;

    switch(data->type)
    {
    case net_get_progress:
        {
            console->cursor.x = 0;
            printf("\r");
            printBack(console, "GET ");
            printFront(console, data->url);

            char buf[8];
            sprintf(buf, " [%i%%]", data->progress.size * 100 / data->progress.total);
            printBack(console, buf);
        }
        break;
    case net_get_error:
        free(exportData);
        printError(console, "file downloading error :(");
        commandDone(console);
        break;
    default:
        break;
    }
}

static void onNativeExportGet(const net_get_data* data)
{
    switch(data->type)
    {
    case net_get_done:
        {
            GameExportData* exportData = (GameExportData*)data->calldata;
            Console* console = exportData->console;

            tic_mem* tic = console->tic;

            char filename[TICNAME_MAX];
            strcpy(filename, exportData->filename);
            free(exportData);

            s32 size = data->done.size;

            printLine(console);

            const char* path = tic_fs_path(console->fs, filename);
            void* buf = NULL;
            if((buf = embedCart(console, data->done.data, &size))
                && fs_write(path, buf, size))
            {
                chmod(path, 0755);
                printFront(console, filename);
                printBack(console, " exported :)");
            }
            else
            {
                printError(console, "error: ");
                printError(console, filename);
                printError(console, " not saved :(");
            }

            commandDone(console);

            if (buf)
                free(buf);
        }
        break;
    default:
        onExportGet(data);
    }
}

static void exportGame(Console* console, const char* name, const char* system, net_get_callback callback)
{
    tic_mem* tic = console->tic;
    printLine(console);
    GameExportData* data = calloc(1, sizeof(GameExportData));
    data->console = console;
    strcpy(data->filename, name);

    char url[TICNAME_MAX] = "/export/" DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) "/";
    strcat(url, system);
    tic_net_get(console->net, url, callback, data);
}

static inline void exportNativeGame(Console* console, const char* name, const char* system)
{
    exportGame(console, name, system, onNativeExportGet);
}

static void onHtmlExportGet(const net_get_data* data)
{
    switch(data->type)
    {
    case net_get_done:
        {
            GameExportData* exportData = (GameExportData*)data->calldata;
            Console* console = exportData->console;

            tic_mem* tic = console->tic;

            char filename[TICNAME_MAX];
            strcpy(filename, exportData->filename);
            free(exportData);

            const char* zipPath = tic_fs_path(console->fs, filename);

            if(!fs_write(zipPath, data->done.data, data->done.size))
            {
                printError(console, "\nerror: ");
                printError(console, filename);
                printError(console, " not saved :(");
                commandDone(console);
                return;
            }

            struct zip_t *zip = zip_open(zipPath, ZIP_DEFAULT_COMPRESSION_LEVEL, 'a');
            bool errorOccured = false;

            if(zip)
            {
                void* cart = malloc(sizeof(tic_cartridge));

                if(cart)
                {
                    s32 cartSize = tic_cart_save(&tic->cart, cart);

                    if(cartSize)
                    {
                        zip_entry_open(zip, "cart.tic");
                        zip_entry_write(zip, cart, cartSize);
                        zip_entry_close(zip);
                    }
                    else errorOccured = true;

                    free(cart);
                }
                else errorOccured = true;

                zip_close(zip);
                if(!errorOccured)
                {
                    printLine(console);
                    printFront(console, filename);
                    printBack(console, " exported :)");
                }
            }
            else errorOccured = true;

            if(errorOccured)
                printError(console, "\ngame not exported :(\n");

            commandDone(console);
        }
        break;
    default:
        onExportGet(data);
    }
}

static const char* getFilename(const char* filename, const char* ext)
{
    if(strcmp(filename + strlen(filename) - strlen(ext), ext) == 0)
        return filename;

    static char Name[TICNAME_MAX];
    strcpy(Name, filename);
    strcat(Name, ext);

    return Name;
}

static void onConsoleExportCommand(Console* console, const char* param)
{
    bool error = true;

    enum {SfxIndex = sizeof "sfx" - 1, MusicIndex = sizeof "music" - 1};

    const char* filename = NULL;
    if(param)
    {
        char* name = strchr(param, ' ');

        if(name && strlen(name + 1))
            *name++ = '\0';
        filename = name;
    }

    if(param && filename)
    {
        if(strcmp(param, "html") == 0)
            exportGame(console, getFilename(filename, ".zip"), param, onHtmlExportGet);
        else if(strcmp(param, "sprites") == 0)
            exportSprites(console, getFilename(filename, ".gif"));
        else if(strcmp(param, "map") == 0)
            exportMap(console, getFilename(filename, ".map"));
        else if(strcmp(param, "cover") == 0)
            exportCover(console, getFilename(filename, ".gif"));
        else if(strncmp(param, "sfx", SfxIndex) == 0)
            exportSfx(console, atoi(param + SfxIndex) % SFX_COUNT, getFilename(filename, ".wav"));
        else if(strncmp(param, "music", MusicIndex) == 0)
            exportMusic(console, atoi(param + MusicIndex) % MUSIC_TRACKS, getFilename(filename, ".wav"));
        else if(strcmp(param, "win") == 0)
            exportNativeGame(console, getFilename(filename, ".exe"), param);
        else if(strcmp(param, "linux") == 0)
            exportNativeGame(console, filename, param);
        else if(strcmp(param, "rpi") == 0)
            exportNativeGame(console, filename, param);
        else if(strcmp(param, "mac") == 0)
            exportNativeGame(console, filename, param);
        else
        {
            printError(console, "\nunknown parameter: ");
            printError(console, param);
            commandDone(console);
        }
    }
    else
    {
        printBack(console, "\nusage: export (");
        printFront(console, "win linux rpi mac html sprites map cover sfx<#> music<#>");
        printBack(console, ") file\n");
        commandDone(console);
    }
}

static CartSaveResult saveCartName(Console* console, const char* name)
{
    tic_mem* tic = console->tic;

    bool success = false;

    if(name && strlen(name))
    {
        u8* buffer = (u8*)malloc(sizeof(tic_cartridge) * 3);

        if(buffer)
        {
            if(strcmp(name, CONFIG_TIC_PATH) == 0)
            {
                console->config->save(console->config);
                studioRomSaved();
                free(buffer);
                return CART_SAVE_OK;
            }
            else
            {
                s32 size = 0;

                if(hasProjectExt(name))
                {
                    size = tic_project_save(name, buffer, &tic->cart);
                }
                else
                {
                    name = getCartName(name);
                    size = tic_cart_save(&tic->cart, buffer);
                }

                if(size && tic_fs_save(console->fs, name, buffer, size, true))
                {
                    setCartName(console, name, tic_fs_path(console->fs, name));
                    success = true;
                    studioRomSaved();
                }
            }

            free(buffer);
        }
    }
    else if (strlen(console->rom.name))
    {
        return saveCartName(console, console->rom.name);
    }
    else return CART_SAVE_MISSING_NAME;

    return success ? CART_SAVE_OK : CART_SAVE_ERROR;
}

static CartSaveResult saveCart(Console* console)
{
    return saveCartName(console, NULL);
}

static void onConsoleSaveCommandConfirmed(Console* console, const char* param)
{
    CartSaveResult rom = saveCartName(console, param);

    if(rom == CART_SAVE_OK)
    {
        printBack(console, "\ncart ");
        printFront(console, console->rom.name);
        printBack(console, " saved!\n");
    }
    else if(rom == CART_SAVE_MISSING_NAME)
        printBack(console, "\ncart name is missing\n");
    else
        printBack(console, "\ncart saving error");

    commandDone(console);
}

static void onConsoleSaveCommand(Console* console, const char* param)
{
    if(param && strlen(param) && 
        (tic_fs_exists(console->fs, param) ||
            tic_fs_exists(console->fs, getCartName(param))))
    {
        static const char* Rows[] =
        {
            "THE CART",
            "ALREADY EXISTS",
            "",
            "DO YOU WANT TO",
            "OVERWRITE IT?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleSaveCommandConfirmed);
    }
    else
    {
        onConsoleSaveCommandConfirmed(console, param);
    }
}

static void onConsoleRunCommand(Console* console, const char* param)
{
    commandDone(console);

    tic_api_reset(console->tic);

    setStudioMode(TIC_RUN_MODE);
}

static void onConsoleResumeCommand(Console* console, const char* param)
{
    commandDone(console);

    tic_core_resume(console->tic);
    resumeRunMode();
}

static void onConsoleEvalCommand(Console* console, const char* param)
{
    printLine(console);

    const tic_script_config* script_config = tic_core_script_config(console->tic);

    if (script_config->eval)
    {
        if(param)
            script_config->eval(console->tic, param);
        else printError(console, "nothing to eval");
    }
    else
    {
        printError(console, "'eval' not implemented for the script");
    }

    commandDone(console);
}

static void onConsoleDelCommandConfirmed(Console* console, const char* param)
{
    if(param && strlen(param))
    {
        if (tic_fs_ispubdir(console->fs))
        {
            printError(console, "\naccess denied");
        }
        else
        {
            if(tic_fs_isdir(console->fs, param))
            {
                printBack(console, tic_fs_deldir(console->fs, param)
                    ? "\ndir not deleted"
                    : "\ndir successfully deleted");
            }
            else
            {
                printBack(console, tic_fs_delfile(console->fs, param)
                    ? "\nfile not deleted"
                    : "\nfile successfully deleted");
            }
        }
    }
    else printBack(console, "\nname is missing");

    commandDone(console);
}

static void onConsoleDelCommand(Console* console, const char* param)
{
    static const char* Rows[] =
    {
        "", "",
        "DO YOU REALLY WANT",
        "TO DELETE FILE?",
    };

    confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleDelCommandConfirmed);
}

static void printTable(Console* console, const char* text)
{
    printf("%s", text);

    const char* textPointer = text;
    const char* endText = textPointer + strlen(text);

    while(textPointer != endText)
    {
        char symbol = *textPointer++;

        scrollConsole(console);

        if(symbol == '\n')
        {
            console->cursor.x = 0;
            console->cursor.y++;
        }
        else
        {
            s32 offset = console->cursor.x + console->cursor.y * CONSOLE_BUFFER_WIDTH;
            *(console->buffer + offset) = symbol;

            u8 color = 0;

            switch(symbol)
            {
            case '+':
            case '|':
            case '-':
                color = CONSOLE_BACK_TEXT_COLOR;
                break;
            default:
                color = CONSOLE_FRONT_TEXT_COLOR;
            }

            *(console->colorBuffer + offset) = color;

            console->cursor.x++;

            if(console->cursor.x >= CONSOLE_BUFFER_WIDTH)
            {
                console->cursor.x = 0;
                console->cursor.y++;
            }
        }

    }
}

static void printRamInfo(Console* console, s32 addr, const char* name, s32 size)
{
    char buf[STUDIO_TEXT_BUFFER_WIDTH];
    sprintf(buf, "\n| %05X | %-17s | %-5i |", addr, name, size);
    printTable(console, buf);
}

static void onConsoleRamCommand(Console* console, const char* param)
{
    printLine(console);

    printTable(console, "\n+-----------------------------------+" \
                        "\n|           96KB RAM LAYOUT         |" \
                        "\n+-------+-------------------+-------+" \
                        "\n| ADDR  | INFO              | BYTES |" \
                        "\n+-------+-------------------+-------+");

    static const struct{s32 addr; const char* info;} Layout[] =
    {
        {0,                                             "<VRAM>"},
        {offsetof(tic_ram, tiles),                      "TILES"},
        {offsetof(tic_ram, sprites),                    "SPRITES"},
        {offsetof(tic_ram, map),                        "MAP"},
        {offsetof(tic_ram, input.gamepads),             "GAMEPADS"},
        {offsetof(tic_ram, input.mouse),                "MOUSE"},
        {offsetof(tic_ram, input.keyboard),             "KEYBOARD"},
        {offsetof(tic_ram, sfxpos),                     "SFX STATE"},
        {offsetof(tic_ram, registers),                  "SOUND REGISTERS"},
        {offsetof(tic_ram, sfx.waveforms),              "WAVEFORMS"},
        {offsetof(tic_ram, sfx.samples),                "SFX"},
        {offsetof(tic_ram, music.patterns.data),        "MUSIC PATTERNS"},
        {offsetof(tic_ram, music.tracks.data),          "MUSIC TRACKS"},
        {offsetof(tic_ram, sound_state),                "SOUND STATE"},
        {offsetof(tic_ram, stereo),                     "STEREO VOLUME"},
        {offsetof(tic_ram, persistent),                 "PERSISTENT MEMORY"},
        {offsetof(tic_ram, flags),                      "SPRITE FLAGS"},
        {offsetof(tic_ram, font),                       "SYSTEM FONT"},
        {offsetof(tic_ram, free),                       "... (free)"},
        {TIC_RAM_SIZE,                                  ""},
    };

    for(s32 i = 0; i < COUNT_OF(Layout)-1; i++)
        printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

    printTable(console, "\n+-------+-------------------+-------+");

    printLine(console);
    commandDone(console);
}

static void onConsoleVRamCommand(Console* console, const char* param)
{
    printLine(console);

    printTable(console, "\n+-----------------------------------+" \
                        "\n|          16KB VRAM LAYOUT         |" \
                        "\n+-------+-------------------+-------+" \
                        "\n| ADDR  | INFO              | BYTES |" \
                        "\n+-------+-------------------+-------+");

    static const struct{s32 addr; const char* info;} Layout[] =
    {
        {offsetof(tic_ram, vram.screen),            "SCREEN"},
        {offsetof(tic_ram, vram.palette),           "PALETTE"},
        {offsetof(tic_ram, vram.mapping),           "PALETTE MAP"},
        {offsetof(tic_ram, vram.vars.colors),       "BORDER COLOR"},
        {offsetof(tic_ram, vram.vars.offset),       "SCREEN OFFSET"},
        {offsetof(tic_ram, vram.vars.cursor),       "MOUSE CURSOR"},
        {offsetof(tic_ram, vram.blit),              "BLIT SEGMENT"},
        {offsetof(tic_ram, vram.reserved),          "... (reserved) "},
        {TIC_VRAM_SIZE,                             ""},
    };

    for(s32 i = 0; i < COUNT_OF(Layout)-1; i++)
        printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

    printTable(console, "\n+-------+-------------------+-------+");

    printLine(console);
    commandDone(console);
}

#if defined(CAN_ADDGET_FILE)

static void onConsoleAddFile(Console* console, const char* name, const u8* buffer, s32 size)
{
    if(name)
    {
        const char* path = tic_fs_path(console->fs, name);

        if(!fs_exists(path))
        {
            if(fs_write(path, buffer, size))
            {
                printLine(console);
                printFront(console, name);
                printBack(console, " successfully added :)");
            }
            else printError(console, "\nerror: file not added :(");
        }
        else
        {
            printError(console, "\nerror: ");
            printError(console, name);
            printError(console, " already exists :(");
        }        
    }

    commandDone(console);
}

static void onConsoleAddCommand(Console* console, const char* param)
{
    void* data = NULL;

    EM_ASM_
    ({
        Module.showAddPopup(function(filename, rom)
        {
            if(filename == null || rom == null)
            {
                dynCall('viiii', $0, [$1, 0, 0, 0]);
            }
            else
            {
                var filePtr = Module._malloc(filename.length + 1);
                stringToUTF8(filename, filePtr, filename.length + 1);

                var dataPtr = Module._malloc(rom.length);
                writeArrayToMemory(rom, dataPtr);

                dynCall('viiii', $0, [$1, filePtr, dataPtr, rom.length]);

                Module._free(filePtr);
                Module._free(dataPtr);
            }
        });
    }, onConsoleAddFile, console);
}

static void onConsoleGetCommand(Console* console, const char* name)
{
    if(name)
    {
        const char* path = tic_fs_path(console->fs, name);

        if(fs_exists(path))
        {
            s32 size = 0;
            void* buffer = fs_read(path, &size);

            EM_ASM_
            ({
                var name = UTF8ToString($0);
                var blob = new Blob([HEAPU8.subarray($1, $1 + $2)], {type: "application/octet-stream"});

                Module.saveAs(blob, name);
            }, name, buffer, size);
        }
        else
        {
            printError(console, "\nerror: ");
            printError(console, name);
            printError(console, " doesn't exist :(");
        }
    }
    else printBack(console, "\nusage: get <file>");

    commandDone(console);
}

#endif

static const struct
{
    const char* command;
    const char* alt;
    const char* info;
    void(*handler)(Console*, const char*);

} AvailableConsoleCommands[] =
{
    {"help",    NULL, "show this info",             onConsoleHelpCommand},
    {"ram",     NULL, "show 96KB RAM layout",        onConsoleRamCommand},
    {"vram",    NULL, "show 16KB VRAM layout",       onConsoleVRamCommand},
    {"exit",    "quit", "exit the application",     onConsoleExitCommand},
    {"new",     NULL, "create new cart",            onConsoleNewCommand},
    {"load",    NULL, "load cart",                  onConsoleLoadCommand},
    {"save",    NULL, "save cart",                  onConsoleSaveCommand},
    {"run",     NULL, "run loaded cart",            onConsoleRunCommand},
    {"resume",  NULL, "resume run cart",            onConsoleResumeCommand},
    {"eval",    "=",  "run code",                   onConsoleEvalCommand},
    {"dir",     "ls", "show list of files",         onConsoleDirCommand},
    {"cd",      NULL, "change directory",           onConsoleChangeDirectory},
    {"mkdir",   NULL, "make directory",             onConsoleMakeDirectory},
    {"folder",  NULL, "open working folder in OS",  onConsoleFolderCommand},

#if defined(CAN_ADDGET_FILE)
    {"add",     NULL, "add file",                   onConsoleAddCommand},
    {"get",     NULL, "download file",              onConsoleGetCommand},
#endif

    {"export",  NULL, "export native game",         onConsoleExportCommand},
    {"import",  NULL, "import sprites from .gif",   onConsoleImportCommand},
    {"del",     NULL, "delete file or dir",         onConsoleDelCommand},
    {"cls",     "clear", "clear screen",            onConsoleClsCommand},
    {"demo",    NULL, "install demo carts",         onConsoleInstallDemosCommand},
    {"config",  NULL, "edit TIC config",            onConsoleConfigCommand},
    {"version", NULL, "show the current version",   onConsoleVersionCommand},
    {"surf",    NULL, "open carts browser",         onConsoleSurfCommand},
    {"menu",    NULL, "show game menu",             onConsoleGameMenuCommand},
};

typedef struct
{
    Console* console;
    char* name;
} PredictFilenameData;

static bool predictFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
    PredictFilenameData* predictFilenameData = data;

    if(strstr(name, predictFilenameData->name) == name)
    {
        strcpy(predictFilenameData->name, name);
        return false;
    }

    return true;
}

static void predictFilenameDone(void* data)
{
    PredictFilenameData* predictFilenameData = data;
    Console* console = predictFilenameData->console;

    console->inputPosition = strlen(console->inputBuffer);
    free(predictFilenameData);
}

static void processConsoleTab(Console* console)
{
    char* input = console->inputBuffer;

    if(strlen(input))
    {
        char* param = strchr(input, ' ');

        if(param && strlen(++param))
        {
            PredictFilenameData data = { console, param };
            tic_fs_enum(console->fs, predictFilename, predictFilenameDone, OBJCOPY(data));
        }
        else
        {
            for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
            {
                const char* command = AvailableConsoleCommands[i].command;

                if(strstr(command, input) == command)
                {
                    strcpy(input, command);
                    console->inputPosition = strlen(input);
                    break;
                }
            }
        }
    }
}

static void toUpperStr(char* str)
{
    while(*str)
    {
        *str = toupper(*str);
        str++;
    }
}

static void onConsoleHelpCommand(Console* console, const char* param)
{
    printBack(console, "\navailable commands:\n\n");

    size_t maxName = 0;
    for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
    {
        size_t len = strlen(AvailableConsoleCommands[i].command);

        {
            const char* alt = AvailableConsoleCommands[i].alt;
            if(alt)
                len += strlen(alt) + 1;         
        }

        if(len > maxName) maxName = len;
    }

    char upName[64];

    for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
    {
        const char* command = AvailableConsoleCommands[i].command;

        {
            strcpy(upName, command);
            toUpperStr(upName);
            printFront(console, upName);
        }

        const char* alt = AvailableConsoleCommands[i].alt;

        if(alt)
        {
            strcpy(upName, alt);
            toUpperStr(upName);
            printBack(console, "/");
            printFront(console, upName);
        }

        size_t len = maxName - strlen(command) - (alt ? strlen(alt) : -1);
        while(len--) printBack(console, " ");

        printBack(console, AvailableConsoleCommands[i].info);
        printLine(console);
    }

    printBack(console, "\npress ");
    printFront(console, "ESC");
    printBack(console, " to enter UI mode\n");

    commandDone(console);
}

static s32 tic_strcasecmp(const char *str1, const char *str2)
{
    char a = 0;
    char b = 0;
    while (*str1 && *str2) {
        a = toupper((u8) *str1);
        b = toupper((u8) *str2);
        if (a != b)
            break;
        ++str1;
        ++str2;
    }
    a = toupper(*str1);
    b = toupper(*str2);
    return (s32) ((u8) a - (u8) b);
}

static void processCommand(Console* console, const char* command)
{
    console->active = false;

    while(*command == ' ')
        command++;

    // trim empty chars
    {
        char* end = (char*)command + strlen(command) - 1;

        while(*end == ' ' && end > command)
            *end-- = '\0';
    }

    char* param = strchr(command, ' ');

    if(param)
        *param++ = '\0';

    if(param && !strlen(param)) param = NULL;

    for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
        if(tic_strcasecmp(command, AvailableConsoleCommands[i].command) == 0 ||
            (AvailableConsoleCommands[i].alt && tic_strcasecmp(command, AvailableConsoleCommands[i].alt) == 0))
        {
            if(AvailableConsoleCommands[i].handler)
            {
                AvailableConsoleCommands[i].handler(console, param);
                command = NULL;
                break;
            }
        }

    if(command)
    {
        printLine(console);
        printError(console, "unknown command:");
        printError(console, console->inputBuffer);
        commandDone(console);
    }
}

static void processCommands(Console* console)
{
    const char* command = console->args.cmd;
    static const char Sep[] = " & ";
    char* next = strstr(command, Sep);

    if(next)
    {
        *next = '\0';
        next += sizeof Sep - 1;
    }

    console->args.cmd = next;

    printFront(console, command);
    processCommand(console, command);
}

static void fillInputBufferFromHistory(Console* console)
{
    memset(console->inputBuffer, 0, sizeof(console->inputBuffer));
    strcpy(console->inputBuffer, console->history->value);
    processConsoleEnd(console);
}

static void onHistoryUp(Console* console)
{
    if(console->history)
    {
        if(console->history->next)
            console->history = console->history->next;
    }
    else console->history = console->historyHead;

    if(console->history)
        fillInputBufferFromHistory(console);
}

static void onHistoryDown(Console* console)
{
    if(console->history)
    {
        if(console->history->prev)
        {
            console->history = console->history->prev;
            fillInputBufferFromHistory(console);
        }
        else
        {
            memset(console->inputBuffer, 0, sizeof(console->inputBuffer));
            console->inputPosition = 0;
        }
    }
}

static void appendHistory(Console* console, const char* value)
{
    HistoryItem* item = (HistoryItem*)malloc(sizeof(HistoryItem));
    item->value = strdup(value);
    item->next = NULL;
    item->prev = NULL;

    if(console->historyHead == NULL)
    {
        console->historyHead = item;
        return;
    }

    console->historyHead->prev = item;
    item->next = console->historyHead;
    console->historyHead = item;
    console->history = NULL;
}

static void processConsoleCommand(Console* console)
{
    console->inputPosition = 0;

    size_t commandSize = strlen(console->inputBuffer);

    if(commandSize)
    {
        printFront(console, console->inputBuffer);
        appendHistory(console, console->inputBuffer);

        processCommand(console, console->inputBuffer);

        memset(console->inputBuffer, 0, sizeof(console->inputBuffer));
    }
    else commandDone(console);
}

static void error(Console* console, const char* info)
{
    consolePrint(console, info ? info : "unknown error", CONSOLE_ERROR_TEXT_COLOR);
    commandDone(console);
}

static void trace(Console* console, const char* text, u8 color)
{
    consolePrint(console, text, color);
    commandDone(console);
}

static void setScroll(Console* console, s32 val)
{
    if(console->scroll.pos != val)
    {
        console->scroll.pos = val;

        if(console->scroll.pos < 0) console->scroll.pos = 0;
        if(console->scroll.pos > console->cursor.y) console->scroll.pos = console->cursor.y;
    }
}

static lua_State* netLuaInit(u8* buffer, s32 size)
{
    if (buffer && size)
    {
        char* script = calloc(1, size + 1);
        memcpy(script, buffer, size);
        lua_State* lua = luaL_newstate();

        if(lua)
        {
            if(luaL_loadstring(lua, (char*)script) == LUA_OK && lua_pcall(lua, 0, LUA_MULTRET, 0) == LUA_OK)
                return lua;

            else lua_close(lua);
        }

        free(script);
    }

    return NULL;
}

static void onHttpVesrsionGet(const net_get_data* data)
{
    Console* console = (Console*)data->calldata;

    switch(data->type)
    {
    case net_get_done:
        {
            lua_State* lua = netLuaInit(data->done.data, data->done.size);

            union 
            {
                struct
                {
                    s32 major;
                    s32 minor;
                    s32 patch;
                };

                s32 data[3];
            } version = 
            {
                .major = TIC_VERSION_MAJOR,
                .minor = TIC_VERSION_MINOR,
                .patch = TIC_VERSION_REVISION,
            };

            if(lua)
            {
                static const char* Fields[] = {"major", "minor", "patch"};

                for(s32 i = 0; i < COUNT_OF(Fields); i++)
                {
                    lua_getglobal(lua, Fields[i]);

                    if(lua_isinteger(lua, -1))
                        version.data[i] = (s32)lua_tointeger(lua, -1);

                    lua_pop(lua, 1);
                }

                lua_close(lua);
            }

            if((version.major > TIC_VERSION_MAJOR) ||
                (version.major == TIC_VERSION_MAJOR && version.minor > TIC_VERSION_MINOR) ||
                (version.major == TIC_VERSION_MAJOR && version.minor == TIC_VERSION_MINOR && version.patch > TIC_VERSION_REVISION))
            {
                char msg[TICNAME_MAX];
                sprintf(msg, " new version %i.%i.%i available\n", version.major, version.minor, version.patch);

                enum{Offset = (2 * STUDIO_TEXT_BUFFER_WIDTH)};

                memcpy(console->buffer + Offset, msg, strlen(msg));
                memset(console->colorBuffer + Offset, tic_color_red, STUDIO_TEXT_BUFFER_WIDTH);
            }
        }
        break;
    default:
        break;
    }
}

static void processMouse(Console* console)
{
    tic_mem* tic = console->tic;
    // process scroll
    {
        tic80_input* input = &console->tic->ram.input;

        if(input->mouse.scrolly)
        {
            enum{Scroll = 3};
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            setScroll(console, console->scroll.pos + delta);
        }
    }

    tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    if(checkMouseDown(&rect, tic_mouse_left))
    {
        setCursor(tic_cursor_hand);

        if(console->scroll.active)
        {
            setScroll(console, (console->scroll.start - tic_api_mouse(tic).y) / TIC_FONT_HEIGHT);
        }
        else
        {
            console->scroll.active = true;
            console->scroll.start = tic_api_mouse(tic).y + console->scroll.pos * TIC_FONT_HEIGHT;
        }            
    }
    else console->scroll.active = false;
}

static void processConsolePgUp(Console* console)
{
    setScroll(console, console->scroll.pos - STUDIO_TEXT_BUFFER_HEIGHT/2);
}

static void processConsolePgDown(Console* console)
{
    setScroll(console, console->scroll.pos + STUDIO_TEXT_BUFFER_HEIGHT/2);
}

static void processKeyboard(Console* console)
{
    tic_mem* tic = console->tic;

    if(!console->active)
        return;

    if(tic->ram.input.keyboard.data != 0)
    {
        console->cursor.delay = CONSOLE_CURSOR_DELAY;

        if(keyWasPressed(tic_key_up)) onHistoryUp(console);
        else if(keyWasPressed(tic_key_down)) onHistoryDown(console);
        else if(keyWasPressed(tic_key_left))
        {
            if(console->inputPosition > 0)
                console->inputPosition--;
        }
        else if(keyWasPressed(tic_key_right))
        {
            console->inputPosition++;
            size_t len = strlen(console->inputBuffer);
            if(console->inputPosition > len)
                console->inputPosition = len;
        }
        else if(keyWasPressed(tic_key_return))      processConsoleCommand(console);
        else if(keyWasPressed(tic_key_backspace))   processConsoleBackspace(console);
        else if(keyWasPressed(tic_key_delete))      processConsoleDel(console);
        else if(keyWasPressed(tic_key_home))        processConsoleHome(console);
        else if(keyWasPressed(tic_key_end))         processConsoleEnd(console);
        else if(keyWasPressed(tic_key_tab))         processConsoleTab(console);
        else if(keyWasPressed(tic_key_pageup))      processConsolePgUp(console);
        else if(keyWasPressed(tic_key_pagedown))    processConsolePgDown(console);

        if(tic_api_key(tic, tic_key_ctrl) 
            && keyWasPressed(tic_key_k))
        {
            onConsoleClsCommand(console, NULL);
            return;
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        {
            scrollConsole(console);
            console->cursor.delay = CONSOLE_CURSOR_DELAY;
        }

        size_t size = strlen(console->inputBuffer);

        if(size < sizeof(console->inputBuffer))
        {
            char* pos = console->inputBuffer + console->inputPosition;
            memmove(pos + 1, pos, strlen(pos));

            *(console->inputBuffer + console->inputPosition++) = sym;
        }

        console->cursor.delay = CONSOLE_CURSOR_DELAY;
    }

}

static void tick(Console* console)
{
    tic_mem* tic = console->tic;

    processMouse(console);
    processKeyboard(console);

    if(console->tickCounter == 0)
    {
        if(!console->embed.yes)
        {
#if defined(TIC_BUILD_WITH_LUA)
            loadDemo(console, LuaScript);
#elif defined(TIC_BUILD_WITH_JS)
            loadDemo(console, JavaScript);
#elif defined(TIC_BUILD_WITH_WREN)
            loadDemo(console, WrenScript);
#elif defined(TIC_BUILD_WITH_SQUIRREL)
            loadDemo(console, SquirrelScript);
#endif          

            printBack(console, "\n hello! type ");
            printFront(console, "help");
            printBack(console, " for help\n");

            if(getConfig()->checkNewVersion)
                tic_net_get(console->net, "/api?fn=version", onHttpVesrsionGet, console);

            commandDone(console);
        }
        else printBack(console, "\n loading cart...");
    }

    tic_api_cls(tic, TIC_COLOR_BG);
    drawConsoleText(console);

    if(console->embed.yes)
    {
        if(console->tickCounter >= (u32)(console->args.skip ? 1 : TIC80_FRAMERATE))
        {
            if(!console->args.skip)
                console->showGameMenu = true;

            memcpy(&tic->cart, console->embed.file, sizeof(tic_cartridge));

            tic_api_reset(tic);

            setStudioMode(TIC_RUN_MODE);

            console->embed.yes = false;
            studioRomLoaded();

            printLine(console);
            commandDone(console);
            console->active = true;

            return;
        }
    }
    else
    {   
        if(console->cursor.delay)
            console->cursor.delay--;

        if(getStudioMode() != TIC_CONSOLE_MODE) return;

        drawConsoleInputText(console);

        if(console->active && console->args.cmd)
            processCommands(console);
    }

    console->tickCounter++;
}

static inline bool isslash(char c)
{
    return c == '/' && c == '\\';
}

static bool cmdLoadCart(Console* console, const char* path)
{
    bool done = false;

    s32 size = 0;
    void* data = fs_read(path, &size);

    if(data)
    {
        const char* cartName = NULL;
        
        {
            const char* ptr = path + strlen(path);
            while(ptr > path && !isslash(*ptr))--ptr;
            cartName = ptr;
        }

        setCartName(console, cartName, path);

        if(hasProjectExt(cartName))
        {
            if(tic_project_load(cartName, data, size, console->embed.file))
                done = console->embed.yes = true;
        }
        else if(tic_tool_has_ext(cartName, CART_EXT))
        {
            tic_mem* tic = console->tic;
            tic_cart_load(console->embed.file, data, size);
            done = console->embed.yes = true;
        }
        
        free(data);
    }

    return done;
}

void initConsole(Console* console, tic_mem* tic, tic_fs* fs, tic_net* net, Config* config, StartArgs args)
{
    if(!console->buffer) console->buffer = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->colorBuffer) console->colorBuffer = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->embed.file) console->embed.file = malloc(sizeof(tic_cartridge));

    *console = (Console)
    {
        .tic = tic,
        .config = config,
        .loadByHash = loadByHash,
        .load = onConsoleLoadCommandConfirmed,
        .updateProject = updateProject,
        .error = error,
        .trace = trace,
        .tick = tick,
        .save = saveCart,
        .cursor = {.x = 0, .y = 0, .delay = 0},
        .scroll =
        {
            .pos = 0,
            .start = 0,
            .active = false,
        },
        .embed =
        {
            .yes = false,
            .file = console->embed.file,
        },
        .inputPosition = 0,
        .history = NULL,
        .historyHead = NULL,
        .tickCounter = 0,
        .active = false,
        .buffer = console->buffer,
        .colorBuffer = console->colorBuffer,
        .fs = fs,
        .net = net,
        .showGameMenu = false,
        .args = args,
    };

    memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
    memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

    printFront(console, "\n " TIC_NAME_FULL "");
    printBack(console, " " TIC_VERSION_LABEL "\n");
    printBack(console, " " TIC_COPYRIGHT "\n");

    if(args.cart)
        if(!cmdLoadCart(console, args.cart))
        {
            printf("error: cart `%s` not loaded\n", args.cart);
            exit(1);
        }

    if(!console->embed.yes)
    {
        char appPath[TICNAME_MAX];

#if defined(__TIC_WINDOWS__)
        {
            wchar_t wideAppPath[TICNAME_MAX];
            GetModuleFileNameW(NULL, wideAppPath, sizeof wideAppPath);
            WideCharToMultiByte(CP_UTF8, 0, wideAppPath, COUNT_OF(wideAppPath), appPath, COUNT_OF(appPath), 0, 0);
        }
#elif defined(__TIC_LINUX__)
        s32 size = readlink("/proc/self/exe", appPath, sizeof appPath);
        appPath[size] = '\0';
#elif defined(__TIC_MACOSX__)
        s32 size = sizeof appPath;
        _NSGetExecutablePath(appPath, &size);
#endif

        s32 appSize = 0;
        u8* app = fs_read(appPath, &appSize);

        if(app)
        {
            s32 size = appSize;
            const u8* ptr = app;

            while(true)
            {
                const EmbedHeader* header = (const EmbedHeader*)ticMemmem(ptr, size, TicCartSig, SIG_SIZE);

                if(header)
                {
                    if(appSize == header->appSize + sizeof(EmbedHeader) + header->cartSize)
                    {
                        u8* data = calloc(1, sizeof(tic_cartridge));

                        if(data)
                        {
                            s32 dataSize = tic_tool_unzip(data, sizeof(tic_cartridge), app + header->appSize + sizeof(EmbedHeader), header->cartSize);

                            if(dataSize)
                            {
                                tic_cart_load(console->embed.file, data, dataSize);
                                console->embed.yes = true;
                            }
                            
                            free(data);
                        }

                        break;
                    }
                    else
                    {
                        ptr = (const u8*)header + SIG_SIZE;
                        size = appSize - (s32)(ptr - app);
                    }
                }
                else break;
            }

            free(app);
        }
    }

    console->active = !console->embed.yes;
}

void freeConsole(Console* console)
{
    free(console->buffer);
    free(console->colorBuffer);
    free(console->embed.file);

    {
        HistoryItem* it = console->historyHead;

        while(it)
        {
            HistoryItem* next = it->next;

            if(it->value) free(it->value);
            
            free(it);

            it = next;
        }        
    }

    free(console);
}
