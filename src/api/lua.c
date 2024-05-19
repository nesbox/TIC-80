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
        lua_State* lua = core->currentVM;

        lua_settop(lua, 0);

        if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
        {
            core->data->error(core->data->data, lua_tostring(lua, -1));
            return false;
        }
    }

    return true;
}

static const char* const LuaKeywords [] =
{
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "goto", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while", 
    "self"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getLuaOutline(const char* code, s32* size)
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

                if(isalnum_(c) || c == ':');
                else if(c == '(')
                {
                    end = ptr;
                    break;
                }
                else break;

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
        else break;
    }

    return items;
}

static void evalLua(tic_mem* tic, const char* code) 
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    if (!lua) return;

    lua_settop(lua, 0);

    if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
    {
        core->data->error(core->data->data, lua_tostring(lua, -1));
    }
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
    .id                 = 10,
    .name               = "lua",
    .fileExtension      = ".lua",
    .projectComment     = "--",
    {
      .init               = initLua,
      .close              = luaapi_close,
      .tick               = luaapi_tick,
      .boot               = luaapi_boot,

      .callback           =
      {
        .scanline       = luaapi_scn,
        .border         = luaapi_bdr,
        .menu           = luaapi_menu,
      },
    },

    .getOutline         = getLuaOutline,
    .eval               = evalLua,

    .blockCommentStart  = "--[[",
    .blockCommentEnd    = "]]",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .singleComment      = "--",
    .blockStringStart   = "[[",
    .blockStringEnd     = "]]",
    .stdStringStartEnd  = "\'\"",
    .blockEnd           = "end",

    .keywords           = LuaKeywords,
    .keywordsCount      = COUNT_OF(LuaKeywords),

    .demo = {DemoRom, sizeof DemoRom},
    .mark = {MarkRom, sizeof MarkRom, "luamark.tic"},

    .demos = (struct tic_demo[])
    {
        {DemoFire,      sizeof DemoFire,        "fire.tic"},
        {DemoP3d,       sizeof DemoP3d,         "p3d.tic"},
        {DemoSfx,       sizeof DemoSfx,         "sfx.tic"},
        {DemoPalette,   sizeof DemoPalette,     "palette.tic"},
        {DemoFont,      sizeof DemoFont,        "font.tic"},
        {DemoMusic,     sizeof DemoMusic,       "music.tic"},
        {DemoQuest,     sizeof DemoQuest,       "quest.tic"},
        {DemoTetris,    sizeof DemoTetris,      "tetris.tic"},
        {DemoBenchmark, sizeof DemoBenchmark,   "benchmark.tic"},
        {DemoBpp,       sizeof DemoBpp,         "bpp.tic"},
        {DemoCar,       sizeof DemoCar,         "car.tic"},
        {0},
    },
};
