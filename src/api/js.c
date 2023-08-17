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
#include <string.h>
#include <quickjs.h>

static inline tic_core* getCore(JSContext *ctx)
{
    return JS_GetContextOpaque(ctx);
}

static s32 getInteger(JSContext *ctx, JSValueConst val)
{
    s32 res;
    JS_ToInt32(ctx, &res, val);
    return res;
}

static s32 getInteger2(JSContext *ctx, JSValueConst val, s32 defValue)
{
    s32 res;
    if(JS_IsUndefined(val))
    {
        res = defValue;
    }
    else
    {
        JS_ToInt32(ctx, &res, val);
    }

    return res;
}

static float getNumber(JSContext *ctx, JSValueConst val)
{
    double res;
    JS_ToFloat64(ctx, &res, val);

    return res;
}

static void js_dump_obj(JSContext *ctx, FILE *f, JSValueConst val)
{
    const char *str;

    tic_core* core = getCore(ctx);
    
    str = JS_ToCString(ctx, val);
    if (str) 
    {
        core->data->error(core->data->data, str);
        JS_FreeCString(ctx, str);
    }
    else 
    {
        core->data->error(core->data->data, "[exception]\n");
    }
}

static void js_std_dump_error1(JSContext *ctx, JSValueConst exception_val)
{
    JSValue val;
    bool is_error;
    
    is_error = JS_IsError(ctx, exception_val);
    js_dump_obj(ctx, stdout, exception_val);
    if (is_error) 
    {
        val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) 
        {
            js_dump_obj(ctx, stdout, val);
        }
        JS_FreeValue(ctx, val);
    }
}

static void js_std_dump_error(JSContext *ctx)
{
    JSValue exception_val;
    
    exception_val = JS_GetException(ctx);
    js_std_dump_error1(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
}

static void closeJavascript(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    JSContext *ctx = core->currentVM;

    if(ctx)
    {
        JSRuntime *rt = JS_GetRuntime(ctx);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        core->currentVM = NULL;
    }
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    const char* text = JS_ToCStringLen(ctx, NULL, argv[0]);
    s32 x = getInteger2(ctx, argv[1], 0);
    s32 y = getInteger2(ctx, argv[2], 0);
    s32 color = getInteger2(ctx, argv[3], TIC_DEFAULT_COLOR);
    bool fixed = JS_ToBool(ctx, argv[4]);
    s32 scale = getInteger2(ctx, argv[5], 1);
    bool alt = JS_ToBool(ctx, argv[6]);

    s32 size = tic_api_print(tic, text ? text : "nil", x, y, color, fixed, scale, alt);

    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, size);
}

static JSValue js_cls(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    s32 color = getInteger2(ctx, argv[0], tic_color_black);

    tic_api_cls(tic, color);

    return JS_UNDEFINED;
}

static JSValue js_pix(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    if(JS_IsUndefined(argv[2]))
    {
        return JS_NewInt32(ctx, tic_api_pix(tic, x, y, 0, true));
    }
    else
    {
        s32 color = getInteger(ctx, argv[2]);
        tic_api_pix(tic, x, y, color, false);
    }

    return JS_UNDEFINED;
}

static JSValue js_line(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    float x0 = getNumber(ctx, argv[0]);
    float y0 = getNumber(ctx, argv[1]);
    float x1 = getNumber(ctx, argv[2]);
    float y1 = getNumber(ctx, argv[3]);
    s32 color = getInteger2(ctx, argv[4], tic_color_black);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_line(tic, x0, y0, x1, y1, color);

    return JS_UNDEFINED;
}

static JSValue js_rect(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);
    s32 w = getInteger2(ctx, argv[2], 0);
    s32 h = getInteger2(ctx, argv[3], 0);
    s32 color = getInteger2(ctx, argv[4], 0);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_rect(tic, x, y, w, h, color);

    return JS_UNDEFINED;
}

static JSValue js_rectb(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);
    s32 w = getInteger2(ctx, argv[2], 0);
    s32 h = getInteger2(ctx, argv[3], 0);
    s32 color = getInteger2(ctx, argv[4], 0);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_rectb(tic, x, y, w, h, color);

    return JS_UNDEFINED;
}

static JSValue js_spr(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    s32 index = getInteger2(ctx, argv[0], 0);
    s32 x = getInteger2(ctx, argv[1], 0);
    s32 y = getInteger2(ctx, argv[2], 0);

    if(JS_IsArray(ctx, argv[3]))
    {
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            JSValue val = JS_GetPropertyUint32(ctx, argv[3], i);
            colors[i] = getInteger2(ctx, val, -1);
            count++;
        }
    }
    else
    {
        colors[0] = getInteger2(ctx, argv[3], 0);
        count = 1;
    }

    s32 scale = getInteger2(ctx, argv[4], 1);

    tic_flip flip = JS_IsBool(argv[5]) 
        ? JS_ToBool(ctx, argv[5]) ? tic_horz_flip : tic_no_flip
        : getInteger2(ctx, argv[5], tic_no_flip);

    tic_rotate rotate = getInteger2(ctx, argv[6], tic_no_rotate);
    s32 w = getInteger2(ctx, argv[7], 1);
    s32 h = getInteger2(ctx, argv[8], 1);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_spr(tic, index, x, y, w, h, colors, count, scale, flip, rotate);

    return JS_UNDEFINED;
}

static JSValue js_btn(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_IsUndefined(argv[0])
        ? JS_NewUint32(ctx, tic_api_btn(tic, -1))
        : JS_NewBool(ctx, tic_api_btn(tic, getInteger(ctx, argv[0]) & 0x1f));
}

static JSValue js_btnp(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    if(JS_IsUndefined(argv[0]))
    {
        return JS_NewUint32(ctx, tic_api_btnp(tic, -1, -1, -1));
    }

    s32 index = getInteger(ctx, argv[0]);
    u32 hold = getInteger2(ctx, argv[1], -1);
    u32 period = getInteger2(ctx, argv[2], -1);

    return JS_NewBool(ctx, tic_api_btnp(tic, index & 0x1f, hold, period));
}

static JSValue js_key(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_core* core = getCore(ctx);
    tic_mem* tic = &core->memory;

    if (JS_IsUndefined(argv[0]))
        return JS_NewBool(ctx, tic_api_key(tic, tic_key_unknown));

    tic_key key = getInteger(ctx, argv[0]);

    if(key < tic_keys_count)
        return JS_NewBool(ctx, tic_api_key(tic, key));
    else 
    {
        JSValue err = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, err, "message", JS_NewString(ctx, "unknown keyboard code"));
        JS_Throw(ctx, err);
    }

    return JS_UNDEFINED;
}

static void throwError(JSContext* ctx, const char* message)
{
    JSValue err = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, err, "message", JS_NewString(ctx, message));
    JS_Throw(ctx, err);
}

static JSValue js_keyp(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_core* core = getCore(ctx);
    tic_mem* tic = &core->memory;

    if (JS_IsUndefined(argv[0]))
        return JS_NewBool(ctx, tic_api_keyp(tic, tic_key_unknown, -1, -1));

    tic_key key = getInteger(ctx, argv[0]);

    if(key < tic_keys_count)
    {
        u32 hold = getInteger2(ctx, argv[1], -1);
        u32 period = getInteger2(ctx, argv[2], -1);

        return JS_NewBool(ctx, tic_api_keyp(tic, key, hold, period));
    }
    else 
    {
        throwError(ctx, "unknown keyboard code");
    }

    return JS_UNDEFINED;
}

static JSValue js_sfx(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    s32 index = getInteger2(ctx, argv[0], -1);

    s32 note = -1;
    s32 octave = -1;
    s32 speed = SFX_DEF_SPEED;

    if (index < SFX_COUNT)
    {
        if(index >= 0)
        {
            tic_sample* effect = tic->ram->sfx.samples.data + index;

            note = effect->note;
            octave = effect->octave;
            speed = effect->speed;

            if(!JS_IsUndefined(argv[1]))
            {
                if(JS_IsString(argv[1]))
                {
                    const char *noteStr = JS_ToCStringLen(ctx, NULL, argv[1]);

                    if(!tic_tool_parse_note(noteStr, &note, &octave))
                    {
                        throwError(ctx, "invalid note, should be like C#4");
                    }

                    JS_FreeCString(ctx, noteStr);
                }
                else
                {
                    s32 id = getInteger(ctx, argv[1]);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
            }
        }
    }
    else
    {
        throwError(ctx, "unknown sfx index");
    }

    s32 duration = getInteger2(ctx, argv[2], -1);
    s32 channel = getInteger2(ctx, argv[3], 0);
    s32 volumes[TIC80_SAMPLE_CHANNELS];

    if(JS_IsArray(ctx, argv[4]))
    {
        for(s32 i = 0; i < COUNT_OF(volumes); i++)
        {
            JSValue val = JS_GetPropertyUint32(ctx, argv[4], i);
            volumes[i] = getInteger(ctx, val);
        }
    }
    else volumes[0] = volumes[1] = getInteger2(ctx, argv[4], MAX_VOLUME);

    speed = getInteger2(ctx, argv[5], speed);

    if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
    {
        tic_api_sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
    }
    else throwError(ctx, "unknown channel");

    return JS_UNDEFINED;
}

typedef struct
{
    JSContext* ctx;
    JSValueConst func;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    RemapData* remap = (RemapData*)data;
    JSContext* ctx = remap->ctx;

    JSValue res = JS_Call(ctx, remap->func, JS_UNDEFINED, 3, 
        (JSValueConst[])
        {
            JS_NewInt32(ctx, result->index), 
            JS_NewInt32(ctx, x), 
            JS_NewInt32(ctx, y), 
        });

    if(JS_IsArray(ctx, res))
    {
        result->index = getInteger2(ctx, JS_GetPropertyUint32(ctx, res, 0), result->index);
        result->flip = getInteger2(ctx, JS_GetPropertyUint32(ctx, res, 1), result->flip);
        result->rotate = getInteger2(ctx, JS_GetPropertyUint32(ctx, res, 2), result->rotate);
    }
    else
    {
        result->index = getInteger2(ctx, res, result->index);
    }
}

static JSValue js_map(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);
    s32 w = getInteger2(ctx, argv[2], TIC_MAP_SCREEN_WIDTH);
    s32 h = getInteger2(ctx, argv[3], TIC_MAP_SCREEN_HEIGHT);
    s32 sx = getInteger2(ctx, argv[4], 0);
    s32 sy = getInteger2(ctx, argv[5], 0);
    s32 scale = getInteger2(ctx, argv[7], 1);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(JS_IsArray(ctx, argv[6]))
    {
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            JSValue val = JS_GetPropertyUint32(ctx, argv[6], i);
            colors[i] = getInteger2(ctx, val, -1);
            count++;
        }
    }
    else
    {
        colors[0] = getInteger2(ctx, argv[6], 0);
        count = 1;
    }

    tic_mem* tic = (tic_mem*)getCore(ctx);

    if(JS_IsFunction(ctx, argv[8]))
    {
        RemapData data = {ctx, argv[8]};
        tic_api_map((tic_mem*)getCore(ctx), x, y, w, h, sx, sy, colors, count, scale, remapCallback, &data);
    }
    else
    {
        tic_api_map(tic, x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);
    }

    return JS_UNDEFINED;
}

static JSValue js_mget(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    u8 value = tic_api_mget(tic, x, y);
    return JS_NewInt32(ctx, value);
}

static JSValue js_mset(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger2(ctx, argv[0], 0);
    s32 y = getInteger2(ctx, argv[1], 0);
    u8 value = getInteger2(ctx, argv[2], 0);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_mset(tic, x, y, value);

    return JS_UNDEFINED;
}

static JSValue js_peek(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);
    s32 bits = getInteger2(ctx, argv[1], BITS_IN_BYTE);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewInt32(ctx, tic_api_peek(tic, address, bits));
}

static JSValue js_poke(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);
    u8 value = getInteger(ctx, argv[1]);
    s32 bits = getInteger2(ctx, argv[2], BITS_IN_BYTE);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_poke(tic, address, value, bits);

    return JS_UNDEFINED;
}

static JSValue js_peek1(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewInt32(ctx, tic_api_peek1(tic, address));
}

static JSValue js_poke1(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);
    u8 value = getInteger(ctx, argv[1]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_poke1(tic, address, value);

    return JS_UNDEFINED;
}

static JSValue js_peek2(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewInt32(ctx, tic_api_peek2(tic, address));
}

static JSValue js_poke2(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);
    u8 value = getInteger(ctx, argv[1]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_poke2(tic, address, value);

    return JS_UNDEFINED;
}

static JSValue js_peek4(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewInt32(ctx, tic_api_peek4(tic, address));
}

static JSValue js_poke4(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 address = getInteger(ctx, argv[0]);
    u8 value = getInteger(ctx, argv[1]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_poke4(tic, address, value);

    return JS_UNDEFINED;
}

static JSValue js_memcpy(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 dest = getInteger(ctx, argv[0]);
    s32 src = getInteger(ctx, argv[1]);
    s32 size = getInteger(ctx, argv[2]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_memcpy(tic, dest, src, size);

    return JS_UNDEFINED;
}

static JSValue js_memset(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 dest = getInteger(ctx, argv[0]);
    u8 value = getInteger(ctx, argv[1]);
    s32 size = getInteger(ctx, argv[2]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_api_memset(tic, dest, value, size);

    return JS_UNDEFINED;
}

static JSValue js_trace(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    const char* text = JS_ToCString(ctx, argv[0]);
    u8 color = getInteger2(ctx, argv[1], TIC_DEFAULT_COLOR);

    tic_api_trace(tic, text, color);

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_pmem(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);
    u32 index = getInteger(ctx, argv[0]);

    if(index < TIC_PERSISTENT_SIZE)
    {
        u32 val = tic_api_pmem(tic, index, 0, false);

        if(!JS_IsUndefined(argv[1]))
        {
            tic_api_pmem(tic, index, getInteger(ctx, argv[1]), true);
        }

        return JS_NewInt32(ctx, val);
    }
    else throwError(ctx, "invalid persistent tic index");

    return JS_UNDEFINED;
}

static JSValue js_time(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewFloat64(ctx, tic_api_time(tic));
}

static JSValue js_tstamp(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    return JS_NewInt32(ctx, tic_api_tstamp(tic));
}

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_api_exit((tic_mem*)getCore(ctx));

    return JS_UNDEFINED;
}

static JSValue js_font(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    const char* text = JS_ToCString(ctx, argv[0]);
    s32 x = getInteger(ctx, argv[1]);
    s32 y = getInteger(ctx, argv[2]);
    u8 chromakey = getInteger(ctx, argv[3]);
    s32 width =  getInteger2(ctx, argv[4], TIC_SPRITESIZE);
    s32 height =  getInteger2(ctx, argv[5], TIC_SPRITESIZE);
    bool fixed = JS_ToBool(ctx, argv[6]);
    s32 scale =  getInteger2(ctx, argv[7], 1);
    bool alt = JS_ToBool(ctx, argv[8]);

    if(scale == 0)
    {
        return JS_NewInt32(ctx, 0);
    }

    s32 size = tic_api_font(tic, text, x, y, &chromakey, 1, width, height, fixed, scale, alt);

    JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, size);
}

static JSValue js_mouse(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_core* core = getCore(ctx);

    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    JSValue arr = JS_NewArray(ctx);

    tic_point pos = tic_api_mouse((tic_mem*)core);

    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, pos.x));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, pos.y));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewBool(ctx, mouse->left));
    JS_SetPropertyUint32(ctx, arr, 3, JS_NewBool(ctx, mouse->middle));
    JS_SetPropertyUint32(ctx, arr, 4, JS_NewBool(ctx, mouse->right));
    JS_SetPropertyUint32(ctx, arr, 5, JS_NewInt32(ctx, mouse->scrollx));
    JS_SetPropertyUint32(ctx, arr, 6, JS_NewInt32(ctx, mouse->scrolly));

    return arr;
}

static JSValue js_circ(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger(ctx, argv[0]);
    s32 y = getInteger(ctx, argv[1]);
    s32 radius = getInteger(ctx, argv[2]);
    s32 color = getInteger(ctx, argv[3]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_circ(tic, x, y, radius, color);

    return JS_UNDEFINED;
}

static JSValue js_circb(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger(ctx, argv[0]);
    s32 y = getInteger(ctx, argv[1]);
    s32 radius = getInteger(ctx, argv[2]);
    s32 color = getInteger(ctx, argv[3]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_circb(tic, x, y, radius, color);

    return JS_UNDEFINED;
}

static JSValue js_elli(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger(ctx, argv[0]);
    s32 y = getInteger(ctx, argv[1]);
    s32 a = getInteger(ctx, argv[2]);
    s32 b = getInteger(ctx, argv[3]);
    s32 color = getInteger(ctx, argv[4]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_elli(tic, x, y, a, b, color);

    return JS_UNDEFINED;
}

static JSValue js_ellib(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger(ctx, argv[0]);
    s32 y = getInteger(ctx, argv[1]);
    s32 a = getInteger(ctx, argv[2]);
    s32 b = getInteger(ctx, argv[3]);
    s32 color = getInteger(ctx, argv[4]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_ellib(tic, x, y, a, b, color);

    return JS_UNDEFINED;
}

static JSValue js_tri(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    float pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = getNumber(ctx, argv[i]);

    s32 color = getInteger(ctx, argv[6]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    return JS_UNDEFINED;
}

static JSValue js_trib(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    float pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = getNumber(ctx, argv[i]);

    s32 color = getInteger(ctx, argv[6]);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_trib(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    return JS_UNDEFINED;
}

#if defined(BUILD_DEPRECATED)

static JSValue js_textri(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    float pt[12];

    for (s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = getNumber(ctx, argv[i]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    bool use_map = JS_ToBool(ctx, argv[12]);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    if(JS_IsArray(ctx, argv[13]))
    {
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            JSValue val = JS_GetPropertyUint32(ctx, argv[13], i);
            colors[i] = getInteger2(ctx, val, -1);
            count++;
        }
    }
    else
    {
        colors[0] = getInteger2(ctx, argv[13], 0);
        count = 1;
    }

    tic_core_textri_dep(getCore(ctx), 
        pt[0], pt[1],   //  xy 1
        pt[2], pt[3],   //  xy 2
        pt[4], pt[5],   //  xy 3
        pt[6], pt[7],   //  uv 1
        pt[8], pt[9],   //  uv 2
        pt[10], pt[11], //  uv 3
        use_map,        //  use_map
        colors, count); //  chroma

    return JS_UNDEFINED;
}

#endif

static JSValue js_ttri(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    float pt[12];

    for (s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = getNumber(ctx, argv[i]);

    tic_mem* tic = (tic_mem*)getCore(ctx);
    tic_texture_src src = getInteger(ctx, argv[12]);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    if(JS_IsArray(ctx, argv[13]))
    {
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            JSValue val = JS_GetPropertyUint32(ctx, argv[13], i);
            colors[i] = getInteger2(ctx, val, -1);
            count++;
        }
    }
    else
    {
        colors[0] = getInteger2(ctx, argv[13], 0);
        count = 1;
    }

    float z[3];
    bool depth = true;

    for (s32 i = 0, index = 14; i < COUNT_OF(z); i++, index++)
    {
        if(JS_IsUndefined(argv[index])) depth = false;
        else z[i] = getNumber(ctx, argv[index]);
    }

    tic_api_ttri(tic,   pt[0], pt[1],               //  xy 1
                        pt[2], pt[3],               //  xy 2
                        pt[4], pt[5],               //  xy 3
                        pt[6], pt[7],               //  uv 1
                        pt[8], pt[9],               //  uv 2
                        pt[10], pt[11],             //  uv 3
                        src,                        //  texture source
                        colors, count,              //  chroma
                        z[0], z[1], z[2], depth);   //  depth 

    return JS_UNDEFINED;
}


static JSValue js_clip(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    s32 x = getInteger(ctx, argv[0]);
    s32 y = getInteger(ctx, argv[1]);
    s32 w = getInteger2(ctx, argv[2], TIC80_WIDTH);
    s32 h = getInteger2(ctx, argv[3], TIC80_HEIGHT);

    tic_mem* tic = (tic_mem*)getCore(ctx);

    tic_api_clip(tic, x, y, w, h);

    return JS_UNDEFINED;
}

static JSValue js_music(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    s32 track = getInteger2(ctx, argv[0], -1);
    tic_api_music(tic, -1, 0, 0, false, false, -1, -1);

    if(track >= 0)
    {
        if(track > MUSIC_TRACKS - 1)
        {
            throwError(ctx, "invalid music track index");
            return JS_UNDEFINED;
        }

        s32 frame = getInteger2(ctx, argv[1], -1);
        s32 row = getInteger2(ctx, argv[2], -1);
        bool loop = JS_IsUndefined(argv[3]) ? true : JS_ToBool(ctx, argv[3]);
        bool sustain = JS_ToBool(ctx, argv[4]);
        s32 tempo = getInteger2(ctx, argv[5], -1);
        s32 speed = getInteger2(ctx, argv[6], -1);

        tic_api_music(tic, track, frame, row, loop, sustain, tempo, speed);
    }

    return JS_UNDEFINED;
}

static JSValue js_vbank(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_core* core = getCore(ctx);
    tic_mem* tic = (tic_mem*)core;

    s32 prev = core->state.vbank.id;

    if(!JS_IsUndefined(argv[0]))
        tic_api_vbank(tic, getInteger2(ctx, argv[0], 0));

    return JS_NewUint32(ctx, prev);
}

static JSValue js_sync(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    u32 mask = getInteger2(ctx, argv[0], 0);
    s32 bank = getInteger2(ctx, argv[1], 0);
    bool toCart = JS_ToBool(ctx, argv[2]);

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else
        throwError(ctx, "sync() error, invalid bank");

    return JS_UNDEFINED;
}

static JSValue js_reset(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_core* core = getCore(ctx);

    core->state.initialized = false;

    return JS_UNDEFINED;
}

static JSValue js_fget(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    u32 index = getInteger2(ctx, argv[0], 0);
    u32 flag = getInteger2(ctx, argv[1], 0);

    bool value = tic_api_fget(tic, index, flag);

    return JS_NewBool(ctx, value);
}

static JSValue js_fset(JSContext *ctx, JSValueConst this_val, s32 argc, JSValueConst *argv)
{
    tic_mem* tic = (tic_mem*)getCore(ctx);

    u32 index = getInteger2(ctx, argv[0], 0);
    u32 flag = getInteger2(ctx, argv[1], 0);
    bool value = JS_ToBool(ctx, argv[2]);

    tic_api_fset(tic, index, flag, value);

    return JS_UNDEFINED;
}

static bool initJavascript(tic_mem* tic, const char* code)
{
    closeJavascript(tic);

    JSRuntime *rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    tic_core* core = (tic_core*)tic;
    core->currentVM = ctx;
    JS_SetContextOpaque(ctx, core);

    {
        JSValue global = JS_GetGlobalObject(ctx);

#define API_FUNC_DEF(name, _, __, paramsCount, ...) \
        JS_SetPropertyStr(ctx, global, #name, JS_NewCFunction(ctx, js_ ## name, #name, paramsCount));

        TIC_API_LIST(API_FUNC_DEF)

#if defined(BUILD_DEPRECATED)
        API_FUNC_DEF(textri, _, _, 14)
#endif

#undef  API_FUNC_DEF

        JS_FreeValue(ctx, global);
    }

    JSValue ret = JS_Eval(ctx, code, strlen(code), "index.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret))
    {
        js_std_dump_error(ctx);
        return false;
    }
    else
        JS_FreeValue(ctx, ret);

    return true;
}

static bool callFunc1(JSContext* ctx, JSValue func, JSValue this_val, JSValue value)
{
    JSValue ret = JS_Call(ctx, func, this_val, 1, (JSValueConst[]){value});
    if (JS_IsException(ret))
    {
        js_std_dump_error(ctx);
        return false;
    }
    else
        JS_FreeValue(ctx, ret);

    return true;
}

static bool callFunc(JSContext* ctx, JSValue func, JSValue this_val)
{
    JSValue ret = JS_Call(ctx, func, this_val, 0, NULL);
    if (JS_IsException(ret))
    {
        js_std_dump_error(ctx);
        return false;
    }
    else
        JS_FreeValue(ctx, ret);

    return true;
}

static void callJavascriptTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    JSContext* ctx = core->currentVM;

    if(ctx)
    {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue func = JS_GetPropertyStr(ctx, global, TIC_FN);

        if(JS_IsFunction(ctx, func))
        {
            {
                JSContext *ctx1;

                for (;;)
                {
                    s32 err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
                    if (err <= 0)
                    {
                        if (err < 0)
                        {
                            js_std_dump_error(ctx);
                        }
                        break;
                    }
                }
            }

            if(callFunc(ctx, func, global))
            {
#if defined(BUILD_DEPRECATED)
                // call OVR() callback for backward compatibility
                {
                    JSValue ovrfunc = JS_GetPropertyStr(ctx, global, OVR_FN);

                    if(JS_IsFunction(ctx, ovrfunc))
                    {
                        OVR(core)
                        {
                            callFunc(ctx, ovrfunc, global);
                        }
                    }

                    JS_FreeValue(ctx, ovrfunc);
                }
#endif
            }
        }
        else core->data->error(core->data->data, "'function TIC()...' isn't found :(");

        JS_FreeValue(ctx, func);
        JS_FreeValue(ctx, global);
    }
}

static void callJavascriptIntCallback(tic_mem* tic, s32 value, void* data, const char* name)
{
    tic_core* core = (tic_core*)tic;
    JSContext* ctx = core->currentVM;

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func = JS_GetPropertyStr(ctx, global, name);

    if(JS_IsFunction(ctx, func))
    {
        callFunc1(ctx, func, global, JS_NewInt32(ctx, value));
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global);
}

static void callJavascriptScanline(tic_mem* tic, s32 row, void* data)
{
    callJavascriptIntCallback(tic, row, data, SCN_FN);

    // try to call old scanline
    callJavascriptIntCallback(tic, row, data, "scanline");
}

static void callJavascriptBorder(tic_mem* tic, s32 row, void* data)
{
    callJavascriptIntCallback(tic, row, data, BDR_FN);
}

static void callJavascriptMenu(tic_mem* tic, s32 index, void* data)
{
    callJavascriptIntCallback(tic, index, data, MENU_FN);
}

static void callJavascriptBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    JSContext* ctx = core->currentVM;

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func = JS_GetPropertyStr(ctx, global, BOOT_FN);

    if(JS_IsFunction(ctx, func))
    {
        callFunc(ctx, func, global);
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global);
}

static const char* const JsKeywords [] =
{
    "await", "break", "case", "catch", "class", "const", "continue", "debugger", 
    "default", "delete", "do", "else", "enum", "export", "extends", "false", 
    "finally", "for", "function", "if", "implements", "import", "in", "instanceof", 
    "interface", "let", "new", "null", "package", "private", "protected", 
    "public", "return", "super", "switch", "static", "this", "throw", "try", 
    "true", "typeof", "var", "void", "while", "with", "yield"
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

static void evalJs(tic_mem* tic, const char* code) 
{
    tic_core* core = (tic_core*)tic;
    core->data->error(core->data->data, "TODO: JS eval not yet implemented\n.");
}

const tic_script_config JsSyntaxConfig =
{
    .id                 = 12,
    .name               = "js",
    .fileExtension      = ".js",
    .projectComment     = "//",
    {
      .init               = initJavascript,
      .close              = closeJavascript,
      .tick               = callJavascriptTick,
      .boot               = callJavascriptBoot,
      .callback           =
      {
        .scanline       = callJavascriptScanline,
        .border         = callJavascriptBorder,
        .menu           = callJavascriptMenu,
      },
    },

    .getOutline         = getJsOutline,
    .eval               = evalJs,

    .blockCommentStart  = "/*",
    .blockCommentEnd    = "*/",
    .blockCommentStart2 = "<!--",
    .blockCommentEnd2   = "-->",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = "//",
    .blockEnd           = "}",

    .keywords           = JsKeywords,
    .keywordsCount      = COUNT_OF(JsKeywords),
};

#endif /* defined(TIC_BUILD_WITH_JS) */
