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
#include "fs.h"
#include "config.h"
#include "ext/gif.h"
#include "ext/file_dialog.h"
#include "project.h"

#include <ctype.h>
#include <string.h>

#if !defined(__TIC_MACOSX__)
#include <malloc.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#define CONSOLE_CURSOR_COLOR tic_color_2
#define CONSOLE_BACK_TEXT_COLOR tic_color_14
#define CONSOLE_FRONT_TEXT_COLOR tic_color_12
#define CONSOLE_ERROR_TEXT_COLOR tic_color_2
#define CONSOLE_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define CONSOLE_CURSOR_DELAY (TIC80_FRAMERATE / 2)
#define CONSOLE_BUFFER_WIDTH (STUDIO_TEXT_BUFFER_WIDTH)
#define CONSOLE_BUFFER_HEIGHT (STUDIO_TEXT_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SCREENS 2
#define CONSOLE_BUFFER_SIZE (CONSOLE_BUFFER_WIDTH * CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)

typedef enum
{
#if defined(TIC_BUILD_WITH_LUA)
    LuaScript,  

#   if defined(TIC_BUILD_WITH_MOON)
    MoonScript, 
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    JavaScript, 
#endif

} ScriptLang;

#if defined(__TIC_WINDOWS__) || defined(__TIC_LINUX__) || defined(__TIC_MACOSX__)
#define CAN_EXPORT_NATIVE 1
#endif

#if defined(CAN_EXPORT_NATIVE)

static const char TicCartSig[] = "TIC.CART";
#define SIG_SIZE (sizeof TicCartSig-1)

typedef struct
{
    u8 sig[SIG_SIZE];
    s32 appSize;
    s32 cartSize;
} EmbedHeader;

#endif

#if defined(__TIC_WINDOWS__)
static const char* ExeExt = ".exe";
#endif

#if defined(TIC_BUILD_WITH_LUA)
static const char DefaultLuaTicPath[] = TIC_LOCAL_VERSION "default.tic";

#   if defined(TIC_BUILD_WITH_MOON)
static const char DefaultMoonTicPath[] = TIC_LOCAL_VERSION "default_moon.tic";
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
static const char DefaultJSTicPath[] = TIC_LOCAL_VERSION "default_js.tic";
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
    fsGetDir(console->fs, dir);
    if(strlen(dir))
        printBack(console, dir);

    printFront(console, ">");
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

        char buf[64] = {0};

        for(s32 i = 0; i < COUNT_OF(Sections); i++)
        {
            sprintf(buf, "%s %s", CART_EXT, Sections[i]);
            char* pos = strstr(param, buf);

            if(pos)
            {
                pos[sizeof(CART_EXT) - 1] = 0;
                const char* name = getCartName(param);
                s32 size = 0;
                void* data = fsLoadFile(console->fs, name, &size);

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
    char path[TICNAME_MAX] = {0};

    {
        switch(script)
        {
#if defined(TIC_BUILD_WITH_LUA)
        case LuaScript: strcpy(path, DefaultLuaTicPath); break;

#   if defined(TIC_BUILD_WITH_MOON)
        case MoonScript: strcpy(path, DefaultMoonTicPath); break;
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
        case JavaScript: strcpy(path, DefaultJSTicPath); break;
#endif        
        }

        void* data = fsLoadRootFile(console->fs, path, size);

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


    }

    u8* data = calloc(1, sizeof(tic_cartridge));

    if(data)
    {
        *size = tic_tool_unzip(data, sizeof(tic_cartridge), demo, romSize);

        if(*size)
            fsSaveRootFile(console->fs, path, data, *size, false);
    }

    return data;
}

static void setCartName(Console* console, const char* name)
{
    if(name != console->romName)
        strcpy(console->romName, name);
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

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    if(strcmp(param, DefaultJSTicPath) == 0)
        data = getDemoCart(console, JavaScript, &size);
#endif


    const char* name = getCartName(param);

    setCartName(console, name);

    loadRom(console->tic, data, size);

    studioRomLoaded();

    printBack(console, "\ncart ");
    printFront(console, console->romName);
    printBack(console, " loaded!\n");

    free(data);
}

static void onCartLoaded(Console* console, const char* name)
{
    setCartName(console, name);

    studioRomLoaded();

    printBack(console, "\ncart ");
    printFront(console, console->romName);
    printBack(console, " loaded!\nuse ");
    printFront(console, "RUN");
    printBack(console, " command to run it\n");

}

static void updateProject(Console* console)
{
    tic_mem* tic = console->tic;

    if(strlen(console->romName) && hasProjectExt(console->romName))
    {
        s32 size = 0;
        void* data = fsLoadFile(console->fs, console->romName, &size);

        if(data)
        {
            tic_cartridge* cart = malloc(sizeof(tic_cartridge));

            if(cart)
            {
                if(tic_project_load(console->romName, data, size, cart))
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

static void onConsoleLoadCommandConfirmed(Console* console, const char* param)
{
    if(onConsoleLoadSectionCommand(console, param)) return;

    if(param)
    {
        s32 size = 0;
        const char* name = getCartName(param);

        void* data = strcmp(name, CONFIG_TIC_PATH) == 0
            ? fsLoadRootFile(console->fs, name, &size)
            : fsLoadFile(console->fs, name, &size);

        if(data)
        {
            console->showGameMenu = fsIsInPublicDir(console->fs);

            loadRom(console->tic, data, size);

            onCartLoaded(console, name);

            free(data);
        }
        else
        {
            const char* name = getName(param, PROJECT_LUA_EXT);

            if(!fsExistsFile(console->fs, name))
                name = getName(param, PROJECT_MOON_EXT);

            if(!fsExistsFile(console->fs, name))
                name = getName(param, PROJECT_JS_EXT);

            if(!fsExistsFile(console->fs, name))
                name = getName(param, PROJECT_WREN_EXT);

            if(!fsExistsFile(console->fs, name))
                name = getName(param, PROJECT_FENNEL_EXT);

            if(!fsExistsFile(console->fs, name))
                name = getName(param, PROJECT_SQUIRREL_EXT);

            void* data = fsLoadFile(console->fs, name, &size);

            if(data && tic_project_load(name, data, size, &console->tic->cart))
                onCartLoaded(console, name);
            else printBack(console, "\ncart loading error");

            free(data);
        }
    }
    else printBack(console, "\ncart name is missing");

    commandDone(console);
}

static void load(Console* console, const char* path, const char* hash)
{
    if(hash)
    {
        s32 size = 0;
        const char* name = getCartName(path);

        void* data = fsLoadFileByHash(console->fs, hash, &size);

        if(data)
        {
            console->showGameMenu = true;

            loadRom(console->tic, data, size);
            onCartLoaded(console, name);

            free(data);     
        }

        commandDone(console);
    }
    else onConsoleLoadCommandConfirmed(console, path);
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

static void printMemoryError(Console* console)
{
    printError(console, "Memory allocation error");
}

static void confirmCommand(Console* console, const char** text, s32 rows, const char* param, ConfirmCallback callback)
{
    CommandConfirmData* data = malloc(sizeof(CommandConfirmData));
    if (data)
    {
        data->console = console;
        data->param = param ? strdup(param) : NULL;
        data->callback = callback;

        showDialog(text, rows, onConfirm, data);
    }
    else
        printMemoryError(console);
}

static void onConsoleLoadDemoCommand(Console* console, const char* param)
{
    if(studioCartChanged())
    {
        static const char* Rows[] =
        {
            "null",
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
            "null",
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

    memset(console->romName, 0, sizeof console->romName);

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


#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
        if(strcmp(param, "js") == 0 || strcmp(param, "javascript") == 0)
        {
            loadDemo(console, JavaScript);
            done = true;
        }
#endif
     

        if(!done)
        {
            printError(console, "null");
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

    if(done) printBack(console, "null");
    else printError(console, "null");

    commandDone(console);
}

static void onConsoleNewCommand(Console* console, const char* param)
{
    if(studioCartChanged())
    {
        static const char* Rows[] =
        {
            "null",
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

    if(!dir)
        printData->count++;

    return true;
}

static void onConsoleChangeDirectory(Console* console, const char* param)
{
    if(param && strlen(param))
    {
        if(strcmp(param, "/") == 0)
        {
            fsHomeDir(console->fs);
        }
        else if(strcmp(param, "..") == 0)
        {
            fsDirBack(console->fs);
        }
        else if(fsIsDir(console->fs, param))
        {
            fsChangeDir(console->fs, param);
        }
        else printBack(console, "null");
    }
    else printBack(console, "null");

    commandDone(console);
}

static void onConsoleMakeDirectory(Console* console, const char* param)
{
commandDone(console);
}

static void onConsoleDirCommand(Console* console, const char* param)
{
    commandDone(console);
}

static void onConsoleClsCommand(Console* console, const char* param)
{
    memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
    memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);
    console->scroll.pos = 0;
    console->cursor.x = console->cursor.y = 0;

    commandDoneLine(console, false);
}

static void installDemoCart(FileSystem* fs, const char* name, const void* cart, s32 size)
{
    u8* data = calloc(1, sizeof(tic_cartridge));

    if(data)
    {
        s32 dataSize = tic_tool_unzip(data, sizeof(tic_cartridge), cart, size);

        if(dataSize)
            fsSaveFile(fs, name, data, dataSize, true);

        free(data);        
    }
}

static void onConsoleInstallDemosCommand(Console* console, const char* param)
{

    static const u8 GameTetris[] =
    {
        #include "../build/assets/tetris.tic.dat"
    };

    static const u8 Benchmark[] =
    {
        #include "../build/assets/benchmark.tic.dat"
    };

    FileSystem* fs = console->fs;

    static const struct {const char* name; const u8* data; s32 size;} Demos[] =
    {
        {"tetris.tic",      GameTetris,     sizeof GameTetris},
        {"benchmark.tic",   Benchmark,      sizeof Benchmark},
    };


    for(s32 i = 0; i < COUNT_OF(Demos); i++)
    {
        installDemoCart(fs, Demos[i].name, Demos[i].data, Demos[i].size);
        printFront(console, Demos[i].name);
    }

    commandDone(console);
}

static void onConsoleGameMenuCommand(Console* console, const char* param)
{
    console->showGameMenu = false;
    showGameMenu();
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

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    else if(strcmp(param, "default js") == 0)
    {
        onConsoleLoadDemoCommand(console, DefaultJSTicPath);
    }
#endif


    commandDone(console);
}

static void onFileDownloaded(GetResult result, void* data)
{
    commandDone(console);
}

static void onImportCover(const char* name, const void* buffer, size_t size, void* data)
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
                    }
                    else printError(console, "null");
                }
                else printError(console, "null");

                gif_close(image);
            }
            else printError(console, "null");
        }
        else printBack(console, "null");
    }
    else printBack(console, "null");

    commandDone(console);
}

static void onImportSprites(const char* name, const void* buffer, size_t size, void* data)
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
            }
            else printError(console, "null");
        }
        else printBack(console, "null");
    }
    else printBack(console, "null");

    commandDone(console);
}

static void injectMap(Console* console, const void* buffer, s32 size)
{
    enum {Size = sizeof(tic_map)};

    memset(getBankMap(), 0, Size);
    memcpy(getBankMap(), buffer, MIN(size, Size));
}

static void onImportMap(const char* name, const void* buffer, size_t size, void* data)
{
    Console* console = (Console*)data;

    if(name && buffer && size <= sizeof(tic_map))
    {
        injectMap(console, buffer, size);

        printLine(console);
    }
    else printBack(console, "null");

    commandDone(console);
}

static void onConsoleImportCommand(Console* console, const char* param)
{
    if(param == NULL)
    {
        commandDone(console);
    }
    else if(strcmp(param, "sprites") == 0)
        fsOpenFileData(onImportSprites, console);
    else if(strcmp(param, "map") == 0)
        fsOpenFileData(onImportMap, console);
    else if(strcmp(param, "cover") == 0)
        fsOpenFileData(onImportCover, console);
    else
    {
        printError(console, "null");
        printError(console, param);
        commandDone(console);
    }
}

static void onSpritesExported(GetResult result, void* data)
{
    commandDone(console);
}

static void onCoverExported(GetResult result, void* data)
{
    commandDone(console);
}

static void exportCover(Console* console)
{
    tic_cover_image* cover = &console->tic->cart.cover;

    if(cover->size)
    {
        void* data = malloc(cover->size);
        if (data)
        {
            memcpy(data, cover->data, cover->size);
            fsGetFileData(onCoverExported, "cover.gif", data, cover->size, DEFAULT_CHMOD, console);
        }
        else
            printMemoryError(console);
    }
    else
    {
        commandDone(console);
    }
}

static void exportSfx(Console* console, s32 sfx)
{
    const char* path = studioExportSfx(sfx);

    s32 size = 0;
    void* data = fsLoadRootFile(console->fs, path, &size);

    if(data)
    {
        char name[TICNAME_MAX];
        sprintf(name, "sfx %i.wav", sfx);
        fsGetFileData(onFileDownloaded, name, data, size, DEFAULT_CHMOD, console);
    }
    else
    {
        commandDone(console);
    }
}

static void exportMusic(Console* console, s32 track)
{
    const char* path = studioExportMusic(track);

    s32 size = 0;
    void* data = fsLoadRootFile(console->fs, path, &size);

    if(data)
    {
        char name[TICNAME_MAX];
        sprintf(name, "track %i.wav", track);
        fsGetFileData(onFileDownloaded, name, data, size, DEFAULT_CHMOD, console);
    }
    else
    {
        commandDone(console);
    }
}

static void exportSprites(Console* console)
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
            if((size = writeGifData(console->tic, buffer, data, Width, Height)))
            {
                // buffer will be freed inside fsGetFileData
                fsGetFileData(onSpritesExported, "sprites.gif", buffer, size, DEFAULT_CHMOD, console);
            }
            else
            {
                commandDone(console);
                free(buffer);
            }

            free(data);
        }
    }
}

static void onMapExported(GetResult result, void* data)
{
    commandDone(console);
}

static void exportMap(Console* console)
{
    enum{Size = sizeof(tic_map)};

    void* buffer = malloc(Size);

    if(buffer)
    {
        memcpy(buffer, getBankMap()->data, Size);
        fsGetFileData(onMapExported, "world.map", buffer, Size, DEFAULT_CHMOD, console);
    }
}

#if defined(__EMSCRIPTEN__)

static void onConsoleExportCommand(Console* console, const char* param)
{
    if(param == NULL || (param && strcmp(param, "native") == 0) || (param && strcmp(param, "html") == 0))
    {
        commandDone(console);
    }
    else if(param && strcmp(param, "sprites") == 0)
        exportSprites(console);
    else if(param && strcmp(param, "map") == 0)
        exportMap(console);
    else if(param && strcmp(param, "cover") == 0)
        exportCover(console);
    else
    {
        printError(console, "\nunknown parameter: ");
        printError(console, param);
        commandDone(console);
    }
}

#else

#if defined(CAN_EXPORT_NATIVE)

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

static void* embedCart(Console* console, s32* size)
{
    tic_mem* tic = console->tic;

    u8* data = NULL;
    s32 appSize = 0;
    void* app = fsReadFile(console->appPath, &appSize);

    if(app)
    {
        void* cart = malloc(sizeof(tic_cartridge));

        if(cart)
        {
            s32 cartSize = tic_cart_save(&tic->cart, cart);

            unsigned long zipSize = sizeof(tic_cartridge);
            u8* zipData = (u8*)malloc(zipSize);

            if(zipData)
            {
                if(zipSize = tic_tool_zip(zipData, zipSize, cart, cartSize))
                {
                    EmbedHeader header = 
                    {
                        .appSize = appSize,
                        .cartSize = zipSize,
                    };

                    memcpy(header.sig, TicCartSig, SIG_SIZE);

                    s32 finalSize = appSize + sizeof header + header.cartSize;
                    data = malloc(finalSize);

                    if(data)
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

        free(app);
    }
    
    return data;
}

static void onConsoleExportNativeCommand(Console* console, const char* cartName)
{
    s32 size = 0;
    void* data = embedCart(console, &size);

    if(data)
    {
        fsGetFileData(onFileDownloaded, cartName, data, size, DEFAULT_CHMOD, console);
    }
    else
    {
        onFileDownloaded(FS_FILE_NOT_DOWNLOADED, console);
    }
}

#endif


static void onConsoleExportCommand(Console* console, const char* param)
{
    if(param)
    {
        bool errorOccured = false;

        if(strcmp(param, "native") == 0 || strcmp(param, "") == 0)
        {
#if defined(CAN_EXPORT_NATIVE)

#if defined(__TIC_WINDOWS__)
            const char* ext = ExeExt;
#else           
            const char* ext = NULL;
#endif

            const char* name = getExportName(console, ext);
            onConsoleExportNativeCommand(console, name);
#else

            printBack(console, "\ngame export isn't supported on this platform\n");
            commandDone(console);
#endif
        }
        else if(strncmp(param, "html", 4) == 0 && (param[4] == ' ' || param[4] == 0))
        {
            printBack(console, "\nhtml export isn't supported\n");
            commandDone(console);       
        }
        else if(strcmp(param, "sprites") == 0)
        {
            exportSprites(console);
        }
        else if(strcmp(param, "map") == 0)
        {
            exportMap(console);
        }
        else if(strcmp(param, "cover") == 0)
        {
            exportCover(console);
        }
        else if(strcmp(param, "sfx") == 0)
        {
            exportSfx(console, 0);
        }
        else if(memcmp(param, "sfx ", sizeof "sfx") == 0)
        {
            s32 sfx = atoi(param + sizeof "sfx");

            if(sfx >= 0 && sfx < SFX_COUNT)
                exportSfx(console, sfx);
            else errorOccured = true;
        }
        else if(strcmp(param, "music") == 0)
        {
            exportMusic(console, 0);
        }
        else if(memcmp(param, "music ", sizeof "music") == 0)
        {
            s32 track = atoi(param + sizeof "music");

            if(track >= 0 && track < MUSIC_TRACKS)
                exportMusic(console, track);
            else errorOccured = true;
        }
        else errorOccured = true;

        if(errorOccured)
        {
            printError(console, "\nunknown parameter: ");
            printError(console, param);
            commandDone(console);
        }
    }
    else
    {
        printError(console, "\nplease, specify parameter\n");
        commandDone(console);
    }
}

#endif

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

                if(size && fsSaveFile(console->fs, name, buffer, size, true))
                {
                    setCartName(console, name);
                    success = true;
                    studioRomSaved();
                }
            }

            free(buffer);
        }
    }
    else if (strlen(console->romName))
    {
        return saveCartName(console, console->romName);
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
    commandDone(console);
}

static void onConsoleSaveCommand(Console* console, const char* param)
{
    if(param && strlen(param) && 
        (fsExistsFile(console->fs, param) ||
            fsExistsFile(console->fs, getCartName(param))))
    {
        static const char* Rows[] =
        {
            "null",
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

    const tic_script_config* script_config = tic_core_script_config(console->tic);
    if (script_config->eval && console->codeLiveReload.active)
    {
        script_config->eval(console->tic, console->tic->cart.code.data);
    }
    tic_core_resume(console->tic);

    resumeRunMode();
}

static void onConsoleEvalCommand(Console* console, const char* param)
{
    commandDone(console);
}

static void onAddFile(const char* name, AddResult result, void* data)
{
    commandDone(console);
}

static void onConsoleAddCommand(Console* console, const char* param)
{
    fsAddFile(console->fs, onAddFile, console);
}

static void onConsoleGetCommand(Console* console, const char* param)
{
    commandDone(console);
}

static void onConsoleDelCommandConfirmed(Console* console, const char* param)
{
    commandDone(console);
}

static void onConsoleDelCommand(Console* console, const char* param)
{
    static const char* Rows[] =
    {
        "null",
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

    printTable(console, "null" \
                        "null" \
                        "null");

    static const struct{s32 addr; const char* info;} Layout[] =
    {
        {offsetof(tic_ram, tiles),                      "null"},
        {offsetof(tic_ram, sprites),                    "null"},
    };

    for(s32 i = 0; i < COUNT_OF(Layout)-1; i++)
        printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

    printTable(console, "null");

    printLine(console);
    commandDone(console);
}

static void onConsoleVRamCommand(Console* console, const char* param)
{
    printLine(console);

    printTable(console, "null" \
                        "null" \
                        "null");

    static const struct{s32 addr; const char* info;} Layout[] =
    {
        {offsetof(tic_ram, vram.screen),            "SCREEN"},
    };

    for(s32 i = 0; i < COUNT_OF(Layout)-1; i++)
        printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

    printTable(console, "null");

    printLine(console);
    commandDone(console);
}

static const struct
{
    const char* command;
    const char* alt;
    const char* info;
    void(*handler)(Console*, const char*);

} AvailableConsoleCommands[] =
{
    {"help",    NULL, "show this info",             onConsoleHelpCommand},
    {"exit",    "quit", "exit the application",     onConsoleExitCommand},
    {"run",     NULL, "run loaded cart",            onConsoleRunCommand},
    {"resume",  NULL, "resume run cart",            onConsoleResumeCommand},
    {"cls",     "clear", "clear screen",            onConsoleClsCommand},
    {"menu",    NULL, "show game menu",             onConsoleGameMenuCommand},
    {"export",  NULL, "export native game",         onConsoleExportCommand},
};

static bool predictFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
    char* buffer = (char*)data;

    if(strstr(name, buffer) == name)
    {
        strcpy(buffer, name);
        return false;
    }

    return true;
}

static void processConsoleTab(Console* console)
{
    char* input = console->inputBuffer;

    if(strlen(input))
    {
        char* param = strchr(input, ' ');

        if(param && strlen(++param))
        {
            fsEnumFiles(console->fs, predictFilename, param);
            console->inputPosition = strlen(input);
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
    while(*command == ' ')
        command++;

    char* end = (char*)command + strlen(command) - 1;

    while(*end == ' ' && end > command)
        *end-- = '\0';

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
            console->history = console->history->prev;

        fillInputBufferFromHistory(console);
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

static void onHttpVesrsionGet(const HttpGetData* data)
{
    Console* console = (Console*)data->calldata;

    switch(data->type)
    {
    case HttpGetDone:
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
                memset(console->colorBuffer + Offset, tic_color_2, STUDIO_TEXT_BUFFER_WIDTH);
            }
        }
        break;
    }
}

static void processKeyboard(Console* console)
{
    tic_mem* tic = console->tic;

    if(tic->ram.input.keyboard.data != 0)
    {
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
        else if(keyWasPressed(tic_key_return)) processConsoleCommand(console);
        else if(keyWasPressed(tic_key_backspace)) processConsoleBackspace(console);
        else if(keyWasPressed(tic_key_delete)) processConsoleDel(console);
        else if(keyWasPressed(tic_key_home)) processConsoleHome(console);
        else if(keyWasPressed(tic_key_end)) processConsoleEnd(console);
        else if(keyWasPressed(tic_key_tab)) processConsoleTab(console);

        if(keyWasPressed(tic_key_capslock) 
            || keyWasPressed(tic_key_ctrl) 
            || keyWasPressed(tic_key_shift) 
            || keyWasPressed(tic_key_alt));
        else if(anyKeyWasPressed())
        {
            scrollConsole(console);
            console->cursor.delay = CONSOLE_CURSOR_DELAY;
        }

        if(tic_api_key(tic, tic_key_ctrl) 
            && tic_api_key(tic, tic_key_k))
        {
            onConsoleClsCommand(console, NULL);
            return;
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
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

    processKeyboard(console);

    if(console->tickCounter == 0)
    {
        if(!console->embed.yes)
        {
#if defined(TIC_BUILD_WITH_LUA)
            loadDemo(console, LuaScript);
#elif defined(TIC_BUILD_WITH_JS)
            loadDemo(console, JavaScript);         

            printBack(console, "\n hello! type ");
            printFront(console, "run");
            printBack(console, " to start\n");


            commandDone(console);
        }
        else printBack(console, "\n Loading...");
    }

    tic_api_cls(tic, TIC_COLOR_BG);
    drawConsoleText(console);

    if(console->embed.yes)
    {
        if(console->tickCounter >= (u32)(console->skipStart ? 1 : TIC80_FRAMERATE))
        {
            if(!console->skipStart)
                console->showGameMenu = true;

            memcpy(&tic->cart, console->embed.file, sizeof(tic_cartridge));

            tic_api_reset(tic);

            setStudioMode(TIC_RUN_MODE);
            console->embed.yes = false;
            console->skipStart = false;
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
    }

    console->tickCounter++;

    if(console->startSurf)
    {
        console->startSurf = false;
        gotoSurf();
    }
}

static bool cmdLoadCart(Console* console, const char* name)
{
    bool done = false;

    s32 size = 0;
    void* data = fsReadFile(name, &size);

    if(data)
    {
        if(hasProjectExt(name))
        {
            if(tic_project_load(name, data, size, console->embed.file))
            {
                char cartName[TICNAME_MAX];
                fsFilename(name, cartName);
                setCartName(console, cartName);
                console->embed.yes = true;
                console->skipStart = true;
                done = true;
            }            
        }
        else if(tic_tool_has_ext(name, CART_EXT))
        {
            tic_mem* tic = console->tic;
            tic_cart_load(console->embed.file, data, size);

            char cartName[TICNAME_MAX];
            fsFilename(name, cartName);
        
            setCartName(console, cartName);

            console->embed.yes = true;
            done = true;
        }
        
        free(data);
    }

    return done;
}

static bool loadFileIntoBuffer(Console* console, char* buffer, const char* fileName)
{
    s32 size = 0;
    void* contents = fsReadFile(fileName, &size);

    if(contents)
    {
        memset(buffer, 0, TIC_CODE_SIZE);

        if(size > TIC_CODE_SIZE)
        {
            char messageBuffer[256];
            sprintf(messageBuffer, "\n code is larger than %i symbols\n", TIC_CODE_SIZE);

            printError(console, messageBuffer);
        }

        memcpy(buffer, contents, MIN(size, TIC_CODE_SIZE-1));
        free(contents);

        return true;
    }

    return false;
}

static void tryReloadCode(Console* console, char* codeBuffer)
{
    if(console->codeLiveReload.active)
    {
        const char* fileName = console->codeLiveReload.fileName;
        loadFileIntoBuffer(console, codeBuffer, fileName);
    }
}

static bool cmdInjectCode(Console* console, const char* param, const char* name)
{
    bool done = false;

    bool watch = strcmp(param, "-code-watch") == 0;
    if(watch || strcmp(param, "-code") == 0)
    {
        bool loaded = loadFileIntoBuffer(console, console->embed.file->code.data, name);

        if(loaded)
        {
            console->embed.yes = true;
            console->skipStart = true;
            done = true;

            if(watch)
            {
                console->codeLiveReload.active = true;
                strcpy(console->codeLiveReload.fileName, name);
            }
        }
    }

    return done;
}

static bool cmdInjectSprites(Console* console, const char* param, const char* name)
{
    bool done = false;

    if(strcmp(param, "-sprites") == 0)
    {
        s32 size = 0;
        void* sprites = fsReadFile(name, &size);

        if(sprites)
        {
            gif_image* image = gif_read_data(sprites, size);

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
                        u8 color = tic_tool_find_closest_color(console->embed.file->bank0.palette.scn.colors, c);

                        setSpritePixel(console->embed.file->bank0.tiles.data, x, y, color);
                    }

                gif_close(image);
            }

            free(sprites);

            console->embed.yes = true;
            console->skipStart = true;
            done = true;
        }

    }

    return done;
}

static bool cmdInjectMap(Console* console, const char* param, const char* name)
{
    bool done = false;

    if(strcmp(param, "-map") == 0)
    {
        s32 size = 0;
        void* map = fsReadFile(name, &size);

        if(map)
        {
            if(size <= sizeof(tic_map))
            {
                injectMap(console, map, size);

                console->embed.yes = true;
                console->skipStart = true;
                done = true;
            }

            free(map);
        }
    }

    return done;
}

static bool checkUIScale(Console* console, const char* param, const char* value)
{
    bool done = false;

    if(strcmp(param, "-uiscale") == 0)
    {
        s32 scale = atoi(value);

        if(scale > 0)
        {
            console->config->data.uiScale = scale;
            done = true;
        }
    }

    return done;
}

static bool checkCommand(Console* console, const char* argument)
{
    char* command = alloca(strlen(argument));
    strcpy(command, argument);

    for(char* ptr = command; *ptr; ptr++)
        if(isspace(*ptr))
        {
            *ptr = '\0';
            break;
        }

    for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
    {
        if(tic_strcasecmp(command, AvailableConsoleCommands[i].command) == 0 ||
            (AvailableConsoleCommands[i].alt &&
             tic_strcasecmp(command, AvailableConsoleCommands[i].alt) == 0))
        {
            processCommand(console, argument);
            return true;
        }
    }

    return false;
}

void initConsole(Console* console, tic_mem* tic, FileSystem* fs, Config* config, s32 argc, char **argv)
{
    if(!console->buffer) console->buffer = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->colorBuffer) console->colorBuffer = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->embed.file) console->embed.file = malloc(sizeof(tic_cartridge));

    *console = (Console)
    {
        .tic = tic,
        .config = config,
        .load = load,
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
        .codeLiveReload =
        {
            .active = false,
            .reload = tryReloadCode,
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
        .showGameMenu = false,
        .startSurf = false,
        .skipStart = true,
        .goFullscreen = false,
        .crtMonitor = false,
    };

    memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
    memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

    memset(console->codeLiveReload.fileName, 0, TICNAME_MAX);

    if(argc)
    {
        strcpy(console->appPath, argv[0]);

#if defined(__TIC_WINDOWS__)
        if(!strstr(console->appPath, ExeExt))
            strcat(console->appPath, ExeExt);
#endif
    }

    printFront(console, "\n " TIC_NAME_FULL "");
    printBack(console, " " TIC_VERSION_LABEL "\n");
    printBack(console, " " TIC_COPYRIGHT "\n");

    if(argc > 1)
    {
        // TODO: should we use default DB16 palette here???
        memcpy(console->embed.file->bank0.palette.scn.data, getConfig()->cart->bank0.palette.scn.data, sizeof(tic_palette));

        u32 argp = 1;

        // trying to load cart
        for (s32 i = 1; i < argc; i++)
        {
            if(strcmp(argv[i], ".") == 0 || cmdLoadCart(console, argv[i]))
            {
                argp |= 1 << i;
                break;
            }
        }

        // process '-key val' params
        for (s32 i = 1; i < argc-1; i++)
        {
            s32 mask = 0b11 << i;

            if(~argp & mask)
            {
                const char* first = argv[i];
                const char* second = argv[i + 1];

                if(cmdInjectCode(console, first, second)
                    || cmdInjectSprites(console, first, second)
                    || cmdInjectMap(console, first, second)
                    || checkUIScale(console, first, second))
                    argp |= mask;
            }
        }

        // proccess single params
        for (s32 i = 1; i < argc; i++)
        {
            if(~argp & 1 << i)
            {
                const char* arg = argv[i];

                if(strcmp(arg, "-nosound") == 0)
                    config->data.noSound = true;

                else if(strcmp(arg, "-fullscreen") == 0)
                    console->goFullscreen = true;

                else if(strcmp(arg, "-crt-monitor") == 0)
                    console->crtMonitor = true;

                else continue;

                argp |= 0b1 << i;
            }
        }

        for (s32 i = 1; i < argc; i++)
            if(~argp & 1 << i)
                if(!checkCommand(console, argv[i]))
                    printf("parameter or file not processed: %s\n", argv[i]);
    }

#if defined(CAN_EXPORT_NATIVE)

    if(!console->embed.yes)
    {
        s32 appSize = 0;
        u8* app = fsReadFile(console->appPath, &appSize);

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
                        size = appSize - (ptr - app);
                    }
                }
                else break;
            }

            free(app);
        }
    }

#endif

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
