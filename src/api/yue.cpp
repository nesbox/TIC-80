extern "C" {
#include "core/core.h"
#include "luaapi.h"
}

#include "yuescript/yue_compiler.h"

static inline bool isalnum_(char c)
{
    return isalnum(c) || c == '_';
}

#define YUE_CODE(...) #__VA_ARGS__

static void evalYuescript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = (lua_State*)core->currentVM;

    yue::YueCompiler compiler;
    auto result = compiler.compile(code, yue::YueConfig());

    if (result.error)
    {
        core->data->error(core->data->data, result.error->displayMessage.c_str());
        return;
    }

    const char* luaCode = result.codes.c_str();
    if (luaL_loadstring(lua, luaCode) == LUA_OK)
    {
        if (lua_pcall(lua, 0, 0, 0) != LUA_OK)
        {
            const char* msg = lua_tostring(lua, -1);
            if (msg)
            {
                core->data->error(core->data->data, msg);
            }
        }
    }
}

static bool initYuescript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    luaapi_close(tic);

    core->currentVM = luaL_newstate();
    lua_State* lua = (lua_State*)core->currentVM;
    luaapi_open(lua);

    luaapi_init(core);

    evalYuescript(tic, code);

    return true;
}

static const char* const YueKeywords[] =
    {
        "false",
        "true",
        "nil",
        "local",
        "global",
        "return",
        "break",
        "continue",
        "for",
        "while",
        "repeat",
        "until",
        "if",
        "else",
        "elseif",
        "then",
        "switch",
        "when",
        "unless",
        "and",
        "or",
        "in",
        "not",
        "super",
        "try",
        "catch",
        "with",
        "export",
        "import",
        "from",
        "class",
        "extends",
        "using",
        "do",
        "macro",
};

static const tic_outline_item* getYueOutline(const char* code, s32* size)
{
    enum
    {
        Size = sizeof(tic_outline_item)
    };

    *size = 0;

    static tic_outline_item* items = NULL;

    if (items)
    {
        free(items);
        items = NULL;
    }

    const char* ptr = code;

    while (true)
    {
        static const char FuncString[] = "->";

        ptr = strstr(ptr, FuncString);

        if (ptr)
        {
            const char* end = ptr;

            ptr += sizeof FuncString - 1;

            while (end >= code && !isalnum_(*end))
                end--;

            const char* start = end;

            for (const char* val = start - 1; val >= code && (isalnum_(*val)); val--, start--)
                ;

            if (end > start)
            {
                tic_outline_item* new_items = (tic_outline_item*)realloc(items, (*size + 1) * Size);
                if (new_items) {
                    items = new_items;
                    items[*size].pos = start;
                    items[*size].size = (s32)(end - start + 1);
                    (*size)++;
                }
            }
        }
        else
            break;
    }

    return items;
}

static const u8 DemoRom[] =
    {
#include "../build/assets/yuedemo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/yuemark.tic.dat"
};

extern "C" TIC_EXPORT const tic_script EXPORT_SCRIPT(Yue) = {
    21,                          // id
    "yue",                       // name
    ".yue",                      // fileExtension
    "--",                        // projectComment
    {
        initYuescript,           // init
        luaapi_close,            // close
        luaapi_tick,             // tick
        luaapi_boot,             // boot
        {                        // callback
            luaapi_scn,          // scanline
            luaapi_bdr,          // border
            luaapi_menu          // menu
        }
    },
    getYueOutline,               // getOutline
    evalYuescript,               // eval
    nullptr,                     // blockCommentStart
    nullptr,                     // blockCommentEnd
    "--[[",                      // blockCommentStart2
    "]]",                        // blockCommentEnd2
    nullptr,                     // blockStringStart
    nullptr,                     // blockStringEnd
    nullptr,                     // stdStringStartEnd
    "--",                        // singleComment
    nullptr,                     // blockEnd
    YueKeywords,                 // keywords
    COUNT_OF(YueKeywords),       // keywordsCount
    nullptr,                     // lang_isalnum
    false,                       // useStructuredEdition
    false,                       // useBinarySection
    0,                           // api_keywordsCount
    nullptr,                     // api_keywords
    {                            // demo
        DemoRom,
        sizeof DemoRom,
        nullptr
    },
    {                            // mark
        MarkRom,
        sizeof MarkRom,
        "yuemark.tic"
    },
    nullptr                      // demos
};