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

#if defined(TIC_BUILD_WITH_JS)

#include "tools.h"

#include <ctype.h>

#include "duktape.h"

static const char TicCore[] = "_TIC80";

static void closeJavascript(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->js)
    {
        duk_destroy_heap(core->js);
        core->js = NULL;
    }
}

static tic_core* getDukCore(duk_context* duk)
{
    duk_push_global_stash(duk);
    duk_get_prop_string(duk, -1, TicCore);
    tic_core* core = duk_to_pointer(duk, -1);
    duk_pop_2(duk);

    return core;
}

static duk_ret_t duk_print(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    const char* text = duk_to_string(duk, 0);
    s32 x = duk_opt_int(duk, 1, 0);
    s32 y = duk_opt_int(duk, 2, 0);
    s32 color = duk_opt_int(duk, 3, TIC_DEFAULT_COLOR);
    bool fixed = duk_opt_boolean(duk, 4, false);
    s32 scale = duk_opt_int(duk, 5, 1);
    bool alt = duk_opt_boolean(duk, 6, false);

    s32 size = tic_api_print(tic, text ? text : "nil", x, y, color, fixed, scale, alt);

    duk_push_uint(duk, size);

    return 1;
}

static duk_ret_t duk_cls(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_cls(tic, duk_opt_int(duk, 0, 0));

    return 0;
}

static duk_ret_t duk_pix(duk_context* duk)
{
    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    if(duk_is_null_or_undefined(duk, 2))
    {
        duk_push_uint(duk, tic_api_pix(tic, x, y, 0, true));
        return 1;
    }
    else
    {
        s32 color = duk_to_int(duk, 2);
        tic_api_pix(tic, x, y, color, false);
    }

    return 0;
}

static duk_ret_t duk_line(duk_context* duk)
{
    s32 x0 = duk_to_int(duk, 0);
    s32 y0 = duk_to_int(duk, 1);
    s32 x1 = duk_to_int(duk, 2);
    s32 y1 = duk_to_int(duk, 3);
    s32 color = duk_to_int(duk, 4);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_line(tic, x0, y0, x1, y1, color);

    return 0;
}

static duk_ret_t duk_rect(duk_context* duk)
{
    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 w = duk_to_int(duk, 2);
    s32 h = duk_to_int(duk, 3);
    s32 color = duk_to_int(duk, 4);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_rect(tic, x, y, w, h, color);

    return 0;
}

static duk_ret_t duk_rectb(duk_context* duk)
{
    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 w = duk_to_int(duk, 2);
    s32 h = duk_to_int(duk, 3);
    s32 color = duk_to_int(duk, 4);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_rectb(tic, x, y, w, h, color);

    return 0;
}

static duk_ret_t duk_spr(duk_context* duk)
{
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    s32 index = duk_opt_int(duk, 0, 0);
    s32 x = duk_opt_int(duk, 1, 0);
    s32 y = duk_opt_int(duk, 2, 0);

    {
        if(!duk_is_null_or_undefined(duk, 3))
        {
            if(duk_is_array(duk, 3))
            {
                for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                {
                    duk_get_prop_index(duk, 3, i);
                    if(duk_is_null_or_undefined(duk, -1))
                    {
                        duk_pop(duk);
                        break;
                    }
                    else
                    {
                        colors[i] = duk_to_int(duk, -1);
                        count++;
                        duk_pop(duk);
                    }
                }
            }
            else
            {
                colors[0] = duk_to_int(duk, 3);
                count = 1;
            }
        }
    }

    s32 scale = duk_opt_int(duk, 4, 1);
    tic_flip flip = duk_opt_int(duk, 5, tic_no_flip);
    tic_rotate rotate = duk_opt_int(duk, 6, tic_no_rotate);
    s32 w = duk_opt_int(duk, 7, 1);
    s32 h = duk_opt_int(duk, 8, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_spr(tic, index, x, y, w, h, colors, count, scale, flip, rotate);

    return 0;
}

static duk_ret_t duk_btn(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_uint(duk, core->memory.ram.input.gamepads.data);
    }
    else
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;
        duk_push_boolean(duk, core->memory.ram.input.gamepads.data & (1 << index));
    }

    return 1;
}

static duk_ret_t duk_btnp(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = (tic_mem*)core;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_uint(duk, tic_api_btnp(tic, -1, -1, -1));
    }
    else if(duk_is_null_or_undefined(duk, 1) && duk_is_null_or_undefined(duk, 2))
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;

        duk_push_boolean(duk, tic_api_btnp(tic, index, -1, -1));
    }
    else
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;
        u32 hold = duk_to_int(duk, 1);
        u32 period = duk_to_int(duk, 2);

        duk_push_boolean(duk, tic_api_btnp(tic, index, hold, period));
    }

    return 1;
}

static s32 duk_key(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = &core->memory;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_boolean(duk, tic_api_key(tic, tic_key_unknown));
    }
    else
    {
        tic_key key = duk_to_int(duk, 0);

        if(key < tic_keys_count)
            duk_push_boolean(duk, tic_api_key(tic, key));
        else return duk_error(duk, DUK_ERR_ERROR, "unknown keyboard code\n");
    }

    return 1;
}

static s32 duk_keyp(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = &core->memory;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_boolean(duk, tic_api_keyp(tic, tic_key_unknown, -1, -1));
    }
    else
    {
        tic_key key = duk_to_int(duk, 0);

        if(key >= tic_keys_count)
        {
            return duk_error(duk, DUK_ERR_ERROR, "unknown keyboard code\n");
        }
        else
        {
            if(duk_is_null_or_undefined(duk, 1) && duk_is_null_or_undefined(duk, 2))
            {
                duk_push_boolean(duk, tic_api_keyp(tic, key, -1, -1));
            }
            else
            {
                u32 hold = duk_to_int(duk, 1);
                u32 period = duk_to_int(duk, 2);

                duk_push_boolean(duk, tic_api_keyp(tic, key, hold, period));
            }
        }
    }

    return 1;
}

static duk_ret_t duk_sfx(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    s32 index = duk_opt_int(duk, 0, -1);

    s32 note = -1;
    s32 octave = -1;
    s32 speed = SFX_DEF_SPEED;

    if (index < SFX_COUNT)
    {
        if(index >= 0)
        {
            tic_sample* effect = tic->ram.sfx.samples.data + index;

            note = effect->note;
            octave = effect->octave;
            speed = effect->speed;

            if(!duk_is_null_or_undefined(duk, 1))
            {
                if(duk_is_string(duk, 1))
                {
                    const char* noteStr = duk_to_string(duk, 1);

                    if(!tic_tool_parse_note(noteStr, &note, &octave))
                    {
                        return duk_error(duk, DUK_ERR_ERROR, "invalid note, should be like C#4\n");
                    }
                }
                else
                {
                    s32 id = duk_to_int(duk, 1);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
            }
        }
    }
    else
    {
        return duk_error(duk, DUK_ERR_ERROR, "unknown sfx index\n");
    }

    s32 duration = duk_opt_int(duk, 2, -1);
    s32 channel = duk_opt_int(duk, 3, 0);
    s32 volumes[TIC_STEREO_CHANNELS];

    if(duk_is_array(duk, 4))
    {
        for(s32 i = 0; i < COUNT_OF(volumes); i++)
        {
            duk_get_prop_index(duk, 4, i);
            if(!duk_is_null_or_undefined(duk, -1))
                volumes[i] = duk_to_int(duk, -1);
            duk_pop(duk);
        }
    }
    else volumes[0] = volumes[1] = duk_opt_int(duk, 4, MAX_VOLUME);

    speed = duk_opt_int(duk, 5, speed);

    if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
    {
        tic_api_sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
    }
    else return duk_error(duk, DUK_ERR_ERROR, "unknown channel\n");

    return 0;
}

typedef struct
{
    duk_context* duk;
    void* remap;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{

    RemapData* remap = (RemapData*)data;
    duk_context* duk = remap->duk;

    duk_push_heapptr(duk, remap->remap);
    duk_push_int(duk, result->index);
    duk_push_int(duk, x);
    duk_push_int(duk, y);
    duk_pcall(duk, 3);

    if(duk_is_array(duk, -1))
    {
        duk_get_prop_index(duk, -1, 0);
        result->index = duk_to_int(duk, -1);
        duk_pop(duk);

        duk_get_prop_index(duk, -1, 1);
        result->flip = duk_to_int(duk, -1);
        duk_pop(duk);

        duk_get_prop_index(duk, -1, 2);
        result->rotate = duk_to_int(duk, -1);
        duk_pop(duk);
    }
    else
    {
        result->index = duk_to_int(duk, -1);
    }

    duk_pop(duk);
}

static duk_ret_t duk_map(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);
    s32 w = duk_opt_int(duk, 2, TIC_MAP_SCREEN_WIDTH);
    s32 h = duk_opt_int(duk, 3, TIC_MAP_SCREEN_HEIGHT);
    s32 sx = duk_opt_int(duk, 4, 0);
    s32 sy = duk_opt_int(duk, 5, 0);
    s32 scale = duk_opt_int(duk, 7, 1);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    {
        if(!duk_is_null_or_undefined(duk, 6))
        {
            if(duk_is_array(duk, 6))
            {
                for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                {
                    duk_get_prop_index(duk, 6, i);
                    if(duk_is_null_or_undefined(duk, -1))
                    {
                        duk_pop(duk);
                        break;
                    }
                    else
                    {
                        colors[i] = duk_to_int(duk, -1);
                        count++;
                        duk_pop(duk);
                    }
                }
            }
            else
            {
                colors[0] = duk_to_int(duk, 6);
                count = 1;
            }
        }
    }

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    if (duk_is_null_or_undefined(duk, 8))
        tic_api_map(tic, x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);
    else
    {
        void* remap = duk_get_heapptr(duk, 8);

        RemapData data = {duk, remap};

        tic_api_map((tic_mem*)getDukCore(duk), x, y, w, h, sx, sy, colors, count, scale, remapCallback, &data);
    }

    return 0;
}

static duk_ret_t duk_mget(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u8 value = tic_api_mget(tic, x, y);
    duk_push_uint(duk, value);
    return 1;
}

static duk_ret_t duk_mset(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);
    u8 value = duk_opt_int(duk, 2, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_mset(tic, x, y, value);

    return 1;
}

static duk_ret_t duk_peek(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    duk_push_uint(duk, tic_api_peek(tic, address));
    return 1;
}

static duk_ret_t duk_poke(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_poke(tic, address, value);

    return 0;
}

static duk_ret_t duk_peek4(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    duk_push_uint(duk, tic_api_peek4(tic, address));
    return 1;
}

static duk_ret_t duk_poke4(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_poke4(tic, address, value);

    return 0;
}

static duk_ret_t duk_memcpy(duk_context* duk)
{
    s32 dest = duk_to_int(duk, 0);
    s32 src = duk_to_int(duk, 1);
    s32 size = duk_to_int(duk, 2);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_memcpy(tic, dest, src, size);

    return 0;
}

static duk_ret_t duk_memset(duk_context* duk)
{
    s32 dest = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);
    s32 size = duk_to_int(duk, 2);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_memset(tic, dest, value, size);

    return 0;
}

static duk_ret_t duk_trace(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    const char* text = duk_opt_string(duk, 0, "");
    u8 color = duk_opt_int(duk, 1, TIC_DEFAULT_COLOR);

    tic_api_trace(tic, text, color);

    return 0;
}

static duk_ret_t duk_pmem(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);
    u32 index = duk_to_int(duk, 0);

    if(index < TIC_PERSISTENT_SIZE)
    {
        u32 val = tic_api_pmem(tic, index, 0, false);

        if(!duk_is_null_or_undefined(duk, 1))
        {
            tic_api_pmem(tic, index, duk_to_uint(duk, 1), true);
        }

        duk_push_int(duk, val);

        return 1;
    }
    else return duk_error(duk, DUK_ERR_ERROR, "invalid persistent tic index\n");

    return 0;
}

static duk_ret_t duk_time(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    duk_push_number(duk, tic_api_time(tic));

    return 1;
}

static duk_ret_t duk_tstamp(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    duk_push_number(duk, tic_api_tstamp(tic));

    return 1;
}

static duk_ret_t duk_exit(duk_context* duk)
{
    tic_api_exit((tic_mem*)getDukCore(duk));

    return 0;
}

static duk_ret_t duk_font(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    const char* text = duk_to_string(duk, 0);
    s32 x = duk_to_int(duk, 1);
    s32 y = duk_to_int(duk, 2);
    u8 chromakey = duk_to_int(duk, 3);
    s32 width =  duk_opt_int(duk, 4, TIC_SPRITESIZE);
    s32 height =  duk_opt_int(duk, 5, TIC_SPRITESIZE);
    bool fixed = duk_opt_boolean(duk, 6, false);
    s32 scale =  duk_opt_int(duk, 7, 1);
    bool alt = duk_opt_boolean(duk, 8, false);
    if(scale == 0)
    {
        duk_push_int(duk, 0);
        return 1;
    }

    s32 size = tic_api_font(tic, text, x, y, chromakey, width, height, fixed, scale, alt);

    duk_push_int(duk, size);

    return 1;
}

static duk_ret_t duk_mouse(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    const tic80_mouse* mouse = &core->memory.ram.input.mouse;

    duk_idx_t idx = duk_push_array(duk);

    {
        tic_point pos = tic_api_mouse((tic_mem*)core);

        duk_push_int(duk, pos.x);
        duk_put_prop_index(duk, idx, 0);
        duk_push_int(duk, pos.y);
        duk_put_prop_index(duk, idx, 1);
    }

    duk_push_boolean(duk, mouse->left);
    duk_put_prop_index(duk, idx, 2);
    duk_push_boolean(duk, mouse->middle);
    duk_put_prop_index(duk, idx, 3);
    duk_push_boolean(duk, mouse->right);
    duk_put_prop_index(duk, idx, 4);
    duk_push_int(duk, mouse->scrollx);
    duk_put_prop_index(duk, idx, 5);
    duk_push_int(duk, mouse->scrolly);
    duk_put_prop_index(duk, idx, 6);

    return 1;
}

static duk_ret_t duk_circ(duk_context* duk)
{
    s32 radius = duk_to_int(duk, 2);
    if(radius < 0) return 0;

    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 color = duk_to_int(duk, 3);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_circ(tic, x, y, radius, color);

    return 0;
}

static duk_ret_t duk_circb(duk_context* duk)
{
    s32 radius = duk_to_int(duk, 2);
    if(radius < 0) return 0;

    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 color = duk_to_int(duk, 3);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_circb(tic, x, y, radius, color);

    return 0;
}

static duk_ret_t duk_tri(duk_context* duk)
{
    s32 pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = duk_to_int(duk, i);

    s32 color = duk_to_int(duk, 6);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    return 0;
}

static duk_ret_t duk_textri(duk_context* duk)
{
    float pt[12];

    for (s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = (float)duk_to_number(duk, i);
    tic_mem* tic = (tic_mem*)getDukCore(duk);
    bool use_map = duk_opt_boolean(duk, 12, false);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    {
        if(!duk_is_null_or_undefined(duk, 13))
        {
            if(duk_is_array(duk, 13))
            {
                for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                {
                    duk_get_prop_index(duk, 13, i);
                    if(duk_is_null_or_undefined(duk, -1))
                    {
                        duk_pop(duk);
                        break;
                    }
                    else
                    {
                        colors[i] = duk_to_int(duk, -1);
                        count++;
                        duk_pop(duk);
                    }
                }
            }
            else
            {
                colors[0] = duk_to_int(duk, 13);
                count = 1;
            }
        }
    }

    tic_api_textri(tic, pt[0], pt[1],   //  xy 1
                        pt[2], pt[3],   //  xy 2
                        pt[4], pt[5],   //  xy 3
                        pt[6], pt[7],   //  uv 1
                        pt[8], pt[9],   //  uv 2
                        pt[10], pt[11],//  uv 3
                        use_map, // usemap
                        colors, count);    //  chroma

    return 0;
}


static duk_ret_t duk_clip(duk_context* duk)
{
    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 w = duk_opt_int(duk, 2, TIC80_WIDTH);
    s32 h = duk_opt_int(duk, 3, TIC80_HEIGHT);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_clip(tic, x, y, w, h);

    return 0;
}

static duk_ret_t duk_music(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    s32 track = duk_opt_int(duk, 0, -1);
    tic_api_music(tic, -1, 0, 0, false, false);

    if(track >= 0)
    {
        s32 frame = duk_opt_int(duk, 1, -1);
        s32 row = duk_opt_int(duk, 2, -1);
        bool loop = duk_opt_boolean(duk, 3, true);
        bool sustain = duk_opt_boolean(duk, 4, false);

        tic_api_music(tic, track, frame, row, loop, sustain);
    }

    return 0;
}

static duk_ret_t duk_sync(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 mask = duk_opt_int(duk, 0, 0);
    s32 bank = duk_opt_int(duk, 1, 0);
    bool toCart = duk_opt_boolean(duk, 2, false);

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else
        return duk_error(duk, DUK_ERR_ERROR, "sync() error, invalid bank");

    return 0;
}

static duk_ret_t duk_reset(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    core->state.initialized = false;

    return 0;
}

static duk_ret_t duk_fget(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 index = duk_opt_int(duk, 0, 0);
    u32 flag = duk_opt_int(duk, 1, 0);

    bool value = tic_api_fget(tic, index, flag);

    duk_push_boolean(duk, value);

    return 1;
}

static duk_ret_t duk_fset(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 index = duk_opt_int(duk, 0, 0);
    u32 flag = duk_opt_int(duk, 1, 0);
    bool value = duk_opt_boolean(duk, 2, false);

    tic_api_fset(tic, index, flag, value);

    return 0;
}

static u64 ForceExitCounter = 0;

s32 duk_timeout_check(void* udata)
{
    tic_core* core = (tic_core*)udata;
    tic_tick_data* tick = core->data;

    return ForceExitCounter++ > 1000 ? tick->forceExit && tick->forceExit(tick->data) : false;
}

static void initDuktape(tic_core* core)
{
    closeJavascript((tic_mem*)core);

    duk_context* duk = core->js = duk_create_heap(NULL, NULL, NULL, core, NULL);

    {
        duk_push_global_stash(duk);
        duk_push_pointer(duk, core);
        duk_put_prop_string(duk, -2, TicCore);
        duk_pop(duk);
    }

#define API_FUNC_DEF(name, paramsCount, ...) {duk_ ## name, paramsCount, #name},
    static const struct{duk_c_function func; s32 params; const char* name;} ApiItems[] = {TIC_API_LIST(API_FUNC_DEF)};
#undef API_FUNC_DEF

    for (s32 i = 0; i < COUNT_OF(ApiItems); i++)
    {
        duk_push_c_function(core->js, ApiItems[i].func, ApiItems[i].params);
        duk_put_global_string(core->js, ApiItems[i].name);
    }
}

static bool initJavascript(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;

    initDuktape(core);
    duk_context* duktape = core->js;

    if (duk_pcompile_string(duktape, 0, code) != 0 || duk_peval_string(duktape, code) != 0)
    {
        core->data->error(core->data->data, duk_safe_to_stacktrace(duktape, -1));
        duk_pop(duktape);
        return false;
    }

    return true;
}

static void callJavascriptTick(tic_mem* tic)
{
    ForceExitCounter = 0;

    tic_core* core = (tic_core*)tic;

    duk_context* duk = core->js;

    if(duk)
    {
        if(duk_get_global_string(duk, TIC_FN))
        {
            if(duk_pcall(duk, 0) != DUK_EXEC_SUCCESS)
            {
                core->data->error(core->data->data, duk_safe_to_stacktrace(duk, -1));
            }
        }
        else core->data->error(core->data->data, "'function TIC()...' isn't found :(");

        duk_pop(duk);
    }
}

static void callJavascriptScanlineName(tic_mem* tic, s32 row, void* data, const char* name)
{
    tic_core* core = (tic_core*)tic;
    duk_context* duk = core->js;

    if(duk_get_global_string(duk, name))
    {
        duk_push_int(duk, row);

        if(duk_pcall(duk, 1) != 0)
            core->data->error(core->data->data, duk_safe_to_stacktrace(duk, -1));
    }

    duk_pop(duk);
}

static void callJavascriptScanline(tic_mem* tic, s32 row, void* data)
{
    callJavascriptScanlineName(tic, row, data, SCN_FN);

    // try to call old scanline
    callJavascriptScanlineName(tic, row, data, "scanline");
}

static void callJavascriptOverline(tic_mem* tic, void* data)
{
    tic_core* core = (tic_core*)tic;
    duk_context* duk = core->js;

    if(duk_get_global_string(duk, OVR_FN))
    {
        if(duk_pcall(duk, 0) != 0)
            core->data->error(core->data->data, duk_safe_to_stacktrace(duk, -1));
    }

    duk_pop(duk);
}

static const char* const JsKeywords [] =
{
    "break", "do", "instanceof", "typeof", "case", "else", "new",
    "var", "catch", "finally", "return", "void", "continue", "for",
    "switch", "while", "debugger", "function", "this", "with",
    "default", "if", "throw", "delete", "in", "try", "const",
    "true", "false"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getJsOutline(const char* code, s32* size)
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

                if(isalnum_(c));
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

void evalJs(tic_mem* tic, const char* code) {
    printf("TODO: JS eval not yet implemented\n.");
}

static const tic_script_config JsSyntaxConfig =
{
    .init               = initJavascript,
    .close              = closeJavascript,
    .tick               = callJavascriptTick,
    .scanline           = callJavascriptScanline,
    .overline           = callJavascriptOverline,

    .getOutline         = getJsOutline,
    .eval               = evalJs,

    .blockCommentStart  = "/*",
    .blockCommentEnd    = "*/",
    .blockCommentStart2 = "<!--",
    .blockCommentEnd2   = "-->",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = "//",

    .keywords           = JsKeywords,
    .keywordsCount      = COUNT_OF(JsKeywords),
};

const tic_script_config* getJsScriptConfig()
{
    return &JsSyntaxConfig;
}

#else

s32 duk_timeout_check(void* udata){return 0;}

#endif /* defined(TIC_BUILD_WITH_JS) */
