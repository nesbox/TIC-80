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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gravity_compiler.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_macros.h"
#include "gravity_vm.h"
#include "gravity_vmmacros.h"

// gravity_config.h defines some things we don't want in this file
#ifdef close
#undef close
#endif

extern bool parse_note(const char* noteStr, s32* note, s32* octave);

// region Macros

#define TIC_GRAVITY_GET_CORE(VM, TICMEM, TICCORE)               \
    tic_mem* TICMEM = (tic_mem*)gravity_vm_delegate(VM)->xdata; \
    tic_core* TICCORE = (tic_core*)(TICMEM);

// endregion

// region Types

typedef struct
{
    gravity_vm* vm;
    tic_mem* tic;
    gravity_delegate_t delegate;
    gravity_closure_t* borderFunction;   // BDR_FN
    gravity_closure_t* bootFunction;     // BOOT_FN
    gravity_closure_t* menuFunction;     // MENU_FN
    gravity_closure_t* scanlineFunction; // SCN_FN
    gravity_closure_t* tickFunction;     // TIC_FN
} GRAVITYVM;

// endregion

// region API

static int grav_get_int_default(gravity_value_t value, int default_value)
{
    if(VALUE_ISA_INT(value) || VALUE_ISA_BOOL(value)) return (int)VALUE_AS_INT(value);
    if(VALUE_ISA_FLOAT(value)) return (int)VALUE_AS_FLOAT(value);
    if(VALUE_ISA_STRING(value))
    {
        const gravity_string_t* str = VALUE_AS_STRING(value);
        char* end;
        float parsed = strtof(str->s, &end);
        if(end != str->s)
        {
            // ignore trailing whitespace as in Lua
            while(isspace((u8)*end))
                end++;
            if(*end == '\0') return (int)parsed;
        }
    }
    return default_value;
}

static float grav_get_float_default(gravity_value_t value, float default_value)
{
    if(VALUE_ISA_INT(value) || VALUE_ISA_BOOL(value)) return (float)VALUE_AS_INT(value);
    if(VALUE_ISA_FLOAT(value)) return (float)VALUE_AS_FLOAT(value);
    if(VALUE_ISA_STRING(value))
    {
        const gravity_string_t* str = VALUE_AS_STRING(value);
        char* end;
        float parsed = strtof(str->s, &end);
        if(end != str->s)
        {
            // ignore trailing whitespace as in Lua
            while(isspace((u8)*end))
                end++;
            if(*end == '\0') return parsed;
        }
    }
    return default_value;
}

static gravity_string_t* grav_get_string_default(gravity_vm* vm, gravity_value_t value, const char* default_value)
{
    if(VALUE_ISA_STRING(value)) return VALUE_AS_STRING(value);

    if(VALUE_ISA_BOOL(value))
    {
        const char* str = VALUE_AS_BOOL(value) ? "true" : "false";
        return VALUE_AS_STRING(gravity_string_to_value(vm, str, (u32)strlen(str)));
    }

    char buf[256];

    if(VALUE_ISA_INT(value))
    {
        snprintf(buf, sizeof(buf), "%d", (int)VALUE_AS_INT(value));
        return VALUE_AS_STRING(gravity_string_to_value(vm, buf, (u32)strlen(buf)));
    }

    if(VALUE_ISA_FLOAT(value))
    {
        snprintf(buf, sizeof(buf), "%g", VALUE_AS_FLOAT(value));
        return VALUE_AS_STRING(gravity_string_to_value(vm, buf, (u32)strlen(buf)));
    }

    if(default_value == NULL) return VALUE_AS_STRING(gravity_string_to_value(vm, "", 0));
    return VALUE_AS_STRING(gravity_string_to_value(vm, default_value, (u32)strlen(default_value)));
}

#define grav_get_int(VALUE)        grav_get_int_default(VALUE, 0)
#define grav_get_float(VALUE)      grav_get_float_default(VALUE, 0.0f)
#define grav_get_string(VM, VALUE) grav_get_string_default(VM, VALUE, NULL)

// MARK: btn
static bool grav_btn(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 2) RETURN_ERROR("invalid parameters, btn [id]");

    s32 index = nargs == 2 ? grav_get_int(args[1]) & 0x1f : -1;
    u32 result = core->api.btn(tic, index);

    gravity_value_t value = nargs == 1 ? VALUE_FROM_INT(result) : VALUE_FROM_BOOL(result);
    RETURN_VALUE(value, rindex);
}

// MARK: btnp
static bool grav_btnp(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 4) RETURN_ERROR("invalid parameters, btnp [id [hold [period]]]");

    s32 index = nargs > 1 ? grav_get_int(args[1]) & 0x1f : -1;
    s32 hold = nargs > 2 ? grav_get_int(args[2]) : -1;
    s32 period = nargs > 3 ? grav_get_int(args[3]) : -1;
    u32 result = core->api.btnp(tic, index, hold, period);

    gravity_value_t value = nargs == 1 ? VALUE_FROM_INT(result) : VALUE_FROM_BOOL(result);
    RETURN_VALUE(value, rindex);
}

// MARK: circ
static bool grav_circ(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 5) RETURN_ERROR("invalid parameters, circ x y radius color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 radius = grav_get_int(args[3]);
    u8 color = (u8)grav_get_int(args[4]);
    core->api.circ(tic, x, y, radius, color);

    RETURN_NOVALUE();
}

// MARK: circb
static bool grav_circb(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 5) RETURN_ERROR("invalid parameters, circb x y radius color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 radius = grav_get_int(args[3]);
    u8 color = (u8)grav_get_int(args[4]);
    core->api.circb(tic, x, y, radius, color);

    RETURN_NOVALUE();
}

// MARK: clip
static bool grav_clip(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs == 1)
    {
        core->api.clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
        RETURN_NOVALUE();
    }

    if(nargs == 5)
    {
        s32 x = grav_get_int(args[1]);
        s32 y = grav_get_int(args[2]);
        s32 w = grav_get_int(args[3]);
        s32 h = grav_get_int(args[4]);
        core->api.clip(tic, x, y, w, h);
        RETURN_NOVALUE();
    }

    RETURN_ERROR("invalid parameters, clip [x y width height]");
}

// MARK: cls
static bool grav_cls(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 2) RETURN_ERROR("invalid parameters, cls [color=0]");

    u8 cls_color = nargs > 1 ? (u8)grav_get_int(args[1]) : 0;
    core->api.cls(tic, cls_color);

    RETURN_NOVALUE();
}

// MARK: elli
static bool grav_elli(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 6) RETURN_ERROR("invalid parameters, elli x y a b color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 a = grav_get_int(args[3]);
    s32 b = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    core->api.elli(tic, x, y, a, b, color);

    RETURN_NOVALUE();
}

// MARK: ellib
static bool grav_ellib(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 6) RETURN_ERROR("invalid parameters, ellib x y a b color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 a = grav_get_int(args[3]);
    s32 b = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    core->api.ellib(tic, x, y, a, b, color);

    RETURN_NOVALUE();
}

// MARK: exit
static bool grav_exit(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    (void)args;
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 1) RETURN_ERROR("invalid parameters, exit");

    core->api.exit(tic);

    RETURN_NOVALUE();
}

// MARK: fget
static bool grav_fget(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 3) RETURN_ERROR("invalid parameters, fget sprite_id flag");

    s32 index = grav_get_int(args[1]);
    u8 flag = (u8)grav_get_int(args[2]);

    RETURN_VALUE(VALUE_FROM_BOOL(core->api.fget(tic, index, flag)), rindex);
}

// MARK: font
static bool grav_font(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 2 || nargs > 10)
        RETURN_ERROR("invalid parameters, font text [x=0 y=0 [transcolor=0 [charwidth=8 [charheight=8 [fixed=false [scale=1 [alt=false]]]]]]]");

    gravity_string_t* text = grav_get_string(vm, args[1]);
    s32 x = nargs >= 3 ? grav_get_int(args[2]) : 0;
    s32 y = nargs >= 4 ? grav_get_int(args[3]) : 0;
    u8 chromakey = nargs >= 5 ? (u8)grav_get_int(args[4]) : 0;
    s32 width = nargs >= 6 ? grav_get_int(args[5]) : TIC_SPRITESIZE;
    s32 height = nargs >= 7 ? grav_get_int(args[6]) : TIC_SPRITESIZE;
    bool fixed = nargs >= 8 ? grav_get_int(args[7]) != 0 : false;
    s32 scale = nargs >= 9 ? grav_get_int(args[8]) : 1;
    bool alt = nargs >= 10 ? grav_get_int(args[9]) != 0 : false;

    if(scale == 0) RETURN_VALUE(VALUE_FROM_INT(0), rindex);

    s32 size = core->api.font(tic, text->s, x, y, &chromakey, 1, width, height, fixed, scale, alt);

    RETURN_VALUE(VALUE_FROM_INT(size), rindex);
}

// MARK: fset
static bool grav_fset(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 4) RETURN_ERROR("invalid parameters, fset sprite_id flag bool");

    s32 index = grav_get_int(args[1]);
    u8 flag = (u8)grav_get_int(args[2]);
    bool value = grav_get_int(args[3]) != 0;
    core->api.fset(tic, index, flag, value);

    RETURN_NOVALUE();
}

// MARK: key
static bool grav_key(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 2) RETURN_ERROR("invalid parameters, key [code]");

    tic_key key = nargs == 2 ? (tic_key)grav_get_int(args[1]) : tic_key_unknown;
    if(key >= tic_keys_count) RETURN_ERROR("unknown keyboard code");

    RETURN_VALUE(VALUE_FROM_BOOL(core->api.key(tic, key)), rindex);
}

// MARK: keyp
static bool grav_keyp(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 4) RETURN_ERROR("invalid parameters, keyp [code [hold [period]]]");

    tic_key key = nargs > 1 ? (tic_key)grav_get_int(args[1]) : tic_key_unknown;
    if(key >= tic_keys_count) RETURN_ERROR("unknown keyboard code");

    s32 hold = nargs > 2 ? grav_get_int(args[2]) : -1;
    s32 period = nargs > 3 ? grav_get_int(args[3]) : -1;

    RETURN_VALUE(VALUE_FROM_BOOL(core->api.keyp(tic, key, hold, period)), rindex);
}

// MARK: line
static bool grav_line(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 6) RETURN_ERROR("invalid parameters, line x0 y0 x1 y1 color");

    float x0 = grav_get_float(args[1]);
    float y0 = grav_get_float(args[2]);
    float x1 = grav_get_float(args[3]);
    float y1 = grav_get_float(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    core->api.line(tic, x0, y0, x1, y1, color);

    RETURN_NOVALUE();
}

// MARK: remap

typedef struct
{
    gravity_vm* vm;
    gravity_closure_t* callback;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    gravity_value_t params[3] = {
        VALUE_FROM_INT(result->index),
        VALUE_FROM_INT(x),
        VALUE_FROM_INT(y),
    };

    RemapData* remap = (RemapData*)data;
    gravity_vm* vm = remap->vm;
    gravity_vm_loadclosure(vm, remap->callback);
    gravity_vm_runclosure(vm, remap->callback, VALUE_FROM_NULL, params, COUNT_OF(params));

    gravity_value_t rv = gravity_vm_result(vm);

    if(VALUE_ISA_LIST(rv))
    {
        gravity_list_t* list = VALUE_AS_LIST(rv);
        u8 new_index = list->array.n >= 1 ? (u8)grav_get_int(list->array.p[0]) : result->index;
        tic_flip new_flip = list->array.n >= 2 ? (tic_flip)grav_get_int(list->array.p[1]) : result->flip;
        tic_rotate new_rotate = list->array.n >= 3 ? (tic_rotate)grav_get_int(list->array.p[2]) : result->rotate;
        result->index = new_index;
        result->flip = new_flip;
        result->rotate = new_rotate;

        // if we've been passed a list in remap, we need to aggressively garbage collect to prevent lag
        gravity_gc_start(vm);
    }
    else
    {
        u8 new_index = (u8)grav_get_int(rv);
        result->index = new_index;
    }
}

// MARK: map
static bool grav_map(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 10) RETURN_ERROR("invalid parameters, map [x=0 [y=0 [w=30 [h=17 [sx=0 [sy=0 [colorkey=-1 [scale=1 [remap=null]]]]]]]]]");

    s32 x = nargs > 1 ? grav_get_int(args[1]) : 0;
    s32 y = nargs > 2 ? grav_get_int(args[2]) : 0;
    s32 w = nargs > 3 ? grav_get_int(args[3]) : TIC_MAP_SCREEN_WIDTH;
    s32 h = nargs > 4 ? grav_get_int(args[4]) : TIC_MAP_SCREEN_HEIGHT;
    s32 sx = nargs > 5 ? grav_get_int(args[5]) : 0;
    s32 sy = nargs > 6 ? grav_get_int(args[6]) : 0;

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(nargs > 7)
    {
        if(VALUE_ISA_LIST(args[7]))
        {
            gravity_list_t* list = VALUE_AS_LIST(args[7]);
            for(s32 i = 0; i < TIC_PALETTE_SIZE && i < (int)list->array.n; i++)
            {
                colors[i] = (u8)grav_get_int(list->array.p[i]);
                count++;
            }
        }
        else
        {
            s32 color_val = grav_get_int_default(args[7], -1);
            colors[0] = (u8)color_val;
            count = color_val < 0 ? 0 : 1;
        }
    }

    s32 scale = nargs > 8 ? grav_get_int(args[8]) : 1;

    if(nargs > 9 && !VALUE_ISA_NULL(args[9]) && !VALUE_ISA_CLOSURE(args[9]))
        RETURN_ERROR("invalid remap function");

    gravity_closure_t* remap = nargs > 9 ? VALUE_AS_CLOSURE(args[9]) : NULL;

    if(remap)
    {
        RemapData data = {vm, remap};
        core->api.map(tic, x, y, w, h, sx, sy, colors, (u8)count, scale, remapCallback, &data);
    }
    else
    {
        core->api.map(tic, x, y, w, h, sx, sy, colors, (u8)count, scale, NULL, NULL);
    }

    RETURN_NOVALUE();
}

// MARK: memcpy
static bool grav_memcpy(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 4) RETURN_ERROR("invalid parameters, memcpy to from length");

    s32 dest = grav_get_int(args[1]);
    s32 src = grav_get_int(args[2]);
    s32 size = grav_get_int(args[3]);
    core->api.memcpy(tic, dest, src, size);

    RETURN_NOVALUE();
}

// MARK: memset
static bool grav_memset(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 4) RETURN_ERROR("invalid parameters, memset addr value length");

    s32 dest = grav_get_int(args[1]);
    u8 value = (u8)grav_get_int(args[2]);
    s32 size = grav_get_int(args[3]);
    core->api.memset(tic, dest, value, size);

    RETURN_NOVALUE();
}

// MARK: mget
static bool grav_mget(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 3) RETURN_ERROR("invalid parameters, mget x y");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);

    RETURN_VALUE(VALUE_FROM_INT(core->api.mget(tic, x, y)), rindex);
}

// MARK: mouse
static bool grav_mouse(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    (void)args;
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 1) RETURN_ERROR("invalid parameters, mouse");

    const int out_value_count = 7;

    tic_point pos = core->api.mouse((tic_mem*)core);
    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    gravity_list_t* list = gravity_list_new(vm, out_value_count);

    list->array.n = out_value_count;
    list->array.p[0] = VALUE_FROM_INT(pos.x);
    list->array.p[1] = VALUE_FROM_INT(pos.y);
    list->array.p[2] = VALUE_FROM_BOOL(mouse->left);
    list->array.p[3] = VALUE_FROM_BOOL(mouse->middle);
    list->array.p[4] = VALUE_FROM_BOOL(mouse->right);
    list->array.p[5] = VALUE_FROM_INT(mouse->scrollx);
    list->array.p[6] = VALUE_FROM_INT(mouse->scrolly);

    RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

// MARK: mset
static bool grav_mset(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 4) RETURN_ERROR("invalid parameters, mset x y tile_id");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    u8 val = (u8)grav_get_int(args[3]);
    core->api.mset(tic, x, y, val);

    RETURN_NOVALUE();
}

// MARK: music
static bool grav_music(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 8) RETURN_ERROR("invalid parameters, music [track [frame [row [loop=true [sustain=false [tempo [speed]]]]]]]");

    if(nargs == 1)
    {
        core->api.music(tic, -1, 0, 0, false, false, -1, -1);
        RETURN_NOVALUE();
    }

    s32 track = grav_get_int(args[1]);

    if(track > MUSIC_TRACKS - 1) RETURN_ERROR("invalid music track index");

    core->api.music(tic, -1, 0, 0, false, false, -1, -1);

    s32 frame = nargs >= 3 ? grav_get_int(args[2]) : -1;
    s32 row = nargs >= 4 ? grav_get_int(args[3]) : -1;
    bool loop = nargs >= 5 ? grav_get_int(args[4]) != 0 : true;
    bool sustain = nargs >= 6 ? grav_get_int(args[5]) != 0 : false;
    s32 tempo = nargs >= 7 ? grav_get_int(args[6]) : -1;
    s32 speed = nargs >= 8 ? grav_get_int(args[7]) : -1;

    core->api.music(tic, track, frame, row, loop, sustain, tempo, speed);

    RETURN_NOVALUE();
}

// MARK: peek
static bool grav_peek(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 2 || nargs > 3) RETURN_ERROR("invalid parameters, peek addr [bits=8]");

    s32 address = grav_get_int(args[1]);
    s32 bits = nargs == 3 ? grav_get_int_default(args[2], BITS_IN_BYTE) : BITS_IN_BYTE;

    RETURN_VALUE(VALUE_FROM_INT(core->api.peek(tic, address, bits)), rindex);
}

// MARK: peek1
static bool grav_peek1(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, peek1 bitaddr");

    s32 address = grav_get_int(args[1]);

    RETURN_VALUE(VALUE_FROM_INT(core->api.peek1(tic, address)), rindex);
}

// MARK: peek2
static bool grav_peek2(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, peek2 addr2");

    s32 address = grav_get_int(args[1]);

    RETURN_VALUE(VALUE_FROM_INT(core->api.peek2(tic, address)), rindex);
}

// MARK: peek4
static bool grav_peek4(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, peek4 addr4");

    s32 address = grav_get_int(args[1]);

    RETURN_VALUE(VALUE_FROM_INT(core->api.peek4(tic, address)), rindex);
}

// MARK: pix
static bool grav_pix(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 3 || nargs > 4) RETURN_ERROR("invalid parameters, pix x y [color]");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    u8 color = nargs == 4 ? (u8)grav_get_int(args[3]) : 0;
    u8 result = core->api.pix(tic, x, y, color, nargs == 3);

    if(nargs == 3) RETURN_VALUE(VALUE_FROM_INT(result), rindex);

    RETURN_NOVALUE();
}

// MARK: pmem
static bool grav_pmem(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 2 || nargs > 3) RETURN_ERROR("invalid parameters, pmem index [val32]");

    s32 index = grav_get_int(args[1]);
    if(index >= (s32)TIC_PERSISTENT_SIZE) RETURN_ERROR("invalid persistent tic index");

    u32 current = core->api.pmem(tic, index, 0, false);
    if(nargs == 3) core->api.pmem(tic, index, (u32)grav_get_int(args[2]), true);

    RETURN_VALUE(VALUE_FROM_INT(current), rindex);
}

// MARK: poke
static bool grav_poke(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 3 || nargs > 4) RETURN_ERROR("invalid parameters, poke addr val [bits=8]");

    s32 address = grav_get_int(args[1]);
    u8 value = (u8)grav_get_int(args[2]);
    s32 bits = nargs == 4 ? grav_get_int_default(args[3], BITS_IN_BYTE) : BITS_IN_BYTE;

    core->api.poke(tic, address, value, bits);

    RETURN_NOVALUE();
}

// MARK: poke1
static bool grav_poke1(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 3) RETURN_ERROR("invalid parameters, poke1 bitaddr bitval");

    s32 address = grav_get_int(args[1]);
    u8 value = (u8)grav_get_int(args[2]);

    core->api.poke1(tic, address, value);

    RETURN_NOVALUE();
}

// MARK: poke2
static bool grav_poke2(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 3) RETURN_ERROR("invalid parameters, poke2 addr2 val2");

    s32 address = grav_get_int(args[1]);
    u8 value = (u8)grav_get_int(args[2]);
    core->api.poke2(tic, address, value);

    RETURN_NOVALUE();
}

// MARK: poke4
static bool grav_poke4(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 3) RETURN_ERROR("invalid parameters, poke4 addr4 val4");

    s32 address = grav_get_int(args[1]);
    u8 value = (u8)grav_get_int(args[2]);
    core->api.poke4(tic, address, value);

    RETURN_NOVALUE();
}

// MARK: print
static bool grav_print(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 2 || nargs > 8)
        RETURN_ERROR("invalid parameters, print text [x=0 [y=0 [color=15 [fixed=false [scale=1 [smallfont=false]]]]]]");

    gravity_string_t* text = grav_get_string(vm, args[1]);
    s32 x = nargs >= 3 ? grav_get_int(args[2]) : 0;
    s32 y = nargs >= 4 ? grav_get_int(args[3]) : 0;
    u8 color = nargs >= 5 ? (u8)grav_get_int(args[4]) : TIC_DEFAULT_COLOR;
    bool fixed = nargs >= 6 ? grav_get_int(args[5]) != 0 : false;
    s32 scale = nargs >= 7 ? grav_get_int(args[6]) : 1;
    bool smallfont = nargs >= 8 ? grav_get_int(args[7]) != 0 : false;

    s32 width = core->api.print(tic, text->s, x, y, color, fixed, scale, smallfont);

    RETURN_VALUE(VALUE_FROM_INT(width), rindex);
}

// MARK: rect
static bool grav_rect(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 6) RETURN_ERROR("invalid parameters, rect x y w h color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 w = grav_get_int(args[3]);
    s32 h = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    core->api.rect(tic, x, y, w, h, color);

    RETURN_NOVALUE();
}

// MARK: rectb
static bool grav_rectb(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 6) RETURN_ERROR("invalid parameters, rectb x y w h color");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 w = grav_get_int(args[3]);
    s32 h = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    core->api.rectb(tic, x, y, w, h, color);

    RETURN_NOVALUE();
}

// MARK: rrect
static bool grav_rrect(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 6 || nargs > 8) RETURN_ERROR("invalid parameters, rrect x y w h color [radius=-1 [exponent=2]]");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 w = grav_get_int(args[3]);
    s32 h = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    s32 radius = nargs >= 7 ? grav_get_int(args[6]) : -1;
    float exponent = nargs >= 8 ? grav_get_float(args[7]) : 2.0f;

    core->api.rrect(tic, x, y, w, h, color, radius, exponent);

    RETURN_NOVALUE();
}

// MARK: rrectb
static bool grav_rrectb(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 6 || nargs > 8) RETURN_ERROR("invalid parameters, rrect x y w h color [radius=-1 [exponent=2]]");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 w = grav_get_int(args[3]);
    s32 h = grav_get_int(args[4]);
    u8 color = (u8)grav_get_int(args[5]);
    s32 radius = nargs >= 7 ? grav_get_int(args[6]) : -1;
    float exponent = nargs >= 8 ? grav_get_float(args[7]) : 2.0f;

    core->api.rrectb(tic, x, y, w, h, color, radius, exponent);

    RETURN_NOVALUE();
}

// MARK: reset
static bool grav_reset(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    (void)args;
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 1) RETURN_ERROR("invalid parameters, reset");

    core->api.reset(tic);

    RETURN_NOVALUE();
}

// MARK: sfx
static bool grav_sfx(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs >= 2)
    {
        s32 note = -1;
        s32 octave = -1;
        s32 duration = -1;
        s32 channel = 0;
        s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
        s32 speed = SFX_DEF_SPEED;

        s32 index = grav_get_int(args[1]);

        if(index < SFX_COUNT)
        {
            if(index >= 0)
            {
                tic_sample* effect = tic->ram->sfx.samples.data + index;

                note = effect->note;
                octave = effect->octave;
                speed = effect->speed;
            }

            if(nargs >= 3)
            {
                if(VALUE_ISA_INT(args[2]) || VALUE_ISA_FLOAT(args[2]))
                {
                    s32 id = grav_get_int(args[2]);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
                else if(VALUE_ISA_STRING(args[2]))
                {
                    gravity_string_t* str = grav_get_string(vm, args[2]);

                    if(!parse_note(str->s, &note, &octave))
                    {
                        RETURN_ERROR("invalid note, should be like C#4");
                    }
                }

                if(nargs >= 4)
                {
                    duration = grav_get_int(args[3]);

                    if(nargs >= 5)
                    {
                        channel = grav_get_int(args[4]);

                        if(nargs >= 6)
                        {
                            if(VALUE_ISA_LIST(args[5]))
                            {
                                gravity_list_t* volume_list = VALUE_AS_LIST(args[5]);
                                s32 arr_len = (s32)volume_list->array.n;
                                for(s32 i = 0; i < COUNT_OF(volumes) && i < arr_len; i++)
                                {
                                    volumes[i] = (s32)VALUE_AS_INT(volume_list->array.p[i]);
                                }
                            }
                            else
                                volumes[0] = volumes[1] = grav_get_int(args[5]);

                            if(nargs >= 7)
                            {
                                speed = grav_get_int(args[6]);
                            }
                        }
                    }
                }
            }

            if(channel >= 0 && channel < TIC_SOUND_CHANNELS)
            {
                core->api.sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
            }
            else
                RETURN_ERROR("unknown channel");
        }
        else
            RETURN_ERROR("unknown sfx index");
    }
    else
        RETURN_ERROR("invalid sfx params");

    RETURN_NOVALUE();
}

// MARK: spr
static bool grav_spr(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 4 || nargs > 10)
        RETURN_ERROR("invalid parameters, spr id x y [colorkey=-1 [scale=1 [flip=0 [rotate=0 [w=1 [h=1]]]]]]");

    s32 index = grav_get_int(args[1]);
    s32 x = grav_get_int(args[2]);
    s32 y = grav_get_int(args[3]);
    s32 scale = nargs > 5 ? grav_get_int(args[5]) : 1;
    tic_flip flip = nargs > 6 ? grav_get_int(args[6]) : tic_no_flip;
    tic_rotate rotate = nargs > 7 ? grav_get_int(args[7]) : tic_no_rotate;
    s32 w = nargs > 8 ? grav_get_int(args[8]) : 1;
    s32 h = nargs > 9 ? grav_get_int(args[9]) : 1;

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(nargs > 4)
    {
        if(VALUE_ISA_LIST(args[4]))
        {
            gravity_list_t* list = VALUE_AS_LIST(args[4]);
            for(s32 i = 0; i < TIC_PALETTE_SIZE && i < (int)list->array.n; i++)
            {
                colors[i] = (u8)grav_get_int(list->array.p[i]);
                count++;
            }
        }
        else
        {
            s32 color_val = grav_get_int_default(args[4], -1);
            colors[0] = (u8)color_val;
            count = color_val < 0 ? 0 : 1;
        }
    }

    core->api.spr(tic, index, x, y, w, h, colors, (u8)count, scale, flip, rotate);

    RETURN_NOVALUE();
}

// MARK: sync
static bool grav_sync(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 4) RETURN_ERROR("invalid parameters, sync [mask=0 [bank=0 [tocart=false]]]");

    u32 mask = nargs >= 2 ? grav_get_int(args[1]) : 0;
    s32 bank = nargs >= 3 ? grav_get_int(args[2]) : 0;
    bool toCart = nargs == 4 ? grav_get_int(args[3]) != 0 : false;

    if(bank >= 0 && bank < TIC_BANKS)
        core->api.sync(tic, mask, bank, toCart);
    else
        RETURN_ERROR("sync() error, invalid bank");

    RETURN_NOVALUE();
}

// MARK: time
static bool grav_time(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    (void)args;
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 1) RETURN_ERROR("invalid parameters, time");

    RETURN_VALUE(VALUE_FROM_FLOAT(core->api.time(tic)), rindex);
}

// MARK: trace
static bool grav_trace(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 2 || nargs > 3) RETURN_ERROR("invalid parameters, trace text [color=15]");

    gravity_string_t* text = grav_get_string(vm, args[1]);
    u8 color = nargs == 3 ? (u8)grav_get_int(args[2]) : TIC_DEFAULT_COLOR;

    core->api.trace(tic, text->s, color);

    RETURN_NOVALUE();
}

// MARK: tri
static bool grav_tri(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 8) RETURN_ERROR("invalid parameters, tri x1 y1 x2 y2 x3 y3 color");

    float pt[6];
    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = grav_get_float(args[i + 1]);

    u8 color = (u8)grav_get_int(args[7]);

    core->api.tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    RETURN_NOVALUE();
}

// MARK: trib
static bool grav_trib(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 8) RETURN_ERROR("invalid parameters, trib x1 y1 x2 y2 x3 y3 color");

    float pt[6];
    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = grav_get_float(args[i + 1]);

    u8 color = (u8)grav_get_int(args[7]);

    core->api.trib(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    RETURN_NOVALUE();
}

// MARK: tstamp
static bool grav_tstamp(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    (void)args;
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 1) RETURN_ERROR("invalid parameters, tstamp");

    RETURN_VALUE(VALUE_FROM_INT(core->api.tstamp(tic)), rindex);
}

// MARK: ttri
static bool grav_ttri(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 13 || nargs > 18)
        RETURN_ERROR(
            "invalid parameters, ttri x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 "
            "[src=0 [chroma=off [z1=0 z2=0 z3=0]]]");

    float pt[12];
    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = grav_get_float(args[i + 1]);

    // check for texture src
    tic_texture_src src = nargs >= 14 ? grav_get_int(args[13]) : tic_tiles_texture;

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    // check for chroma
    if(nargs >= 15)
    {
        if(VALUE_ISA_LIST(args[14]))
        {
            gravity_list_t* list = VALUE_AS_LIST(args[14]);
            for(s32 i = 0; i < TIC_PALETTE_SIZE && i < (int)list->array.n; i++)
            {
                colors[i] = (u8)grav_get_int(list->array.p[i]);
                count++;
            }
        }
        else
        {
            s32 color_val = grav_get_int_default(args[14], -1);
            colors[0] = (u8)color_val;
            count = color_val < 0 ? 0 : 1;
        }
    }

    float z[3] = {0, 0, 0};
    bool depth = false;

    if(nargs == 18)
    {
        for(s32 i = 0; i < COUNT_OF(z); i++)
            z[i] = grav_get_float(args[15 + i]);

        depth = true;
    }

    core->api.ttri(tic, pt[0], pt[1],        //  xy 1
                   pt[2], pt[3],             //  xy 2
                   pt[4], pt[5],             //  xy 3
                   pt[6], pt[7],             //  uv 1
                   pt[8], pt[9],             //  uv 2
                   pt[10], pt[11],           //  uv 3
                   src,                      // texture source
                   colors, count,            // chroma
                   z[0], z[1], z[2], depth); // depth

    RETURN_NOVALUE();
}

// MARK: vbank
static bool grav_vbank(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    s32 prev = core->state.vbank.id;

    if(nargs > 2) RETURN_ERROR("invalid parameters, vbank [id]");
    if(nargs == 2) core->api.vbank(tic, grav_get_int(args[1]));

    RETURN_VALUE(VALUE_FROM_INT(prev), rindex);
}

// MARK: paint
static bool grav_paint(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs < 4 || nargs > 5) RETURN_ERROR("invalid parameters, paint x y color [bordercolor=-1]");

    s32 x = grav_get_int(args[1]);
    s32 y = grav_get_int(args[2]);
    s32 color = grav_get_int(args[3]);
    s32 bordercolor = nargs == 5 ? grav_get_int(args[4]) : -1;

    core->api.paint(tic, x, y, (u8)color, (u8)bordercolor);

    RETURN_NOVALUE();
}

// MARK: fft
static bool grav_fft(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 3 || nargs < 2) RETURN_ERROR("invalid parameters, fft start_freq [end_freq]");

    s32 start_freq = grav_get_int(args[1]);
    s32 end_freq = nargs == 3 ? grav_get_int(args[2]) : -1;

    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.fft(tic, start_freq, end_freq)), rindex);
}

// MARK: ffts
static bool grav_ffts(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 3 || nargs < 2) RETURN_ERROR("invalid parameters, ffts start_freq [end_freq]");

    s32 start_freq = grav_get_int(args[1]);
    s32 end_freq = nargs == 3 ? grav_get_int(args[2]) : -1;

    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.ffts(tic, start_freq, end_freq)), rindex);
}

// MARK: fftr
static bool grav_fftr(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 3 || nargs < 2) RETURN_ERROR("invalid parameters, fftr start_freq [end_freq]");

    s32 start_freq = grav_get_int(args[1]);
    s32 end_freq = nargs == 3 ? grav_get_int(args[2]) : -1;

    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.fftr(tic, start_freq, end_freq)), rindex);
}

// MARK: fftrs
static bool grav_fftrs(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs > 3 || nargs < 2) RETURN_ERROR("invalid parameters, fftrs start_freq [end_freq]");

    s32 start_freq = grav_get_int(args[1]);
    s32 end_freq = nargs == 3 ? grav_get_int(args[2]) : -1;

    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.fftrs(tic, start_freq, end_freq)), rindex);
}

// MARK: vqt
static bool grav_vqt(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqt bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqt(tic, bin)), rindex);
}

// MARK: vqts
static bool grav_vqts(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqts bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqts(tic, bin)), rindex);
}

// MARK: vqtr
static bool grav_vqtr(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtr bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtr(tic, bin)), rindex);
}

// MARK: vqtrs
static bool grav_vqtrs(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtrs bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtrs(tic, bin)), rindex);
}

// MARK: vqtw
static bool grav_vqtw(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtw bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtw(tic, bin)), rindex);
}

// MARK: vqtsw
static bool grav_vqtsw(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtsw bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtsw(tic, bin)), rindex);
}

// MARK: vqtrw
static bool grav_vqtrw(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtrw bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtrw(tic, bin)), rindex);
}

// MARK: vqtrsw
static bool grav_vqtrsw(gravity_vm* vm, gravity_value_t* args, uint16_t nargs, uint32_t rindex)
{
    TIC_GRAVITY_GET_CORE(vm, tic, core);

    if(nargs != 2) RETURN_ERROR("invalid parameters, vqtrsw bin");

    s32 bin = grav_get_int(args[1]);
    RETURN_VALUE(VALUE_FROM_FLOAT((float)core->api.vqtrsw(tic, bin)), rindex);
}

// endregion

// region Callbacks

static void report_error(gravity_vm* vm, error_type_t type,
                         const char* description, error_desc_t desc, void* xdata)
{
    (void)vm;
    (void)type;
    tic_mem* mem = xdata;
    tic_core* core = (tic_core*)mem;

    if(core->data)
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "L%d, C%d: %s\n", desc.lineno, desc.colno, description);
        core->data->error(core->data->data, buf);
    }
}

static const char* get_precode(void* xdata)
{
    (void)xdata;
    static char buffer[16384];
    buffer[0] = '\0';
#define TIC_GRAVITY_EXTERN(name, ...)              \
    {                                              \
        strcat(buffer, "extern var " #name ";\n"); \
    }
    TIC_API_LIST(TIC_GRAVITY_EXTERN);
#undef TIC_GRAVITY_EXTERN
    return buffer;
}

static void initAPI(GRAVITYVM* vm)
{
    // bind TIC-80 API
#define API_FUNC_DEF(name, ...) {grav_##name, #name},
    static const struct
    {
        gravity_c_internal func;
        const char* name;
    } ApiItems[] = {TIC_API_LIST(API_FUNC_DEF)};
#undef API_FUNC_DEF

    for(s32 i = 0; i < COUNT_OF(ApiItems); i++)
        gravity_vm_setvalue(vm->vm, ApiItems[i].name, NEW_CLOSURE_VALUE(ApiItems[i].func));
}

static void closeGravity(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if(core->currentVM)
    {
        GRAVITYVM* currentVM = core->currentVM;
        core->currentVM = NULL;

        gravity_vm_free(currentVM->vm);
        gravity_core_free();
        free(currentVM);
    }
}

static bool initGravity(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;

    // ensure we close any previous vm
    closeGravity(tic);

    // allocate and populate the vm wrapper
    core->currentVM = malloc(sizeof(GRAVITYVM));
    GRAVITYVM* currentVM = core->currentVM;
    memset(currentVM, 0, sizeof(GRAVITYVM));
    gravity_delegate_t delegate = {
        .error_callback = report_error,
        .precode_callback = get_precode,
        .xdata = tic};
    currentVM->delegate = delegate;

    // create the gravity vm
    gravity_vm* vm = gravity_vm_new(&currentVM->delegate);
    currentVM->vm = vm;

    // compile the code
    gravity_compiler_t* compiler = gravity_compiler_create(&currentVM->delegate);
    gravity_closure_t* closure = gravity_compiler_run(compiler, code, strlen(code), 0, true, true);
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    // if the code failed compilation, bail
    if(!closure) return false;

    // register TIC-80 functions
    initAPI(currentVM);

    // execute the code
    gravity_vm_loadclosure(vm, closure);
    gravity_vm_runclosure(vm, closure, VALUE_FROM_NULL, NULL, 0);

    // register TIC-80 callbacks
#define TIC_GRAVITY_GET_CALLBACK(STORE, VM, FUNC_NAME)                                        \
    {                                                                                         \
        gravity_value_t funcval = gravity_vm_getvalue(VM, FUNC_NAME, (u32)strlen(FUNC_NAME)); \
        STORE = VALUE_ISA_CLOSURE(funcval) ? VALUE_AS_CLOSURE(funcval) : NULL;                \
    }
    TIC_GRAVITY_GET_CALLBACK(currentVM->borderFunction, vm, BDR_FN);
    TIC_GRAVITY_GET_CALLBACK(currentVM->bootFunction, vm, BOOT_FN);
    TIC_GRAVITY_GET_CALLBACK(currentVM->menuFunction, vm, MENU_FN);
    TIC_GRAVITY_GET_CALLBACK(currentVM->scanlineFunction, vm, SCN_FN);
    TIC_GRAVITY_GET_CALLBACK(currentVM->tickFunction, vm, TIC_FN);
#undef TIC_GRAVITY_GET_CALLBACK

    return true;
}

static void callGravityTick(tic_mem* tic)
{
    GRAVITYVM* currentVM = ((tic_core*)tic)->currentVM;
    if(currentVM->tickFunction)
    {
        gravity_vm_loadclosure(currentVM->vm, currentVM->tickFunction);
        gravity_vm_runclosure(currentVM->vm, currentVM->tickFunction, VALUE_FROM_NULL, NULL, 0);
    }
}

static void callGravityBoot(tic_mem* tic)
{
    GRAVITYVM* currentVM = ((tic_core*)tic)->currentVM;
    if(currentVM->bootFunction)
    {
        gravity_vm_loadclosure(currentVM->vm, currentVM->bootFunction);
        gravity_vm_runclosure(currentVM->vm, currentVM->bootFunction, VALUE_FROM_NULL, NULL, 0);
    }
}

static void callGravityScanline(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    GRAVITYVM* currentVM = ((tic_core*)tic)->currentVM;
    if(currentVM->scanlineFunction)
    {
        gravity_vm_loadclosure(currentVM->vm, currentVM->scanlineFunction);
        gravity_value_t params[1] = {VALUE_FROM_INT(row)};
        gravity_vm_runclosure(currentVM->vm, currentVM->scanlineFunction, VALUE_FROM_NULL, params, 1);
    }
}

static void callGravityBorder(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    GRAVITYVM* currentVM = ((tic_core*)tic)->currentVM;
    if(currentVM->borderFunction)
    {
        gravity_vm_loadclosure(currentVM->vm, currentVM->borderFunction);
        gravity_value_t params[1] = {VALUE_FROM_INT(row)};
        gravity_vm_runclosure(currentVM->vm, currentVM->borderFunction, VALUE_FROM_NULL, params, 1);
    }
}

static void callGravityMenu(tic_mem* tic, s32 index, void* data)
{
    (void)data;
    GRAVITYVM* currentVM = ((tic_core*)tic)->currentVM;
    if(currentVM->menuFunction)
    {
        gravity_vm_loadclosure(currentVM->vm, currentVM->menuFunction);
        gravity_value_t params[1] = {VALUE_FROM_INT(index)};
        gravity_vm_runclosure(currentVM->vm, currentVM->menuFunction, VALUE_FROM_NULL, params, 1);
    }
}

static const tic_outline_item* getGravityOutline(const char* code, s32* size)
{
    (void)code;
    (void)size;
    // TODO
    return NULL;
}

static void evalGravity(tic_mem* tic, const char* code)
{
    (void)tic;
    (void)code;
    // TODO
}

// endregion

// region Script Config

static const char* const GravityKeywords[] =
    {
        "if",
        "in",
        "or",
        "is",
        "for",
        "var",
        "and",
        "not",
        "func",
        "else",
        "true",
        "enum",
        "case",
        "null",
        "file",
        "lazy",
        "super",
        "false",
        "break",
        "while",
        "class",
        "const",
        "event",
        "_func",
        "_args",
        "struct",
        "repeat",
        "switch",
        "return",
        "public",
        "static",
        "default",
        "private",
        "continue",
        "internal",
        "undefined",
        // "import","extern","module", // these shouldn't be available in TIC-80
};

static const u8 DemoRom[] =
    {
#include "../build/assets/gravitydemo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/gravitymark.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(Gravity) =
    {
        .id = 0x66,
        .name = "gravity",
        .fileExtension = ".gravity",
        .projectComment = "//",
        .init = initGravity,
        .close = closeGravity,
        .tick = callGravityTick,
        .boot = callGravityBoot,

        .callback =
            {
                .scanline = callGravityScanline,
                .border = callGravityBorder,
                .menu = callGravityMenu,
            },

        .getOutline = getGravityOutline,
        .eval = evalGravity,

        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .blockCommentStart2 = NULL,
        .blockCommentEnd2 = NULL,
        .singleComment = "//",
        .blockStringStart = "@\"",
        .blockStringEnd = "\"",
        .blockEnd = "}",
        .stdStringStartEnd = "\'\"",

        .keywords = GravityKeywords,
        .keywordsCount = COUNT_OF(GravityKeywords),

        .demo = {DemoRom, sizeof DemoRom, "gravitydemo.tic"},
        .mark = {MarkRom, sizeof MarkRom, "gravitymark.tic"},
};

// endregion
