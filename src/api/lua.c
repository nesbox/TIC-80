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

#if defined(TIC_BUILD_WITH_LUA)

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ctype.h>

s32 luaopen_lpeg(lua_State *lua);

static inline s32 getLuaNumber(lua_State* lua, s32 index)
{
    return (s32)lua_tonumber(lua, index);
}

static void registerLuaFunction(tic_core* core, lua_CFunction func, const char *name)
{
    lua_pushlightuserdata(core->currentVM, core);
    lua_pushcclosure(core->currentVM, func, 1);
    lua_setglobal(core->currentVM, name);
}

static tic_core* getLuaCore(lua_State* lua)
{
    tic_core* core = lua_touserdata(lua, lua_upvalueindex(1));
    return core;
}

static s32 lua_peek(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top >= 1)
    {
        s32 address = getLuaNumber(lua, 1);
        s32 bits = BITS_IN_BYTE;

        if(top == 2)
            bits = getLuaNumber(lua, 2);

        lua_pushinteger(lua, tic_api_peek(tic, address, bits));
        return 1;
    }
    else luaL_error(lua, "invalid parameters, peek(addr,bits)\n");

    return 0;
}

static s32 lua_poke(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top >= 2)
    {
        s32 address = getLuaNumber(lua, 1);
        u8 value = getLuaNumber(lua, 2);
        s32 bits = BITS_IN_BYTE;

        if(top == 3)
            bits = getLuaNumber(lua, 3);

        tic_api_poke(tic, address, value, bits);
    }
    else luaL_error(lua, "invalid parameters, poke(addr,val,bits)\n");

    return 0;
}

static s32 lua_peek1(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 1)
    {
        s32 address = getLuaNumber(lua, 1);
        lua_pushinteger(lua, tic_api_peek1(tic, address));
        return 1;
    }
    else luaL_error(lua, "invalid parameters, peek1(addr)\n");

    return 0;
}

static s32 lua_poke1(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 2)
    {
        s32 address = getLuaNumber(lua, 1);
        u8 value = getLuaNumber(lua, 2);

        tic_api_poke1(tic, address, value);
    }
    else luaL_error(lua, "invalid parameters, poke1(addr,val)\n");

    return 0;
}

static s32 lua_peek2(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 1)
    {
        s32 address = getLuaNumber(lua, 1);
        lua_pushinteger(lua, tic_api_peek2(tic, address));
        return 1;
    }
    else luaL_error(lua, "invalid parameters, peek2(addr)\n");

    return 0;
}

static s32 lua_poke2(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 2)
    {
        s32 address = getLuaNumber(lua, 1);
        u8 value = getLuaNumber(lua, 2);

        tic_api_poke2(tic, address, value);
    }
    else luaL_error(lua, "invalid parameters, poke2(addr,val)\n");

    return 0;
}

static s32 lua_peek4(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 1)
    {
        s32 address = getLuaNumber(lua, 1);
        lua_pushinteger(lua, tic_api_peek4(tic, address));
        return 1;
    }
    else luaL_error(lua, "invalid parameters, peek4(addr)\n");

    return 0;
}

static s32 lua_poke4(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 2)
    {
        s32 address = getLuaNumber(lua, 1);
        u8 value = getLuaNumber(lua, 2);

        tic_api_poke4(tic, address, value);
    }
    else luaL_error(lua, "invalid parameters, poke4(addr,val)\n");

    return 0;
}

static s32 lua_cls(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    tic_api_cls(tic, top == 1 ? getLuaNumber(lua, 1) : 0);

    return 0;
}

static s32 lua_pix(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top >= 2)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        if(top >= 3)
        {
            s32 color = getLuaNumber(lua, 3);
            tic_api_pix(tic, x, y, color, false);
        }
        else
        {
            lua_pushinteger(lua, tic_api_pix(tic, x, y, 0, true));
            return 1;
        }

    }
    else luaL_error(lua, "invalid parameters, pix(x y [color])\n");

    return 0;
}

static s32 lua_line(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 5)
    {
        float x0 = lua_tonumber(lua, 1);
        float y0 = lua_tonumber(lua, 2);
        float x1 = lua_tonumber(lua, 3);
        float y1 = lua_tonumber(lua, 4);
        s32 color = getLuaNumber(lua, 5);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_line(tic, x0, y0, x1, y1, color);
    }
    else luaL_error(lua, "invalid parameters, line(x0,y0,x1,y1,color)\n");

    return 0;
}

static s32 lua_rect(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 5)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 w = getLuaNumber(lua, 3);
        s32 h = getLuaNumber(lua, 4);
        s32 color = getLuaNumber(lua, 5);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_rect(tic, x, y, w, h, color);
    }
    else luaL_error(lua, "invalid parameters, rect(x,y,w,h,color)\n");

    return 0;
}

static s32 lua_rectb(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 5)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 w = getLuaNumber(lua, 3);
        s32 h = getLuaNumber(lua, 4);
        s32 color = getLuaNumber(lua, 5);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_rectb(tic, x, y, w, h, color);
    }
    else luaL_error(lua, "invalid parameters, rectb(x,y,w,h,color)\n");

    return 0;
}

static s32 lua_circ(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 4)
    {       
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 radius = getLuaNumber(lua, 3);
        s32 color = getLuaNumber(lua, 4);

        tic_api_circ(tic, x, y, radius, color);
    }
    else luaL_error(lua, "invalid parameters, circ(x,y,radius,color)\n");

    return 0;
}

static s32 lua_circb(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 4)
    {
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 radius = getLuaNumber(lua, 3);
        s32 color = getLuaNumber(lua, 4);

        tic_api_circb(tic, x, y, radius, color);
    }
    else luaL_error(lua, "invalid parameters, circb(x,y,radius,color)\n");

    return 0;
}

static s32 lua_elli(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 5)
    {       
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 a = getLuaNumber(lua, 3);
        s32 b = getLuaNumber(lua, 4);
        s32 color = getLuaNumber(lua, 5);

        tic_api_elli(tic, x, y, a, b, color);
    }
    else luaL_error(lua, "invalid parameters, elli(x,y,a,b,color)\n");

    return 0;
}

static s32 lua_ellib(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 5)
    {
        tic_mem* tic = (tic_mem*)getLuaCore(lua);
       
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 a = getLuaNumber(lua, 3);
        s32 b = getLuaNumber(lua, 4);
        s32 color = getLuaNumber(lua, 5);

        tic_api_ellib(tic, x, y, a, b, color);
    }
    else luaL_error(lua, "invalid parameters, ellib(x,y,a,b,color)\n");

    return 0;
}

static s32 lua_tri(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 7)
    {
        float pt[6];

        for(s32 i = 0; i < COUNT_OF(pt); i++)
            pt[i] = lua_tonumber(lua, i+1);
        
        s32 color = getLuaNumber(lua, 7);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
    }
    else luaL_error(lua, "invalid parameters, tri(x1,y1,x2,y2,x3,y3,color)\n");

    return 0;
}

static s32 lua_trib(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 7)
    {
        float pt[6];

        for(s32 i = 0; i < COUNT_OF(pt); i++)
            pt[i] = lua_tonumber(lua, i+1);
        
        s32 color = getLuaNumber(lua, 7);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_trib(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
    }
    else luaL_error(lua, "invalid parameters, trib(x1,y1,x2,y2,x3,y3,color)\n");

    return 0;
}

#if defined(BUILD_DEPRECATED)

static s32 lua_textri(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if (top >= 12)
    {
        float pt[12];

        for (s32 i = 0; i < COUNT_OF(pt); i++)
            pt[i] = (float)lua_tonumber(lua, i + 1);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);
        static u8 colors[TIC_PALETTE_SIZE];
        s32 count = 0;
        bool use_map = false;

        //  check for use map 
        if (top >= 13)
            use_map = lua_toboolean(lua, 13);

        //  check for chroma 
        if(top >= 14)
        {
            if(lua_istable(lua, 14))
            {
                for(s32 i = 1; i <= TIC_PALETTE_SIZE; i++)
                {
                    lua_rawgeti(lua, 14, i);
                    if(lua_isnumber(lua, -1))
                    {
                        colors[i-1] = getLuaNumber(lua, -1);
                        count++;
                        lua_pop(lua, 1);
                    }
                    else
                    {
                        lua_pop(lua, 1);
                        break;
                    }
                }
            }
            else 
            {
                colors[0] = getLuaNumber(lua, 14);
                count = 1;
            }
        }

        tic_core_textri_dep(getLuaCore(lua), 
            pt[0], pt[1],   //  xy 1
            pt[2], pt[3],   //  xy 2
            pt[4], pt[5],   //  xy 3
            pt[6], pt[7],   //  uv 1
            pt[8], pt[9],   //  uv 2
            pt[10], pt[11], //  uv 3
            use_map,        // use map
            colors, count); // chroma
    }

    return 0;
}

#endif

static s32 lua_ttri(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if (top >= 12)
    {
        float pt[12];

        for (s32 i = 0; i < COUNT_OF(pt); i++)
            pt[i] = (float)lua_tonumber(lua, i + 1);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);
        static u8 colors[TIC_PALETTE_SIZE];
        s32 count = 0;
        tic_texture_src src = tic_tiles_texture;

        //  check for texture src
        if (top >= 13)
        {
            src = lua_isboolean(lua, 13) 
                ? (lua_toboolean(lua, 13) ? tic_map_texture : tic_tiles_texture) 
                : lua_tointeger(lua, 13);
        }
        //  check for chroma 
        if(top >= 14)
        {
            if(lua_istable(lua, 14))
            {
                for(s32 i = 1; i <= TIC_PALETTE_SIZE; i++)
                {
                    lua_rawgeti(lua, 14, i);
                    if(lua_isnumber(lua, -1))
                    {
                        colors[i-1] = getLuaNumber(lua, -1);
                        count++;
                        lua_pop(lua, 1);
                    }
                    else
                    {
                        lua_pop(lua, 1);
                        break;
                    }
                }
            }
            else 
            {
                colors[0] = getLuaNumber(lua, 14);
                count = 1;
            }
        }

        float z[3];
        bool depth = false;

        if(top == 17)
        {
            for (s32 i = 0; i < COUNT_OF(z); i++)
                z[i] = (float)lua_tonumber(lua, i + 15);

            depth = true;
        }

        tic_api_ttri(tic, pt[0], pt[1],   //  xy 1
                            pt[2], pt[3],   //  xy 2
                            pt[4], pt[5],   //  xy 3
                            pt[6], pt[7],   //  uv 1
                            pt[8], pt[9],   //  uv 2
                            pt[10], pt[11], //  uv 3
                            src,            // texture source
                            colors, count,  // chroma
                            z[0], z[1], z[2], depth); // depth
    }
    else luaL_error(lua, "invalid parameters, ttri(x1,y1,x2,y2,x3,y3,u1,v1,u2,v2,u3,v3,[src=0],[chroma=off],[z1=0],[z2=0],[z3=0])\n");
    return 0;
}


static s32 lua_clip(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 0)
    {
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    }
    else if(top == 4)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        s32 w = getLuaNumber(lua, 3);
        s32 h = getLuaNumber(lua, 4);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_clip((tic_mem*)getLuaCore(lua), x, y, w, h);
    }
    else luaL_error(lua, "invalid parameters, use clip(x,y,w,h) or clip()\n");

    return 0;
}

static s32 lua_btnp(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = (tic_mem*)core;

    s32 top = lua_gettop(lua);

    if (top == 0)
    {
        lua_pushinteger(lua, tic_api_btnp(tic, -1, -1, -1));
    }
    else if(top == 1)
    {
        s32 index = getLuaNumber(lua, 1) & 0x1f;

        lua_pushboolean(lua, tic_api_btnp(tic, index, -1, -1));
    }
    else if (top == 3)
    {
        s32 index = getLuaNumber(lua, 1) & 0x1f;
        u32 hold = getLuaNumber(lua, 2);
        u32 period = getLuaNumber(lua, 3);

        lua_pushboolean(lua, tic_api_btnp(tic, index, hold, period));
    }
    else
    {
        luaL_error(lua, "invalid params, btnp [ id [ hold period ] ]\n");
        return 0;
    }

    return 1;
}

static s32 lua_btn(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = (tic_mem*)core;

    s32 top = lua_gettop(lua);

    if (top == 0)
    {
        lua_pushinteger(lua, tic_api_btn(tic, -1));
    }
    else if (top == 1)
    {
        bool pressed = tic_api_btn(tic, getLuaNumber(lua, 1) & 0x1f);
        lua_pushboolean(lua, pressed);
    }
    else
    {
        luaL_error(lua, "invalid params, btn [ id ]\n");
        return 0;
    } 

    return 1;
}

static s32 lua_spr(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    s32 index = 0;
    s32 x = 0;
    s32 y = 0;
    s32 w = 1;
    s32 h = 1;
    s32 scale = 1;
    tic_flip flip = tic_no_flip;
    tic_rotate rotate = tic_no_rotate;
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(top >= 1) 
    {
        index = getLuaNumber(lua, 1);

        if(top >= 3)
        {
            x = getLuaNumber(lua, 2);
            y = getLuaNumber(lua, 3);

            if(top >= 4)
            {
                if(lua_istable(lua, 4))
                {
                    for(s32 i = 1; i <= TIC_PALETTE_SIZE; i++)
                    {
                        lua_rawgeti(lua, 4, i);
                        if(lua_isnumber(lua, -1))
                        {
                            colors[i-1] = getLuaNumber(lua, -1);
                            count++;
                            lua_pop(lua, 1);
                        }
                        else
                        {
                            lua_pop(lua, 1);
                            break;
                        }
                    }
                }
                else 
                {
                    colors[0] = getLuaNumber(lua, 4);
                    count = 1;
                }

                if(top >= 5)
                {
                    scale = getLuaNumber(lua, 5);

                    if(top >= 6)
                    {
                        flip = getLuaNumber(lua, 6);

                        if(top >= 7)
                        {
                            rotate = getLuaNumber(lua, 7);

                            if(top >= 9)
                            {
                                w = getLuaNumber(lua, 8);
                                h = getLuaNumber(lua, 9);
                            }
                        }
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    tic_api_spr(tic, index, x, y, w, h, colors, count, scale, flip, rotate);

    return 0;
}

static s32 lua_mget(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 2)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        u8 value = tic_api_mget(tic, x, y);
        lua_pushinteger(lua, value);
        return 1;
    }
    else luaL_error(lua, "invalid params, mget(x,y)\n");

    return 0;
}

static s32 lua_mset(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 3)
    {
        s32 x = getLuaNumber(lua, 1);
        s32 y = getLuaNumber(lua, 2);
        u8 val = getLuaNumber(lua, 3);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        tic_api_mset(tic, x, y, val);
    }
    else luaL_error(lua, "invalid params, mget(x,y)\n");

    return 0;
}

typedef struct
{
    lua_State* lua;
    s32 reg;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    RemapData* remap = (RemapData*)data;
    lua_State* lua = remap->lua;

    lua_rawgeti(lua, LUA_REGISTRYINDEX, remap->reg);
    lua_pushinteger(lua, result->index);
    lua_pushinteger(lua, x);
    lua_pushinteger(lua, y);
    lua_pcall(lua, 3, 3, 0);

    result->index = getLuaNumber(lua, -3);
    result->flip = getLuaNumber(lua, -2);
    result->rotate = getLuaNumber(lua, -1);
}

static s32 lua_map(lua_State* lua)
{
    s32 x = 0;
    s32 y = 0;
    s32 w = TIC_MAP_SCREEN_WIDTH;
    s32 h = TIC_MAP_SCREEN_HEIGHT;
    s32 sx = 0;
    s32 sy = 0;
    s32 scale = 1;
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    s32 top = lua_gettop(lua);

    if(top >= 2) 
    {
        x = getLuaNumber(lua, 1);
        y = getLuaNumber(lua, 2);

        if(top >= 4)
        {
            w = getLuaNumber(lua, 3);
            h = getLuaNumber(lua, 4);

            if(top >= 6)
            {
                sx = getLuaNumber(lua, 5);
                sy = getLuaNumber(lua, 6);

                if(top >= 7)
                {
                    if(lua_istable(lua, 7))
                    {
                        for(s32 i = 1; i <= TIC_PALETTE_SIZE; i++)
                        {
                            lua_rawgeti(lua, 7, i);
                            if(lua_isnumber(lua, -1))
                            {
                                colors[i-1] = getLuaNumber(lua, -1);
                                count++;
                                lua_pop(lua, 1);
                            }
                            else
                            {
                                lua_pop(lua, 1);
                                break;
                            }
                        }
                    }
                    else 
                    {
                        colors[0] = getLuaNumber(lua, 7);
                        count = 1;
                    }

                    if(top >= 8)
                    {
                        scale = getLuaNumber(lua, 8);

                        if(top >= 9)
                        {
                            if (lua_isfunction(lua, 9))
                            {
                                s32 remap = luaL_ref(lua, LUA_REGISTRYINDEX);

                                RemapData data = {lua, remap};

                                tic_mem* tic = (tic_mem*)getLuaCore(lua);

                                tic_api_map(tic, x, y, w, h, sx, sy, colors, count, scale, remapCallback, &data);

                                luaL_unref(lua, LUA_REGISTRYINDEX, data.reg);

                                return 0;
                            }                           
                        }
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    tic_api_map((tic_mem*)getLuaCore(lua), x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);

    return 0;
}

static s32 lua_music(lua_State* lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top == 0) tic_api_music(tic, -1, 0, 0, false, false, -1, -1);
    else if(top >= 1)
    {
        s32 track = getLuaNumber(lua, 1);

        if(track > MUSIC_TRACKS - 1)
        {
            luaL_error(lua, "invalid music track index");
            return 0;
        }

        tic_api_music(tic, -1, 0, 0, false, false, -1, -1);

        s32 frame = -1;
        s32 row = -1;
        bool loop = true;
        bool sustain = false;
        s32 tempo = -1;
        s32 speed = -1;

        if(top >= 2)
        {
            frame = getLuaNumber(lua, 2);

            if(top >= 3)
            {
                row = getLuaNumber(lua, 3);

                if(top >= 4)
                {
                    loop = lua_toboolean(lua, 4);

                    if (top >= 5)
                    {
                        sustain = lua_toboolean(lua, 5);

                        if (top >= 6)
                        {
                            tempo = getLuaNumber(lua, 6);

                            if (top >= 7)
                            {
                                speed = getLuaNumber(lua, 7);
                            }
                        }
                    }

                }
            }
        }

        tic_api_music(tic, track, frame, row, loop, sustain, tempo, speed);
    }
    else luaL_error(lua, "invalid params, use music(track)\n");

    return 0;
}

static s32 lua_sfx(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top >= 1)
    {
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        s32 note = -1;
        s32 octave = -1;
        s32 duration = -1;
        s32 channel = 0;
        s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
        s32 speed = SFX_DEF_SPEED;

        s32 index = getLuaNumber(lua, 1);

        if(index < SFX_COUNT)
        {
            if (index >= 0)
            {
                tic_sample* effect = tic->ram->sfx.samples.data + index;

                note = effect->note;
                octave = effect->octave;
                speed = effect->speed;
            }

            if(top >= 2)
            {
                if(lua_isinteger(lua, 2))
                {
                    s32 id = getLuaNumber(lua, 2);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
                else if(lua_isstring(lua, 2))
                {
                    const char* noteStr = lua_tostring(lua, 2);

                    if(!tic_tool_parse_note(noteStr, &note, &octave))
                    {
                        luaL_error(lua, "invalid note, should be like C#4\n");
                        return 0;
                    }
                }

                if(top >= 3)
                {
                    duration = getLuaNumber(lua, 3);

                    if(top >= 4)
                    {
                        channel = getLuaNumber(lua, 4);

                        if(top >= 5)
                        {
                            if(lua_istable(lua, 5))
                            {
                                for(s32 i = 0; i < COUNT_OF(volumes); i++)
                                {
                                    volumes[i] = lua_rawgeti(lua, 5, i + 1);
                                    lua_pop(lua, 1);
                                }
                            }
                            else volumes[0] = volumes[1] = getLuaNumber(lua, 5);

                            if(top >= 6)
                            {
                                speed = getLuaNumber(lua, 6);
                            }
                        }
                    }                   
                }
            }

            if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
            {
                tic_api_sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
            }
            else luaL_error(lua, "unknown channel\n");
        }
        else luaL_error(lua, "unknown sfx index\n");
    }
    else luaL_error(lua, "invalid sfx params\n");

    return 0;
}

static s32 lua_vbank(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = (tic_mem*)core;

    s32 prev = core->state.vbank.id;

    if(lua_gettop(lua) == 1)
        tic_api_vbank(tic, getLuaNumber(lua, 1));

    lua_pushinteger(lua, prev);
    return 1;
}

static s32 lua_sync(lua_State* lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    bool toCart = false;
    u32 mask = 0;
    s32 bank = 0;

    if(lua_gettop(lua) >= 1)
    {
        mask = getLuaNumber(lua, 1);

        if(lua_gettop(lua) >= 2)
        {
            bank = getLuaNumber(lua, 2);

            if(lua_gettop(lua) >= 3)
            {
                toCart = lua_toboolean(lua, 3);
            }
        }
    }

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else
        luaL_error(lua, "sync() error, invalid bank");

    return 0;
}

static s32 lua_reset(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);

    core->state.initialized = false;

    return 0;
}

static s32 lua_key(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = &core->memory;

    s32 top = lua_gettop(lua);

    if (top == 0)
    {
        lua_pushboolean(lua, tic_api_key(tic, tic_key_unknown));
    }
    else if (top == 1)
    {
        tic_key key = getLuaNumber(lua, 1);

        if(key < tic_keys_count)
            lua_pushboolean(lua, tic_api_key(tic, key));
        else
        {
            luaL_error(lua, "unknown keyboard code\n");
            return 0;
        }
    }
    else
    {
        luaL_error(lua, "invalid params, key [code]\n");
        return 0;
    } 

    return 1;
}

static s32 lua_keyp(lua_State* lua)
{
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = &core->memory;

    s32 top = lua_gettop(lua);

    if (top == 0)
    {
        lua_pushboolean(lua, tic_api_keyp(tic, tic_key_unknown, -1, -1));
    }
    else
    {
        tic_key key = getLuaNumber(lua, 1);

        if(key >= tic_keys_count)
        {
            luaL_error(lua, "unknown keyboard code\n");
        }
        else
        {
            if(top == 1)
            {
                lua_pushboolean(lua, tic_api_keyp(tic, key, -1, -1));
            }
            else if(top == 3)
            {
                u32 hold = getLuaNumber(lua, 2);
                u32 period = getLuaNumber(lua, 3);

                lua_pushboolean(lua, tic_api_keyp(tic, key, hold, period));
            }
            else
            {
                luaL_error(lua, "invalid params, keyp [ code [ hold period ] ]\n");
                return 0;
            }
        }
    }

    return 1;
}

static s32 lua_memcpy(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 3)
    {
        s32 dest = getLuaNumber(lua, 1);
        s32 src = getLuaNumber(lua, 2);
        s32 size = getLuaNumber(lua, 3);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);
        tic_api_memcpy(tic, dest, src, size);
    }
    else luaL_error(lua, "invalid params, memcpy(dest,src,size)\n");

    return 0;
}

static s32 lua_memset(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top == 3)
    {
        s32 dest = getLuaNumber(lua, 1);
        u8 value = getLuaNumber(lua, 2);
        s32 size = getLuaNumber(lua, 3);

        tic_mem* tic = (tic_mem*)getLuaCore(lua);
        tic_api_memset(tic, dest, value, size);
    }
    else luaL_error(lua, "invalid params, memset(dest,val,size)\n");

    return 0;
}

static const char* printString(lua_State* lua, s32 index)
{
    lua_getglobal(lua, "tostring");
    lua_pushvalue(lua, -1);
    lua_pushvalue(lua, index);
    lua_call(lua, 1, 1);

    const char* text = lua_tostring(lua, -1);

    lua_pop(lua, 2);

    return text;
}

static s32 lua_font(lua_State* lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);
    s32 top = lua_gettop(lua);

    if(top >= 1)
    {
        const char* text = printString(lua, 1);
        s32 x = 0;
        s32 y = 0;
        s32 width = TIC_SPRITESIZE;
        s32 height = TIC_SPRITESIZE;
        u8 chromakey = 0;
        bool fixed = false;
        s32 scale = 1;
        bool alt = false;

        if(top >= 3)
        {
            x = getLuaNumber(lua, 2);
            y = getLuaNumber(lua, 3);

            if(top >= 4)
            {
                chromakey = getLuaNumber(lua, 4);

                if(top >= 6)
                {
                    width = getLuaNumber(lua, 5);
                    height = getLuaNumber(lua, 6);

                    if(top >= 7)
                    {
                        fixed = lua_toboolean(lua, 7);

                        if(top >= 8)
                        {
                            scale = getLuaNumber(lua, 8);

                            if(top >= 9)
                            {
                                alt = lua_toboolean(lua, 9);
                            }
                        }
                    }
                }
            }
        }

        if(scale == 0)
        {
            lua_pushinteger(lua, 0);
            return 1;
        }

        s32 size = tic_api_font(tic, text, x, y, &chromakey, 1, width, height, fixed, scale, alt);

        lua_pushinteger(lua, size);

        return 1;
    }

    return 0;
}

static s32 lua_print(lua_State* lua)
{
    s32 top = lua_gettop(lua);

    if(top >= 1) 
    {
        tic_mem* tic = (tic_mem*)getLuaCore(lua);

        s32 x = 0;
        s32 y = 0;
        s32 color = TIC_DEFAULT_COLOR;
        bool fixed = false;
        s32 scale = 1;
        bool alt = false;

        const char* text = printString(lua, 1);

        if(top >= 3)
        {
            x = getLuaNumber(lua, 2);
            y = getLuaNumber(lua, 3);

            if(top >= 4)
            {
                color = getLuaNumber(lua, 4) % TIC_PALETTE_SIZE;

                if(top >= 5)
                {
                    fixed = lua_toboolean(lua, 5);

                    if(top >= 6)
                    {
                        scale = getLuaNumber(lua, 6);

                        if(top >= 7)
                        {
                            alt = lua_toboolean(lua, 7);
                        }
                    }
                }
            }
        }

        if(scale == 0)
        {
            lua_pushinteger(lua, 0);
            return 1;
        }

        s32 size = tic_api_print(tic, text ? text : "nil", x, y, color, fixed, scale, alt);

        lua_pushinteger(lua, size);

        return 1;
    }

    return 0;
}

static s32 lua_trace(lua_State *lua)
{
    s32 top = lua_gettop(lua);
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    if(top >= 1)
    {
        const char* text = printString(lua, 1);
        u8 color = TIC_DEFAULT_COLOR;

        if(top >= 2)
        {
            color = getLuaNumber(lua, 2);
        }

        tic_api_trace(tic, text, color);
    }
    else luaL_error(lua, "invalid params, trace(text,[color])\n");

    return 0;
}

static s32 lua_pmem(lua_State *lua)
{
    s32 top = lua_gettop(lua);
    tic_core* core = getLuaCore(lua);
    tic_mem* tic = &core->memory;

    if(top >= 1)
    {
        u32 index = getLuaNumber(lua, 1);

        if(index < TIC_PERSISTENT_SIZE)
        {
            u32 val = tic_api_pmem(tic, index, 0, false);

            if(top >= 2)
            {
                tic_api_pmem(tic, index, (u32)lua_tointeger(lua, 2), true);
            }

            lua_pushinteger(lua, val);

            return 1;           
        }

        luaL_error(lua, "invalid persistent tic index\n");
    }
    else luaL_error(lua, "invalid params, pmem(index [val]) -> val\n");

    return 0;
}

static s32 lua_time(lua_State *lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);
    
    lua_pushnumber(lua, tic_api_time(tic));

    return 1;
}

static s32 lua_tstamp(lua_State *lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);

    lua_pushnumber(lua, tic_api_tstamp(tic));

    return 1;
}

static s32 lua_exit(lua_State *lua)
{
    tic_api_exit((tic_mem*)getLuaCore(lua));
    
    return 0;
}

static s32 lua_mouse(lua_State *lua)
{
    tic_core* core = getLuaCore(lua);

    {
        tic_point pos = tic_api_mouse((tic_mem*)core);

        lua_pushinteger(lua, pos.x);
        lua_pushinteger(lua, pos.y);
    }

    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    lua_pushboolean(lua, mouse->left);
    lua_pushboolean(lua, mouse->middle);
    lua_pushboolean(lua, mouse->right);
    lua_pushinteger(lua, mouse->scrollx);
    lua_pushinteger(lua, mouse->scrolly);

    return 7;
}

static s32 lua_fget(lua_State* lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);
    s32 top = lua_gettop(lua);

    if(top >= 1)
    {
        u32 index = getLuaNumber(lua, 1);

        if(top >= 2)
        {
            u32 flag = getLuaNumber(lua, 2);
            lua_pushboolean(lua, tic_api_fget(tic, index, flag));
            return 1;
        }
    }

    luaL_error(lua, "invalid params, fget(sprite,flag)\n");

    return 0;
}

static s32 lua_fset(lua_State* lua)
{
    tic_mem* tic = (tic_mem*)getLuaCore(lua);
    s32 top = lua_gettop(lua);

    if(top >= 1)
    {
        u32 index = getLuaNumber(lua, 1);

        if(top >= 2)
        {
            u32 flag = getLuaNumber(lua, 2);

            if(top >= 3)
            {
                bool value = lua_toboolean(lua, 3);
                tic_api_fset(tic, index, flag, value);
                return 0;
            }
        }
    }

    luaL_error(lua, "invalid params, fset(sprite,flag,value)\n");

    return 0;
}

static s32 lua_dofile(lua_State *lua)
{
    luaL_error(lua, "unknown method: \"dofile\"\n");

    return 0;
}

static s32 lua_loadfile(lua_State *lua)
{
    luaL_error(lua, "unknown method: \"loadfile\"\n");

    return 0;
}

void lua_open_builtins(lua_State *lua)
{
    static const luaL_Reg loadedlibs[] =
    {
        { "_G", luaopen_base },
        { LUA_LOADLIBNAME, luaopen_package },
        { LUA_COLIBNAME, luaopen_coroutine },
        { LUA_TABLIBNAME, luaopen_table },
        { LUA_STRLIBNAME, luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math },
        { LUA_DBLIBNAME, luaopen_debug },
        { NULL, NULL }
    };

    for (const luaL_Reg *lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(lua, lib->name, lib->func, 1);
        lua_pop(lua, 1);
    }
}

void initLuaAPI(tic_core* core)
{
    static const struct{lua_CFunction func; const char* name;} ApiItems[] = 
    {
#define API_FUNC_DEF(name, ...) {lua_ ## name, #name},
        TIC_API_LIST(API_FUNC_DEF)
#undef  API_FUNC_DEF

#if defined(BUILD_DEPRECATED)    
        {lua_textri, "textri"},
#endif
    };

    for (s32 i = 0; i < COUNT_OF(ApiItems); i++)
        registerLuaFunction(core, ApiItems[i].func, ApiItems[i].name);

    registerLuaFunction(core, lua_dofile, "dofile");
    registerLuaFunction(core, lua_loadfile, "loadfile");
}

void closeLua(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->currentVM)
    {
        lua_close(core->currentVM);
        core->currentVM = NULL;
    }
}

static bool initLua(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;

    closeLua(tic);

    lua_State* lua = core->currentVM = luaL_newstate();
    lua_open_builtins(lua);

    initLuaAPI(core);

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

/*
** Message handler which appends stract trace to exceptions.
** This function was extractred from lua.c.
*/
static s32 msghandler (lua_State *lua) 
{
    const char *msg = lua_tostring(lua, 1);
    if (msg == NULL) /* is error object not a string? */
    {
        if (luaL_callmeta(lua, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(lua, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(lua, "(error object is a %s value)", luaL_typename(lua, 1));
    }
    luaL_traceback(lua, lua, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message handler function.
** Please use this function for all top level lua functions.
** This function was extractred from lua.c (and stripped of signal handling)
*/
static s32 docall (lua_State *lua, s32 narg, s32 nres) 
{
    s32 status = 0;
    s32 base = lua_gettop(lua) - narg;  /* function index */
    lua_pushcfunction(lua, msghandler);  /* push message handler */
    lua_insert(lua, base);  /* put it under function and args */
    status = lua_pcall(lua, narg, nres, base);
    lua_remove(lua, base);  /* remove message handler from the stack */
    return status;
}

void callLuaTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    lua_State* lua = core->currentVM;

    if(lua)
    {
        lua_getglobal(lua, TIC_FN);
        if(lua_isfunction(lua, -1)) 
        {
            if(docall(lua, 0, 0) != LUA_OK) 
            {                
                core->data->error(core->data->data, lua_tostring(lua, -1));
                return;
            }

#if defined(BUILD_DEPRECATED)
            // call OVR() callback for backward compatibility
            {
                lua_getglobal(lua, OVR_FN);
                if(lua_isfunction(lua, -1))
                {
                    OVR(core)
                    {
                        if(docall(lua, 0, 0) != LUA_OK)
                            core->data->error(core->data->data, lua_tostring(lua, -1));
                    }
                }
                else lua_pop(lua, 1);
            }
#endif
        }
        else 
        {       
            lua_pop(lua, 1);
            core->data->error(core->data->data, "'function TIC()...' isn't found :(");
        }
    }
}

void callLuaIntCallback(tic_mem* tic, s32 value, void* data, const char* name)
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    if (lua)
    {
        lua_getglobal(lua, name);
        if(lua_isfunction(lua, -1))
        {
            lua_pushinteger(lua, value);
            if(docall(lua, 1, 0) != LUA_OK)
                core->data->error(core->data->data, lua_tostring(lua, -1));
        }
        else lua_pop(lua, 1);
    }
}

void callLuaScanline(tic_mem* tic, s32 row, void* data)
{
    callLuaIntCallback(tic, row, data, SCN_FN);

    // try to call old scanline
    callLuaIntCallback(tic, row, data, "scanline");
}

void callLuaBorder(tic_mem* tic, s32 row, void* data)
{
    callLuaIntCallback(tic, row, data, BDR_FN);
}

void callLuaMenu(tic_mem* tic, s32 index, void* data)
{
    callLuaIntCallback(tic, index, data, MENU_FN);
}

void callLuaBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    if (lua)
    {
        lua_getglobal(lua, BOOT_FN);
        if(lua_isfunction(lua, -1))
        {
            if(docall(lua, 0, 0) != LUA_OK)
                core->data->error(core->data->data, lua_tostring(lua, -1));
        }
        else lua_pop(lua, 1);
    }
}

static const char* const LuaKeywords [] =
{
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "goto", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while"
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

static void evalLua(tic_mem* tic, const char* code) {
    tic_core* core = (tic_core*)tic;
    lua_State* lua = core->currentVM;

    if (!lua) return;

    lua_settop(lua, 0);

    if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
    {
        core->data->error(core->data->data, lua_tostring(lua, -1));
    }
}

tic_script_config LuaSyntaxConfig = 
{
    .id                 = 10,
    .name               = "lua",
    .fileExtension      = ".lua",
    .projectComment     = "--",
    {
      .init               = initLua,
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
};

#endif /* defined(TIC_BUILD_WITH_LUA) */
