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
#include "start.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/config.h"
#include "studio/project.h"
#include "ext/png.h"
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

#define CONSOLE_CURSOR_COLOR        tic_color_red
#define CONSOLE_INPUT_COLOR         tic_color_white
#define CONSOLE_BACK_TEXT_COLOR     tic_color_grey
#define CONSOLE_FRONT_TEXT_COLOR    tic_color_light_grey
#define CONSOLE_ERROR_TEXT_COLOR    tic_color_red
#define CONSOLE_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define CONSOLE_CURSOR_DELAY        (TIC80_FRAMERATE / 2)
#define CONSOLE_BUFFER_WIDTH        (STUDIO_TEXT_BUFFER_WIDTH)
#define CONSOLE_BUFFER_HEIGHT       (STUDIO_TEXT_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SCREENS      64
#define CONSOLE_BUFFER_SCREEN       (CONSOLE_BUFFER_WIDTH * CONSOLE_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SIZE         (CONSOLE_BUFFER_SCREEN * CONSOLE_BUFFER_SCREENS)
#define CONSOLE_BUFFER_ROWS         (CONSOLE_BUFFER_SIZE / CONSOLE_BUFFER_WIDTH)

#define HELP_CMD_LIST(macro)    \
    macro(version)              \
    macro(welcome)              \
    macro(spec)                 \
    macro(ram)                  \
    macro(vram)                 \
    macro(commands)             \
    macro(api)                  \
    macro(terms)                \
    macro(license)

#define IMPORT_CMD_LIST(macro)  \
    macro(tiles)                \
    macro(sprites)              \
    macro(map)                  \
    macro(code)                 \
    macro(screen)

#define EXPORT_CMD_LIST(macro)  \
    macro(win)                  \
    macro(linux)                \
    macro(rpi)                  \
    macro(mac)                  \
    macro(html)                 \
    macro(tiles)                \
    macro(sprites)              \
    macro(map)                  \
    macro(sfx)                  \
    macro(music)                \
    macro(screen)

static const char* PngExt = PNG_EXT;

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
    while(console->cursor.pos.y >= CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)
    {
        scrollBuffer(console->text);
        scrollBuffer((char*)console->color);

        console->cursor.pos.y--;
    }

    s32 minScroll = console->cursor.pos.y - CONSOLE_BUFFER_HEIGHT + 1;
    if(console->scroll.pos < minScroll)
        console->scroll.pos = minScroll;
}

static void setSymbol(Console* console, char sym, u8 color, s32 offset)
{
    console->text[offset] = sym;
    console->color[offset] = color;
}

static s32 cursorOffset(Console* console)
{
    return console->cursor.pos.x + console->cursor.pos.y * CONSOLE_BUFFER_WIDTH;
}

static tic_point cursorPos(Console* console)
{
    s32 offset = cursorOffset(console) + console->input.pos;
    return (tic_point) 
    {
        offset % CONSOLE_BUFFER_WIDTH,
        offset / CONSOLE_BUFFER_WIDTH
    };
}

static void nextLine(Console* console)
{
    console->cursor.pos.x = 0;
    console->cursor.pos.y++;
}

static bool iswrap(char sym)
{
    switch(sym)
    {
    case '|': return true;
    }

    return isspace(sym);
}

static void consolePrint(Console* console, const char* text, u8 color)
{
#ifndef BAREMETALPI
    printf("%s", text);
#endif

    console->cursor.pos = cursorPos(console);

    for(const char* ptr = text, *next = ptr; *ptr; ptr++)
    {
        char symbol = *ptr;

        scrollConsole(console);

        if (symbol == '\n')
            nextLine(console);
        else
        {
            if(!iswrap(symbol))
            {
                const char* cur = ptr;
                s32 len = CONSOLE_BUFFER_WIDTH;

                while(*cur && !iswrap(*cur++)) len--;

                if(len > 0 && len <= console->cursor.pos.x)
                    nextLine(console);
            }

            setSymbol(console, symbol, iswrap(symbol) ? tic_color_dark_grey : color, cursorOffset(console));

            console->cursor.pos.x++;

            if (console->cursor.pos.x >= CONSOLE_BUFFER_WIDTH)
                nextLine(console);
        }        
    }

    console->input.text = console->text + cursorOffset(console);
    console->input.pos = 0;
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

static void clearSelection(Console* console)
{
    ZEROMEM(console->select);
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

    clearSelection(console);
}

static void commandDone(Console* console)
{
    commandDoneLine(console, true);
}

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color, bool alt)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, true, 1, alt);
}

static void drawCursor(Console* console)
{
    if(!console->active)
        return;

    tic_point pos = cursorPos(console);
    pos.x *= STUDIO_TEXT_WIDTH;
    pos.y -= console->scroll.pos;
    pos.y *= STUDIO_TEXT_HEIGHT;

    u8 symbol = console->input.text[console->input.pos];

    bool inverse = console->cursor.delay || console->tickCounter % CONSOLE_CURSOR_BLINK_PERIOD < CONSOLE_CURSOR_BLINK_PERIOD / 2;

    if(inverse)
        tic_api_rect(console->tic, pos.x - 1, pos.y - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, CONSOLE_CURSOR_COLOR);

    drawChar(console->tic, symbol, pos.x, pos.y, inverse ? TIC_COLOR_BG : CONSOLE_INPUT_COLOR, false);
}

static void drawConsoleText(Console* console)
{
    tic_mem* tic = console->tic;
    const char* ptr = console->text + console->scroll.pos * CONSOLE_BUFFER_WIDTH;
    const u8* colorPointer = console->color + console->scroll.pos * CONSOLE_BUFFER_WIDTH;

    const char* end = ptr + CONSOLE_BUFFER_SCREEN;
    tic_point pos = {0};

    struct
    {
        const char* start;
        const char* end;
    } select = 
    {
        console->select.start,
        console->select.end
    };

    if(select.start > select.end)
        SWAP(select.start, select.end, const char*);

    while(ptr < end)
    {
        char symbol = *ptr++;
        u8 color = *colorPointer++;
        bool hasSymbol = symbol && symbol != ' ';
        bool drawSelection = ptr > select.start && ptr <= select.end;
        s32 x = pos.x * STUDIO_TEXT_WIDTH;
        s32 y = pos.y * STUDIO_TEXT_HEIGHT;

        if(drawSelection)
            tic_api_rect(tic, x, y - 1, STUDIO_TEXT_WIDTH, STUDIO_TEXT_HEIGHT, hasSymbol ? color : CONSOLE_INPUT_COLOR);

        if(hasSymbol)
            drawChar(console->tic, symbol, x, y, drawSelection ? TIC_COLOR_BG : color, false);

        if(++pos.x == CONSOLE_BUFFER_WIDTH)
        {
            pos.y++;
            pos.x = 0;
        }
    }
}

static void processConsoleHome(Console* console)
{
    console->input.pos = 0;
}

static void processConsoleEnd(Console* console)
{
    console->input.pos = strlen(console->input.text);
}

static s32 getInputOffset(Console* console)
{
    return (console->input.text - console->text) + console->input.pos;
}

static void processConsoleDel(Console* console)
{
    char* pos = console->input.text + console->input.pos;
    u8* color = console->color + getInputOffset(console);
    size_t size = strlen(pos);
    memmove(pos, pos + 1, size);
    memmove(color, color + 1, size);
}

static void processConsoleBackspace(Console* console)
{
    if(console->input.pos > 0)
    {
        console->input.pos--;

        processConsoleDel(console);
    }
}

static void onHelpCommand(Console* console, const char* param);

static void onExitCommand(Console* console, const char* param)
{
    exitStudio();
    commandDone(console);
}

static bool loadRom(tic_mem* tic, const void* data, s32 size)
{
    tic_cart_load(&tic->cart, data, size);
    tic_api_reset(tic);

    return true;
}

static bool onLoadSectionCommand(Console* console, const char* param)
{
    bool result = false;

    if(param)
    {
        static const char* Sections[] =
        {
            "code",

#define     SECTION_DEF(NAME, ...) #NAME,
            TIC_SYNC_LIST(SECTION_DEF)
#undef      SECTION_DEF
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
                        case 0: 
                            memcpy(&tic->cart.code, &cart->code, sizeof(tic_code)); 
                            break;

#define                 SECTION_DEF(NAME, _, INDEX) case (INDEX + 1): memcpy(&tic->cart.bank0.NAME, &cart->bank0.NAME, sizeof(tic_##NAME)); break;
                        TIC_SYNC_LIST(SECTION_DEF)
#undef                  SECTION_DEF
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
    if(console->rom.name != name)
        strcpy(console->rom.name, name);

    if(console->rom.path != path)
        strcpy(console->rom.path, path);
}

static void onLoadDemoCommandConfirmed(Console* console, const char* param)
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
    tic_fs_hashload(console->fs, hash, loadByHashDone, MOVE(loadByHashData));
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

static void onLoadCommandConfirmed(Console* console, const char* param)
{
    if(onLoadSectionCommand(console, param)) return;

    if(param)
    {
        tic_mem* tic = console->tic;

        const char* name = getCartName(param);

        if (tic_fs_ispubdir(console->fs))
        {
            LoadPublicCartData loadPublicCartData = { console, strdup(name) };
            tic_fs_enum(console->fs, compareFilename, fileFound, MOVE(loadPublicCartData));

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
                loadRom(tic, data, size);
                onCartLoaded(console, name);
                free(data);
            }
            else if(tic_tool_has_ext(param, PngExt) && tic_fs_exists(console->fs, param))
            {
                png_buffer buffer;
                buffer.data = tic_fs_load(console->fs, param, &buffer.size);
                tic_cartridge* cart = loadPngCart(buffer);

                if(cart)
                {
                    memcpy(&tic->cart, cart, sizeof(tic_cartridge));
                    tic_api_reset(tic);

                    onCartLoaded(console, param);
                    free(cart);
                }
                else printBack(console, "\ncart loading error");
               
                free(buffer.data);
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

                if(data && tic_project_load(name, data, size, &tic->cart))
                    onCartLoaded(console, name);
                else printBack(console, "\ncart loading error");

                free(data);
            }
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

static void onLoadDemoCommand(Console* console, const char* param)
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

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onLoadDemoCommandConfirmed);
    }
    else
    {
        onLoadDemoCommandConfirmed(console, param);
    }
}

static void onLoadCommand(Console* console, const char* param)
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

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onLoadCommandConfirmed);
    }
    else
    {
        onLoadCommandConfirmed(console, param);
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

static void onNewCommandConfirmed(Console* console, const char* param)
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

static void onNewCommand(Console* console, const char* param)
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

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onNewCommandConfirmed);
    }
    else
    {
        onNewCommandConfirmed(console, param);
    }
}

typedef struct
{
    const char* name;
    bool dir;
} FileItem;

typedef struct
{
    Console* console;
    FileItem* items;
    s32 count;
} PrintFileNameData;

static bool printFilename(const char* name, const char* info, s32 id, void* ctx, bool dir)
{
    PrintFileNameData* data = ctx;

    data->items = realloc(data->items, (data->count + 1) * sizeof *data->items);
    data->items[data->count++] = (FileItem){strdup(name), dir};

    return true;
}

static s32 casecmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) 
    {
        if (tolower((u8) *str1) != tolower((u8) *str2))
            break;

        ++str1;
        ++str2;
    }

    return (s32) ((u8) tolower(*str1) - (u8) tolower(*str2));
}

static inline s32 itemcmp(const void* a, const void* b)
{
    const FileItem* item1 = a;
    const FileItem* item2 = b;

    if(item1->dir != item2->dir)
        return item1->dir ? -1 : 1;

    return casecmp(item1->name, item2->name);
}

static void onDirDone(void* ctx)
{
    PrintFileNameData* data = ctx;
    Console* console = data->console;

    qsort(data->items, data->count, sizeof *data->items, itemcmp);

    for(const FileItem *item = data->items, *end = item + data->count; item < end; item++)
    {
        printLine(console);

        if(item->dir)
        {
            printBack(console, "[");
            printBack(console, item->name);
            printBack(console, "]");
        }
        else printFront(console, item->name);

        free((void*)item->name);
    }

    if (data->count == 0)
    {
        printBack(console, "\n\nuse ");
        printFront(console, "DEMO");
        printBack(console, " command to install demo carts");
    }
    else free(data->items);

    printLine(console);
    commandDone(console);

    free(ctx);
}

typedef struct
{
    Console* console;
    char* name;
} ChangeDirData;

static void onChangeDirectoryDone(bool dir, void* data)
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

static void onChangeDirectory(Console* console, const char* param)
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
            tic_fs_isdir_async(console->fs, param, onChangeDirectoryDone, MOVE(data));
            return;
        }
    }
    else printBack(console, "\ninvalid dir name");

    commandDone(console);
}

static void onMakeDirectory(Console* console, const char* param)
{
    if(param && strlen(param))
    {
        tic_fs_makedir(console->fs, param);

        char msg[TICNAME_MAX];
        sprintf(msg, "\ncreated [%s] folder :)", param);
        printBack(console, msg);
    }
    else printError(console, "\ninvalid dir name");

    commandDone(console);
}

static void onDirCommand(Console* console, const char* param)
{
    printLine(console);

    PrintFileNameData data = {console};
    tic_fs_enum(console->fs, printFilename, onDirDone, MOVE(data));
}

static void onFolderCommand(Console* console, const char* param)
{

    printBack(console, "\nStorage path:\n");
    printFront(console, tic_fs_pathroot(console->fs, ""));

    tic_fs_openfolder(console->fs);

    commandDone(console);
}

static void onClsCommand(Console* console, const char* param)
{
    memset(console->text, 0, CONSOLE_BUFFER_SIZE);
    memset(console->color, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

    ZEROMEM(console->scroll);
    ZEROMEM(console->cursor);
    ZEROMEM(console->input);

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

static void onInstallDemosCommand(Console* console, const char* param)
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

static void onGameMenuCommand(Console* console, const char* param)
{
    console->showGameMenu = false;
    showGameMenu();
    commandDone(console);
}

static void onSurfCommand(Console* console, const char* param)
{
    gotoSurf();
}

static void onConfigCommand(Console* console, const char* param)
{
    if(param == NULL)
    {
        onLoadCommand(console, CONFIG_TIC_PATH);
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
        onLoadDemoCommand(console, DefaultLuaTicPath);
    }

#   if defined(TIC_BUILD_WITH_MOON)
    else if(strcmp(param, "default moon") == 0 || strcmp(param, "default moonscript") == 0)
    {
        onLoadDemoCommand(console, DefaultMoonTicPath);
    }
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    else if(strcmp(param, "default fennel") == 0)
    {
        onLoadDemoCommand(console, DefaultFennelTicPath);
    }
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
    else if(strcmp(param, "default js") == 0)
    {
        onLoadDemoCommand(console, DefaultJSTicPath);
    }
#endif

#if defined(TIC_BUILD_WITH_WREN)
    else if(strcmp(param, "default wren") == 0)
    {
        onLoadDemoCommand(console, DefaultWrenTicPath);
    }
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    else if(strcmp(param, "default squirrel") == 0)
    {
        onLoadDemoCommand(console, DefaultSquirrelTicPath);
    }
#endif
    
    else
    {
        printError(console, "\nunknown parameter: ");
        printError(console, param);
    }

    commandDone(console);
}

static void onFileImported(Console* console, const char* filename, bool result)
{
    if(result)
    {
        printLine(console);
        printBack(console, filename);
        printBack(console, " imported :)");
    }
    else
    {
        char buf[TICNAME_MAX];
        sprintf(buf, "\nerror: %s not imported :(", filename);
        printError(console, buf);
    }

    commandDone(console);
}

static void onImportTilesBase(Console* console, const char* name, const void* buffer, s32 size, tic_tile* base)
{
    png_buffer png = {(u8*)buffer, size};
    bool error = false;

    png_img img = png_read(png);

    if(img.data)
    {
        if(img.width == TIC_SPRITESHEET_SIZE && img.height == TIC_SPRITESHEET_SIZE)
        {
            const tic_palette* pal = getBankPalette(false);
            tic_bank* bank = &console->tic->cart.bank0;
            s32 i = 0;
            for(const png_rgba *pix = img.pixels, *end = pix + (TIC_SPRITESHEET_SIZE * TIC_SPRITESHEET_SIZE); pix < end; pix++, i++)
                setSpritePixel(base, i % TIC_SPRITESHEET_SIZE, i / TIC_SPRITESHEET_SIZE, tic_nearest_color(pal->colors, (tic_rgb*)pix, TIC_PALETTE_SIZE));
        }
        else error = true;

        free(img.data);
    }
    else error = true;

    onFileImported(console, name, !error);
}

static void onImport_tiles(Console* console, const char* name, const void* buffer, s32 size)
{
    onImportTilesBase(console, name, buffer, size, getBankTiles()->data);
}

static void onImport_sprites(Console* console, const char* name, const void* buffer, s32 size)
{
    onImportTilesBase(console, name, buffer, size, getBankTiles()->data + TIC_BANK_SPRITES);
}

static void onImport_map(Console* console, const char* name, const void* buffer, s32 size)
{
    bool ok = name && buffer && size <= sizeof(tic_map);

    if(ok)
    {
        enum {Size = sizeof(tic_map)};

        memset(getBankMap(), 0, Size);
        memcpy(getBankMap(), buffer, MIN(size, Size));
    }
        
    onFileImported(console, name, ok);
}

static void onImport_code(Console* console, const char* name, const void* buffer, s32 size)
{
    tic_mem* tic = console->tic;
    bool error = false;

    if(name && buffer && size <= sizeof(tic_code))
    {
        enum {Size = sizeof(tic_code)};

        memset(tic->cart.code.data, 0, Size);
        memcpy(tic->cart.code.data, buffer, MIN(size, Size));

        studioRomLoaded();
    }
    else error = true;

    onFileImported(console, name, !error);
}

static void onImport_screen(Console* console, const char* name, const void* buffer, s32 size)
{
    png_buffer png = {(u8*)buffer, size};
    bool error = false;

    png_img img = png_read(png);

    if(img.data)
    {
        if(img.width == TIC80_WIDTH && img.height == TIC80_HEIGHT)
        {
            const tic_palette* pal = getBankPalette(false);
            tic_bank* bank = &console->tic->cart.bank0;
            s32 i = 0;
            for(const png_rgba *pix = img.pixels, *end = pix + (TIC80_WIDTH * TIC80_HEIGHT); pix < end; pix++)
                tic_tool_poke4(bank->screen.data, i++, tic_nearest_color(pal->colors, (tic_rgb*)pix, TIC_PALETTE_SIZE));
        }
        else error = true;

        free(img.data);
    }
    else error = true;

    onFileImported(console, name, !error);
}

static void printUsage(Console* console, const char* command);

static void onImportCommand(Console* console, const char* param)
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
            static const struct {const char* section; void (*handler)(Console*, const char*, const void*, s32);} Handlers[] = 
            {
#define         IMPORT_CMD_DEF(name) {#name, onImport_##name},
                IMPORT_CMD_LIST(IMPORT_CMD_DEF)
#undef          IMPORT_CMD_DEF
            };

            for(s32 i = 0; i < COUNT_OF(Handlers); i++)
            {
                if(strcmp(param, Handlers[i].section) == 0)
                {
                    Handlers[i].handler(console, filename, data, size);
                    error = false;
                    break;
                }                
            }
        }
        else
        {
            char msg[TICNAME_MAX];
            sprintf(msg, "\nerror, %s file not loaded", filename);
            printError(console, msg);
            commandDone(console);
            return;
        }
    }

    if(error)
    {
        printError(console, "\nerror: invalid parameters.");
        printUsage(console, "import");

        commandDone(console);
    }
}

static void onFileExported(Console* console, const char* filename, bool result)
{
    if(result)
    {
        printLine(console);
        printBack(console, filename);
        printBack(console, " exported :)");
    }
    else
    {
        char buf[TICNAME_MAX];
        sprintf(buf, "\nerror: %s not exported :(", filename);
        printError(console, buf);
    }

    commandDone(console);
}

static void exportSprites(Console* console, const char* filename, tic_tile* base)
{
    tic_mem* tic = console->tic;
    const tic_cartridge* cart = &tic->cart;

    png_img img = {TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE, malloc(TIC_SPRITESHEET_SIZE * TIC_SPRITESHEET_SIZE * sizeof(png_rgba))};
    
    const tic_palette* pal = getBankPalette(false);
    for(s32 i = 0; i < TIC_SPRITESHEET_SIZE * TIC_SPRITESHEET_SIZE; i++)
        img.values[i] = tic_rgba(&pal->colors[getSpritePixel(base, i % TIC_SPRITESHEET_SIZE, i / TIC_SPRITESHEET_SIZE)]);

    png_buffer png = png_write(img);

    onFileExported(console, filename, tic_fs_save(console->fs, filename, png.data, png.size, true));

    free(png.data);
    free(img.data);
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
            console->cursor.pos.x = 0;
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

            onFileExported(console, filename, (buf = embedCart(console, data->done.data, &size)) && fs_write(path, buf, size));

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
            bool errorOccured = !fs_write(zipPath, data->done.data, data->done.size);

            if(!errorOccured)
            {
                struct zip_t *zip = zip_open(zipPath, ZIP_DEFAULT_COMPRESSION_LEVEL, 'a');

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
                }
                else errorOccured = true;                
            }

            onFileExported(console, filename, !errorOccured);
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

static void onExport_win(Console* console, const char* param, const char* filename)
{
    exportNativeGame(console, getFilename(filename, ".exe"), param);
}

static void onExport_linux(Console* console, const char* param, const char* filename)
{
    exportNativeGame(console, filename, param);
}

static void onExport_rpi(Console* console, const char* param, const char* filename)
{
    exportNativeGame(console, filename, param);
}

static void onExport_mac(Console* console, const char* param, const char* filename)
{
    exportNativeGame(console, filename, param);
}

static void onExport_html(Console* console, const char* param, const char* filename)
{
    exportGame(console, getFilename(filename, ".zip"), param, onHtmlExportGet);
}

static void onExport_tiles(Console* console, const char* param, const char* filename)
{
    exportSprites(console, getFilename(filename, PngExt), getBankTiles()->data);
}

static void onExport_sprites(Console* console, const char* param, const char* filename)
{
    exportSprites(console, getFilename(filename, PngExt), getBankTiles()->data + TIC_BANK_SPRITES);
}

static void onExport_map(Console* console, const char* param, const char* path)
{
    enum{Size = sizeof(tic_map)};
    const char* filename = getFilename(path, ".map");

    void* buffer = malloc(Size);

    if(buffer)
    {
        memcpy(buffer, getBankMap()->data, Size);

        onFileExported(console, filename, tic_fs_save(console->fs, filename, buffer, Size, true));

        free(buffer);
    }
}

static void onExport_sfx(Console* console, const char* param, const char* name)
{
    const char* filename = getFilename(name, ".wav");
    const char* path = studioExportSfx(atoi(param + STRLEN("sfx")) % SFX_COUNT, filename);

    onFileExported(console, filename, path);
}

static void onExport_music(Console* console, const char* param, const char* name)
{
    const char* filename = getFilename(name, ".wav");
    const char* path = studioExportMusic(atoi(param + STRLEN("music")) % MUSIC_TRACKS, filename);

    onFileExported(console, filename, path);
}

static void onExport_screen(Console* console, const char* param, const char* name)
{
    const char* filename = getFilename(name, ".wav");

    tic_mem* tic = console->tic;
    const tic_cartridge* cart = &tic->cart;

    png_img img = {TIC80_WIDTH, TIC80_HEIGHT, malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(png_rgba))};
    const tic_palette* pal = getBankPalette(false);

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        img.values[i] = tic_rgba(&pal->colors[tic_tool_peek4(cart->bank0.screen.data, i)]);

    png_buffer png = png_write(img);

    onFileExported(console, filename, tic_fs_save(console->fs, filename, png.data, png.size, true));

    free(png.data);
    free(img.data);
}

static void onExportCommand(Console* console, const char* param)
{
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
        static const struct Handler {const char* param; void(*handler)(Console*, const char*, const char*);} Handlers[] = 
        {
#define     EXPORT_CMD_DEF(name) {#name, onExport_##name}, 
            EXPORT_CMD_LIST(EXPORT_CMD_DEF)
#undef      EXPORT_CMD_DEF
        };

        FOR(const struct Handler*, ptr, Handlers)
            if(strcmp(param, ptr->param) == 0)
            {
                ptr->handler(console, param, filename);
                return;
            }
    }

    {
        printError(console, "\nerror: invalid parameters.");
        printUsage(console, "export");
        commandDone(console);
    }
}

static void drawShadowText(tic_mem* tic, const char* text, s32 x, s32 y, tic_color color, s32 scale)
{
    tic_api_print(tic, text, x, y + scale, tic_color_black, false, scale, false);
    tic_api_print(tic, text, x, y, color, false, scale, false);
}

const char* readMetatag(const char* code, const char* tag, const char* comment);

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
                else if(tic_tool_has_ext(name, PngExt))
                {
                    png_buffer cover;

                    {
                        enum{CoverWidth = 256};

                        static const u8 Cartridge[] = 
                        {
                            #include "../build/assets/cart.png.dat"
                        };

                        png_buffer template = {(u8*)Cartridge, sizeof Cartridge};
                        png_img img = png_read(template);

                        // draw screen
                        {
                            enum{PaddingLeft = 8, PaddingTop = 8};

                            const tic_bank* bank = &tic->cart.bank0;
                            const tic_rgb* pal = bank->palette.scn.colors;
                            const u8* screen = bank->screen.data;
                            u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;

                            for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                                ptr[i / TIC80_WIDTH * CoverWidth + i % TIC80_WIDTH] = tic_rgba(pal + tic_tool_peek4(screen, i));
                        }

                        // draw title/author/desc
                        {
                            enum{Width = 224, Height = 40, PaddingTop = 162, PaddingLeft = 16, Scale = 2, Row = TIC_FONT_HEIGHT * 2 * Scale};

                            tic_api_cls(tic, tic_color_dark_grey);

                            const char* comment = tic_core_script_config(tic)->singleComment;

                            const char* title = tic_tool_metatag(tic->cart.code.data, "title", comment);
                            if(title)
                                drawShadowText(tic, title, 0, 0, tic_color_white, Scale);

                            const char* author = tic_tool_metatag(tic->cart.code.data, "author", comment);
                            if(author)
                            {
                                char buf[TICNAME_MAX];
                                snprintf(buf, sizeof buf, "by %s", author);
                                drawShadowText(tic, buf, 0, Row, tic_color_grey, Scale);
                            }

                            u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;
                            const u8* screen = tic->ram.vram.screen.data;
                            const tic_rgb* pal = getConfig()->cart->bank0.palette.scn.colors;

                            for(s32 y = 0; y < Height; y++)
                                for(s32 x = 0; x < Width; x++)
                                    ptr[CoverWidth * y + x] = tic_rgba(pal + tic_tool_peek4(screen, y * TIC80_WIDTH + x));
                        }

                        cover = png_write(img);

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

static void onSaveCommandConfirmed(Console* console, const char* param)
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

static void onSaveCommand(Console* console, const char* param)
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

        confirmCommand(console, Rows, COUNT_OF(Rows), param, onSaveCommandConfirmed);
    }
    else
    {
        onSaveCommandConfirmed(console, param);
    }
}

static void onRunCommand(Console* console, const char* param)
{
    commandDone(console);

    tic_api_reset(console->tic);

    setStudioMode(TIC_RUN_MODE);
}

static void onResumeCommand(Console* console, const char* param)
{
    commandDone(console);

    tic_core_resume(console->tic);
    resumeRunMode();
}

static void onEvalCommand(Console* console, const char* param)
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

static void onDelCommandConfirmed(Console* console, const char* param)
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

static void onDelCommand(Console* console, const char* param)
{
    static const char* Rows[] =
    {
        "", "",
        "DO YOU REALLY WANT",
        "TO DELETE FILE?",
    };

    confirmCommand(console, Rows, COUNT_OF(Rows), param, onDelCommandConfirmed);
}

#if defined(CAN_ADDGET_FILE)

static void onAddFile(Console* console, const char* name, const u8* buffer, s32 size)
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

static void onAddCommand(Console* console, const char* param)
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
    }, onAddFile, console);
}

static void onGetCommand(Console* console, const char* name)
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

static const char HelpUsage[] = "help [<text>"
#define HELP_CMD_DEF(name) "|" #name
    HELP_CMD_LIST(HELP_CMD_DEF)
#undef  HELP_CMD_DEF
    "]";

static struct Command
{
    const char* name;
    const char* alt;
    const char* help;
    const char* usage;
    void(*handler)(Console*, const char*);

} Commands[] =
{
    {
        "help",
        NULL,
        "show help info about commands/api/...", 
        HelpUsage,
        onHelpCommand
    },
    {
        "exit",
        "quit",
        "exit the application.", 
        NULL,
        onExitCommand
    },
    {
        "new",
        NULL,
        "creates a new `Hello World` cartridge.",
        "new [lua|moon|js|wren|fennel|squirrel]",
        onNewCommand
    },
    {
        "load",
        NULL,
        "load cartridge from the local filesystem (there's no need to type the .tic extension).\n"
        "you can also load just the section (sprites, map etc) from another cart.",
        "load <cart> [code"
#define SECTION_DEF(NAME, ...) "|" #NAME
        TIC_SYNC_LIST(SECTION_DEF)
#undef  SECTION_DEF
        "]",
        onLoadCommand},
    {
        "save",
        NULL,
        "save cartridge to the local filesystem, use "
#if defined(TIC_BUILD_WITH_LUA)
        PROJECT_LUA_EXT " "
#if defined(TIC_BUILD_WITH_MOON)
        PROJECT_MOON_EXT " "
#endif
#if defined(TIC_BUILD_WITH_SQUIRREL)
        PROJECT_FENNEL_EXT " "
#endif
#endif
#if defined(TIC_BUILD_WITH_FENNEL)
        PROJECT_JS_EXT " "
#endif
#if defined(TIC_BUILD_WITH_JS)
        PROJECT_WREN_EXT " "
#endif
#if defined(TIC_BUILD_WITH_WREN)
        PROJECT_SQUIRREL_EXT " "
#endif
        "cart extension to save it in text format.", 
        "save <cart>",
        onSaveCommand
    },

    {
        "run",
        NULL,
        "run current cart / project.", 
        NULL,
        onRunCommand
    },
    {
        "resume",
        NULL,
        "resume last run cart / project.", 
        NULL,
        onResumeCommand
    },
    {
        "eval",
        "=",
        "run code provided code.", 
        NULL,
        onEvalCommand
    },
    {
        "dir",
        "ls",
        "show list of local files.", 
        NULL,
        onDirCommand
    },
    {
        "cd",
        NULL,
        "change directory.", 
        "\ncd <path>\ncd /\ncd ..",
        onChangeDirectory
    },
    {
        "mkdir",
        NULL,
        "make a directory.", 
        "mkdir <name>",
        onMakeDirectory
    },
    {
        "folder",
        NULL,
        "open working directory in OS.", 
        NULL,
        onFolderCommand
    },

#if defined(CAN_ADDGET_FILE)
    {
        "add",
        NULL,
        "upload file to the browser local storage.", 
        NULL,
        onAddCommand
    },
    {
        "get",
        NULL,
        "download file from the browser local storage.", 
        "get <file>",
        onGetCommand
    },
#endif

    {
        "export",
        NULL,
        "export cart to HTML,\n"
        "native build (win linux rpi mac),\n"
        "export sprites/map/... as a .png image "
        "or export sfx and music to .wav files.", 
        "\nexport ["
#define EXPORT_CMD_DEF(name) #name "|"
        EXPORT_CMD_LIST(EXPORT_CMD_DEF)
#undef  EXPORT_CMD_DEF
        "...] <file>",
        onExportCommand
    },
    {
        "import",
        NULL,
        "import code/sprites/map/... from an external file.", 
        "import ["
#define IMPORT_CMD_DEF(name) #name "|"
        IMPORT_CMD_LIST(IMPORT_CMD_DEF)
#undef  IMPORT_CMD_DEF
        "...] <file>",
        onImportCommand
    },
    {
        "del",
        NULL,
        "delete from the filesystem.", 
        "del <file|folder>",
        onDelCommand
    },
    {
        "cls",
        "clear",
        "clear console screen.", 
        NULL,
        onClsCommand
    },
    {
        "demo",
        NULL,
        "install demo carts to the current directory.", 
        NULL,
        onInstallDemosCommand
    },
    {
        "config",
        NULL,
        "edit system configuration cartridge,\n"
        "use `reset` param to reset current configuration,\n"
        "use `default` to edit default cart template.", 
        "config [reset|default]",
        onConfigCommand
    },
    {
        "surf",
        NULL,
        "open carts browser.", 
        NULL,
        onSurfCommand
    },
    {
        "menu",
        NULL,
        "show game menu where you can setup keyboard/gamepad buttons mapping.", 
        NULL,
        onGameMenuCommand
    },
};

typedef struct Command Command;

static struct ApiItem {const char* name; const char* def; const char* help;} Api[] = 
{
#define TIC_CALLBACK_DEF(name, def, help) {name, def, help},
    TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef TIC_CALLBACK_DEF

#define TIC_API_DEF(name, def, help, ...) {#name, def, help},
    TIC_API_LIST(TIC_API_DEF)
#undef TIC_API_DEF
};

typedef struct ApiItem ApiItem;

typedef struct
{
    Console* console;
    char* name;
} PredictFilenameData;

static bool predictFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
    PredictFilenameData* predictFilenameData = data;
    Console* console = predictFilenameData->console;

    if(strstr(name, predictFilenameData->name) == name)
    {
        strcpy(predictFilenameData->name, name);
        memset(console->color + getInputOffset(console), CONSOLE_INPUT_COLOR, strlen(name));
        return false;
    }

    return true;
}

static void predictFilenameDone(void* data)
{
    PredictFilenameData* predictFilenameData = data;
    Console* console = predictFilenameData->console;

    console->input.pos = strlen(console->input.text);
    free(predictFilenameData);
}

static void insertInputText(Console* console, const char* text)
{
    s32 size = strlen(text);
    s32 offset = getInputOffset(console);

    if(size < CONSOLE_BUFFER_SIZE - offset)
    {
        char* pos = console->text + offset;
        u8* color = console->color + offset;

        {
            s32 len = strlen(pos);
            memmove(pos + size, pos, len);
            memmove(color + size, color, len);
        }

        memcpy(pos, text, size);
        memset(color, CONSOLE_INPUT_COLOR, size);

        console->input.pos += size;
    }

    clearSelection(console);
}

static void processConsoleTab(Console* console)
{
    char* input = console->input.text;

    if(strlen(input))
    {
        char* param = strchr(input, ' ');

        if(param && strlen(++param))
        {
            PredictFilenameData data = { console, param };
            tic_fs_enum(console->fs, predictFilename, predictFilenameDone, MOVE(data));
        }
        else
        {
            for(s32 i = 0; i < COUNT_OF(Commands); i++)
            {
                const char* command = Commands[i].name;

                if(strstr(command, input) == command)
                {
                    insertInputText(console, command + console->input.pos);
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

static void printUsage(Console* console, const char* command)
{
    FOR(const Command*, cmd, Commands)
    {
        if(strcmp(command, cmd->name) == 0)
        {
            consolePrint(console, "\n---=== COMMAND ===---\n", tic_color_green);
            printBack(console, cmd->help);

            if(cmd->usage)
            {
                printFront(console, "\n\nusage: ");
                printBack(console, cmd->usage);
            }

            printLine(console);
            break;
        }
    }
}

static void printApi(Console* console, const char* param)
{
    FOR(const ApiItem*, api, Api)
    {
        if(strcmp(param, api->name) == 0)
        {
            printLine(console);
            consolePrint(console, "---=== API ===---\n", tic_color_blue);
            consolePrint(console, api->def, tic_color_light_blue);
            printFront(console, "\n\n");
            printBack(console, api->help);
            printLine(console);
            break;
        }
    }
}

static void onHelp_api(Console* console)
{
    consolePrint(console, "\nAPI functions:\n", tic_color_blue);
    {
        char buf[TICNAME_MAX] = {[0] = 0};

        FOR(const ApiItem*, api, Api)
            strcat(buf, api->name), strcat(buf, " ");

        printBack(console, buf);
    }
}

static void onHelp_commands(Console* console)
{
    consolePrint(console, "\nConsole commands:\n", tic_color_green);
    {
        char buf[TICNAME_MAX] = {[0] = 0};

        FOR(const Command*, cmd, Commands)
            strcat(buf, cmd->name), strcat(buf, " ");

        printBack(console, buf);
    }
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
            nextLine(console);
        else
        {
            u8 color = 0;

            switch(symbol)
            {
            case '+':
            case '|':
            case '-':
                color = tic_color_dark_grey;
                break;
            default:
                color = CONSOLE_FRONT_TEXT_COLOR;
            }

            setSymbol(console, symbol, color, cursorOffset(console));

            console->cursor.pos.x++;

            if(console->cursor.pos.x >= CONSOLE_BUFFER_WIDTH)
                nextLine(console);
        }
    }
}

static void printRamInfo(Console* console, s32 addr, const char* name, s32 size)
{
    char buf[TICNAME_MAX];
    sprintf(buf, "\n| %05X | %-17s | %-5i |", addr, name, size);
    printTable(console, buf);
}

static void onHelp_ram(Console* console)
{
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
        {offsetof(tic_ram, music_state),                "MUSIC STATE"},
        {offsetof(tic_ram, stereo),                     "STEREO VOLUME"},
        {offsetof(tic_ram, persistent),                 "PERSISTENT MEMORY"},
        {offsetof(tic_ram, flags),                      "SPRITE FLAGS"},
        {offsetof(tic_ram, font),                       "SYSTEM FONT"},
        {offsetof(tic_ram, music_params),               "MUSIC PARAMETERS"},
        {offsetof(tic_ram, free),                       "... (free)"},
        {TIC_RAM_SIZE,                                  ""},
    };

    for(s32 i = 0; i < COUNT_OF(Layout)-1; i++)
        printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

    printTable(console, "\n+-------+-------------------+-------+");
}

static void onHelp_vram(Console* console)
{
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
}

static void onHelp_version(Console* console)
{
    consolePrint(console, "\n"TIC_VERSION, CONSOLE_BACK_TEXT_COLOR);
}

static void onHelp_spec(Console* console)
{
    printBack(console, "\n"
        "# Specification\n"
        "DISPLAY: 240x136 pixels, 16 colors palette.\n"
        "INPUT: 4 gamepads with 8 buttons / mouse / keyboard.\n"
        "SPRITES: 256 8x8 tiles and 256 8x8 sprites.\n"
        "MAP: 240x136 cells, 1920x1088 pixels.\n"
        "SOUND: 4 channels with configareble waveforms.\n"
        "CODE: 64KB of Lua, Moonscript, JS, Wren, Fennel or Squirrel.");
}

static void onHelp_welcome(Console* console)
{
    printBack(console, "\n"
        "TIC-80 is a fantasy computer for making, playing and sharing tiny games.\n\n"
        "There are built-in tools for development: code, sprites, maps, sound editors and the command line,"
        "which is enough to create a mini retro game.\n"
        "At the exit you will get a cartridge file, which can be stored and played on the website.\n\n"
        "Also, the game can be packed into a player that works on all popular platforms and distribute as you wish.\n"
        "To make a retro styled game the whole process of creation takes place under some technical limitations:"
        "240x136 pixels display, 16 color palette, 256 8x8 color sprites, 4 channel sound and etc.");
}

static void onHelp_terms(Console* console)
{
    printBack(console, "\n"
        "# Terms of Use\n"
        "- All cartridges posted on the " TIC_WEBSITE " website are the property of their authors.\n"
        "- Do not redistribute the cartridge without permission, directly from the author.\n"
        "- By uploading cartridges to the site, you grant Nesbox the right to freely use and distribute them."
        "All other rights by default remain with the author.\n"
        "- Do not post material that violates copyright, obscenity or any other laws.\n"
        "- Nesbox reserves the right to remove or filter any material without prior notice.\n\n"
        "# Privacy Policy\n"
        "We store only the user's email and password in encrypted form and will not transfer any personal"
        "information to third parties without explicit permission.");
}

static void onHelp_license(Console* console)
{
    printBack(console, "\n"
        "MIT License"
        "\n\n"
        "Copyright (c) 2017-" TIC_VERSION_YEAR " Vadim Grigoruk @nesbox // grigoruk@gmail.com"
        "\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy "
        "of this software and associated documentation files (the 'Software'), to deal "
        "in the Software without restriction, including without limitation the rights "
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
        "copies of the Software, and to permit persons to whom the Software is "
        "furnished to do so, subject to the following conditions: "
        "The above copyright notice and this permission notice shall be included in all "
        "copies or substantial portions of the Software."
        "\n\n"
        "THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
        "SOFTWARE.");
}

static void onHelpCommand(Console* console, const char* param)
{
    if(param)
    {
        printUsage(console, param);
        printApi(console, param);

        static const struct Handler {const char* cmd; void(*handler)(Console*);} Handlers[] = 
        {
#define     HELP_CMD_DEF(name) {#name, onHelp_##name},
            HELP_CMD_LIST(HELP_CMD_DEF)
#undef      HELP_CMD_DEF
        };

        FOR(const struct Handler*, ptr, Handlers)
            if(strcmp(ptr->cmd, param) == 0)
                ptr->handler(console);
    }
    else
    {
        printFront(console, "\n\nusage: ");
        printBack(console, HelpUsage);

        printBack(console, "\n\ntype ");
        printFront(console, "help commands");
        printBack(console, " to show commands");

        printBack(console, "\n\npress ");
        printFront(console, "ESC");
        printBack(console, " to enter UI mode\n");
    }

    commandDone(console);
}

static void processCommand(Console* console, const char* text)
{
    console->active = false;

    while(*text == ' ') text++;

    if (*text)
    {
        char* command = strdup(text);

        printf("%s", command);

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

        bool done = false;
        for(s32 i = 0; i < COUNT_OF(Commands); i++)
            if(casecmp(command, Commands[i].name) == 0 ||
                (Commands[i].alt && casecmp(command, Commands[i].alt) == 0))
            {
                if(Commands[i].handler)
                {
                    Commands[i].handler(console, param);
                    done = true;
                    break;
                }
            }

        if(!done)
        {
            printLine(console);
            printError(console, "unknown command:");
            printError(console, command);
            commandDone(console);
        }

        free(command);
    }
    else commandDone(console);
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

static void fillHistory(Console* console)
{
    if(console->history.size)
    {
        console->input.pos = 0;
        memset(console->input.text, '\0', strlen(console->input.text));

        const char* item = console->history.items[console->history.index];
        strcpy(console->input.text, item);
        memset(console->color + getInputOffset(console), CONSOLE_INPUT_COLOR, strlen(item));
        processConsoleEnd(console);
    }
}

static void onHistoryUp(Console* console)
{
    fillHistory(console);

    if(console->history.index > 0)
        console->history.index--;
}

static void onHistoryDown(Console* console)
{
    if(console->history.index < console->history.size - 1)
    {
        console->history.index++;
        fillHistory(console);
    }
    else
    {
        memset(console->input.text, '\0', strlen(console->input.text));
        processConsoleEnd(console);
    }
}

static void appendHistory(Console* console, const char* value)
{
    console->history.index = console->history.size++;
    console->history.items = realloc(console->history.items, sizeof(char*) * console->history.size);
    console->history.items[console->history.index] = strdup(value);
}

static void processConsoleCommand(Console* console)
{
    size_t commandSize = strlen(console->input.text);

    if(commandSize)
    {
        appendHistory(console, console->input.text);
        processCommand(console, console->input.text);
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
        console->scroll.pos = MIN(CLAMP(val, 0, console->cursor.pos.y), CONSOLE_BUFFER_ROWS - CONSOLE_BUFFER_HEIGHT);
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

                strcpy(console->text + Offset, msg);
                memset(console->color + Offset, tic_color_red, strlen(msg));
            }
        }
        break;
    default:
        break;
    }
}

static char* getSelectionText(Console* console)
{
    const char* start = console->select.start;
    const char* end = console->select.end;

    if (start > end)
        SWAP(start, end, const char*);

    s32 size = end - start;
    if (size)
    {
        size += size / CONSOLE_BUFFER_WIDTH + 1;
        char* clipboard = malloc(size);
        memset(clipboard, 0, size);
        char* dst = clipboard;

        s32 index = (start - console->text) % CONSOLE_BUFFER_WIDTH;

        for (const char* ptr = start; ptr < end; ptr++, index++)
        {
            if (index && (index % CONSOLE_BUFFER_WIDTH) == 0)
                *dst++ = '\n';

            if (*ptr)
                *dst++ = *ptr;
        }

        return clipboard;
    }

    return NULL;
}

static void copyToClipboard(Console* console)
{
    char* text = getSelectionText(console);

    if (text)
    {
        tic_sys_clipboard_set(text);
        free(text);
        clearSelection(console);
    }
}

static void copyFromClipboard(Console* console)
{
    if(tic_sys_clipboard_has())
    {
        const char* clipboard = tic_sys_clipboard_get();

        if(clipboard)
        {
            char* text = strdup(clipboard);

            char* dst = text;
            for(const char* src = clipboard; *src; src++)
                if(isprint(*src))
                    *dst++ = *src;

            insertInputText(console, text);
            free(text);

            tic_sys_clipboard_free(clipboard);
        }
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

    if(checkMousePos(&rect))
        setCursor(tic_cursor_ibeam);

#if defined(__TIC_ANDROID__)

    if(checkMouseDown(&rect, tic_mouse_left))
    {
        setCursor(tic_cursor_hand);

        if(console->scroll.active)
        {
            setScroll(console, (console->scroll.start - tic_api_mouse(tic).y) / STUDIO_TEXT_HEIGHT);
        }
        else
        {
            console->scroll.active = true;
            console->scroll.start = tic_api_mouse(tic).y + console->scroll.pos * STUDIO_TEXT_HEIGHT;
        }            
    }
    else console->scroll.active = false;

#else

    if(checkMouseDown(&rect, tic_mouse_left))
    {
        tic_point m = tic_api_mouse(tic);

        console->select.end = console->text 
            + m.x / STUDIO_TEXT_WIDTH 
            + (m.y / STUDIO_TEXT_HEIGHT + console->scroll.pos) * CONSOLE_BUFFER_WIDTH;

        if(!console->select.active)
        {
            console->select.active = true;
            console->select.start = console->select.end;
        }
    }
    else console->select.active = false;

#endif

    if(checkMouseClick(&rect, tic_mouse_middle))
    {
        char* text = getSelectionText(console);

        if (text)
        {
            insertInputText(console, text);
            tic_sys_clipboard_set(text);
            free(text);
        }
        else
            copyFromClipboard(console);
    }
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
        switch(getClipboardEvent())
        {
        case TIC_CLIPBOARD_COPY: copyToClipboard(console); break;
        case TIC_CLIPBOARD_PASTE: copyFromClipboard(console); break;
        default: break;
        }

        console->cursor.delay = CONSOLE_CURSOR_DELAY;

        if(keyWasPressed(tic_key_up)) onHistoryUp(console);
        else if(keyWasPressed(tic_key_down)) onHistoryDown(console);
        else if(keyWasPressed(tic_key_left))
        {
            if(console->input.pos > 0)
                console->input.pos--;
        }
        else if(keyWasPressed(tic_key_right))
        {
            console->input.pos++;
            size_t len = strlen(console->input.text);
            if(console->input.pos > len)
                console->input.pos = len;
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
            onClsCommand(console, NULL);
            return;
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        insertInputText(console, (char[]){sym, '\0'});
        scrollConsole(console);

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

    if (getStudioMode() != TIC_CONSOLE_MODE) return;

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

        drawCursor(console);

        if(console->active && console->args.cmd)
            processCommands(console);
    }

    console->tickCounter++;
}

static inline bool isslash(char c)
{
    return c == '/' || c == '\\';
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
            cartName = ptr + isslash(*ptr);
        }

        setCartName(console, cartName, path);

        if(hasProjectExt(cartName))
        {
            if(tic_project_load(cartName, data, size, console->embed.file))
                done = console->embed.yes = true;
        }
        else if(tic_tool_has_ext(cartName, PngExt))
        {
            tic_cartridge* cart = loadPngCart((png_buffer){data, size});

            if(cart)
            {
                memcpy(console->embed.file, cart, sizeof(tic_cartridge));
                free(cart);
                done = console->embed.yes = true;
            }
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

static s32 cmdcmp(const void* a, const void* b)
{
    return strcmp(((const Command*)a)->name, ((const Command*)b)->name);
}

static s32 apicmp(const void* a, const void* b)
{
    return strcmp(((const ApiItem*)a)->name, ((const ApiItem*)b)->name);
}

void initConsole(Console* console, tic_mem* tic, tic_fs* fs, tic_net* net, Config* config, StartArgs args)
{
    if(!console->text)          console->text = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->color)         console->color = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->embed.file)    console->embed.file = malloc(sizeof(tic_cartridge));

    *console = (Console)
    {
        .tic = tic,
        .config = config,
        .loadByHash = loadByHash,
        .load = onLoadCommandConfirmed,
        .updateProject = updateProject,
        .error = error,
        .trace = trace,
        .tick = tick,
        .save = saveCart,
        .done = commandDone,
        .cursor = {.pos.x = 1, .pos.y = 3, .delay = 0},
        .embed =
        {
            .yes = false,
            .file = console->embed.file,
        },
        .input = console->text,
        .tickCounter = 0,
        .active = false,
        .text = console->text,
        .color = console->color,
        .fs = fs,
        .net = net,
        .showGameMenu = false,
        .args = args,
    };

    qsort(Commands, COUNT_OF(Commands), sizeof Commands[0], cmdcmp);
    qsort(Api, COUNT_OF(Api), sizeof Api[0], apicmp);

    memset(console->text, 0, CONSOLE_BUFFER_SIZE);
    memset(console->color, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

    {
        Start* start = getStartScreen();

        memcpy(console->text, start->text, STUDIO_TEXT_BUFFER_SIZE);
        memcpy(console->color, start->color, STUDIO_TEXT_BUFFER_SIZE);

        printLine(console);
        for(const char* ptr = console->text, *end = ptr + STUDIO_TEXT_BUFFER_SIZE; 
            ptr < end; ptr += CONSOLE_BUFFER_WIDTH)
            if(*ptr)
                puts(ptr);
    }

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
    free(console->text);
    free(console->color);
    free(console->embed.file);

    if(console->history.items)
    {
        for(char **ptr = console->history.items, **end = ptr + console->history.size; ptr < end; ptr++)
            free(*ptr);

        free(console->history.items);
    }

    free(console);
}
