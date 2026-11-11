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

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ctype.h>

static bool initLua(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;

    luaapi_close(tic);

    lua_State* lua = core->currentVM = luaL_newstate();
    luaapi_open(lua);

    luaapi_init(core);

    {
        lua_State* vm = core->currentVM;

        lua_settop(vm, 0);

        if(luaL_loadstring(vm, code) != LUA_OK || lua_pcall(vm, 0, LUA_MULTRET, 0) != LUA_OK)
        {
            core->data->error(core->data->data, lua_tostring(vm, -1));
            return false;
        }
    }

    return true;
}

static const char* const LuaKeywords[] =
    {
        "and", "break", "do", "else", "elseif",
        "end", "false", "for", "function", "goto", "if",
        "in", "local", "nil", "not", "or", "repeat",
        "return", "then", "true", "until", "while",
        "self"};

static inline bool isalnum_(char c)
{
    return isalnum((u8)c) || c == '_';
}

static const tic_outline_item* getLuaOutline(const char* code, s32* size)
{
    enum
    {
        Size = sizeof(tic_outline_item)
    };

    *size = 0;

    static tic_outline_item* items = NULL;

    if(items)
    {
        free(items);
        items = NULL;
    }

    const char* ptr = code;

    while(true)
    {
        static const char FuncString[] = "function ";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            ptr += sizeof FuncString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(isalnum_(c) || c == ':' || c == '.')
                    ;
                else if(c == '(')
                {
                    end = ptr;
                    break;
                }
                else
                    break;

                ptr++;
            }

            if(end > start)
            {
                items = realloc(items, (*size + 1) * Size);

                items[*size].pos = start;
                items[*size].size = (s32)(end - start);

                (*size)++;
            }
        }
        else
            break;
    }

    return items;
}

static void evalLua(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    if(!lua) return;

    lua_settop(lua, 0);

    if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
    {
        core->data->error(core->data->data, lua_tostring(lua, -1));
    }
}

// Whether a Lua message line carries a source position: chunk:line: <text>.
static bool luaHasPosition(const char* line, const char* end)
{
    for(const char* ptr = line; ptr < end; ptr++)
        if(*ptr == ':' && ptr + 1 < end && isdigit((u8)ptr[1]))
        {
            const char* num = ptr + 1;

            while(num < end && isdigit((u8)*num))
                num++;

            if(num + 1 < end && num[0] == ':' && num[1] == ' ')
                return true;
        }

    return false;
}

// The line's position when its chunk id is the cart's own. A cut chunk id has
// "..."; one Lua did not cut can only be a one-line source, the whole code.
static bool luaCartPosition(const char* line, const char* end, const char* code, s32 codeSize, bool codeSingle, s32* value)
{
    static const char Prefix[] = "[string \"";
    const char* chunk = strstr(line, Prefix);

    if(!chunk || chunk >= end)
        return false;

    const char* body = chunk + sizeof Prefix - 1;
    const char* close = strstr(body, "\"]:");

    if(!close || close >= end)
        return false;

    const bool truncated = close - body >= 3 && memcmp(close - 3, "...", 3) == 0;
    const s32 size = (s32)(close - body) - (truncated ? 3 : 0);

    if(size <= 0 || size > codeSize || memcmp(body, code, size) != 0)
        return false;

    if(!truncated && (!codeSingle || size != codeSize))
        return false;

    const char* num = close + 3;
    s32 lineValue = 0;

    while(num < end && isdigit((u8)*num))
        lineValue = lineValue * 10 + (*num++ - '0');

    if(num == close + 3 || num >= end || *num != ':')
        return false;

    *value = lineValue;
    return true;
}

// A Lua message names its source by the chunk id: [string "<first line>"...].
// Only the cart's own chunk starts with the cart's first line, so an eval
// snippet or a file cannot point into the cart's code.
static s32 luaErrorLine(tic_mem* memory, const char* text)
{
    const char* code = memory->cart.code.data;
    const char* codeEnd = strchr(code, '\n');
    const s32 codeSize = codeEnd ? (s32)(codeEnd - code) : (s32)strlen(code);
    const bool codeSingle = codeEnd == NULL;

    const char* firstEnd = strchr(text, '\n');
    if(!firstEnd)
        firstEnd = text + strlen(text);

    s32 value = 0;

    // A position in the message is the error's own: when it names another
    // chunk, the error is not in the cart and the traceback is not consulted.
    if(luaHasPosition(text, firstEnd))
        return luaCartPosition(text, firstEnd, code, codeSize, codeSingle, &value) ? value : 0;

    // No position in the message: the first traceback frame of the cart.
    for(const char* line = *firstEnd ? firstEnd + 1 : firstEnd; *line;)
    {
        const char* end = strchr(line, '\n');
        if(!end)
            end = line + strlen(line);

        if(luaCartPosition(line, end, code, codeSize, codeSingle, &value))
            return value;

        line = *end ? end + 1 : end;
    }

    return 0;
}

static const u8 DemoRom[] =
    {
#include "../build/assets/luademo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/luamark.tic.dat"
};

static const u8 DemoFire[] =
    {
#include "../build/assets/fire.tic.dat"
};

static const u8 DemoP3d[] =
    {
#include "../build/assets/p3d.tic.dat"
};

static const u8 DemoSfx[] =
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

static const u8 DemoQuest[] =
    {
#include "../build/assets/quest.tic.dat"
};

static const u8 DemoTetris[] =
    {
#include "../build/assets/tetris.tic.dat"
};

static const u8 DemoBenchmark[] =
    {
#include "../build/assets/benchmark.tic.dat"
};

static const u8 DemoBpp[] =
    {
#include "../build/assets/bpp.tic.dat"
};

static const u8 DemoCar[] =
    {
#include "../build/assets/car.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(Lua) =
    {
        .id = 10,
        .name = "lua",
        .fileExtension = ".lua",
        .projectComment = "--",
        {
            .init = initLua,
            .close = luaapi_close,
            .tick = luaapi_tick,
            .boot = luaapi_boot,

            .callback =
                {
                    .scanline = luaapi_scn,
                    .border = luaapi_bdr,
                    .menu = luaapi_menu,
                },
        },

        .getOutline = getLuaOutline,
        .eval = evalLua,
        .errorLine = luaErrorLine,

        .blockCommentStart = "--[[",
        .blockCommentEnd = "]]",
        .blockCommentStart2 = NULL,
        .blockCommentEnd2 = NULL,
        .singleComment = "--",
        .blockStringStart = "[[",
        .blockStringEnd = "]]",
        .stdStringStartEnd = "\'\"",
        .blockEnd = "end",

        .keywords = LuaKeywords,
        .keywordsCount = COUNT_OF(LuaKeywords),

        .demo = {DemoRom, sizeof DemoRom, NULL},
        .mark = {MarkRom, sizeof MarkRom, "luamark.tic"},

        .demos = (struct tic_demo[]){
            {DemoFire, sizeof DemoFire, "fire.tic"},
            {DemoP3d, sizeof DemoP3d, "p3d.tic"},
            {DemoSfx, sizeof DemoSfx, "sfx.tic"},
            {DemoPalette, sizeof DemoPalette, "palette.tic"},
            {DemoFont, sizeof DemoFont, "font.tic"},
            {DemoMusic, sizeof DemoMusic, "music.tic"},
            {DemoQuest, sizeof DemoQuest, "quest.tic"},
            {DemoTetris, sizeof DemoTetris, "tetris.tic"},
            {DemoBenchmark, sizeof DemoBenchmark, "benchmark.tic"},
            {DemoBpp, sizeof DemoBpp, "bpp.tic"},
            {DemoCar, sizeof DemoCar, "car.tic"},
            {0},
        },
};
