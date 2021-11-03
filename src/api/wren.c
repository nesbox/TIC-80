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

#if defined(TIC_BUILD_WITH_WREN)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "tools.h"
#include "wren.h"

static WrenHandle* game_class;
static WrenHandle* new_handle;
static WrenHandle* update_handle;
static WrenHandle* scanline_handle;
static WrenHandle* border_handle;
static WrenHandle* overline_handle;

static bool loaded = false;

static char const* tic_wren_api = "\n\
class TIC {\n\
    foreign static btn()\n\
    foreign static btn(id)\n\
    foreign static btnp(id)\n\
    foreign static btnp(id, hold, period)\n\
    foreign static key(id)\n\
    foreign static keyp(id)\n\
    foreign static keyp(id, hold, period)\n\
    foreign static mouse()\n\
    foreign static font(text)\n\
    foreign static font(text, x, y)\n\
    foreign static font(text, x, y, alpha_color)\n\
    foreign static font(text, x, y, alpha_color, w, h)\n\
    foreign static font(text, x, y, alpha_color, w, h, fixed)\n\
    foreign static font(text, x, y, alpha_color, w, h, fixed, scale)\n\
    foreign static spr(id)\n\
    foreign static spr(id, x, y)\n\
    foreign static spr(id, x, y, alpha_color)\n\
    foreign static spr(id, x, y, alpha_color, scale)\n\
    foreign static spr(id, x, y, alpha_color, scale, flip)\n\
    foreign static spr(id, x, y, alpha_color, scale, flip, rotate)\n\
    foreign static spr(id, x, y, alpha_color, scale, flip, rotate, cell_width, cell_height)\n\
    foreign static map(cell_x, cell_y)\n\
    foreign static map(cell_x, cell_y, cell_w, cell_h)\n\
    foreign static map(cell_x, cell_y, cell_w, cell_h, x, y)\n\
    foreign static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color)\n\
    foreign static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color, scale)\n\
    foreign static mset(cell_x, cell_y)\n\
    foreign static mset(cell_x, cell_y, index)\n\
    foreign static mget(cell_x, cell_y)\n\
    foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3)\n\
    foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map)\n\
    foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map, alpha_color)\n\
    foreign static pix(x, y)\n\
    foreign static pix(x, y, color)\n\
    foreign static line(x0, y0, x1, y1, color)\n\
    foreign static circ(x, y, radius, color)\n\
    foreign static circb(x, y, radius, color)\n\
    foreign static rect(x, y, w, h, color)\n\
    foreign static rectb(x, y, w, h, color)\n\
    foreign static tri(x1, y1, x2, y2, x3, y3, color)\n\
    foreign static cls()\n\
    foreign static cls(color)\n\
    foreign static clip()\n\
    foreign static clip(x, y, w, h)\n\
    foreign static peek(addr)\n\
    foreign static poke(addr, val)\n\
    foreign static peek(addr, bits)\n\
    foreign static poke(addr, val, bits)\n\
    foreign static peek4(addr)\n\
    foreign static poke4(addr, val)\n\
    foreign static memcpy(dst, src, size)\n\
    foreign static memset(dst, src, size)\n\
    foreign static pmem(index)\n\
    foreign static pmem(index, val)\n\
    foreign static sfx(id)\n\
    foreign static sfx(id, note)\n\
    foreign static sfx(id, note, duration)\n\
    foreign static sfx(id, note, duration, channel)\n\
    foreign static sfx(id, note, duration, channel, volume)\n\
    foreign static sfx(id, note, duration, channel, volume, speed)\n\
    foreign static music()\n\
    foreign static music(track)\n\
    foreign static music(track, frame)\n\
    foreign static music(track, frame, loop)\n\
    foreign static music(track, frame, loop, sustain)\n\
    foreign static time()\n\
    foreign static tstamp()\n\
    foreign static sync()\n\
    foreign static sync(mask)\n\
    foreign static sync(mask, bank)\n\
    foreign static sync(mask, bank, tocart)\n\
    foreign static reset()\n\
    foreign static exit()\n\
    foreign static map_width__\n\
    foreign static map_height__\n\
    foreign static spritesize__\n\
    foreign static print__(v, x, y, color, fixed, scale, alt)\n\
    foreign static trace__(msg, color)\n\
    foreign static spr__(id, x, y, alpha_color, scale, flip, rotate)\n\
    foreign static fget(index, flag)\n\
    foreign static fset(index, flag, val)\n\
    foreign static mgeti__(index)\n\
    static print(v) { TIC.print__(v.toString, 0, 0, 15, false, 1, false) }\n\
    static print(v,x,y) { TIC.print__(v.toString, x, y, 15, false, 1, false) }\n\
    static print(v,x,y,color) { TIC.print__(v.toString, x, y, color, false, 1, false) }\n\
    static print(v,x,y,color,fixed) { TIC.print__(v.toString, x, y, color, fixed, 1, false) }\n\
    static print(v,x,y,color,fixed,scale) { TIC.print__(v.toString, x, y, color, fixed, scale, false) }\n\
    static print(v,x,y,color,fixed,scale,alt) { TIC.print__(v.toString, x, y, color, fixed, scale, alt) }\n\
    static trace(v) { TIC.trace__(v.toString, 15) }\n\
    static trace(v,color) { TIC.trace__(v.toString, color) }\n\
    static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color, scale, remap) {\n\
        var map_w = TIC.map_width__\n\
        var map_h = TIC.map_height__\n\
        var size = TIC.spritesize__ * scale\n\
        var jj = y\n\
        var ii = x\n\
        var flip = 0\n\
        var rotate = 0\n\
        for (j in cell_y...cell_y+cell_h) {\n\
            ii = x\n\
            for (i in cell_x...cell_x+cell_w) {\n\
                var mi = i\n\
                var mj = j\n\
                while(mi < 0) mi = mi + map_w\n\
                while(mj < 0) mj = mj + map_h\n\
                while(mi >= map_w) mi = mi - map_w\n\
                while(mj >= map_h) mj = mj - map_h\n\
                var index = mi + mj * map_w\n\
                var tile_index = TIC.mgeti__(index)\n\
                var ret = remap.call(tile_index, mi, mj)\n\
                if (ret.type == List) {\n\
                    tile_index = ret[0]\n\
                    flip = ret[1]\n\
                    rotate = ret[2]\n\
                } else if (ret.type == Num) {\n\
                    tile_index = ret\n\
                }\n\
                TIC.spr__(tile_index, ii, jj, alpha_color, scale, flip, rotate)\n\
                ii = ii + size\n\
            }\n\
            jj = jj + size\n\
        }\n\
    }\n\
    " TIC_FN "(){}\n\
    " SCN_FN "(row){}\n\
    " BDR_FN "(row){}\n\
    " OVR_FN "(){}\n\
}\n";

static inline void wrenError(WrenVM* vm, const char* msg)
{
    wrenEnsureSlots(vm, 1);
    wrenSetSlotString(vm, 0, msg);
    wrenAbortFiber(vm, 0);
}

static inline s32 getWrenNumber(WrenVM* vm, s32 index)
{
    return (s32)wrenGetSlotDouble(vm, index);
}

static inline bool isNumber(WrenVM* vm, s32 index)
{
    return wrenGetSlotType(vm, index) == WREN_TYPE_NUM;
}

static inline bool isString(WrenVM* vm, s32 index)
{
    return wrenGetSlotType(vm, index) == WREN_TYPE_STRING;
}

static inline bool isList(WrenVM* vm, s32 index)
{
    return wrenGetSlotType(vm, index) == WREN_TYPE_LIST;
}

static void closeWren(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if(core->currentVM)
    {   
        // release handles
        if (loaded)
        {
            wrenReleaseHandle(core->currentVM, new_handle);
            wrenReleaseHandle(core->currentVM, update_handle);
            wrenReleaseHandle(core->currentVM, scanline_handle);
            wrenReleaseHandle(core->currentVM, border_handle);
            wrenReleaseHandle(core->currentVM, overline_handle);
            if (game_class != NULL) 
            {
                wrenReleaseHandle(core->currentVM, game_class);
            }
        }

        wrenFreeVM(core->currentVM);
        core->currentVM = NULL;

    }
    loaded = false;
}

static tic_core* getWrenCore(WrenVM* vm)
{
    tic_core* core = wrenGetUserData(vm);

    return core;
}

static void wren_map_width(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, TIC_MAP_WIDTH);
}

static void wren_map_height(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, TIC_MAP_HEIGHT);
}

static void wren_mgeti(WrenVM* vm)
{
    s32 index = getWrenNumber(vm, 1);

    if(index < 0 || index >= TIC_MAP_WIDTH * TIC_MAP_HEIGHT) 
    {
        wrenSetSlotDouble(vm, 0, 0);
        return;
    }

    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    wrenSetSlotDouble(vm, 0, *(tic->ram.map.data + index));
}

static void wren_spritesize(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, TIC_SPRITESIZE);
}

static void wren_btn(WrenVM* vm)
{
    tic_core* core = getWrenCore(vm);
    tic_mem* tic = (tic_mem*)core;

    s32 top = wrenGetSlotCount(vm);

    if (top == 1)
    {
        wrenSetSlotDouble(vm, 0, tic_api_btn(tic, -1));
    }
    else if (top == 2)
    {
        bool pressed = tic_api_btn(tic, getWrenNumber(vm, 1) & 0x1f);
        wrenSetSlotBool(vm, 0, pressed);
    }
    
}

static void wren_btnp(WrenVM* vm)
{   
    tic_core* core = getWrenCore(vm);
    tic_mem* tic = (tic_mem*)core;

    s32 top = wrenGetSlotCount(vm);

    if (top == 1)
    {
        wrenSetSlotBool(vm, 0, tic_api_btnp(tic, -1, -1, -1));
    }
    else if(top == 2)
    {
        s32 index = getWrenNumber(vm, 1) & 0xf;

        wrenSetSlotBool(vm, 0, tic_api_btnp(tic, index, -1, -1));
    }
    else if (top == 4)
    {
        s32 index = getWrenNumber(vm, 1) & 0xf;
        u32 hold = getWrenNumber(vm, 2);
        u32 period = getWrenNumber(vm, 3);

        wrenSetSlotBool(vm, 0, tic_api_btnp(tic, index, hold, period));
    }
}

static void wren_key(WrenVM* vm)
{
    tic_core* core = getWrenCore(vm);
    tic_mem* tic = &core->memory;

    s32 top = wrenGetSlotCount(vm);

    if (top == 1)
    {
        wrenSetSlotBool(vm, 0, tic_api_key(tic, tic_key_unknown));
    }
    else if (top == 2)
    {
        tic_key key = getWrenNumber(vm, 1);

        if(key < tic_keys_count)
            wrenSetSlotBool(vm, 0, tic_api_key(tic, key));
        else
        {
            wrenError(vm, "unknown keyboard code\n");
            return;
        }
    }
}

static void wren_keyp(WrenVM* vm)
{
    tic_core* core = getWrenCore(vm);
    tic_mem* tic = &core->memory;

    s32 top = wrenGetSlotCount(vm);

    if (top == 1)
    {
        wrenSetSlotBool(vm, 0, tic_api_keyp(tic, tic_key_unknown, -1, -1));
    }
    else
    {
        tic_key key = getWrenNumber(vm, 1);

        if(key >= tic_keys_count)
        {
            wrenError(vm, "unknown keyboard code\n");
        }
        else
        {
            if(top == 2)
            {
                wrenSetSlotBool(vm, 0, tic_api_keyp(tic, key, -1, -1));
            }
            else if(top == 4)
            {
                u32 hold = getWrenNumber(vm, 2);
                u32 period = getWrenNumber(vm, 3);

                wrenSetSlotBool(vm, 0, tic_api_keyp(tic, key, hold, period));
            }
        }
    }
}


static void wren_mouse(WrenVM* vm)
{
    tic_core* core = getWrenCore(vm);

    const tic80_mouse* mouse = &core->memory.ram.input.mouse;

    wrenEnsureSlots(vm, 6);
    wrenSetSlotNewList(vm, 0);

    {
        tic_point pos = tic_api_mouse((tic_mem*)core);

        wrenSetSlotDouble(vm, 1, pos.x);
        wrenInsertInList(vm, 0, 0, 1);

        wrenSetSlotDouble(vm, 1, pos.y);
        wrenInsertInList(vm, 0, 1, 1);
    }

    wrenSetSlotBool(vm, 1, mouse->left ? true : false);
    wrenInsertInList(vm, 0, 2, 1);
    wrenSetSlotBool(vm, 1, mouse->middle ? true : false);
    wrenInsertInList(vm, 0, 3, 1);
    wrenSetSlotBool(vm, 1, mouse->right ? true : false);
    wrenInsertInList(vm, 0, 4, 1);
    wrenSetSlotDouble(vm, 1, mouse->scrollx);
    wrenInsertInList(vm, 0, 5, 1);
    wrenSetSlotDouble(vm, 1, mouse->scrolly);
    wrenInsertInList(vm, 0, 6, 1);
}

static void wren_print(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    const char* text = wrenGetSlotString(vm, 1);

    s32 x = getWrenNumber(vm, 2);
    s32 y = getWrenNumber(vm, 3);

    s32 color = getWrenNumber(vm, 4) % TIC_PALETTE_SIZE;

    bool fixed = wrenGetSlotBool(vm, 5);

    s32 scale = getWrenNumber(vm, 6);

    if(scale == 0)
    {
        wrenSetSlotDouble(vm, 0, 0);
        return;
    }

    bool alt = wrenGetSlotBool(vm, 7);

    s32 size = tic_api_print(tic, text, x, y, color, fixed, scale, alt);

    wrenSetSlotDouble(vm, 0, size);
}

static void wren_font(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    s32 top = wrenGetSlotCount(vm);

    if(top > 1)
    {
        const char* text = NULL;
        if (isString(vm, 1))
        {
            text = wrenGetSlotString(vm, 1);
        }

        s32 x = 0;
        s32 y = 0;
        s32 width = TIC_SPRITESIZE;
        s32 height = TIC_SPRITESIZE;
        u8 chromakey = 0;
        bool fixed = false;
        s32 scale = 1;
        bool alt = false;

        if(top > 3)
        {
            x = getWrenNumber(vm, 2);
            y = getWrenNumber(vm, 3);

            if(top > 4)
            {
                chromakey = getWrenNumber(vm, 4);

                if(top > 6)
                {
                    width = getWrenNumber(vm, 5);
                    height = getWrenNumber(vm, 6);

                    if(top > 7)
                    {
                        fixed = wrenGetSlotBool(vm, 7);

                        if(top > 8)
                        {
                            scale = getWrenNumber(vm, 8);

                            if(top > 9)
                            {
                                alt = wrenGetSlotBool(vm, 9);
                            }
                        }
                    }
                }
            }
        }

        if(scale == 0)
        {
            wrenSetSlotDouble(vm, 0, 0);
            return;
        }

        s32 size = tic_api_font(tic, text ? text : "null", x, y, chromakey, width, height, fixed, scale, alt);
        wrenSetSlotDouble(vm, 0, size);
    }
}

static void wren_trace(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    const char* text = wrenGetSlotString(vm, 1);
    u8 color = (u8)getWrenNumber(vm, 2);

    tic_api_trace(tic, text, color);
}

static void wren_spr(WrenVM* vm)
{   
    s32 top = wrenGetSlotCount(vm);

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

    if(top > 1) 
    {
        index = getWrenNumber(vm, 1);

        if(top > 3)
        {
            x = getWrenNumber(vm, 2);
            y = getWrenNumber(vm, 3);

            if(top > 4)
            {
                if(isList(vm, 4))
                {
                    wrenEnsureSlots(vm, top+1);
                    int list_count = wrenGetListCount(vm, 4);
                    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                    {
                        wrenGetListElement(vm, 4, i, top);
                        if(i < list_count && isNumber(vm, top))
                        {
                            colors[i] = getWrenNumber(vm, top);
                            count++;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else 
                {
                    colors[0] = getWrenNumber(vm, 4);
                    count = 1;
                }

                if(top > 5)
                {
                    scale = getWrenNumber(vm, 5);

                    if(top > 6)
                    {
                        flip = getWrenNumber(vm, 6);

                        if(top > 7)
                        {
                            rotate = getWrenNumber(vm, 7);

                            if(top > 9)
                            {
                                w = getWrenNumber(vm, 8);
                                h = getWrenNumber(vm, 9);
                            }
                        }
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_spr(tic, index, x, y, w, h, colors, count, scale, flip, rotate);
}

static void wren_spr_internal(WrenVM* vm) 
{   
    s32 top = wrenGetSlotCount(vm);

    s32 index = getWrenNumber(vm, 1);
    s32 x = getWrenNumber(vm, 2);
    s32 y = getWrenNumber(vm, 3);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
            
    if(isList(vm, 4))
    {
        wrenEnsureSlots(vm, top+1);
        int list_count = wrenGetListCount(vm, 4);
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            wrenGetListElement(vm, 4, i, top);
            if(i < list_count && isNumber(vm, top))
            {
                colors[i] = getWrenNumber(vm, top);
                count++;
            }
            else
            {
                break;
            }
        }
    }
    else 
    {
        colors[0] = getWrenNumber(vm, 4);
        count = 1;
    }

    s32 scale = getWrenNumber(vm, 5);
    s32 flip = getWrenNumber(vm, 6);
    s32 rotate = getWrenNumber(vm, 7);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_spr(tic, index, x, y, 1, 1, colors, count, scale, flip, rotate);
}

static void wren_map(WrenVM* vm)
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

    s32 top = wrenGetSlotCount(vm);

    if(top > 2) 
    {
        x = getWrenNumber(vm, 1);
        y = getWrenNumber(vm, 2);

        if(top > 4)
        {
            w = getWrenNumber(vm, 3);
            h = getWrenNumber(vm, 4);

            if(top > 6)
            {
                sx = getWrenNumber(vm, 5);
                sy = getWrenNumber(vm, 6);

                if(top > 7)
                {
                    if(isList(vm, 7))
                    {
                        wrenEnsureSlots(vm, top+1);
                        int list_count = wrenGetListCount(vm, 7);
                        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                        {
                            wrenGetListElement(vm, 7, i, top);
                            if(i < list_count && isNumber(vm, top))
                            {
                                colors[i] = getWrenNumber(vm, top);
                                count++;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else 
                    {
                        colors[0] = getWrenNumber(vm, 7);
                        count = 1;
                    }

                    if(top > 8)
                    {
                        scale = getWrenNumber(vm, 8);
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_map(tic, x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);
}

static void wren_mset(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    u8 value = getWrenNumber(vm, 3);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_mset(tic, x, y, value);
}

static void wren_mget(WrenVM* vm)
{       
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    u8 value = tic_api_mget(tic, x, y);
    wrenSetSlotDouble(vm, 0, value);
}

static void wren_textri(WrenVM* vm)
{
    int top = wrenGetSlotCount(vm);

    float pt[12];

    for (s32 i = 0; i < COUNT_OF(pt); i++)
    {
        pt[i] = (float)getWrenNumber(vm, i + 1);
    }

    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    bool use_map = false;

    //  check for use map 
    if (top > 13)
    {
        use_map = wrenGetSlotBool(vm, 13);
    }

    //  check for chroma 
    if(isList(vm, 14))
    {
        wrenEnsureSlots(vm, top+1);
        int list_count = wrenGetListCount(vm, 14);
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            wrenGetListElement(vm, 14, i, top);
            if(i < list_count && isNumber(vm, top))
            {
                colors[i] = getWrenNumber(vm, top);
                count++;
            }
            else
            {
                break;
            }
        }
    }
    else 
    {
        colors[0] = getWrenNumber(vm, 14);
        count = 1;
    }

    tic_api_textri(tic, pt[0], pt[1],   //  xy 1
                                pt[2], pt[3],   //  xy 2
                                pt[4], pt[5],   //  xy 3
                                pt[6], pt[7],   //  uv 1
                                pt[8], pt[9],   //  uv 2
                                pt[10], pt[11], //  uv 3
                                use_map,        // use map
                                colors, count);        // chroma
}

static void wren_pix(WrenVM* vm)
{
    int top = wrenGetSlotCount(vm);

    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    if(top > 3)
    {
        s32 color = getWrenNumber(vm, 3);
        tic_api_pix(tic, x, y, color, false);
    }
    else
    {
        wrenSetSlotDouble(vm, 0, tic_api_pix(tic, x, y, 0, true));
    }
}

static void wren_line(WrenVM* vm)
{
    s32 x0 = getWrenNumber(vm, 1);
    s32 y0 = getWrenNumber(vm, 2);
    s32 x1 = getWrenNumber(vm, 3);
    s32 y1 = getWrenNumber(vm, 4);
    s32 color = getWrenNumber(vm, 5);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_line(tic, x0, y0, x1, y1, color);
}

static void wren_circ(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 radius = getWrenNumber(vm, 3);
    s32 color = getWrenNumber(vm, 4);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_circ(tic, x, y, radius, color);
}

static void wren_circb(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 radius = getWrenNumber(vm, 3);
    s32 color = getWrenNumber(vm, 4);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_circb(tic, x, y, radius, color);
}

static void wren_elli(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 a = getWrenNumber(vm, 3);
    s32 b = getWrenNumber(vm, 4);
    s32 color = getWrenNumber(vm, 5);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_elli(tic, x, y, a, b, color);
}

static void wren_ellib(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 a = getWrenNumber(vm, 3);
    s32 b = getWrenNumber(vm, 4);
    s32 color = getWrenNumber(vm, 5);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_ellib(tic, x, y, a, b, color);
}

static void wren_rect(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 w = getWrenNumber(vm, 3);
    s32 h = getWrenNumber(vm, 4);
    s32 color = getWrenNumber(vm, 5);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_rect(tic, x, y, w, h, color);
}

static void wren_rectb(WrenVM* vm)
{
    s32 x = getWrenNumber(vm, 1);
    s32 y = getWrenNumber(vm, 2);
    s32 w = getWrenNumber(vm, 3);
    s32 h = getWrenNumber(vm, 4);
    s32 color = getWrenNumber(vm, 5);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_rectb(tic, x, y, w, h, color);
}

static void wren_tri(WrenVM* vm)
{       
    s32 pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
    {
        pt[i] = getWrenNumber(vm, i+1);
    }
    
    s32 color = getWrenNumber(vm, 7);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
}

static void wren_trib(WrenVM* vm)
{       
    s32 pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
    {
        pt[i] = getWrenNumber(vm, i+1);
    }
    
    s32 color = getWrenNumber(vm, 7);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_trib(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
}

static void wren_cls(WrenVM* vm)
{
    int top = wrenGetSlotCount(vm);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    tic_api_cls(tic, top == 1 ? 0 : getWrenNumber(vm, 1));
}

static void wren_clip(WrenVM* vm)
{
    s32 top = wrenGetSlotCount(vm);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    if(top == 1)
    {
        tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    } 
    else 
    {
        s32 x = getWrenNumber(vm, 1);
        s32 y = getWrenNumber(vm, 2);
        s32 w = getWrenNumber(vm, 3);
        s32 h = getWrenNumber(vm, 4);

        tic_api_clip(tic, x, y, w, h);
    }
}

static void wren_peek(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 address = getWrenNumber(vm, 1);
    s32 bits = BITS_IN_BYTE;

    if(wrenGetSlotCount(vm) > 2)
        bits = getWrenNumber(vm, 2);

    wrenSetSlotDouble(vm, 0, tic_api_peek(tic, address, bits));
}

static void wren_poke(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 address = getWrenNumber(vm, 1);
    u8 value = getWrenNumber(vm, 2) & 0xff;
    s32 bits = BITS_IN_BYTE;
    if(wrenGetSlotCount(vm) > 3)
        bits = getWrenNumber(vm, 3);

    tic_api_poke(tic, address, value, bits);
}

static void wren_peek4(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 address = getWrenNumber(vm, 1);

    wrenSetSlotDouble(vm, 0, tic_api_peek4(tic, address));
}

static void wren_poke4(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 address = getWrenNumber(vm, 1);
    u8 value = getWrenNumber(vm, 2);

    tic_api_poke4(tic, address, value);
}

static void wren_memcpy(WrenVM* vm)
{
    s32 dest = getWrenNumber(vm, 1);
    s32 src = getWrenNumber(vm, 2);
    s32 size = getWrenNumber(vm, 3);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    tic_api_memcpy(tic, dest, src, size);
}

static void wren_memset(WrenVM* vm)
{
    s32 dest = getWrenNumber(vm, 1);
    u8 value = getWrenNumber(vm, 2);
    s32 size = getWrenNumber(vm, 3);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    tic_api_memset(tic, dest, value, size);
}

static void wren_pmem(WrenVM* vm)
{
    s32 top = wrenGetSlotCount(vm);
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    u32 index = getWrenNumber(vm, 1);

    if(index < TIC_PERSISTENT_SIZE)
    {
        u32 val = tic_api_pmem(tic, index, 0, false);

        if(top > 2)
        {
            tic_api_pmem(tic, index, getWrenNumber(vm, 2), true);
        }

        wrenSetSlotDouble(vm, 0, val);
    }
    else wrenError(vm, "invalid persistent tic index\n");
}

static void wren_sfx(WrenVM* vm)
{
    s32 top = wrenGetSlotCount(vm);

    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 index = getWrenNumber(vm, 1);

    if(index < SFX_COUNT)
    {

        s32 note = -1;
        s32 octave = -1;
        s32 duration = -1;
        s32 channel = 0;
        s32 volumes[TIC_STEREO_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
        s32 speed = SFX_DEF_SPEED;

        if (index >= 0)
        {
            tic_sample* effect = tic->ram.sfx.samples.data + index;

            note = effect->note;
            octave = effect->octave;
            speed = effect->speed;
        }

        if(top > 2)
        {
            if(isNumber(vm, 2))
            {
                s32 id = getWrenNumber(vm, 2);
                note = id % NOTES;
                octave = id / NOTES;
            }
            else if(isString(vm, 2))
            {
                const char* noteStr = wrenGetSlotString(vm, 2);

                if(!tic_tool_parse_note(noteStr, &note, &octave))
                {
                    wrenError(vm, "invalid note, should be like C#4\n");
                    return;
                }
            }

            if(top > 3)
            {
                duration = getWrenNumber(vm, 3);

                if(top > 4)
                {
                    channel = getWrenNumber(vm, 4);

                    if(top > 5)
                    {
                        if(isList(vm, 5) && wrenGetListCount(vm, 5) == COUNT_OF(volumes))
                        {
                            for(s32 i = 0; i < COUNT_OF(volumes); i++)
                            {
                                wrenGetListElement(vm, 5, i, top);
                                if(isNumber(vm, top))
                                    volumes[i] = getWrenNumber(vm, top);                                
                            }
                        }
                        else volumes[0] = volumes[1] = getWrenNumber(vm, 5);

                        if(top > 6)
                        {
                            speed = getWrenNumber(vm, 6);
                        }
                    }
                }                   
            }
        }

        if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
        {
            tic_api_sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
        }       
        else wrenError(vm, "unknown channel\n");
    }
    else wrenError(vm, "unknown sfx index\n");
}

static void wren_music(WrenVM* vm)
{
    s32 top = wrenGetSlotCount(vm);
    
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 track = -1;
    s32 frame = -1;
    s32 row = -1;
    bool loop = true;
    bool sustain = false;
    s32 tempo = -1;
    s32 speed = -1;

    if(top > 1)
    {
        track = getWrenNumber(vm, 1);

        if(top > 2)
        {
            frame = getWrenNumber(vm, 2);

            if(top > 3)
            {
                row = getWrenNumber(vm, 3);

                if(top > 4)
                {
                    loop = wrenGetSlotBool(vm, 4);

                    if(top > 5)
                    {
                        sustain = wrenGetSlotBool(vm, 5);

                        if (top > 6)
                        {
                            tempo = getWrenNumber(vm, 6);

                            if (top > 7)
                            {
                                speed = getWrenNumber(vm, 7);
                            }
                        }
                    }
                }
            }
        }
    }

    tic_api_music(tic, track, frame, row, loop, sustain, tempo, speed);
}

static void wren_time(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    
    wrenSetSlotDouble(vm, 0, tic_api_time(tic));
}

static void wren_tstamp(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    wrenSetSlotDouble(vm, 0, tic_api_tstamp(tic));
}

static void wren_sync(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    bool toCart = false;
    u32 mask = 0;
    s32 bank = 0;

    s32 top = wrenGetSlotCount(vm);

    if(top > 1)
    {
        mask = getWrenNumber(vm, 1);

        if(top > 2)
        {
            bank = getWrenNumber(vm, 2);

            if(top > 3)
            {
                toCart = wrenGetSlotBool(vm, 3);
            }
        }
    }

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else wrenError(vm, "sync() error, invalid bank");
}

static void wren_reset(WrenVM* vm)
{
    tic_core* core = getWrenCore(vm);

    core->state.initialized = false;
}

static void wren_exit(WrenVM* vm)
{
    tic_api_exit((tic_mem*)getWrenCore(vm));
}

static void wren_fget(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);

    s32 top = wrenGetSlotCount(vm);

    if(top > 1)
    {
        u32 index = getWrenNumber(vm, 1);

        if(top > 2)
        {
            u32 flag = getWrenNumber(vm, 2);
            wrenSetSlotBool(vm, 0, tic_api_fget(tic, index, flag));
            return;
        }
    }

    wrenError(vm, "invalid params, fget(sprite,flag)\n");
}

static void wren_fset(WrenVM* vm)
{
    tic_mem* tic = (tic_mem*)getWrenCore(vm);
    s32 top = wrenGetSlotCount(vm);

    if(top > 1)
    {
        u32 index = getWrenNumber(vm, 1);

        if(top > 2)
        {
            u32 flag = getWrenNumber(vm, 2);

            if(top > 3)
            {
                bool value = wrenGetSlotBool(vm, 3);
                tic_api_fset(tic, index, flag, value);
                return;
            }
        }
    }

    wrenError(vm, "invalid params, fset(sprite,flag,value)\n");
}

static WrenForeignMethodFn foreignTicMethods(const char* signature)
{
    if (strcmp(signature, "static TIC.btn()"                    ) == 0) return wren_btn;
    if (strcmp(signature, "static TIC.btn(_)"                   ) == 0) return wren_btn;
    if (strcmp(signature, "static TIC.btnp(_)"                  ) == 0) return wren_btnp;
    if (strcmp(signature, "static TIC.btnp(_,_,_)"              ) == 0) return wren_btnp;
    if (strcmp(signature, "static TIC.key(_)"                   ) == 0) return wren_key;
    if (strcmp(signature, "static TIC.keyp(_)"                  ) == 0) return wren_keyp;
    if (strcmp(signature, "static TIC.keyp(_,_,_)"              ) == 0) return wren_keyp;
    if (strcmp(signature, "static TIC.mouse()"                  ) == 0) return wren_mouse;

    if (strcmp(signature, "static TIC.font(_)"                  ) == 0) return wren_font;
    if (strcmp(signature, "static TIC.font(_,_,_)"              ) == 0) return wren_font;
    if (strcmp(signature, "static TIC.font(_,_,_,_)"            ) == 0) return wren_font;
    if (strcmp(signature, "static TIC.font(_,_,_,_,_,_)"        ) == 0) return wren_font;
    if (strcmp(signature, "static TIC.font(_,_,_,_,_,_,_)"      ) == 0) return wren_font;
    if (strcmp(signature, "static TIC.font(_,_,_,_,_,_,_,_)"    ) == 0) return wren_font;

    if (strcmp(signature, "static TIC.spr(_)"                   ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_)"               ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_,_)"             ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_,_,_)"           ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_,_,_,_)"         ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_,_,_,_,_)"       ) == 0) return wren_spr;
    if (strcmp(signature, "static TIC.spr(_,_,_,_,_,_,_,_,_)"   ) == 0) return wren_spr;

    if (strcmp(signature, "static TIC.map(_,_)"                 ) == 0) return wren_map;
    if (strcmp(signature, "static TIC.map(_,_,_,_)"             ) == 0) return wren_map;
    if (strcmp(signature, "static TIC.map(_,_,_,_,_,_)"         ) == 0) return wren_map;
    if (strcmp(signature, "static TIC.map(_,_,_,_,_,_,_)"       ) == 0) return wren_map;
    if (strcmp(signature, "static TIC.map(_,_,_,_,_,_,_,_)"     ) == 0) return wren_map;

    if (strcmp(signature, "static TIC.mset(_,_)"                ) == 0) return wren_mset;
    if (strcmp(signature, "static TIC.mset(_,_,_)"              ) == 0) return wren_mset;
    if (strcmp(signature, "static TIC.mget(_,_)"                ) == 0) return wren_mget;

    if (strcmp(signature, "static TIC.textri(_,_,_,_,_,_,_,_,_,_,_,_)"       ) == 0) return wren_textri;
    if (strcmp(signature, "static TIC.textri(_,_,_,_,_,_,_,_,_,_,_,_,_)"     ) == 0) return wren_textri;
    if (strcmp(signature, "static TIC.textri(_,_,_,_,_,_,_,_,_,_,_,_,_,_)"   ) == 0) return wren_textri;

    if (strcmp(signature, "static TIC.pix(_,_)"                 ) == 0) return wren_pix;
    if (strcmp(signature, "static TIC.pix(_,_,_)"               ) == 0) return wren_pix;
    if (strcmp(signature, "static TIC.line(_,_,_,_,_)"          ) == 0) return wren_line;
    if (strcmp(signature, "static TIC.circ(_,_,_,_)"            ) == 0) return wren_circ;
    if (strcmp(signature, "static TIC.circb(_,_,_,_)"           ) == 0) return wren_circb;
    if (strcmp(signature, "static TIC.rect(_,_,_,_,_)"          ) == 0) return wren_rect;
    if (strcmp(signature, "static TIC.rectb(_,_,_,_,_)"         ) == 0) return wren_rectb;
    if (strcmp(signature, "static TIC.tri(_,_,_,_,_,_,_)"       ) == 0) return wren_tri;

    if (strcmp(signature, "static TIC.cls()"                    ) == 0) return wren_cls;
    if (strcmp(signature, "static TIC.cls(_)"                   ) == 0) return wren_cls;
    if (strcmp(signature, "static TIC.clip()"                   ) == 0) return wren_clip;
    if (strcmp(signature, "static TIC.clip(_,_,_,_)"            ) == 0) return wren_clip;

    if (strcmp(signature, "static TIC.peek(_)"                  ) == 0) return wren_peek;
    if (strcmp(signature, "static TIC.poke(_,_)"                ) == 0) return wren_poke;
    if (strcmp(signature, "static TIC.peek(_,_)"                ) == 0) return wren_peek;
    if (strcmp(signature, "static TIC.poke(_,_,_)"              ) == 0) return wren_poke;
    if (strcmp(signature, "static TIC.peek4(_)"                 ) == 0) return wren_peek4;
    if (strcmp(signature, "static TIC.poke4(_,_)"               ) == 0) return wren_poke4;
    if (strcmp(signature, "static TIC.memcpy(_,_,_)"            ) == 0) return wren_memcpy;
    if (strcmp(signature, "static TIC.memset(_,_,_)"            ) == 0) return wren_memset;
    if (strcmp(signature, "static TIC.pmem(_)"                  ) == 0) return wren_pmem;
    if (strcmp(signature, "static TIC.pmem(_,_)"                ) == 0) return wren_pmem;

    if (strcmp(signature, "static TIC.sfx(_)"                   ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.sfx(_,_)"                 ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.sfx(_,_,_)"               ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.sfx(_,_,_,_)"             ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.sfx(_,_,_,_,_)"           ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.sfx(_,_,_,_,_,_)"         ) == 0) return wren_sfx;
    if (strcmp(signature, "static TIC.music()"                  ) == 0) return wren_music;
    if (strcmp(signature, "static TIC.music(_)"                 ) == 0) return wren_music;
    if (strcmp(signature, "static TIC.music(_,_)"               ) == 0) return wren_music;
    if (strcmp(signature, "static TIC.music(_,_,_)"             ) == 0) return wren_music;
    if (strcmp(signature, "static TIC.music(_,_,_,_)"           ) == 0) return wren_music;

    if (strcmp(signature, "static TIC.time()"                   ) == 0) return wren_time;
    if (strcmp(signature, "static TIC.tstamp()"                 ) == 0) return wren_tstamp;
    if (strcmp(signature, "static TIC.sync()"                   ) == 0) return wren_sync;
    if (strcmp(signature, "static TIC.sync(_)"                  ) == 0) return wren_sync;
    if (strcmp(signature, "static TIC.sync(_,_)"                ) == 0) return wren_sync;
    if (strcmp(signature, "static TIC.sync(_,_,_)"              ) == 0) return wren_sync;
    if (strcmp(signature, "static TIC.reset()"                  ) == 0) return wren_reset;
    if (strcmp(signature, "static TIC.exit()"                   ) == 0) return wren_exit;
    if (strcmp(signature, "static TIC.fget(_,_)"                ) == 0) return wren_fget;
    if (strcmp(signature, "static TIC.fset(_,_,_)"              ) == 0) return wren_fset;

    // internal functions
    if (strcmp(signature, "static TIC.map_width__"              ) == 0) return wren_map_width;
    if (strcmp(signature, "static TIC.map_height__"             ) == 0) return wren_map_height;
    if (strcmp(signature, "static TIC.spritesize__"             ) == 0) return wren_spritesize;
    if (strcmp(signature, "static TIC.print__(_,_,_,_,_,_,_)"   ) == 0) return wren_print;
    if (strcmp(signature, "static TIC.trace__(_,_)"             ) == 0) return wren_trace;
    if (strcmp(signature, "static TIC.spr__(_,_,_,_,_,_,_)"     ) == 0) return wren_spr_internal;
    if (strcmp(signature, "static TIC.mgeti__(_)"               ) == 0) return wren_mgeti;

    return NULL;
}

#define API_FUNC_DEF(name, ...) wren_##name,
static const WrenForeignMethodFn ApiFuncList[] = {TIC_API_LIST(API_FUNC_DEF)};
#undef API_FUNC_DEF

static WrenForeignMethodFn bindForeignMethod(
    WrenVM* vm, const char* module, const char* className,
    bool isStatic, const char* signature)
{  
    if (strcmp(module, "main") != 0) return NULL;

    // For convenience, concatenate all of the method qualifiers into a single signature string.
    char fullName[256];
    fullName[0] = '\0';
    if (isStatic) 
    {
        strcat(fullName, "static ");
    }

    strcat(fullName, className);
    strcat(fullName, ".");
    strcat(fullName, signature);

    return foreignTicMethods(fullName);
}

static void initAPI(tic_core* core)
{
    wrenSetUserData(core->currentVM, core);

    if (wrenInterpret(core->currentVM, "main", tic_wren_api) != WREN_RESULT_SUCCESS)
    {                   
        core->data->error(core->data->data, "can't load TIC wren api");
    }
}

static void reportError(WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message)
{
    tic_core* core = getWrenCore(vm);

    char buffer[1024];

    if (module)
    {
        snprintf(buffer, sizeof buffer, "\"%s\", %d ,\"%s\"",module, line, message);
    } else {
        snprintf(buffer, sizeof buffer, "%d, \"%s\"",line, message);
    }

    core->data->error(core->data->data, buffer);
}

static void writeFn(WrenVM* vm, const char* text) 
{
    tic_core* core = getWrenCore(vm);
    u8 color = tic_color_dark_blue;
    core->data->trace(core->data->data, text ? text : "null", color);
}

static bool initWren(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeWren(tic);

    WrenConfiguration config; 
    wrenInitConfiguration(&config);

    config.bindForeignMethodFn = bindForeignMethod;

    config.errorFn = reportError;
    config.writeFn = writeFn;

    WrenVM* vm = core->currentVM = wrenNewVM(&config);

    initAPI(core);
    
    if (wrenInterpret(core->currentVM, "main", code) != WREN_RESULT_SUCCESS)
    {
        return false;
    }

    loaded = true;

    // make handles
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, "main", "Game", 0);
    game_class = wrenGetSlotHandle(vm, 0); // handle from game class 

    new_handle = wrenMakeCallHandle(vm, "new()");
    update_handle = wrenMakeCallHandle(vm, TIC_FN "()");
    scanline_handle = wrenMakeCallHandle(vm, SCN_FN "(_)");
    border_handle = wrenMakeCallHandle(vm, BDR_FN "(_)");
    overline_handle = wrenMakeCallHandle(vm, OVR_FN "()");

    // create game class
    if (game_class)
    {
        wrenEnsureSlots(vm, 1);
        wrenSetSlotHandle(vm, 0, game_class);
        wrenCall(vm, new_handle);
        wrenReleaseHandle(core->currentVM, game_class); // release game class handle
        game_class = NULL;
        if (wrenGetSlotCount(vm) == 0) 
        {
            core->data->error(core->data->data, "Error in game class :(");
            return false;
        }
        game_class = wrenGetSlotHandle(vm, 0); // handle from game object 
    } else {
        core->data->error(core->data->data, "'Game class' isn't found :(");   
        return false;
    }

    return true;
}

static void callWrenTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    WrenVM* vm = core->currentVM;

    if(vm && game_class)
    {
        wrenEnsureSlots(vm, 1);
        wrenSetSlotHandle(vm, 0, game_class);
        wrenCall(vm, update_handle);
    }
}

static void callWrenScanline(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    WrenVM* vm = core->currentVM;

    if(vm && game_class)
    {
        wrenEnsureSlots(vm, 2);
        wrenSetSlotHandle(vm, 0, game_class);
        wrenSetSlotDouble(vm, 1, row);
        wrenCall(vm, scanline_handle);
    }
}

static void callWrenBorder(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    WrenVM* vm = core->currentVM;

    if(vm && game_class)
    {
        wrenEnsureSlots(vm, 2);
        wrenSetSlotHandle(vm, 0, game_class);
        wrenSetSlotDouble(vm, 1, row);
        wrenCall(vm, border_handle);
    }
}

static void callWrenOverline(tic_mem* tic, void* data)
{
    tic_core* core = (tic_core*)tic;
    WrenVM* vm = core->currentVM;

    if (vm && game_class)
    {
        wrenEnsureSlots(vm, 1);
        wrenSetSlotHandle(vm, 0, game_class);
        wrenCall(vm, overline_handle);
    }
}

static const char* const WrenKeywords [] =
{
    "as", "break", "class", "construct", "continue", "else", "false",
    "for", "foreign", "if", "import", "in", "is", "null", "return",
    "static", "super", "this", "true", "var", "while"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getWrenOutline(const char* code, s32* size)
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
        static const char ClassString[] = "class ";

        ptr = strstr(ptr, ClassString);

        if(ptr)
        {
            ptr += sizeof ClassString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(isalnum_(c));
                else if(c == ' ' || c == '{')
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

static void evalWren(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    wrenInterpret(core->currentVM, "main", code);
}

tic_script_config WrenSyntaxConfig = 
{
    .name               = "wren",
    .fileExtension      = ".wren",
    .projectComment     = "//",
    .init               = initWren,
    .close              = closeWren,
    .tick               = callWrenTick,
    .callback           =
    {
        .scanline           = callWrenScanline,
        .border             = callWrenBorder,
        .overline           = callWrenOverline,
    },

    .getOutline         = getWrenOutline,
    .eval               = evalWren,

    .blockCommentStart  = "/*",
    .blockCommentEnd    = "*/",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = "//",

    .keywords           = WrenKeywords,
    .keywordsCount      = COUNT_OF(WrenKeywords),
};

const tic_script_config* get_wren_script_config()
{
    return &WrenSyntaxConfig;
}

#endif /* defined(TIC_BUILD_WITH_WREN) */
