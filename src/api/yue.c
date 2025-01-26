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

#include "core/core.h"
#include "luaapi.h"
#include "yue_wrapper/yue_wrapper.h"

static inline bool isalnum_(char c)
{
    return isalnum(c) || c == '_';
}

#define YUE_CODE(...) #__VA_ARGS__

static void evalYuescript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = (lua_State*)core->currentVM;

    yue_get_parser_instance();

    YueCompiler_t* compiler = yue_compiler_create(NULL, NULL, false);
    YueConfig_t* config = yue_config_create();
    CompileInfo_t* result = yue_compile(compiler, code, config);

    if (result->error_msg)
    {
        core->data->error(core->data->data, result->error_display_message);
        yue_compile_info_free(result);
        yue_config_free(config);
        yue_compiler_destroy(compiler);
        return;
    }

    const char* luaCode = result->codes;
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

    yue_compile_info_free(result);
    yue_config_free(config);
    yue_compiler_destroy(compiler);
}

static bool initYuescript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    luaapi_close(tic);

    yue_get_parser_instance();

    core->currentVM = luaL_newstate();
    lua_State* lua = (lua_State*)core->currentVM;

    void (*open_func)(void*) = (void (*)(void*))luaapi_open;
    open_func(lua);

    // Create compiler with the Lua state and open_func
    YueCompiler_t* compiler = yue_compiler_create(lua, open_func, false);
    YueConfig_t* config = yue_config_create();
    CompileInfo_t* result = yue_compile(compiler, code, config);

    luaapi_init(core);

    if (result->error_msg)
    {
        core->data->error(core->data->data, result->error_display_message);
        yue_compile_info_free(result);
        yue_config_free(config);
        yue_compiler_destroy(compiler);
        return false;
    }

    bool success = true;
    if (luaL_loadstring(lua, result->codes) != LUA_OK || 
        lua_pcall(lua, 0, 0, 0) != LUA_OK)
    {
        const char* msg = lua_tostring(lua, -1);
        if (msg) core->data->error(core->data->data, msg);
        success = false;
    }

    yue_compile_info_free(result);
    yue_config_free(config);
    yue_compiler_destroy(compiler);
    return success;
}

static void closeYuescript(tic_mem* tic)
{
    luaapi_close(tic);
    yue_cleanup();
}

static const char* const YueKeywords[] = {
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

    while (1)
    {
        static const char FuncString[] = "=->";

        ptr = strstr(ptr, FuncString);

        if (ptr)
        {
            const char* end = ptr;

            ptr += sizeof FuncString - 1;

            while (end >= code && !isalnum_(*end))
                end--;

            const char* start = end;

            for (const char* val = start - 1; val >= code && isalnum_(*val); val--, start--)
                ;

            if (end > start)
            {
                tic_outline_item* new_items = (tic_outline_item*)realloc(items, (*size + 1) * Size);
                if (new_items)
                {
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

static const u8 DemoRom[] = {
#include "../build/assets/yuedemo.tic.dat"
};

static const u8 MarkRom[] = {
#include "../build/assets/yuemark.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(Yue) =
{
    .id = 21,
    .name = "yue",
    .fileExtension = ".yue",
    .projectComment = "--",
    {
        .init = initYuescript,
        .close = closeYuescript,
        .tick = luaapi_tick,
        .boot = luaapi_boot,

        .callback =
            {
                .scanline = luaapi_scn,
                .border = luaapi_bdr,
                .menu = luaapi_menu,
            },
    },

    .getOutline = getYueOutline,
    .eval = evalYuescript,

    .blockCommentStart = "--[[",
    .blockCommentEnd = "]]",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2 = NULL,

    .blockStringStart = NULL,
    .blockStringEnd = NULL,
    .singleComment = "--",
    .blockEnd = NULL,

    .keywords = YueKeywords,
    .keywordsCount = COUNT_OF(YueKeywords),

    .demo = {DemoRom, sizeof DemoRom},
    .mark = {MarkRom, sizeof MarkRom, "yuemark.tic"},
};