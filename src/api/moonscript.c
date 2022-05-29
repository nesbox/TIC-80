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

// Moonscript requires Lua
#if defined(TIC_BUILD_WITH_LUA)

#include "lua_api.h"

#if defined(TIC_BUILD_WITH_MOON)

static const char _ms_loadstring[] = "_ms_loadstring";

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

#include "moonscript.h"

#define MOON_CODE(...) #__VA_ARGS__

static const char* execute_moonscript_src = MOON_CODE(
    local fn, err = require('moonscript.base').loadstring(...)

    if not fn then
        error(err)
    end
    return fn()
);

static void setloaded(lua_State* l, char* name)
{
    s32 top = lua_gettop(l);
    lua_getglobal(l, "package");
    lua_getfield(l, -1, "loaded");
    lua_getfield(l, -1, name);
    if (lua_isnil(l, -1)) {
        lua_pop(l, 1);
        lua_pushvalue(l, top);
        lua_setfield(l, -2, name);
    }

    lua_settop(l, top);
}

static void evalMoonscript(tic_mem* tic, const char* code) {
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    lua_getglobal(lua, _ms_loadstring);

    lua_pushstring(lua, code);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK)
    {
        const char* msg = lua_tostring(lua, -1);
        if (msg)
        {
            core->data->error(core->data->data, msg);
        }
    }
}

static bool initMoonscript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeLua(tic);

    lua_State* lua = core->currentVM = luaL_newstate();
    lua_open_builtins(lua);

    luaopen_lpeg(lua);
    setloaded(lua, "lpeg");

    initLuaAPI(core);

    {
        lua_State* moon = lua;

        lua_settop(moon, 0);

        if (luaL_loadbuffer(moon, (const char *)moonscript_lua, moonscript_lua_len, "moonscript.lua") != LUA_OK)
        {
            core->data->error(core->data->data, "failed to load moonscript.lua");
            return false;
        }

        lua_call(moon, 0, 0);

        if (luaL_loadbuffer(moon, execute_moonscript_src, strlen(execute_moonscript_src), "execute_moonscript") != LUA_OK)
        {
            core->data->error(core->data->data, "failed to load moonscript compiler");
            return false;
        }

        lua_setglobal(lua, _ms_loadstring);
        lua_getglobal(lua, _ms_loadstring);

        lua_pushstring(moon, code);
        if (lua_pcall(moon, 1, 1, 0) != LUA_OK)
        {
            const char* msg = lua_tostring(moon, -1);

            if (msg)
            {
                core->data->error(core->data->data, msg);
                return false;
            }
        }
    }

    return true;
}

static const char* const MoonKeywords [] =
{
    "false", "true", "nil", "local", "return",
    "break", "continue", "for", "while",
    "if", "else", "elseif", "unless", "switch",
    "when", "and", "or", "in", "do",
    "not", "super", "try", "catch",
    "with", "export", "import", "then",
    "from", "class", "extends", "new", "using",
};

static const tic_outline_item* getMoonOutline(const char* code, s32* size)
{
    enum{Size = sizeof(tic_outline_item)};

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
        static const char FuncString[] = "=->";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            const char* end = ptr;

            ptr += sizeof FuncString - 1;

            while(end >= code && !isalnum_(*end))  end--;

            const char* start = end;

            for (const char* val = start-1; val >= code && (isalnum_(*val)); val--, start--);

            if(end > start)
            {
                items = realloc(items, (*size + 1) * Size);

                items[*size].pos = start;
                items[*size].size = (s32)(end - start + 1);

                (*size)++;
            }
        }
        else break;
    }

    return items;
}

tic_script_config MoonSyntaxConfig = 
{
    .id                 = 13,
    .name               = "moon",
    .fileExtension      = ".moon",
    .projectComment     = "--",
    {
      .init               = initMoonscript,
      .close              = closeLua,
      .tick               = callLuaTick,
      .boot               = callLuaBoot,

      .callback           =
      {
        .scanline       = callLuaScanline,
        .border         = callLuaBorder,
        .menu           = callLuaMenu,
      },
    },

    .getOutline         = getMoonOutline,
    .eval               = evalMoonscript,

    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = "--",
    .blockEnd           = NULL,

    .keywords           = MoonKeywords,
    .keywordsCount      = COUNT_OF(MoonKeywords),
};

#endif /* defined(TIC_BUILD_WITH_MOON) */

#endif /* defined(TIC_BUILD_WITH_LUA) */
