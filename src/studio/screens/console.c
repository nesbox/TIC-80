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
#include "tools.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/config.h"
#include "ext/png.h"
#include "zip.h"
#include "studio/demos.h"

#if defined(TIC80_PRO)
#include "studio/project.h"
#else
#include "cart.h"
#endif

#include <ctype.h>
#include <string.h>

#if !defined(__TIC_MACOSX__)
#include <malloc.h>
#endif

#if defined (TIC_BUILD_WITH_LUA)
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
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
#define CONSOLE_LINK_TEXT_COLOR     tic_color_blue
#define CONSOLE_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define CONSOLE_CURSOR_DELAY        (TIC80_FRAMERATE / 2)
#define CONSOLE_BUFFER_WIDTH        (STUDIO_TEXT_BUFFER_WIDTH)
#define CONSOLE_BUFFER_HEIGHT       (STUDIO_TEXT_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SCREENS      64
#define CONSOLE_BUFFER_SCREEN       (CONSOLE_BUFFER_WIDTH * CONSOLE_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SIZE         (CONSOLE_BUFFER_SCREEN * CONSOLE_BUFFER_SCREENS)
#define CONSOLE_BUFFER_ROWS         (CONSOLE_BUFFER_SIZE / CONSOLE_BUFFER_WIDTH)
#define DEFAULT_CHMOD               0755

#define HELP_CMD_LIST(macro)    \
    macro(version)              \
    macro(welcome)              \
    macro(spec)                 \
    macro(ram)                  \
    macro(vram)                 \
    macro(commands)             \
    macro(api)                  \
    macro(keys)                 \
    macro(buttons)                 \
    macro(startup)              \
    macro(terms)                \
    macro(license)

#define IMPORT_CMD_LIST(macro)  \
    macro(binary)               \
    macro(tiles)                \
    macro(sprites)              \
    macro(map)                  \
    macro(code)                 \
    macro(screen)

#define IMPORT_KEYS_LIST(macro) \
    macro(bank)                 \
    macro(x)                    \
    macro(y)                    \
    macro(w)                    \
    macro(h)                    \
    macro(vbank)

#define EXPORT_CMD_LIST(macro)  \
    macro(win)                  \
    macro(winxp)                \
    macro(linux)                \
    macro(rpi)                  \
    macro(mac)                  \
    macro(html)                 \
    macro(binary)               \
    macro(tiles)                \
    macro(sprites)              \
    macro(map)                  \
    macro(mapimg)               \
    macro(sfx)                  \
    macro(music)                \
    macro(screen)               \
    macro(help)

#if defined(TIC80_PRO)
#   define ALONE_KEY(macro) macro(alone)
#else
#   define ALONE_KEY(macro)
#endif

#define EXPORT_KEYS_LIST(macro) \
    macro(bank)                 \
    macro(vbank)                \
    macro(id)                   \
    ALONE_KEY(macro)

static const char* WelcomeText =
    "TIC-80 is a fantasy computer for making, playing and sharing tiny games.\n\n"
    "It has built-in tools for development: code, sprites, maps, sound editors and the command line, "
    "which is enough to create a mini retro game.\n"
    "In the end, you will get a cartridge file, which can be stored and played on the website.\n\n"
    "Also, the game can be packed into a player that works on all popular platforms and distributed as you wish.\n"
    "To make a retro-style game, the whole creation process takes place under some technical limitations: "
    "240x136 pixels display, 16 color palette, 256 8x8 color sprites, 4 channel sound, etc.";

static const struct SpecRow {const char* section; const char* info;} SpecText1[] =
{
    {"DISPLAY", "240x136 pixels, 16 colors palette."},
    {"INPUT",   "4 gamepads with 8 buttons / mouse / keyboard."},
    {"SPRITES", "256 8x8 tiles and 256 8x8 sprites."},
    {"MAP",     "240x136 cells, 1920x1088 pixels."},
    {"SOUND",   "4 channels with configurable waveforms."},
    {"CODE",    "64KB of $LANG_NAMES$.",
    },
};

static const char* TermsText =
    "## Terms of Use\n"
    "- All cartridges posted on the " TIC_WEBSITE " website are the property of their authors.\n"
    "- Do not redistribute the cartridge without permission, directly from the author.\n"
    "- By uploading cartridges to the site, you grant Nesbox the right to freely use and distribute them. "
    "All other rights by default remain with the author.\n"
    "- Do not post material that violates copyright, obscenity or any other laws.\n"
    "- Nesbox reserves the right to remove or filter any material without prior notice.\n\n"
    "## Privacy Policy\n"
    "We store only the user's email and password in encrypted form and will not transfer any personal "
    "information to third parties without explicit permission.";

static const char* LicenseText =
    "## MIT License\n"
    "\n"
    "Copyright (c) 2017-" TIC_VERSION_YEAR " Vadim Grigoruk @nesbox // grigoruk@gmail.com\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person obtaining a copy "
    "of this software and associated documentation files (the 'Software'), to deal "
    "in the Software without restriction, including without limitation the rights "
    "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
    "copies of the Software, and to permit persons to whom the Software is "
    "furnished to do so, subject to the following conditions: "
    "The above copyright notice and this permission notice shall be included in all "
    "copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
    "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
    "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
    "SOFTWARE.";

static const struct StartupOption {const char* name; const char* help;} StartupOptions[] =
{
#define CMD_PARAMS_DEF(name, ctype, type, post, help) {#name post, help},
    CMD_PARAMS_LIST(CMD_PARAMS_DEF)
#undef CMD_PARAMS_DEF
};

struct CommandDesc
{
    char* command;

    struct Param
    {
        char* key;
        char* val;
    }* params;

    s32 count;

    char* src;
};

static const char* PngExt = PNG_EXT;

#if defined(__EMSCRIPTEN__)
#define CAN_ADDGET_FILE 1
#endif


// You must free the result if result is non-NULL. TODO: find a better place for this function?
char *str_replace(const char *orig, char *rep, char *with) {
    char *result; // the return string
    const char *ins;    // the next insert point
    char *tmp;    // varies
    s32 len_rep;  // length of rep (the string to remove)
    s32 len_with; // length of with (the string to replace rep with)
    s32 len_front; // distance between rep and end of last rep
    s32 count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

static char* replaceHelpTokens(const char* text)
{
    char langnames[10240] = {0};
    char langextensions[10240] = {0};
    char langnamespipe[10240] = {0};

    FOR_EACH_LANG(ln)
        bool isLast = *(conf+1) == NULL;
        bool isSecondToLast = *(conf+2) == NULL;

        strcat(langnames, ln->name);
        if (!isLast)
            strcat(langnames, isSecondToLast ? " or " : ", ");

        strcat(langextensions, ln->fileExtension);
        strcat(langextensions, " ");

        strcat(langnamespipe, ln->name);
        if (!isLast)
            strcat(langnamespipe, "|");
    FOR_EACH_LANG_END

    char* replaced1 = str_replace(text, "$LANG_NAMES$", langnames);
    char* replaced2 = str_replace(replaced1, "$LANG_EXTENSIONS$", langextensions);
    char* replaced3 = str_replace(replaced2, "$LANG_NAMES_PIPE$", langnamespipe);
    free(replaced2);
    free(replaced1);
    return replaced3;
}


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

    size_t inputLines = (console->cursor.pos.x + console->input.pos) / CONSOLE_BUFFER_WIDTH;
    s32 minScroll = console->cursor.pos.y + inputLines - CONSOLE_BUFFER_HEIGHT + 1;
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

static void consolePrintOffset(Console* console, const char* text, u8 color, s32 wrapLineOffset)
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
                {
                    nextLine(console);
                    console->cursor.pos.x = wrapLineOffset;
                }
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

static void consolePrint(Console* console, const char* text, u8 color)
{
    consolePrintOffset(console, text, color, 0);
}

static void printBack(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_BACK_TEXT_COLOR);
}

static void printFront(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_FRONT_TEXT_COLOR);
}

static void printLink(Console* console, const char* text)
{
    consolePrint(console, text, CONSOLE_LINK_TEXT_COLOR);
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
    if(!console->args.cli)
    {
        if(newLine)
            printLine(console);

        char dir[TICNAME_MAX];
        tic_fs_dir(console->fs, dir);
        if(strlen(dir))
            printBack(console, dir);

        printFront(console, ">");
    }

    console->active = true;

    clearSelection(console);

    FREE(console->desc->src);
    FREE(console->desc->command);
    FREE(console->desc->params);

    memset(console->desc, 0, sizeof(CommandDesc));
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

static void deleteText(Console* console, s32 start, s32 end)
{
    s32 offset = console->input.text - console->text;
    s32 size = CONSOLE_BUFFER_SIZE - offset - end;
    memmove(console->input.text + start, console->input.text + end, size);

    u8* color = console->color + offset;
    memmove(color + start, color + end, size);
}

static void processConsoleDel(Console* console)
{
    deleteText(console, console->input.pos, console->input.pos + 1);
}

static void processConsoleBackspace(Console* console)
{
    if(console->input.pos > 0)
    {
        console->input.pos--;

        processConsoleDel(console);
    }
}

static void onHelpCommand(Console* console);

static void onExitCommand(Console* console)
{
    exitStudio(console->studio);
    commandDone(console);
}

static void onEditCommand(Console* console)
{
    gotoCode(console->studio);
    commandDone(console);
}

static void loadCartSection(Console* console, const tic_cartridge* cart, const char* section)
{
    tic_mem* tic = console->tic;

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

static char* getDemoCartPath(char* path, tic_script_config* script)
{
    strcpy(path, TIC_LOCAL_VERSION "default_");
    strcat(path, script->name);
    strcat(path, ".tic");

    return path;
}

static void* getDemoCart(Console* console, tic_script_config* script, s32* size)
{
    char path[1024];
    getDemoCartPath(path, script);

    {
        void* data = tic_fs_loadroot(console->fs, path, size);

        if(data && *size)
            return data;
    }
    tic_script_config_extra* ex = getConfigExtra(script);

    const u8* demo = ex->demoRom;
    s32 romSize = ex->demoRomSize;

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

static void onLoadDemoCommandConfirmed(Console* console, tic_script_config* script)
{
    void* data = NULL;
    s32 size = 0;

    {
        char path[1024];
        getDemoCartPath(path, script);
        const char* name = getCartName(path);
        setCartName(console, name, tic_fs_path(console->fs, name));
    }

    data = getDemoCart(console, script, &size);
    tic_cart_load(&console->tic->cart, data, size);
    tic_api_reset(console->tic);

    studioRomLoaded(console->studio);

    printBack(console, "\ncart ");
    printFront(console, console->rom.name);
    printBack(console, " loaded!\n");

    free(data);
}

static void onCartLoaded(Console* console, const char* name, const char* section)
{
    tic_api_reset(console->tic);

    if(!section)
        setCartName(console, name, tic_fs_path(console->fs, name));

    studioRomLoaded(console->studio);

    printBack(console, "\ncart ");
    printFront(console, console->rom.name);
    printBack(console, " loaded!\nuse ");
    printFront(console, "RUN");
    printBack(console, " command to run it\n");

}

static inline tic_cartridge* newCart()
{
    return malloc(sizeof(tic_cartridge));
}

static void updateProject(Console* console)
{
    tic_mem* tic = console->tic;
    const char* path = console->rom.path;

    if(*path)
    {
        s32 size = 0;
        void* data = fs_read(path, &size);

        if(data) SCOPE(free(data))
        {
#if defined(TIC80_PRO)
            if(tic_project_ext(path))
                tic_project_load(console->rom.name, data, size, &tic->cart);
            else
#endif
                tic_cart_load(&tic->cart, data, size);

            studioRomLoaded(console->studio);
        }
    }
}

typedef struct
{
    Console* console;
    char* name;
    char* section;
    fs_done_callback callback;
    void* calldata;
} LoadByHashData;

static void loadByHashDone(const u8* buffer, s32 size, void* data)
{
    LoadByHashData* loadByHashData = data;
    Console* console = loadByHashData->console;

    tic_cartridge* cart = newCart();

    SCOPE(free(cart))
    {
        tic_cart_load(cart, buffer, size);
        loadCartSection(console, cart, loadByHashData->section);
        onCartLoaded(console, loadByHashData->name, loadByHashData->section);
    }

    if (loadByHashData->callback)
        loadByHashData->callback(loadByHashData->calldata);

    FREE(loadByHashData->name);
    FREE(loadByHashData->section);
    FREE(loadByHashData);

    commandDone(console);
}

static void loadByHash(Console* console, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data)
{
    console->active = false;

    LoadByHashData loadByHashData = { console, strdup(name), section ? strdup(section) : NULL, callback, data};
    tic_fs_hashload(console->fs, name, hash, loadByHashDone, MOVE(loadByHashData));
}

typedef struct
{
    Console* console;
    char* name;
    char* hash;
    char* section;
} LoadPublicCartData;

static bool compareFilename(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    LoadPublicCartData* loadPublicCartData = data;
    Console* console = loadPublicCartData->console;

    if (strcmp(name, loadPublicCartData->name) == 0 && hash && strlen(hash))
    {
        loadPublicCartData->hash = strdup(hash);
        return false;
    }

    return true;
}

static void fileFound(void* data)
{
    LoadPublicCartData* loadPublicCartData = data;
    Console* console = loadPublicCartData->console;

    if (loadPublicCartData->hash)
        loadByHash(console, loadPublicCartData->name, loadPublicCartData->hash, loadPublicCartData->section, NULL, NULL);
    else
    {
        char msg[TICNAME_MAX];
        sprintf(msg, "\nerror: `%s` file not loaded", loadPublicCartData->name);
        printError(console, msg);
        commandDone(console);
    }

    FREE(loadPublicCartData->name);
    FREE(loadPublicCartData->hash);
    FREE(loadPublicCartData->section);
    FREE(loadPublicCartData);
}

static bool printUsage(Console* console, const char* command);

static void onLoadCommandConfirmed(Console* console)
{
    if(console->desc->count > 0)
    {
        tic_mem* tic = console->tic;

        const char* param = console->desc->params->key;
        const char* name = getCartName(param);
        const char* section = console->desc->count > 1 ? console->desc->params[1].key : NULL;

        if(section)
        {
            static const char* Sections[] =
            {
                "code",
#define         SECTION_DEF(name, ...) #name,
                TIC_SYNC_LIST(SECTION_DEF)
#undef          SECTION_DEF
            };

            bool found = false;
            for(const char** it = Sections, **end = it + COUNT_OF(Sections); it != end; ++it)
                if(strcmp(*it, section) == 0)
                {
                    found = true;
                    break;
                }

            if(!found)
            {
                printError(console, "\nunknown section: ");
                printError(console, section);
                printLine(console);
                printUsage(console, console->desc->command);
                commandDone(console);
                return;
            }
        }

        if (tic_fs_ispubdir(console->fs))
        {
            LoadPublicCartData loadPublicCartData = { console, strdup(name), NULL, section ? strdup(section) : NULL };
            tic_fs_enum(console->fs, compareFilename, fileFound, MOVE(loadPublicCartData));

            return;
        }
        else
        {
            s32 size = 0;
            void* data = strcmp(name, CONFIG_TIC_PATH) == 0
                ? tic_fs_loadroot(console->fs, name, &size)
                : tic_fs_load(console->fs, name, &size);

            if(data) SCOPE(free(data))
            {
                tic_cartridge* cart = newCart();

                SCOPE(free(cart))
                {
                    tic_cart_load(cart, data, size);
                    loadCartSection(console, cart, section);
                    onCartLoaded(console, name, section);
                }
            }
            else if(tic_tool_has_ext(param, PngExt) && tic_fs_exists(console->fs, param))
            {
                png_buffer buffer;
                buffer.data = tic_fs_load(console->fs, param, &buffer.size);

                SCOPE(free(buffer.data))
                {
                    tic_cartridge* cart = loadPngCart(buffer);

                    if(cart) SCOPE(free(cart))
                    {
                        loadCartSection(console, cart, section);
                        onCartLoaded(console, param, section);
                    }
                    else printError(console, "\npng cart loading error");
                }
            }
            else
            {
                const char* name = param;

#if defined(TIC80_PRO)
                if(tic_project_ext(name))
                {
                    void* data = tic_fs_load(console->fs, name, &size);

                    if(data) SCOPE(free(data))
                    {
                        tic_cartridge* cart = newCart();

                        SCOPE(free(cart))
                        {
                            tic_project_load(name, data, size, cart);
                            loadCartSection(console, cart, section);
                            onCartLoaded(console, name, section);
                        }
                    }
                    else printError(console, "\nproject loading error");

                }
                else printError(console, "\nfile not found");
#else
                if(tic_project_ext(name)) {
                    printError(console, "\nproject loading error");
                    printFront(console, "\nThis version only supports binary .png or .tic cartridges.");
                    printLine(console);
                    printFront(console, "\nTIC-80 ");
                    consolePrint(console,"PRO",tic_color_light_blue);
                    printFront(console, " is needed for text files.");
                    printLine(console);
                    printFront(console, "\nLearn more:\n");
                    printLink(console, "https://tic80.com/pro");
                } else {
                    printError(console, "\ncart loading error");
                }

#endif
            }
        }
    }
    else
        printUsage(console, console->desc->command);

    commandDone(console);
}

typedef void(*ConsoleConfirmCallback)(Console* console);

typedef struct
{
    Console* console;
    ConsoleConfirmCallback callback;
} CommandConfirmData;

static void onConfirm(Studio* studio, bool yes, void* data)
{
    CommandConfirmData* confirmData = (CommandConfirmData*)data;

    if(yes)
    {
        confirmData->callback(confirmData->console);
    }
    else commandDone(confirmData->console);

    free(confirmData);
}

static void confirmCommand(Console* console, const char** text, s32 rows, ConsoleConfirmCallback callback)
{
    if(console->args.cli)
    {
        for(s32 i = 0; i < rows; i++)
        {
            printError(console, text[i]);
            printLine(console);
        }

        commandDone(console);
    }
    else
    {
        CommandConfirmData data = {console, callback};
        confirmDialog(console->studio, text, rows, onConfirm, MOVE(data));
    }
}

typedef void(*LoadDemoConfirmCallback)(Console* console, tic_script_config* script);

typedef struct
{
    Console* console;
    LoadDemoConfirmCallback callback;
    tic_script_config* script;
} LoadDemoConfirmData;

static void onLoadDemoConfirm(Studio* studio, bool yes, void* data)
{
    LoadDemoConfirmData* demoData = (LoadDemoConfirmData*)data;

    if(yes)
    {
        demoData->callback(demoData->console, demoData->script);
    }
    else commandDone(demoData->console);

    free(demoData);
}

static const char* LoadWarningRows[] =
{
    "WARNING!",
    "You have unsaved changes",
    "Do you really want to load cart?",
};

static void onLoadDemoCommand(Console* console, tic_script_config* script)
{
    if(studioCartChanged(console->studio))
    {
        LoadDemoConfirmData data = {console, onLoadDemoCommandConfirmed, script};
        confirmDialog(console->studio, LoadWarningRows, COUNT_OF(LoadWarningRows), onLoadDemoConfirm, MOVE(data));
    }
    else
    {
        onLoadDemoCommandConfirmed(console, script);
    }
}

static void onLoadCommand(Console* console)
{
    if(studioCartChanged(console->studio))
    {
        confirmCommand(console, LoadWarningRows, COUNT_OF(LoadWarningRows), onLoadCommandConfirmed);
    }
    else
    {
        onLoadCommandConfirmed(console);
    }
}

static void loadDemo(Console* console, tic_script_config* script)
{
    if (script == NULL) script = Languages[0]; // can be called with null, meaning first language (Lua)
    s32 size = 0;
    u8* data = getDemoCart(console, script, &size);

    if(data)
    {
        tic_cart_load(&console->tic->cart, data, size);
        tic_api_reset(console->tic);

        free(data);
    }

    memset(console->rom.name, 0, sizeof console->rom.name);

    studioRomLoaded(console->studio);
}

static void onNewCommandConfirmed(Console* console)
{
    bool done = false;

    if(console->desc->count)
    {
        const char* param = console->desc->params->key;

        FOR_EACH_LANG(ln)
            if(strcmp(param, ln->name) == 0)
            {
                loadDemo(console, ln);
                done = true;
            }
        FOR_EACH_LANG_END

        if(!done)
        {
            printError(console, "\nunknown parameter: ");
            printError(console, param);
            commandDone(console);
            return;
        }
    }
    else
    {
        printError(console, "\nerror: choose a language for the new cart.");
        printUsage(console, console->desc->command);
    }

    if(done) printBack(console, "\nnew cart has been created");
    else printError(console, "\ncart not created");

    commandDone(console);
}

static void onNewCommand(Console* console)
{
    if(studioCartChanged(console->studio))
    {
        static const char* Rows[] =
        {
            "WARNING!",
            "You have unsaved changes",
            "Do you really want to create new cart?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), onNewCommandConfirmed);
    }
    else
    {
        onNewCommandConfirmed(console);
    }
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

typedef struct
{
    Console* console;
    char* incompleteWord; // Original word that's being completed.
    char* options; // Options to show to the user.
    char* commonPrefix; // Common prefix of all options.
} TabCompleteData;

static void addTabCompleteOption(TabCompleteData* data, const char* option)
{
    if (strstr(option, data->incompleteWord) == option)
    {
        // Possibly reduce the common prefix of all possible options.
        if (strlen(data->options) == 0)
        {
            // This is the first option to be added. Initialize the prefix.
            strncpy(data->commonPrefix, option, CONSOLE_BUFFER_SCREEN);
        }
        else
        {
            // Only leave the longest common prefix.
            char* tmpCommonPrefix = data->commonPrefix;
            char* tmpOption = (char*) option;

            while (*tmpCommonPrefix && *tmpOption && *tmpCommonPrefix == *tmpOption) {
                tmpCommonPrefix++;
                tmpOption++;
            }

            *tmpCommonPrefix = 0;
        }

        // The option matches the incomplete word, add it to the list.
        strncat(data->options, option, CONSOLE_BUFFER_SCREEN);
        strncat(data->options, " ", CONSOLE_BUFFER_SCREEN);
    }
}

// Used to show tab-complete options, for example.
static void provideHint(Console* console, const char* hint)
{
    char* input = malloc(CONSOLE_BUFFER_SCREEN);
    strncpy(input, console->input.text, CONSOLE_BUFFER_SCREEN);

    printLine(console);
    printBack(console, hint);
    commandDone(console);
    insertInputText(console, input);

    free(input);
}

static void finishTabComplete(const TabCompleteData* data)
{
    bool anyOptions = strlen(data->options) > 0;
    if (anyOptions) {
        // Adding one at the right because all options end with a space.
        bool justOneOptionLeft = strlen(data->options) == strlen(data->commonPrefix)+1;

        if (strlen(data->commonPrefix) == strlen(data->incompleteWord) && !justOneOptionLeft)
        {
            provideHint(data->console, data->options);
        }
        processConsoleEnd(data->console);
        insertInputText(data->console, data->commonPrefix+strlen(data->incompleteWord));

        if (justOneOptionLeft)
        {
            insertInputText(data->console, " ");
        }
    }

    free(data->options);
    free(data->commonPrefix);
}

static void tabCompleteLanguages(TabCompleteData* data)
{
    FOR_EACH_LANG(ln)
        addTabCompleteOption(data, ln->name);
    FOR_EACH_LANG_END
    finishTabComplete(data);
}

static void tabCompleteExport(TabCompleteData* data)
{
#define EXPORT_CMD_DEF(name) addTabCompleteOption(data, #name);
    EXPORT_CMD_LIST(EXPORT_CMD_DEF)
#undef  EXPORT_CMD_DEF
    finishTabComplete(data);
}

static void tabCompleteImport(TabCompleteData* data)
{
#define IMPORT_CMD_DEF(name) addTabCompleteOption(data, #name);
    IMPORT_CMD_LIST(IMPORT_CMD_DEF)
#undef  IMPORT_CMD_DEF
    finishTabComplete(data);
}

static bool addFileAndDirToTabComplete(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    addTabCompleteOption(data, name);

    return true;
}

static bool addFilenameToTabComplete(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    if (!dir)
        addTabCompleteOption(data, name);

    return true;
}

static bool addDirToTabComplete(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    if (dir)
        addTabCompleteOption(data, name);

    return true;
}

static void finishTabCompleteAndFreeData(void* data) {
    finishTabComplete((const TabCompleteData *) data);
    free(data);
}

static void tabCompleteFiles(TabCompleteData* data)
{
    tic_fs_enum(data->console->fs, addFilenameToTabComplete, finishTabCompleteAndFreeData, MOVE(*data));
}

static void tabCompleteDirs(TabCompleteData* data)
{
    tic_fs_enum(data->console->fs, addDirToTabComplete, finishTabCompleteAndFreeData, MOVE(*data));
}

static void tabCompleteFilesAndDirs(TabCompleteData* data)
{
    tic_fs_enum(data->console->fs, addFileAndDirToTabComplete, finishTabCompleteAndFreeData, MOVE(*data));
}

static void tabCompleteConfig(TabCompleteData* data)
{
    addTabCompleteOption(data, "reset");
    addTabCompleteOption(data, "default");
    finishTabComplete(data);
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

static bool printFilename(const char* name, const char* title, const char* hash, s32 id, void* ctx, bool dir)
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

static void onChangeDirectory(Console* console)
{
    if(console->desc->count)
    {
        const char* param = console->desc->params->key;

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
    } else {
        tic_fs_homedir(console->fs);
    }

    commandDone(console);
}

static void onMakeDirectory(Console* console)
{
    if(console->desc->count)
    {
        char msg[TICNAME_MAX];
        const char* param = console->desc->params->key;

        if (tic_fs_exists(console->fs, param)) {
            sprintf(msg, "\nerror, [%s] already exists :(", param);
            printError(console, msg);
            commandDone(console);
            return;
        }

        sprintf(msg, "\ncreated [%s] folder :)", param);

        printBack(console, tic_fs_makedir(console->fs, param)
            ? "\nerror, dir not created :("
            : msg);

    }
    else printError(console, "\ninvalid dir name");

    commandDone(console);
}

static void onDirCommand(Console* console)
{
    printLine(console);

    PrintFileNameData data = {console};
    tic_fs_enum(console->fs, printFilename, onDirDone, MOVE(data));
}

static void onFolderCommand(Console* console)
{

    printBack(console, "\nStorage path:\n");
    printFront(console, tic_fs_pathroot(console->fs, ""));

    tic_fs_openfolder(console->fs);

    commandDone(console);
}

static void onClsCommand(Console* console)
{
    memset(console->text, 0, CONSOLE_BUFFER_SIZE);
    memset(console->color, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

    ZEROMEM(console->scroll);
    ZEROMEM(console->cursor);
    ZEROMEM(console->input);

    printf("\r");

    commandDoneLine(console, false);
}

static void onInstallDemosCommand(Console* console)
{
    tic_fs* fs = console->fs;
    u8* data = (u8*)newCart();

    SCOPE(free(data))
    {
        printBack(console, "\nadded carts:\n\n");

#if defined(TIC_BUILD_WITH_LUA)

        static const u8 demofire[] =
        {
            #include "../build/assets/fire.tic.dat"
        };

        static const u8 demop3d[] =
        {
            #include "../build/assets/p3d.tic.dat"
        };

        static const u8 demosfx[] =
        {
            #include "../build/assets/sfx.tic.dat"
        };

        static const u8 demopalette[] =
        {
            #include "../build/assets/palette.tic.dat"
        };

        static const u8 demofont[] =
        {
            #include "../build/assets/font.tic.dat"
        };

        static const u8 demomusic[] =
        {
            #include "../build/assets/music.tic.dat"
        };

        static const u8 demoquest[] =
        {
            #include "../build/assets/quest.tic.dat"
        };

        static const u8 demotetris[] =
        {
            #include "../build/assets/tetris.tic.dat"
        };

        static const u8 demobenchmark[] =
        {
            #include "../build/assets/benchmark.tic.dat"
        };

        static const u8 demobpp[] =
        {
            #include "../build/assets/bpp.tic.dat"
        };

        static const u8 democar[] =
        {
            #include "../build/assets/car.tic.dat"
        };

#define DEMOS_LIST(macro)       \
        macro(fire)             \
        macro(font)             \
        macro(music)            \
        macro(p3d)              \
        macro(palette)          \
        macro(quest)            \
        macro(sfx)              \
        macro(tetris)           \
        macro(benchmark)        \
        macro(bpp)              \
        macro(car)

        static const struct Demo {const char* name; const u8* data; s32 size;} Demos[] =
        {
#define     DEMOS_DEF(name) {#name ".tic", demo ## name, sizeof demo ## name},
            DEMOS_LIST(DEMOS_DEF)
#undef      DEMOS_DEF
        };

#undef  DEMOS_LIST

        FOR(const struct Demo*, demo, Demos)
        {
            tic_fs_save(fs, demo->name, data, tic_tool_unzip(data, sizeof(tic_cartridge), demo->data, demo->size), true);
            printFront(console, demo->name);
            printLine(console);
        }
#endif

        static const char* Bunny = "bunny";

        tic_fs_makedir(fs, Bunny);
        tic_fs_changedir(fs, Bunny);

        FOR_EACH_LANG(ln)
        {
            tic_script_config_extra* ex = getConfigExtra(ln);
            if (ex->markRom != NULL) { // having a Mark is not mandatory
                char cartname[1024];
                strcpy(cartname, ln->name);
                strcat(cartname, "mark.tic");

                tic_fs_save(fs, cartname, data, tic_tool_unzip(data, sizeof(tic_cartridge), ex->markRom, ex->markRomSize), true);
                printFront(console, Bunny);
                printFront(console, "/");
                printFront(console, cartname);
                printLine(console);
            }
        }
	FOR_EACH_LANG_END

        tic_fs_dirback(fs);
    }

    commandDone(console);
}

static void onGameMenuCommand(Console* console)
{
    gotoMenu(console->studio);
    commandDone(console);
}

static void onSurfCommand(Console* console)
{
    gotoSurf(console->studio);
}

static void loadExternal(Console* console, const char* path)
{
    CommandDesc desc =
    {
        .params = malloc(sizeof *desc.params),
        .count = 1,
    };

    *desc.params = (struct Param){.key = strdup(path)};
    *console->desc = desc;

    onLoadCommandConfirmed(console);
}

static void onConfigCommand(Console* console)
{
    if(console->desc->count)
    {
        if(strcmp(console->desc->params->key, "reset") == 0)
        {
            console->config->reset(console->config);
            printBack(console, "\nconfiguration reset :)");
        }
        else if(strcmp(console->desc->params->key, "default") == 0)
        {
            if (console->desc->count == 1)
            {
                onLoadDemoCommand(console, Languages[0]);
            }
            else
            {
                FOR_EACH_LANG(script)
                    if (strcmp(console->desc->params[1].key, script->name) == 0)
                        onLoadDemoCommand(console, script);
                FOR_EACH_LANG_END
            }
        }
        else
        {
            printError(console, "\nunknown parameter:\n");
            printError(console, console->desc->params->key);
        }
    }
    else
    {
        CommandDesc desc =
        {
            .params = malloc(sizeof *desc.params),
            .count = 1,
        };

        *desc.params = (struct Param){.key = strdup(CONFIG_TIC_PATH)};
        *console->desc = desc;

        onLoadCommand(console);

        return;
    }

    commandDone(console);
}

typedef struct
{
#define IMPORT_KEYS_DEF(key) s32 key;
    IMPORT_KEYS_LIST(IMPORT_KEYS_DEF)
#undef IMPORT_KEYS_DEF
} ImportParams;

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

static inline tic_bank* getBank(Console* console, s32 bank)
{
    return &console->tic->cart.banks[bank];
}

static inline const tic_palette* getPalette(Console* console, s32 bank, s32 vbank)
{
    return vbank
        ? &getBank(console, bank)->palette.vbank1
        : &getBank(console, bank)->palette.vbank0;
}

static void onImportTilesBase(Console* console, const char* name, const void* buffer, s32 size, tic_tile* base, ImportParams params)
{
    png_buffer png = {(u8*)buffer, size};
    bool error = true;

    png_img img = png_read(png, NULL);

    if(img.data) SCOPE(free(img.data))
    {
        const tic_palette* pal = getPalette(console, params.bank, params.vbank);

        for(s32 j = 0, y = params.y, h = y + (params.h ? params.h : img.height); y < h; ++y, ++j)
            for(s32 i = 0, x = params.x, w = x + (params.w ? params.w : img.width); x < w; ++x, ++i)
                if(x >= 0 && x < TIC_SPRITESHEET_SIZE && y >= 0 && y < TIC_SPRITESHEET_SIZE)
                    setSpritePixel(base, x, y, tic_nearest_color(pal->colors,
                        (tic_rgb*)(img.pixels + i + j * img.width), TIC_PALETTE_SIZE));

        error = false;
    }

    onFileImported(console, name, !error);
}

static void onImport_tiles(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    onImportTilesBase(console, name, buffer, size, getBank(console, params.bank)->tiles.data, params);
}

static void onImport_binary(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    bool ok = name && buffer && size <= TIC_BINARY_SIZE;

    if(ok)
    {
        tic_binary* binary = &console->tic->cart.binary;
        binary->size = size;
        memcpy(binary->data, buffer, size);
    }

    onFileImported(console, name, ok);
}

static void onImport_sprites(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    onImportTilesBase(console, name, buffer, size, getBank(console, params.bank)->sprites.data, params);
}

static void onImport_map(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    bool ok = name && buffer && size <= sizeof(tic_map);

    if(ok)
    {
        enum {Size = sizeof(tic_map)};

        tic_map* map = &getBank(console, params.bank)->map;
        memset(map, 0, Size);
        memcpy(map, buffer, MIN(size, Size));
    }

    onFileImported(console, name, ok);
}

static void onImport_code(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    tic_mem* tic = console->tic;
    bool error = false;

    if(name && buffer && size <= sizeof(tic_code))
    {
        enum {Size = sizeof(tic_code)};

        memset(tic->cart.code.data, 0, Size);
        memcpy(tic->cart.code.data, buffer, MIN(size, Size));

        studioRomLoaded(console->studio);
    }
    else error = true;

    onFileImported(console, name, !error);
}

static void onImport_screen(Console* console, const char* name, const void* buffer, s32 size, ImportParams params)
{
    png_buffer png = {(u8*)buffer, size};
    bool error = true;

    png_img img = png_read(png, NULL);

    if(img.data) SCOPE(free(img.data))
    {
        if(img.width == TIC80_WIDTH && img.height == TIC80_HEIGHT)
        {
            tic_bank* bank = getBank(console, params.bank);
            const tic_palette* pal = getPalette(console, params.bank, params.vbank);

            s32 i = 0;
            for(const png_rgba *pix = img.pixels, *end = pix + (TIC80_WIDTH * TIC80_HEIGHT); pix < end; pix++)
                tic_tool_poke4(bank->screen.data, i++, tic_nearest_color(pal->colors, (tic_rgb*)pix, TIC_PALETTE_SIZE));

            error = false;
        }
    }

    onFileImported(console, name, !error);
}

static void onImportCommand(Console* console)
{
    bool error = true;

    if(console->desc->count > 1)
    {
        ImportParams params = {0};

        for(const struct Param* it = console->desc->params, *end = it + console->desc->count; it < end; ++it)
        {
#define     IMPORT_KEYS_DEF(name) if(it->val && strcmp(it->key, #name) == 0) params.name = atoi(it->val);
            IMPORT_KEYS_LIST(IMPORT_KEYS_DEF)
#undef      IMPORT_KEYS_DEF
        }

        const char* filename = console->desc->params[1].key;
        s32 size = 0;
        const void* data = tic_fs_load(console->fs, filename, &size);

        if(data) SCOPE(free((void*)data))
        {
            static const struct Handler
            {
                const char* section;
                void (*handler)(Console*, const char*, const void*, s32, ImportParams);
            } Handlers[] =
            {
#define         IMPORT_CMD_DEF(name) {#name, onImport_##name},
                IMPORT_CMD_LIST(IMPORT_CMD_DEF)
#undef          IMPORT_CMD_DEF
            };

            const char* section = console->desc->params[0].key;
            FOR(const struct Handler*, ptr, Handlers)
                if(strcmp(section, ptr->section) == 0)
                {
                    ptr->handler(console, filename, data, size, params);
                    error = false;
                    break;
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
        printUsage(console, console->desc->command);

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

typedef struct
{
#define EXPORT_KEYS_DEF(key) s32 key;
    EXPORT_KEYS_LIST(EXPORT_KEYS_DEF)
#undef EXPORT_KEYS_DEF
} ExportParams;

static void exportSprites(Console* console, const char* filename, tic_tile* base, ExportParams params)
{
    tic_mem* tic = console->tic;
    const tic_cartridge* cart = &tic->cart;

    png_img img = {TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE, malloc(TIC_SPRITESHEET_SIZE * TIC_SPRITESHEET_SIZE * sizeof(png_rgba))};

    SCOPE(free(img.data))
    {
        const tic_palette* pal = getPalette(console, params.bank, params.vbank);

        for(s32 i = 0; i < TIC_SPRITESHEET_SIZE * TIC_SPRITESHEET_SIZE; i++)
            img.values[i] = tic_rgba(&pal->colors[getSpritePixel(base, i % TIC_SPRITESHEET_SIZE, i / TIC_SPRITESHEET_SIZE)]);

        png_buffer png = png_write(img, (png_buffer){NULL, 0});

        SCOPE(free(png.data))
        {
            onFileExported(console, filename, tic_fs_save(console->fs, filename, png.data, png.size, true));
        }
    }
}

static void* embedCart(Console* console, u8* app, s32* size)
{
    tic_mem* tic = console->tic;
    u8* data = NULL;
    void* cart = newCart();

    SCOPE(free(cart))
    {
        s32 cartSize = tic_cart_save(&tic->cart, cart);

        s32 zipSize = sizeof(tic_cartridge);
        u8* zipData = (u8*)malloc(zipSize);

        SCOPE(free(zipData))
        {
            if((zipSize = tic_tool_zip(zipData, zipSize, cart, cartSize)))
            {
                s32 appSize = *size;

                EmbedHeader header =
                {
                    .appSize = appSize,
                    .cartSize = zipSize,
                };

                memcpy(header.sig, CART_SIG, STRLEN(CART_SIG));

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
        }
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
        printError(console, "file downloading error :(");
        commandDone(console);
        free(exportData);
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
            chmod(path, DEFAULT_CHMOD);

            if (buf)
                free(buf);
        }
        break;
    default:
        onExportGet(data);
    }
}

static void exportGame(Console* console, const char* name, const char* system, net_get_callback callback, ExportParams params)
{
    tic_mem* tic = console->tic;
    printLine(console);
    GameExportData data = {console};
    strcpy(data.filename, name);

    char url[TICNAME_MAX] = "/export/" DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) TIC_VERSION_STATUS "/";
    strcat(url, system);

#if defined(TIC80_PRO)
    if (params.alone)
        strcat(url, tic_core_script_config(console->tic)->name);
#endif

    tic_net_get(console->net, url, callback, MOVE(data));
}

static inline void exportNativeGame(Console* console, const char* name, const char* system, ExportParams params)
{
    exportGame(console, name, system, onNativeExportGet, params);
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
            bool errorOccurred = !fs_write(zipPath, data->done.data, data->done.size);

            if(!errorOccurred)
            {
                struct zip_t *zip = zip_open(zipPath, ZIP_DEFAULT_COMPRESSION_LEVEL, 'a');

                if(zip) SCOPE(zip_close(zip))
                {
                    void* cart = newCart();

                    SCOPE(free(cart))
                    {
                        s32 cartSize = tic_cart_save(&tic->cart, cart);

                        if(cartSize)
                        {
                            zip_entry_open(zip, "cart.tic");
                            zip_entry_write(zip, cart, cartSize);
                            zip_entry_close(zip);
                        }
                        else errorOccurred = true;
                    }
                }
                else errorOccurred = true;
            }

            onFileExported(console, filename, !errorOccurred);
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

static void onExport_win(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportNativeGame(console, getFilename(filename, ".exe"), param, params);
}

static void onExport_winxp(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportNativeGame(console, getFilename(filename, ".exe"), param, params);
}

static void onExport_linux(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportNativeGame(console, filename, param, params);
}

static void onExport_rpi(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportNativeGame(console, filename, param, params);
}

static void onExport_mac(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportNativeGame(console, filename, param, params);
}

static void onExport_html(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportGame(console, getFilename(filename, ".zip"), param, onHtmlExportGet, params);
}

static void onExport_tiles(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportSprites(console, getFilename(filename, PngExt), getBank(console, params.bank)->tiles.data, params);
}

static void onExport_binary(Console* console, const char* param, const char* path, ExportParams params)
{
    const char* filename = getFilename(path, ".binary");

    tic_binary *binary = &console->tic->cart.binary;
    // TODO: do we need this buffer at all, could we just handle `binary.data` directly to `tic_fs_save`?
    void* buffer = malloc(binary->size);

    SCOPE(free(buffer))
    {
        memcpy(buffer, binary->data, binary->size);

        onFileExported(console, filename, tic_fs_save(console->fs, filename, buffer, binary->size, true));
    }
}

static void onExport_sprites(Console* console, const char* param, const char* filename, ExportParams params)
{
    exportSprites(console, getFilename(filename, PngExt), getBank(console, params.bank)->sprites.data, params);
}

static void onExport_map(Console* console, const char* param, const char* path, ExportParams params)
{
    enum{Size = sizeof(tic_map)};
    const char* filename = getFilename(path, ".map");

    void* buffer = malloc(Size);

    SCOPE(free(buffer))
    {
        tic_map* map = &getBank(console, params.bank)->map;
        memcpy(buffer, map->data, Size);

        onFileExported(console, filename, tic_fs_save(console->fs, filename, buffer, Size, true));
    }
}

static void onExport_mapimg(Console* console, const char* param, const char* path, ExportParams params)
{
    const char* filename = getFilename(path, ".png");

    enum{Width = TIC_MAP_WIDTH * TIC_SPRITESIZE, Height = TIC_MAP_HEIGHT * TIC_SPRITESIZE};

    png_img img = {Width, Height, malloc(Width * Height * sizeof(png_rgba))};

    SCOPE(free(img.data))
    {
        {
            tic_mem* tic = console->tic;

            tic_api_sync(tic, -1, params.bank, false);

            for(s32 r = 0; r < TIC_MAP_ROWS; r++)
                for(s32 c = 0; c < TIC_MAP_COLS; c++)
                {
                    tic_api_map(tic, c * TIC_MAP_SCREEN_WIDTH, r * TIC_MAP_SCREEN_HEIGHT,
                        TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, NULL, 0, 1, NULL, NULL);

                    tic_core_blit(tic);

                    for(s32 j = 0; j < TIC80_HEIGHT; j++)
                        for(s32 i = 0; i < TIC80_WIDTH; i++)
                            img.values[(i + c * TIC80_WIDTH) + (j + r * TIC80_HEIGHT) * Width] =
                                tic->product.screen[(i + TIC80_MARGIN_LEFT) + (j + TIC80_MARGIN_TOP) * TIC80_FULLWIDTH];
                }
        }

        png_buffer png = png_write(img, (png_buffer){NULL, 0});

        SCOPE(free(png.data))
        {
            onFileExported(console, filename, tic_fs_save(console->fs, filename, png.data, png.size, true));
        }
    }
}

static void onExport_sfx(Console* console, const char* param, const char* name, ExportParams params)
{
    const char* filename = getFilename(name, ".wav");
    bool error = true;

    if(params.id >= 0 && params.id < SFX_COUNT)
        error = studioExportSfx(console->studio, params.id, filename) == NULL;

    onFileExported(console, filename, !error);
}

static void onExport_music(Console* console, const char* type, const char* name, ExportParams params)
{
    const char* filename = getFilename(name, ".wav");
    bool error = true;

    if(params.id >= 0 && params.id < MUSIC_TRACKS)
        error = studioExportMusic(console->studio, params.id, params.bank, filename) == NULL;

    onFileExported(console, filename, !error);
}

static void onExport_screen(Console* console, const char* param, const char* name, ExportParams params)
{
    const char* filename = getFilename(name, ".png");

    tic_mem* tic = console->tic;
    const tic_cartridge* cart = &tic->cart;

    png_img img = {TIC80_WIDTH, TIC80_HEIGHT, malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(png_rgba))};

    SCOPE(free(img.data))
    {
        const tic_palette* pal = getPalette(console, params.bank, params.vbank);

        tic_bank* bank = getBank(console, params.bank);
        for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
            img.values[i] = tic_rgba(&pal->colors[tic_tool_peek4(bank->screen.data, i)]);

        png_buffer png = png_write(img, (png_buffer){NULL, 0});

        SCOPE(free(png.data))
        {
            onFileExported(console, filename, tic_fs_save(console->fs, filename, png.data, png.size, true));
        }
    }
}

static void onExport_help(Console* console, const char* param, const char* name, ExportParams params);

static void onExportCommand(Console* console)
{
    if(console->desc->count > 1)
    {
        ExportParams params = {0};

        for(const struct Param* it = console->desc->params, *end = it + console->desc->count; it < end; ++it)
        {
#define     EXPORT_KEYS_DEF(name) if(it->val && strcmp(it->key, #name) == 0) params.name = atoi(it->val);
            EXPORT_KEYS_LIST(EXPORT_KEYS_DEF)
#undef      EXPORT_KEYS_DEF
        }

        const char* filename = console->desc->params[1].key;

        static const struct Handler
        {
            const char* type;
            void(*handler)(Console*, const char*, const char*, ExportParams);
        } Handlers[] =
        {
#define     EXPORT_CMD_DEF(name) {#name, onExport_##name},
            EXPORT_CMD_LIST(EXPORT_CMD_DEF)
#undef      EXPORT_CMD_DEF
        };

        const char* type = console->desc->params[0].key;

        FOR(const struct Handler*, ptr, Handlers)
            if(strcmp(type, ptr->type) == 0)
            {
                ptr->handler(console, type, filename, params);
                return;
            }
    }

    {
        printError(console, "\nerror: invalid parameters.");
        printUsage(console, console->desc->command);
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
                studioRomSaved(console->studio);
                free(buffer);
                return CART_SAVE_OK;
            }
            else
            {
                s32 size = 0;

                if(tic_tool_has_ext(name, PngExt))
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

                            const char* comment = tic_core_script_config(tic)->singleComment;

                            char* title = tic_tool_metatag(tic->cart.code.data, "title", comment);
                            if(title)
                            {
                                drawShadowText(tic, title, 0, 0, tic_color_white, Scale);
                                free(title);
                            }

                            char* author = tic_tool_metatag(tic->cart.code.data, "author", comment);
                            if(author)
                            {
                                char buf[TICNAME_MAX];
                                snprintf(buf, sizeof buf, "by %s", author);
                                drawShadowText(tic, buf, 0, Row, tic_color_grey, Scale);
                                free(author);
                            }

                            u32* ptr = img.values + PaddingTop * CoverWidth + PaddingLeft;
                            const u8* screen = tic->ram->vram.screen.data;
                            const tic_rgb* pal = getConfig(console->studio)->cart->bank0.palette.vbank0.colors;

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
                else if(tic_project_ext(name))
                {
                    size = tic_project_save(name, buffer, &tic->cart);
                }
#endif
                else
                {
                    name = getCartName(name);
                    size = tic_cart_save(&tic->cart, buffer);
                }

                if(size && tic_fs_save(console->fs, name, buffer, size, true))
                {
                    setCartName(console, name, tic_fs_path(console->fs, name));
                    success = true;
                    studioRomSaved(console->studio);
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

static void onSaveCommandConfirmed(Console* console)
{
    CartSaveResult rom = saveCartName(console, console->desc->count ? console->desc->params->key : NULL);

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

static void onSaveCommand(Console* console)
{
    const char* param = console->desc->count ? console->desc->params->key : NULL;

    if(param && strlen(param) &&
        (tic_fs_exists(console->fs, param) ||
            tic_fs_exists(console->fs, getCartName(param))))
    {
        static const char* Rows[] =
        {
            "WARNING!",
            "The cart already exists",
            "Do you want to overwrite it?",
        };

        confirmCommand(console, Rows, COUNT_OF(Rows), onSaveCommandConfirmed);
    }
    else
    {
        onSaveCommandConfirmed(console);
    }
}

static void onRunCommand(Console* console)
{
    commandDone(console);

    runGame(console->studio);
}

static void onResumeCommand(Console* console)
{
    commandDone(console);

    resumeGame(console->studio);
}

static void onEvalCommand(Console* console)
{
    printLine(console);

    const tic_script_config* script_config = tic_core_script_config(console->tic);

    if (script_config->eval)
    {
        if(console->desc->count)
            script_config->eval(console->tic,
                                console->desc->src+strlen(console->desc->command));
        else printError(console, "nothing to eval");
    }
    else
    {
        printError(console, "'eval' not implemented for the script");
    }

    commandDone(console);
}

static void onDelCommandConfirmed(Console* console)
{
    if(console->desc->count)
    {
        if (tic_fs_ispubdir(console->fs))
        {
            printError(console, "\naccess denied");
        }
        else
        {
            const char* param = console->desc->params->key;
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

static void onDelCommand(Console* console)
{
    static const char* Rows[] =
    {
        "WARNING!",
        "Do you really want to delete file?",
    };

    confirmCommand(console, Rows, COUNT_OF(Rows), onDelCommandConfirmed);
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

static void onAddCommand(Console* console)
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

static void onGetCommand(Console* console)
{
    if(console->desc->count)
    {
        const char* name = console->desc->params->key;
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

// Declare this here to resolve a cyclic dependency with COMMANDS_LIST.
static void tabCompleteHelp(TabCompleteData* data);

static const char HelpUsage[] = "help [<text>"
#define HELP_CMD_DEF(name) "|" #name
    HELP_CMD_LIST(HELP_CMD_DEF)
#undef  HELP_CMD_DEF
    "]";

#define SECTION_DEF(NAME, ...)  "|" #NAME
#define EXPORT_CMD_DEF(name)    #name "|"
#define EXPORT_KEYS_DEF(name)   #name "=0 "
#define IMPORT_CMD_DEF(name)    #name "|"
#define IMPORT_KEYS_DEF(key)    #key"=0 "

#if defined(CAN_ADDGET_FILE)
#define ADDGET_FILE(macro)                                                              \
    macro("add",                                                                        \
        NULL,                                                                           \
        "upload file to the browser local storage.",                                    \
        NULL,                                                                           \
        onAddCommand,                                                                   \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("get",                                                                        \
        NULL,                                                                           \
        "download file from the browser local storage.",                                \
        "get <file>",                                                                   \
        onGetCommand,                                                                   \
        tabCompleteFiles,                                                               \
        NULL)                                                                           \

#else
#define ADDGET_FILE(macro)
#endif

// macro(name, alt, help, usage, handler, tab-complete for first param, for second param)
#define COMMANDS_LIST(macro)                                                            \
    macro("help",                                                                       \
        NULL,                                                                           \
        "show help info about commands/api/...",                                        \
        HelpUsage,                                                                      \
        onHelpCommand,                                                                  \
        tabCompleteHelp,                                                                \
        NULL)                                                                           \
                                                                                        \
    macro("exit",                                                                       \
        "quit",                                                                         \
        "exit the application.",                                                        \
        NULL,                                                                           \
        onExitCommand,                                                                  \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("edit",                                                                       \
        NULL,                                                                           \
        "open cart editors.",                                                           \
        NULL,                                                                           \
        onEditCommand,                                                                  \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("new",                                                                        \
        NULL,                                                                           \
        "creates a new `Hello World` cartridge.",                                       \
        "new <$LANG_NAMES_PIPE$>",                                                      \
        onNewCommand,                                                                   \
        tabCompleteLanguages,                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("load",                                                                       \
        NULL,                                                                           \
        "load cartridge from the local filesystem"                                      \
        "(there's no need to type the .tic extension).\n"                               \
        "you can also load just the section (sprites, map etc) from another cart.",     \
        "load <cart> [code" TIC_SYNC_LIST(SECTION_DEF) "]",                             \
        onLoadCommand,                                                                  \
        tabCompleteFiles,                                                               \
        NULL)                                                                           \
                                                                                        \
    macro("save",                                                                       \
        NULL,                                                                           \
        "save cartridge to the local filesystem, use $LANG_EXTENSIONS$"                 \
        "cart extension to save it in text format (PRO feature).",                      \
        "save <cart>",                                                                  \
        onSaveCommand,                                                                  \
        tabCompleteFiles,                                                               \
        NULL)                                                                           \
                                                                                        \
    macro("run",                                                                        \
        NULL,                                                                           \
        "run current cart / project.",                                                  \
        NULL,                                                                           \
        onRunCommand,                                                                   \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("resume",                                                                     \
        NULL,                                                                           \
        "resume last run cart / project.",                                              \
        NULL,                                                                           \
        onResumeCommand,                                                                \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("eval",                                                                       \
        "=",                                                                            \
        "run code provided code.",                                                      \
        NULL,                                                                           \
        onEvalCommand,                                                                  \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("dir",                                                                        \
        "ls",                                                                           \
        "show list of local files.",                                                    \
        NULL,                                                                           \
        onDirCommand,                                                                   \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("cd",                                                                         \
        NULL,                                                                           \
        "change directory.",                                                            \
        "\ncd <path>\ncd /\ncd ..",                                                     \
        onChangeDirectory,                                                              \
        tabCompleteDirs,                                                                \
        NULL)                                                                           \
                                                                                        \
    macro("mkdir",                                                                      \
        NULL,                                                                           \
        "make a directory.",                                                            \
        "mkdir <name>",                                                                 \
        onMakeDirectory,                                                                \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("folder",                                                                     \
        NULL,                                                                           \
        "open working directory in OS.",                                                \
        NULL,                                                                           \
        onFolderCommand,                                                                \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("export",                                                                     \
        NULL,                                                                           \
        "export cart to HTML,\n"                                                        \
        "native build (win linux rpi mac),\n"                                           \
        "export sprites/map/... as a .png image "                                       \
        "or export sfx and music to .wav files.",                                       \
        "\nexport [" EXPORT_CMD_LIST(EXPORT_CMD_DEF) "...] "                            \
        "<file> [" EXPORT_KEYS_LIST(EXPORT_KEYS_DEF) "...]" ,                           \
        onExportCommand,                                                                \
        tabCompleteExport,                                                              \
        tabCompleteFiles)                                                               \
                                                                                        \
    macro("import",                                                                     \
        NULL,                                                                           \
        "import code/sprites/map/... from an external file.",                           \
        "\nimport [" IMPORT_CMD_LIST(IMPORT_CMD_DEF) "...] "                            \
        "<file> [" IMPORT_KEYS_LIST(IMPORT_KEYS_DEF) "...]",                            \
        onImportCommand,                                                                \
        tabCompleteImport,                                                              \
        tabCompleteFiles)                                                               \
                                                                                        \
    macro("del",                                                                        \
        NULL,                                                                           \
        "delete from the filesystem.",                                                  \
        "del <file|folder>",                                                            \
        onDelCommand,                                                                   \
        tabCompleteFilesAndDirs,                                                        \
        NULL)                                                                           \
                                                                                        \
    macro("cls",                                                                        \
        "clear",                                                                        \
        "clear console screen.",                                                        \
        NULL,                                                                           \
        onClsCommand,                                                                   \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("demo",                                                                       \
        NULL,                                                                           \
        "install demo carts to the current directory.",                                 \
        NULL,                                                                           \
        onInstallDemosCommand,                                                          \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("config",                                                                     \
        NULL,                                                                           \
        "edit system configuration cartridge,\n"                                        \
        "use `reset` param to reset current configuration,\n"                           \
        "use `default` to edit default cart template.",                                 \
        "config [reset|default]",                                                       \
        onConfigCommand,                                                                \
        tabCompleteConfig,                                                              \
        NULL)                                                                           \
                                                                                        \
    macro("surf",                                                                       \
        NULL,                                                                           \
        "open carts browser.",                                                          \
        NULL,                                                                           \
        onSurfCommand,                                                                  \
        NULL,                                                                           \
        NULL)                                                                           \
                                                                                        \
    macro("menu",                                                                       \
        NULL,                                                                           \
        "show game menu where you can setup video, sound and input options.",           \
        NULL,                                                                           \
        onGameMenuCommand,                                                              \
        NULL,                                                                           \
        NULL)                                                                           \
    ADDGET_FILE(macro)

static struct Command
{
    const char* name;
    const char* alt;
    const char* help;
    const char* usage;
    void(*handler)(Console*);
    void(*tabComplete1)(TabCompleteData*);
    void(*tabComplete2)(TabCompleteData*);

} Commands[] =
{
#define COMMANDS_DEF(name, alt, help, usage, handler, tabComplete1, tabComplete2) \
    {name, alt, help, usage, handler, tabComplete1, tabComplete2},
    COMMANDS_LIST(COMMANDS_DEF)
#undef COMMANDS_DEF
};

#undef SECTION_DEF
#undef EXPORT_CMD_DEF
#undef EXPORT_KEYS_DEF
#undef IMPORT_CMD_DEF
#undef IMPORT_KEYS_DEF

typedef struct Command Command;

#define API_LIST(macro)         \
    TIC_CALLBACK_LIST(macro)    \
    TIC_API_LIST(macro)

static struct ApiItem {const char* name; const char* def; const char* help;} Api[] =
{
#define TIC_API_DEF(name, def, help, ...) {#name, def, help},
    API_LIST(TIC_API_DEF)
#undef TIC_API_DEF
};

typedef struct ApiItem ApiItem;

static void tabCompleteHelp(TabCompleteData* data)
{
#define HELP_CMD_DEF(name) addTabCompleteOption(data, #name);
    HELP_CMD_LIST(HELP_CMD_DEF)
#undef  HELP_CMD_DEF

    for(s32 i = 0; i < COUNT_OF(Commands); i++)
    {
        addTabCompleteOption(data, Commands[i].name);
    }

#define TIC_API_DEF(name, def, help, ...) addTabCompleteOption(data, #name);
    API_LIST(TIC_API_DEF)
#undef TIC_API_DEF

    finishTabComplete(data);
}


static s32 createRamTable(char* buf)
{
    char* ptr = buf;
    ptr += sprintf(ptr, "\n+-----------------------------------+"
                        "\n|           96KB RAM LAYOUT         |"
                        "\n+-------+-------------------+-------+"
                        "\n| ADDR  | INFO              | BYTES |"
                        "\n+-------+-------------------+-------+");

    static const struct Row {s32 addr; const char* info;} Rows[] =
    {
        {0,                                         "<VRAM>"},
        {offsetof(tic_ram, tiles),                  "TILES"},
        {offsetof(tic_ram, sprites),                "SPRITES"},
        {offsetof(tic_ram, map),                    "MAP"},
        {offsetof(tic_ram, input.gamepads),         "GAMEPADS"},
        {offsetof(tic_ram, input.mouse),            "MOUSE"},
        {offsetof(tic_ram, input.keyboard),         "KEYBOARD"},
        {offsetof(tic_ram, sfxpos),                 "SFX STATE"},
        {offsetof(tic_ram, registers),              "SOUND REGISTERS"},
        {offsetof(tic_ram, sfx.waveforms),          "WAVEFORMS"},
        {offsetof(tic_ram, sfx.samples),            "SFX"},
        {offsetof(tic_ram, music.patterns.data),    "MUSIC PATTERNS"},
        {offsetof(tic_ram, music.tracks.data),      "MUSIC TRACKS"},
        {offsetof(tic_ram, music_state),            "MUSIC STATE"},
        {offsetof(tic_ram, stereo),                 "STEREO VOLUME"},
        {offsetof(tic_ram, persistent),             "PERSISTENT MEMORY"},
        {offsetof(tic_ram, flags),                  "SPRITE FLAGS"},
        {offsetof(tic_ram, font.regular),           "FONT"},
        {offsetof(tic_ram, font.regular.params),    "FONT PARAMS"},
        {offsetof(tic_ram, font.alt),               "ALT FONT"},
        {offsetof(tic_ram, font.alt.params),        "ALT FONT PARAMS"},
        {offsetof(tic_ram, mapping),                "BUTTONS MAPPING"},
        {offsetof(tic_ram, free),                   "** RESERVED **"},
        {TIC_RAM_SIZE,                              ""},
    };

    for(const struct Row* row = Rows, *end = row + COUNT_OF(Rows) - 1; row < end; row++)
        ptr += sprintf(ptr, "\n| %05X | %-17s | %-5i |", row->addr, row->info, (row + 1)->addr - row->addr);

    ptr += sprintf(ptr, "\n+-------+-------------------+-------+\n");

    return strlen(buf);
}

static s32 createVRamTable(char* buf)
{
    char* ptr = buf;
    ptr += sprintf(ptr, "\n+-----------------------------------+"
                        "\n|          16KB VRAM LAYOUT         |"
                        "\n+-------+-------------------+-------+"
                        "\n| ADDR  | INFO              | BYTES |"
                        "\n+-------+-------------------+-------+");

    static const struct Row {s32 addr; const char* info;} Rows[] =
    {
        {offsetof(tic_ram, vram.screen),        "SCREEN"},
        {offsetof(tic_ram, vram.palette),       "PALETTE"},
        {offsetof(tic_ram, vram.mapping),       "PALETTE MAP"},
        {offsetof(tic_ram, vram.vars),          "BORDER COLOR"},
        {offsetof(tic_ram, vram.vars.offset),   "SCREEN OFFSET"},
        {offsetof(tic_ram, vram.vars.cursor),   "MOUSE CURSOR"},
        {offsetof(tic_ram, vram.blit),          "BLIT SEGMENT"},
        {offsetof(tic_ram, vram.reserved),      "... (reserved) "},
        {TIC_VRAM_SIZE,                         ""},
    };

    for(const struct Row* row = Rows, *end = row + COUNT_OF(Rows) - 1; row < end; row++)
        ptr += sprintf(ptr, "\n| %05X | %-17s | %-5i |", row->addr, row->info, (row + 1)->addr - row->addr);

    ptr += sprintf(ptr, "\n+-------+-------------------+-------+\n");

    return strlen(buf);
}

static s32 createKeysTable(char* buf)
{
    char* ptr = buf;
    ptr += sprintf(ptr, "\n+----+------------+ +----+------------+"
                        "\n|CODE|    KEY     | |CODE|    KEY     |"
                        "\n+----+------------+ +----+------------+");

    static const struct Row {s32 code; const char* key;} Rows[] =
    {
        {1,  "A"},
        {2,  "B"},
        {3,  "C"},
        {4,  "D"},
        {5,  "E"},
        {6,  "F"},
        {7,  "G"},
        {8,  "H"},
        {9,  "I"},
        {10, "J"},
        {11, "K"},
        {12, "L"},
        {13, "M"},
        {14, "N"},
        {15, "O"},
        {16, "P"},
        {17, "Q"},
        {18, "R"},
        {19, "S"},
        {20, "T"},
        {21, "U"},
        {22, "V"},
        {23, "W"},
        {24, "X"},
        {25, "Y"},
        {26, "Z"},
        {27, "0"},
        {28, "1"},
        {29, "2"},
        {30, "3"},
        {31, "4"},
        {32, "5"},
        {33, "6"},
        {34, "7"},
        {35, "8"},
        {36, "9"},
        {37, "MINUS"},
        {38, "EQUALS"},
        {39, "LEFTBRACKET"},
        {40, "RIGHTBRACKT"},
        {41, "BACKSLASH"},
        {42, "SEMICOLON"},
        {43, "APOSTROPHE"},
        {44, "GRAVE"},
        {45, "COMMA"},
        {46, "PERIOD"},
        {47, "SLASH"},
        {48, "SPACE"},
        {49, "TAB"},
        {50, "RETURN"},
        {51, "BACKSPACE"},
        {52, "DELETE"},
        {53, "INSERT"},
        {54, "PAGEUP"},
        {55, "PAGEDOWN"},
        {56, "HOME"},
        {57, "END"},
        {58, "UP"},
        {59, "DOWN"},
        {60, "LEFT"},
        {61, "RIGHT"},
        {62, "CAPSLOCK"},
        {63, "CTRL"},
        {64, "SHIFT"},
        {65, "ALT"},
        {66, "ESC"},
        {67, "F1"},
        {68, "F2"},
        {69, "F3"},
        {70, "F4"},
        {71, "F5"},
        {72, "F6"},
        {73, "F7"},
        {74, "F8"},
        {75, "F9"},
        {76, "F10"},
        {77, "F11"},
        {78, "F12"},
        {79, "NUM0"},
        {80, "NUM1"},
        {81, "NUM2"},
        {82, "NUM3"},
        {83, "NUM4"},
        {84, "NUM5"},
        {85, "NUM6"},
        {86, "NUM7"},
        {87, "NUM8"},
        {88, "NUM9"},
        {89, "NUMPLUS"},
        {90, "NUMMINUS"},
        {91, "NUMMULTIPLY"},
        {92, "NUMDIVIDE"},
        {93, "NUMENTER"},
        {94, "NUMPERIOD"},
    };

    for(const struct Row *row = Rows, *alt = row + COUNT_OF(Rows) / 2, *end = alt; row != end; ++row, ++alt)
    {
        ptr += sprintf(ptr, "\n| %2d | %-11s| | %2d | %-11s|", row->code, row->key, alt->code, alt->key);
    }

    ptr += sprintf(ptr, "\n+----+------------+ +----+------------+\n");

    return strlen(buf);
}

static s32 createButtonsTable(char* buf)
{
    char* ptr = buf;
    ptr += sprintf(ptr, "\n+--------+----+----+----+----+"
                        "\n| ACTION | P1 | P2 | P3 | P4 |"
                        "\n+--------+----+----+----+----+");

    static const struct Row {const char* action;} Rows[] =
    {
        {"UP"},
        {"DOWN"},
        {"LEFT"},
        {"RIGHT"},
        {"A"},
        {"B"},
        {"X"},
        {"Y"},
    };

    int id = 0;
    for(const struct Row* row = Rows, *end = row + COUNT_OF(Rows); row < end; row++) {
        ptr += sprintf(ptr, "\n| %6s | %2d | %2d | %2d | %2d |", row->action, id, id + 8, id + 16, id + 24);
        id++;
    }

    ptr += sprintf(ptr, "\n+--------+----+----+----+----+");

    return strlen(buf);
}

static void onExport_help(Console* console, const char* param, const char* name, ExportParams params)
{
    const char* filename = getFilename(name, ".md");

    char* buf = malloc(TIC_BANK_SIZE), *ptr = buf;

    SCOPE(free(buf))
    {
        ptr += sprintf(ptr, "# " TIC_NAME_FULL "\n" TIC_VERSION"\n" TIC_COPYRIGHT"\n");
        ptr += sprintf(ptr, "\n## Welcome\n%s\n", WelcomeText);
        ptr += sprintf(ptr, "\n## Specification\n```\n");

        FOR(const struct SpecRow*, row, SpecText1)
            ptr += sprintf(ptr, "%-10s%s\n", row->section, row->info);

        ptr += sprintf(ptr, "```\n```\n");
        ptr += createRamTable(ptr);
        ptr += sprintf(ptr, "```\n```");
        ptr += createVRamTable(ptr);
        ptr += sprintf(ptr, "```\n\n## Console commands\n");

        FOR(const Command*, cmd, Commands)
            ptr += sprintf(ptr, "\n### %s\n%s\nusage: `%s`\n",
                cmd->name, cmd->help, cmd->usage ? cmd->usage : cmd->name);

        ptr += sprintf(ptr, "\n## API functions\n");

        FOR(const ApiItem*, api, Api)
            ptr += sprintf(ptr, "\n### %s\n`%s`\n%s\n", api->name, api->def, api->help);

        ptr += sprintf(ptr, "\n## Button IDs\n");
        ptr += sprintf(ptr, "```");
        ptr += createButtonsTable(ptr);
        ptr += sprintf(ptr, "```\n");

        ptr += sprintf(ptr, "\n## Key IDs\n");
        ptr += sprintf(ptr, "```");
        ptr += createKeysTable(ptr);
        ptr += sprintf(ptr, "```\n");

        ptr += sprintf(ptr, "\n## Startup options\n```\n");
        FOR(const struct StartupOption*, opt, StartupOptions)
            ptr += sprintf(ptr, "--%-14s %s\n", opt->name, opt->help);

        ptr += sprintf(ptr, "```\n\n%s\n\n%s", TermsText, LicenseText);

        char* helpReplaced = replaceHelpTokens(buf);

        SCOPE(free(helpReplaced))
        {
            onFileExported(console, filename, tic_fs_save(console->fs, filename, helpReplaced, strlen(helpReplaced), true));
        }
    }
}

TabCompleteData newTabCompleteData(Console* console, char* incompleteWord) {
    TabCompleteData data = { console, .incompleteWord = incompleteWord };
    data.options = malloc(CONSOLE_BUFFER_SCREEN);
    data.commonPrefix = malloc(CONSOLE_BUFFER_SCREEN);
    data.options[0] = '\0';
    data.commonPrefix[0] = '\0';

    return data;
}

static void processConsoleTab(Console* console)
{
    char* input = console->input.text;
    char* param = strchr(input, ' ');

    if(param)
    {
        // Tab-complete command's parameters.
        param++;
        char* secondParam = strchr(param, ' ');
        if (secondParam)
            secondParam++;

        for(s32 i = 0; i < COUNT_OF(Commands); i++)
        {
            s32 commandLen = param-input-1;
            bool commandMatches = (strlen(Commands[i].name) == commandLen &&
                                       strncmp(Commands[i].name, input, commandLen) == 0) ||
                                  (Commands[i].alt &&
                                      strlen(Commands[i].name) == commandLen &&
                                      strncmp(Commands[i].alt, input, commandLen) == 0);

            if (commandMatches)
            {
                if (secondParam) {
                    if (Commands[i].tabComplete2) {
                        TabCompleteData data = newTabCompleteData(console, secondParam);
                        Commands[i].tabComplete2(&data);
                    }
                } else {
                    if (Commands[i].tabComplete1) {
                        TabCompleteData data = newTabCompleteData(console, param);
                        Commands[i].tabComplete1(&data);
                    }
                }
            }
        }
    }
    else
    {
        // Tab-complete commands.
        TabCompleteData data = newTabCompleteData(console, input);
        for(s32 i = 0; i < COUNT_OF(Commands); i++)
        {
            addTabCompleteOption(&data, Commands[i].name);
            if (Commands[i].alt)
                addTabCompleteOption(&data, Commands[i].alt);
        }
        finishTabComplete(&data);
    }
    scrollConsole(console);
}

static void toUpperStr(char* str)
{
    while(*str)
    {
        *str = toupper(*str);
        str++;
    }
}

static bool printUsage(Console* console, const char* command)
{
    FOR(const Command*, cmd, Commands)
    {
        if(strcmp(command, cmd->name) == 0)
        {
            consolePrint(console, "\n---=== COMMAND ===---\n", tic_color_green);
            char* helpReplaced = replaceHelpTokens(cmd->help);
            printBack(console, helpReplaced);
            free(helpReplaced);

            if(cmd->usage)
            {
                printFront(console, "\n\nusage: ");
                char* usageReplaced = replaceHelpTokens(cmd->usage);
                printBack(console, usageReplaced);
                free(usageReplaced);
            }

            printLine(console);
            return true;
        }
    }

    return false;
}

static bool printApi(Console* console, const char* param)
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
            return true;
        }
    }

    return false;
}

#define STRBUF_SIZE(name, ...) + STRLEN(#name) + STRLEN(Sep)

static void onHelp_api(Console* console)
{
    consolePrint(console, "\nAPI functions:\n", tic_color_blue);
    {
        const char Sep[] = " ";

        // calc buf size on compile time
        char buf[API_LIST(STRBUF_SIZE) + 1] = {[0] = 0};

        FOR(const ApiItem*, api, Api)
            strcat(buf, api->name), strcat(buf, Sep);

        printBack(console, buf);
    }
}

static void onHelp_commands(Console* console)
{
    consolePrint(console, "\nConsole commands:\n", tic_color_green);
    {
        const char Sep[] = " ";

        // calc buf size on compile time
        char buf[COMMANDS_LIST(STRBUF_SIZE) + 1] = {[0] = 0};

        FOR(const Command*, cmd, Commands)
            strcat(buf, cmd->name), strcat(buf, Sep);

        printBack(console, buf);
    }
}

#undef STRBUF_SIZE

static void printTable(Console* console, const char* text)
{
#ifndef BAREMETALPI
    printf("%s", text);
#endif

    for(const char* textPointer = text, *endText = textPointer + strlen(text); textPointer != endText;)
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

static void onHelp_ram(Console* console)
{
    char buf[2048];
    createRamTable(buf);
    printTable(console, buf);
}

static void onHelp_vram(Console* console)
{
    char buf[1024];
    createVRamTable(buf);
    printTable(console, buf);
}

static void onHelp_keys(Console* console)
{
    char buf[4096];
    createKeysTable(buf);
    printTable(console, buf);
}

static void onHelp_buttons(Console* console)
{
    char buf[1024];
    createButtonsTable(buf);
    printTable(console, buf);
}

static void onHelp_version(Console* console)
{
    consolePrint(console, "\n"TIC_VERSION, CONSOLE_BACK_TEXT_COLOR);
}

static void onHelp_spec(Console* console)
{
    printLine(console);

    char buf[TICNAME_MAX];

    FOR(const struct SpecRow*, row, SpecText1)
    {
#define OFFSET 8
        char* rowReplaced = replaceHelpTokens(row->info);
        sprintf(buf, "%-" DEF2STR(OFFSET) "s%s\n", row->section, rowReplaced);
        consolePrintOffset(console, buf, tic_color_grey, OFFSET);
        free(rowReplaced);
#undef  OFFSET
    }
}

static void onHelp_welcome(Console* console)
{
    printLine(console);
    printBack(console, WelcomeText);
}

static void onHelp_startup(Console* console)
{
    char buf[TICNAME_MAX];
    printFront(console, "\nStartup options:\n");
    FOR(const struct StartupOption*, opt, StartupOptions)
    {
#define OFFSET 12
#define PREFIX "--"
        sprintf(buf, PREFIX "%-" DEF2STR(OFFSET) "s%s\n", opt->name, opt->help);
        consolePrintOffset(console, buf, tic_color_grey, OFFSET + STRLEN(PREFIX));
#undef  PREFIX
#undef  OFFSET
    }
}

static void onHelp_terms(Console* console)
{
    printLine(console);
    printBack(console, TermsText);
}

static void onHelp_license(Console* console)
{
    printLine(console);
    printBack(console, LicenseText);
}

static void onHelpCommand(Console* console)
{
    if(console->desc->count)
    {
        const char* param = console->desc->params->key;
        bool foundTopic = false;

        if(printUsage(console, param)) {
            foundTopic = true;
        }
        if(printApi(console, param)) {
            foundTopic = true;
        }

        static const struct Handler {const char* cmd; void(*handler)(Console*);} Handlers[] =
        {
#define         HELP_CMD_DEF(name) {#name, onHelp_##name},
                HELP_CMD_LIST(HELP_CMD_DEF)
#undef          HELP_CMD_DEF
        };

        FOR(const struct Handler*, ptr, Handlers)
            if(strcmp(ptr->cmd, param) == 0)
            {
                foundTopic = true;
                ptr->handler(console);
                break;
            }

        if (!foundTopic) {
            printError(console, "\nunknown topic: ");
            printError(console, param);
        }
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
        printBack(console, " to switch editor/console\n");
    }

    commandDone(console);
}

static CommandDesc parseCommand(const char* input)
{
    CommandDesc desc = {.src = strdup(input),
                        .command = strdup(input)};

    char* token = strtok(desc.command, " ");

    while((token = strtok(NULL, " ")))
    {
        desc.params = realloc(desc.params, ++desc.count * sizeof *desc.params);
        desc.params[desc.count - 1].key = token;
    }

    for(struct Param* it = desc.params, *end = it + desc.count; it < end; it++)
    {
        if (strcmp(it->key, "=") == 0) continue;

        it->key = strtok(it->key, "=");
        it->val = strtok(NULL, "=");
    }

    return desc;
}

static void processCommand(Console* console, const char* text)
{
    console->active = false;

    *console->desc = parseCommand(text);

    if (console->desc->command)
    {
        const char* command = console->desc->command;

        FOR(const Command*, cmd, Commands)
            if(casecmp(console->desc->command, cmd->name) == 0 ||
                (cmd->alt && casecmp(console->desc->command, cmd->alt) == 0))
            {
                cmd->handler(console);
                command = NULL;
                break;
            }

        if(command)
        {
            printLine(console);
            printError(console, "unknown command: ");
            printError(console, command);
            commandDone(console);
        }
    }
    else commandDone(console);
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
    if(console->history.size)
        if(strcmp(console->history.items[console->history.index = console->history.size - 1], value) == 0)
            return;

    console->history.index = console->history.size++;
    console->history.items = realloc(console->history.items, sizeof(char*) * console->history.size);
    console->history.items[console->history.index] = strdup(value);
}

static void processConsoleCommand(Console* console)
{
    size_t commandSize = strlen(console->input.text);

    if(commandSize)
    {
        printf("%s", console->input.text);
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

#if defined (TIC_BUILD_WITH_LUA)

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

static void onHttpVersionGet(const net_get_data* data)
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
	      {
                .major = TIC_VERSION_MAJOR,
                .minor = TIC_VERSION_MINOR,
                .patch = TIC_VERSION_REVISION,
	      },
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
                sprintf(msg, " new version %i.%i.%i available", version.major, version.minor, version.patch);

                enum{Offset = (2 * STUDIO_TEXT_BUFFER_WIDTH)};

                memset(console->text + Offset, ' ', STUDIO_TEXT_BUFFER_WIDTH);
                strcpy(console->text + Offset, msg);
                memset(console->color + Offset, tic_color_red, strlen(msg));
            }
        }
        break;
    default:
        break;
    }
}

#endif

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
        tic80_input* input = &console->tic->ram->input;

        if(input->mouse.scrolly)
        {
            enum{Scroll = 3};
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            setScroll(console, console->scroll.pos + delta);
        }
    }

    tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    if(checkMousePos(console->studio, &rect))
        setCursor(console->studio, tic_cursor_ibeam);

#if defined(__TIC_ANDROID__)

    if(checkMouseDown(console->studio, &rect, tic_mouse_left))
    {
        setCursor(console->studio, tic_cursor_hand);

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

    if(checkMouseDown(console->studio, &rect, tic_mouse_left))
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

    if(checkMouseClick(console->studio, &rect, tic_mouse_middle))
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

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static s32 leftWordPos(Console* console)
{
    const char* start = console->input.text;
    const char* pos = console->input.text + console->input.pos - 1;

    if(pos > start)
    {
        if(isalnum_(*pos)) while(pos > start && isalnum_(*(pos-1))) pos--;
        else while(pos > start && !isalnum_(*(pos-1))) pos--;
        return pos - console->input.text;
    }

    return console->input.pos;
}

static s32 rightWordPos(Console* console)
{
    const char* end = console->input.text + strlen(console->input.text);
    const char* pos = console->input.text + console->input.pos;

    if(pos < end)
    {
        if(isalnum_(*pos)) while(pos < end && isalnum_(*pos)) pos++;
        else while(pos < end && !isalnum_(*pos)) pos++;
        return pos - console->input.text;
    }

    return console->input.pos;
}

static void leftWord(Console* console)
{
    console->input.pos = leftWordPos(console);
}

static void rightWord(Console* console)
{
    console->input.pos = rightWordPos(console);
}

static void deleteWord(Console* console)
{
    s32 pos = rightWordPos(console);
    deleteText(console, console->input.pos, pos);
}

static void backspaceWord(Console* console)
{
    s32 pos = leftWordPos(console);
    deleteText(console, pos, console->input.pos);
    console->input.pos = pos;
}

static void processKeyboard(Console* console)
{
    tic_mem* tic = console->tic;

    if(!console->active)
        return;

    if(tic->ram->input.keyboard.data != 0)
    {
        switch(getClipboardEvent(console->studio))
        {
        case TIC_CLIPBOARD_COPY: copyToClipboard(console); break;
        case TIC_CLIPBOARD_PASTE: copyFromClipboard(console); break;
        default: break;
        }

        console->cursor.delay = CONSOLE_CURSOR_DELAY;

        bool ctrl = tic_api_key(tic, tic_key_ctrl);
        bool alt = tic_api_key(tic, tic_key_alt);

        if (ctrl || alt)
        {
            if (ctrl)
            {
#if defined(__TIC_LINUX__)
                tic_keycode clearKey = tic_key_l;
#else
                tic_keycode clearKey = tic_key_k;
#endif

                if (keyWasPressed(console->studio, tic_key_a))      processConsoleHome(console);
                else if (keyWasPressed(console->studio, tic_key_e)) processConsoleEnd(console);
                else if (keyWasPressed(console->studio, clearKey))
                {
                    onClsCommand(console);
                    return;
                }
            }

            if (keyWasPressed(console->studio, tic_key_left))           leftWord(console);
            else if(keyWasPressed(console->studio, tic_key_right))      rightWord(console);
            else if(keyWasPressed(console->studio, tic_key_delete))     deleteWord(console);
            else if(keyWasPressed(console->studio, tic_key_backspace))  backspaceWord(console);
        }
        else
        {
            if(keyWasPressed(console->studio, tic_key_up)) onHistoryUp(console);
            else if(keyWasPressed(console->studio, tic_key_down)) onHistoryDown(console);
            else if(keyWasPressed(console->studio, tic_key_left))
            {
                if(console->input.pos > 0)
                    console->input.pos--;
            }
            else if(keyWasPressed(console->studio, tic_key_right))
            {
                console->input.pos++;
                size_t len = strlen(console->input.text);
                if(console->input.pos > len)
                    console->input.pos = len;
            }
            else if(enterWasPressed(console->studio))                            processConsoleCommand(console);
            else if(keyWasPressed(console->studio, tic_key_backspace))   processConsoleBackspace(console);
            else if(keyWasPressed(console->studio, tic_key_delete))      processConsoleDel(console);
            else if(keyWasPressed(console->studio, tic_key_home))        processConsoleHome(console);
            else if(keyWasPressed(console->studio, tic_key_end))         processConsoleEnd(console);
            else if(keyWasPressed(console->studio, tic_key_tab))         processConsoleTab(console);
            else if(keyWasPressed(console->studio, tic_key_pageup))      processConsolePgUp(console);
            else if(keyWasPressed(console->studio, tic_key_pagedown))    processConsolePgDown(console);
        }
    }

    char sym = getKeyboardText(console->studio);

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

    Start* start = getStartScreen(console->studio);

    if(console->tickCounter == 0)
    {
        if(!start->embed)
        {
            loadDemo(console, 0);

            if(!console->args.cli)
            {
                printBack(console, "\n hello! type ");
                printFront(console, "help");
                printBack(console, " for help\n");

#if defined (TIC_BUILD_WITH_LUA)
                if(getConfig(console->studio)->checkNewVersion)
                    tic_net_get(console->net, "/api?fn=version", onHttpVersionGet, console);
#endif
            }

            commandDone(console);
        }
        else printBack(console, "\n loading cart...");
    }

    if (getStudioMode(console->studio) != TIC_CONSOLE_MODE) return;

    tic_api_cls(tic, TIC_COLOR_BG);
    drawConsoleText(console);

    if(start->embed)
    {
        if(console->tickCounter >= (u32)(console->args.skip ? 1 : TIC80_FRAMERATE))
        {
            runGame(console->studio);

            start->embed = false;
            studioRomLoaded(console->studio);

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

        if(console->active)
        {
            if(console->commands.current < console->commands.count)
            {
                const char* command = console->commands.items[console->commands.current];
                if(!console->args.cli)
                    printFront(console, command);

                processCommand(console, command);

                console->commands.current++;
            }
            else if(getConfig(console->studio)->cli)
                exitStudio(console->studio);
        }
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
        tic_mem* tic = console->tic;

        if(tic_tool_has_ext(cartName, PngExt))
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
        else if(tic_project_ext(cartName))
        {
            if(tic_project_load(cartName, data, size, &tic->cart))
                done = true;
        }
#endif

        free(data);
    }

    if(done)
        studioRomLoaded(console->studio);

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

void initConsole(Console* console, Studio* studio, tic_fs* fs, tic_net* net, Config* config, StartArgs args)
{
    if(!console->text)  console->text = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->color) console->color = malloc(CONSOLE_BUFFER_SIZE);
    if(!console->desc)  console->desc = malloc(sizeof(CommandDesc));

    *console = (Console)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .config = config,
        .loadByHash = loadByHash,
        .load = loadExternal,
        .loadCart = cmdLoadCart,
        .updateProject = updateProject,
        .error = error,
        .trace = trace,
        .tick = tick,
        .save = saveCart,
        .done = commandDone,
        .cursor = {.pos.x = 1, .pos.y = 3, .delay = 0},
        .input = console->text,
        .tickCounter = 0,
        .active = false,
        .text = console->text,
        .color = console->color,
        .fs = fs,
        .net = net,
        .args = args,
        .desc = console->desc,
    };

    // parse --cmd param
    {
        char* command = args.cmd;
        while(command)
        {
            console->commands.items = realloc(console->commands.items, sizeof(char*) * (console->commands.count + 1));
            console->commands.items[console->commands.count++] = command;

            static const char Sep[] = " & ";
            command = strstr(command, Sep);

            if(command)
            {
                *command = '\0';
                command += STRLEN(Sep);
            }
        }
    }

    qsort(Commands, COUNT_OF(Commands), sizeof Commands[0], cmdcmp);
    qsort(Api, COUNT_OF(Api), sizeof Api[0], apicmp);

    memset(console->text, 0, CONSOLE_BUFFER_SIZE);
    memset(console->color, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);
    memset(console->desc, 0, sizeof(CommandDesc));

    Start* start = getStartScreen(console->studio);

    if(!console->args.cli)
    {
        memcpy(console->text, start->text, STUDIO_TEXT_BUFFER_SIZE);
        memcpy(console->color, start->color, STUDIO_TEXT_BUFFER_SIZE);

        printLine(console);
        for(const char* ptr = console->text, *end = ptr + STUDIO_TEXT_BUFFER_SIZE;
            ptr < end; ptr += CONSOLE_BUFFER_WIDTH)
            if(*ptr)
                puts(ptr);
    }

    if (args.cart)
        if (!cmdLoadCart(console, args.cart))
        {
            printf("error: cart `%s` not loaded\n", args.cart);
            exit(1);
        }
        else
            getStartScreen(console->studio)->embed = true;

    console->active = !start->embed;
}

void freeConsole(Console* console)
{
    free(console->text);
    free(console->color);

    if(console->history.items)
    {
        for(char **ptr = console->history.items, **end = ptr + console->history.size; ptr < end; ptr++)
            free(*ptr);

        free(console->history.items);
    }

    FREE(console->commands.items);
    free(console->desc);
    free(console);
}
