#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pocketpy.h"

/*
cmake -B build2 -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_STATIC=On -DBUILD_WITH_ALL=Off -DBUILD_WITH_PYTHON=On
cmake --build build2 --parallel 8 --config MinSizeRel
*/
extern bool parse_note(const char* noteStr, s32* note, s32* octave);

struct CachedNames
{
    py_Name TIC;
    py_Name BOOT;
    py_Name SCN;
    py_Name BDR;
    py_Name MENU;
} N;

static void* get_core()
{
    return py_getvmctx();
}

static void log_and_clearexc(py_Ref p0)
{
    char* msg = py_formatexc();
    tic_core* core = get_core();
    core->data->error(core->data->data, msg);
    PK_FREE(msg);
    py_clearexc(p0);
}

static int prepare_colorindex(py_Ref index, u8* buffer)
{
    if (py_istype(index, tp_int))
    {
        int value = py_toint(index);
        if (value == -1)
        {
            return 0;
        }
        else
        {
            buffer[0] = value;
            return 1;
        }
    }
    else if (py_istype(index, tp_list))
    {
        int list_len = py_list_len(index);
        if (list_len > TIC_PALETTE_SIZE)
        {
            list_len = TIC_PALETTE_SIZE;
        }

        for (int i = 0; i < list_len; i++)
        {
            py_ItemRef item = py_list_getitem(index, i);
            if (!py_checkint(item)) return -1;
            buffer[i] = py_toint(item);
        }
        return list_len;
    }
    else
    {
        TypeError("`colorkey` should be int or list, not %t", py_typeof(index));
        return -1;
    }
}

/*****************TIC-80 API BEGIN*****************/
// btn(id: int) -> bool
// u32 (*btn)(tic_mem*, s32)
static bool py_btn(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 button_id = py_toint(py_arg(0));

    tic_core* core = get_core();
    u32 pressed = core->api.btn((tic_mem*)core, button_id & 0x1f);
    py_newbool(py_retval(), pressed);
    return true;
}

// btnp(id: int, hold=-1, period=-1) -> bool
// u32 (*btnp)(tic_mem*, s32, s32, s32)
static bool py_btnp(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    s32 button_id = py_toint(py_arg(0));
    s32 hold = py_toint(py_arg(1));
    s32 period = py_toint(py_arg(2));

    tic_core* core = get_core();
    u32 pressed = core->api.btnp((tic_mem*)core, button_id, hold, period);
    py_newbool(py_retval(), pressed);
    return true;
}

// cls(color=0)
// void (*cls)(tic_mem*, u8)
static bool py_cls(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    u8 color = py_toint(py_arg(0));

    tic_core* core = get_core();
    core->api.cls((tic_mem*)core, color);
    py_newnone(py_retval());
    return true;
}

// spr(id: int, x: int, y: int, colorkey=-1, scale=1, flip=0, rotate=0, w=1, h=1)
// void (*spr)(tic_mem*, s32, s32, s32, s32, s32, u8*, u8, s32, tic_flip, tic_rotate)
static bool py_spr(int argc, py_Ref argv)
{
    for (int i = 0; i < 9; i++)
    {
        if (i == 3) continue;
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 spr_id = py_toint(py_arg(0));
    s32 x = py_toint(py_arg(1));
    s32 y = py_toint(py_arg(2));

    u8 colors[TIC_PALETTE_SIZE];
    int colors_count = prepare_colorindex(py_arg(3), colors);
    if (colors_count == -1) return false;

    s32 scale = py_toint(py_arg(4));
    tic_flip flip = py_toint(py_arg(5));
    tic_rotate rotate = py_toint(py_arg(6));
    s32 w = py_toint(py_arg(7));
    s32 h = py_toint(py_arg(8));

    tic_core* core = get_core();
    core->api.spr((tic_mem*)core, spr_id, x, y, w, h, colors, colors_count, scale, flip, rotate);
    py_newnone(py_retval());
    return true;
}

// print(text, x=0, y=0, color=15, fixed=False, scale=1, alt=False)
// s32 (*print)(tic_mem*, const char*, s32, s32, u8, bool, s32, bool)
static bool py_print(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_str);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_bool);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_bool);
    const char* text = py_tostr(py_arg(0));
    s32 x = py_toint(py_arg(1));
    s32 y = py_toint(py_arg(2));
    u8 color = py_toint(py_arg(3));
    bool fixed = py_tobool(py_arg(4));
    s32 scale = py_toint(py_arg(5));
    bool alt = py_tobool(py_arg(6));

    tic_core* core = get_core();
    s32 ps = core->api.print((tic_mem*)core, text, x, y, color, fixed, scale, alt);
    py_newint(py_retval(), ps);
    return true;
}

// circ(x: int, y: int, radius: int, color: int)
// void (*circ)(tic_mem*, s32, s32, s32, u8)
static bool py_circ(int argc, py_Ref argv)
{
    for (int i = 0; i < 4; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 radius = py_toint(py_arg(2));
    u8 color = py_toint(py_arg(3));

    tic_core* core = get_core();
    core->api.circ((tic_mem*)core, x, y, radius, color);
    py_newnone(py_retval());
    return true;
}

// circb(x: int, y: int, radius: int, color: int)
// void (*circb)(tic_mem*, s32, s32, s32, u8)
static bool py_circb(int argc, py_Ref argv)
{
    for (int i = 0; i < 4; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 radius = py_toint(py_arg(2));
    u8 color = py_toint(py_arg(3));

    tic_core* core = get_core();
    core->api.circb((tic_mem*)core, x, y, radius, color);
    py_newnone(py_retval());
    return true;
}

// clip(x: int, y: int, width: int, height: int)
// void (*clip)(tic_mem*, s32, s32, s32, s32)
static bool py_clip(int argc, py_Ref argv)
{
    for (int i = 0; i < 4; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 width = py_toint(py_arg(2));
    s32 height = py_toint(py_arg(3));

    tic_core* core = get_core();
    core->api.clip((tic_mem*)core, x, y, width, height);
    py_newnone(py_retval());
    return true;
}

// elli(x: int, y: int, a: int, b: int, color: int)
// void (*elli)(tic_mem*, s32, s32, s32, s32, u8)
static bool py_elli(int argc, py_Ref argv)
{
    for (int i = 0; i < 5; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 a = py_toint(py_arg(2));
    s32 b = py_toint(py_arg(3));
    u8 color = py_toint(py_arg(4));

    tic_core* core = get_core();
    core->api.elli((tic_mem*)core, x, y, a, b, color);
    py_newnone(py_retval());
    return true;
}

// ellib(x: int, y: int, a: int, b: int, color: int)
// void (*ellib)(tic_mem*, s32, s32, s32, s32, u8)
static bool py_ellib(int argc, py_Ref argv)
{
    for (int i = 0; i < 5; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 a = py_toint(py_arg(2));
    s32 b = py_toint(py_arg(3));
    u8 color = py_toint(py_arg(4));

    tic_core* core = get_core();
    core->api.ellib((tic_mem*)core, x, y, a, b, color);
    py_newnone(py_retval());
    return true;
}

// exit()
// void (*exit)(tic_mem*)
static bool py_exit(int argc, py_Ref argv)
{
    tic_core* core = get_core();
    core->api.exit((tic_mem*)core);
    py_newnone(py_retval());
    return true;
}

// fget(sprite_id: int, flag: int) -> bool
// bool (*fget)(tic_mem*, s32, u8)
static bool py_fget(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 sprite_id = py_toint(py_arg(0));
    u8 flag = py_toint(py_arg(1));

    tic_core* core = get_core();
    bool res = core->api.fget((tic_mem*)core, sprite_id, flag);
    py_newbool(py_retval(), res);
    return true;
}

// fset(sprite_id: int, flag: int, b: bool)
// void (*fset)(tic_mem*, s32, u8, bool)
static bool py_fset(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_bool);
    s32 sprite_id = py_toint(py_arg(0));
    u8 flag = py_toint(py_arg(1));
    bool b = py_tobool(py_arg(2));

    tic_core* core = get_core();
    core->api.fset((tic_mem*)core, sprite_id, flag, b);
    py_newnone(py_retval());
    return true;
}

// font(text: str, x: int, y: int, chromakey: int, char_width=8, char_height=8, fixed=False, scale=1, alt=False) -> int
// s32 (*font)(tic_mem*, const char*, s32, s32, u8*, u8, s32, s32, bool, s32, bool)
static bool py_font(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_str);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_bool);
    PY_CHECK_ARG_TYPE(7, tp_int);
    PY_CHECK_ARG_TYPE(8, tp_bool);
    const char* text = py_tostr(py_arg(0));
    s32 x = py_toint(py_arg(1));
    s32 y = py_toint(py_arg(2));
    u8 chromakey = py_toint(py_arg(3));
    s32 char_width = py_toint(py_arg(4));
    s32 char_height = py_toint(py_arg(5));
    bool fixed = py_tobool(py_arg(6));
    s32 scale = py_toint(py_arg(7));
    bool alt = py_tobool(py_arg(8));

    tic_core* core = get_core();
    s32 res = core->api.font((tic_mem*)core, text, x, y, &chromakey,
                             1, char_width, char_height, fixed, scale, alt);
    py_newint(py_retval(), res);
    return true;
}

// key(code=-1) -> bool
// bool (*key)(tic_mem*, tic_key)
static bool py_key(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 code = py_toint(py_arg(0));

    if (code <= -1 || code >= tic_keys_count)
    {
        code = tic_key_unknown;
    }

    tic_core* core = get_core();
    bool pressed = core->api.key((tic_mem*)core, (tic_key)code);
    py_newbool(py_retval(), pressed);
    return true;
}

// keyp(code=-1, hold=-1, period=-17) -> int
// bool (*keyp)(tic_mem*, tic_key, s32, s32)
static bool py_keyp(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    s32 code = py_toint(py_arg(0));
    s32 hold = py_toint(py_arg(1));
    s32 period = py_toint(py_arg(2));

    if (code <= -1 || code >= tic_keys_count)
    {
        code = tic_key_unknown;
    }

    tic_core* core = get_core();
    bool pressed = core->api.keyp((tic_mem*)core, (tic_key)code, hold, period);
    py_newbool(py_retval(), pressed);
    return true;
}

// line(x0: float, y0: float, x1: float, y1: float, color: int)
// void (*line)(tic_mem*, float, float, float, float, u8)
static bool py_line(int argc, py_Ref argv)
{
    float x0, y0, x1, y1;
    u8 color;
    if (!py_castfloat32(py_arg(0), &x0)) return false;
    if (!py_castfloat32(py_arg(1), &y0)) return false;
    if (!py_castfloat32(py_arg(2), &x1)) return false;
    if (!py_castfloat32(py_arg(3), &y1)) return false;
    PY_CHECK_ARG_TYPE(4, tp_int);
    color = py_toint(py_arg(4));

    tic_core* core = get_core();
    core->api.line((tic_mem*)core, x0, y0, x1, y1, color);
    py_newnone(py_retval());
    return true;
}

static void remap_callback(void* callable, s32 x, s32 y, RemapResult* out)
{
    py_StackRef p0 = py_peek(0);
    py_push(callable);
    py_pushnil();
    py_newint(py_pushtmp(), x);
    py_newint(py_pushtmp(), y);
    bool ok = py_vectorcall(2, 0);
    if (!ok)
    {
        log_and_clearexc(p0);
        return;
    }

    py_Ref res = py_retval();
    if (!py_checktype(res, tp_tuple))
    {
        log_and_clearexc(p0);
        return;
    }

    if (py_tuple_len(res) != 3)
    {
        TypeError("remap function should return a tuple of 3 items");
        log_and_clearexc(p0);
        return;
    }

    py_ItemRef arg0 = py_tuple_getitem(res, 0);
    py_ItemRef arg1 = py_tuple_getitem(res, 1);
    py_ItemRef arg2 = py_tuple_getitem(res, 2);

    if (!py_checktype(arg0, tp_int) ||
        !py_checktype(arg1, tp_int) ||
        !py_checktype(arg2, tp_int))
    {
        log_and_clearexc(p0);
        return;
    }

    out->index = py_toint(arg0);
    out->flip = py_toint(arg1);
    out->rotate = py_toint(arg2);
}

// map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None)
// void (*map)(tic_mem*, s32, s32, s32, s32, s32, s32, u8*, u8, s32, RemapFunc, void*)
static bool py_map(int argc, py_Ref argv)
{
    for (int i = 0; i < 8; i++)
    {
        if (i == 6) continue;
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 w = py_toint(py_arg(2));
    s32 h = py_toint(py_arg(3));
    s32 sx = py_toint(py_arg(4));
    s32 sy = py_toint(py_arg(5));

    u8 colors[TIC_PALETTE_SIZE];
    int colors_count = prepare_colorindex(py_arg(6), colors);
    if (colors_count == -1) return false;

    s32 scale = py_toint(py_arg(7));

    tic_core* core = get_core();
    if (!py_isnone(py_arg(8)))
    {
        core->api.map((tic_mem*)core, x, y, w, h, sx, sy, colors, colors_count, scale, remap_callback, py_arg(8));
    }
    else
    {
        core->api.map((tic_mem*)core, x, y, w, h, sx, sy, colors, colors_count, scale, NULL, NULL);
    }
    py_newnone(py_retval());
    return true;
}

// memcpy(dest: int, source: int, size: int)
// void (*memcpy)(tic_mem*, s32, s32, s32)
static bool py_memcpy(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    s32 dest = py_toint(py_arg(0));
    s32 source = py_toint(py_arg(1));
    s32 size = py_toint(py_arg(2));

    tic_core* core = get_core();
    core->api.memcpy((tic_mem*)core, dest, source, size);
    py_newnone(py_retval());
    return true;
}

// memset(dest: int, value: int, size: int)
// void (*memset)(tic_mem*, s32, u8, s32)
static bool py_memset(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    s32 dest = py_toint(py_arg(0));
    u8 value = py_toint(py_arg(1));
    s32 size = py_toint(py_arg(2));

    tic_core* core = get_core();
    core->api.memset((tic_mem*)core, dest, value, size);
    py_newnone(py_retval());
    return true;
}

// mget(x: int, y: int) -> int
// u8 (*mget)(tic_mem*, s32, s32)
static bool py_mget(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));

    tic_core* core = get_core();
    u8 res = core->api.mget((tic_mem*)core, x, y);
    py_newint(py_retval(), res);
    return true;
}

// mset(x: int, y: int, tile_id: int)
// void (*mset)(tic_mem*, s32, s32, u8)
static bool py_mset(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    u8 tile_id = py_toint(py_arg(2));

    tic_core* core = get_core();
    core->api.mset((tic_mem*)core, x, y, tile_id);
    py_newnone(py_retval());
    return true;
}

// mouse() -> tuple[int, int, bool, bool, bool, int, int]
static bool py_mouse(int argc, py_Ref argv)
{
    tic_core* core = get_core();

    tic_point pos = core->api.mouse((tic_mem*)core);
    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    py_Ref res = py_retval();
    py_newtuple(res, 7);
    py_newint(py_tuple_getitem(res, 0), pos.x);
    py_newint(py_tuple_getitem(res, 1), pos.y);
    py_newbool(py_tuple_getitem(res, 2), mouse->left);
    py_newbool(py_tuple_getitem(res, 3), mouse->middle);
    py_newbool(py_tuple_getitem(res, 4), mouse->right);
    py_newint(py_tuple_getitem(res, 5), mouse->scrollx);
    py_newint(py_tuple_getitem(res, 6), mouse->scrolly);
    return true;
}

// music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1)
// void (*music)(tic_mem*, s32, s32, s32, bool, bool, s32, s32)
static bool py_music(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_bool);
    PY_CHECK_ARG_TYPE(4, tp_bool);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_int);
    s32 track = py_toint(py_arg(0));
    s32 frame = py_toint(py_arg(1));
    s32 row = py_toint(py_arg(2));
    bool loop = py_tobool(py_arg(3));
    bool sustain = py_tobool(py_arg(4));
    s32 tempo = py_toint(py_arg(5));
    s32 speed = py_toint(py_arg(6));

    if (track > MUSIC_TRACKS - 1)
    {
        return ValueError("invalid music track number");
    }

    tic_core* core = get_core();
    core->api.music((tic_mem*)core, -1, 0, 0, false, false, -1, -1);
    core->api.music((tic_mem*)core, track, frame, row, loop, sustain, tempo, speed);
    py_newnone(py_retval());
    return true;
}

// peek(addr: int, bits=8) -> int
// u8 (*peek)(tic_mem*, s32, s32)
static bool py_peekbits(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 addr = py_toint(py_arg(0));
    s32 bits = py_toint(py_arg(1));

    tic_core* core = get_core();
    u8 res = core->api.peek((tic_mem*)core, addr, bits);
    py_newint(py_retval(), res);
    return true;
}

// peek1(addr: int) -> int
// u8 (*peek1)(tic_mem*, s32)
static bool py_peek1(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 addr = py_toint(py_arg(0));

    tic_core* core = get_core();
    u8 res = core->api.peek1((tic_mem*)core, addr);
    py_newint(py_retval(), res);
    return true;
}

// peek2(addr: int) -> int
// u8 (*peek2)(tic_mem*, s32)
static bool py_peek2(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 addr = py_toint(py_arg(0));

    tic_core* core = get_core();
    u8 res = core->api.peek2((tic_mem*)core, addr);
    py_newint(py_retval(), res);
    return true;
}

// peek4(addr: int) -> int
// u8 (*peek4)(tic_mem*, s32)
static bool py_peek4(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 addr = py_toint(py_arg(0));

    tic_core* core = get_core();
    u8 res = core->api.peek4((tic_mem*)core, addr);
    py_newint(py_retval(), res);
    return true;
}

// pix(x: int, y: int, color: int | None = None) -> int | None
// u8 (*pix)(tic_mem*, s32, s32, u8, bool)
static bool py_pix(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));

    tic_core* core = get_core();
    if (!py_isnone(py_arg(2)))
    {
        // set pixel
        PY_CHECK_ARG_TYPE(2, tp_int);
        u8 color = py_toint(py_arg(2));
        core->api.pix((tic_mem*)core, x, y, color, false);
        py_newnone(py_retval());
    }
    else
    {
        // get pixel
        u8 res = core->api.pix((tic_mem*)core, x, y, 0, true);
        py_newint(py_retval(), res);
    }
    return true;
}

// pmem(index: int, value: int | None = None) -> int | None
// u32 (*pmem)(tic_mem*, s32, u32, bool)
static bool py_pmem(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    s32 index = py_toint(py_arg(0));

    if (index >= TIC_PERSISTENT_SIZE)
    {
        return ValueError("invalid tic persistent index");
    }

    tic_core* core = get_core();
    if (!py_isnone(py_arg(1)))
    {
        // set persistent memory
        PY_CHECK_ARG_TYPE(1, tp_int);
        s32 value = py_toint(py_arg(1));
        core->api.pmem((tic_mem*)core, index, value, false);
        py_newnone(py_retval());
    }
    else
    {
        // get persistent memory
        u32 res = core->api.pmem((tic_mem*)core, index, 0, true);
        py_newint(py_retval(), res);
    }
    return true;
}

// poke(addr: int, value: int, bits=8)
// void (*poke)(tic_mem*, s32, u8, s32)
static bool py_pokebits(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 addr = py_toint(py_arg(0));
    s32 value = py_toint(py_arg(1));
    s32 bits = 8;

    tic_core* core = get_core();
    core->api.poke((tic_mem*)core, addr, value, bits);
    py_newnone(py_retval());
    return true;
}

// poke1(addr: int, value: int)
// void (*poke1)(tic_mem*, s32, u8)
static bool py_poke1(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 addr = py_toint(py_arg(0));
    s32 value = py_toint(py_arg(1));

    tic_core* core = get_core();
    core->api.poke1((tic_mem*)core, addr, value);
    py_newnone(py_retval());
    return true;
}

// poke2(addr: int, value: int)
// void (*poke2)(tic_mem*, s32, u16)
static bool py_poke2(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 addr = py_toint(py_arg(0));
    s32 value = py_toint(py_arg(1));

    tic_core* core = get_core();
    core->api.poke2((tic_mem*)core, addr, value);
    py_newnone(py_retval());
    return true;
}

// poke4(addr: int, value: int)
// void (*poke4)(tic_mem*, s32, u32)
static bool py_poke4(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    s32 addr = py_toint(py_arg(0));
    s32 value = py_toint(py_arg(1));

    tic_core* core = get_core();
    core->api.poke4((tic_mem*)core, addr, value);
    py_newnone(py_retval());
    return true;
}

// rect(x: int, y: int, w: int, h: int, color: int)
// void (*rect)(tic_mem*, s32, s32, s32, s32, u8)
static bool py_rect(int argc, py_Ref argv)
{
    for (int i = 0; i < 5; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 w = py_toint(py_arg(2));
    s32 h = py_toint(py_arg(3));
    u8 color = py_toint(py_arg(4));

    tic_core* core = get_core();
    core->api.rect((tic_mem*)core, x, y, w, h, color);
    py_newnone(py_retval());
    return true;
}

// rectb(x: int, y: int, w: int, h: int, color: int)
// void (*rectb)(tic_mem*, s32, s32, s32, s32, u8)
static bool py_rectb(int argc, py_Ref argv)
{
    for (int i = 0; i < 5; i++)
    {
        PY_CHECK_ARG_TYPE(i, tp_int);
    }
    s32 x = py_toint(py_arg(0));
    s32 y = py_toint(py_arg(1));
    s32 w = py_toint(py_arg(2));
    s32 h = py_toint(py_arg(3));
    u8 color = py_toint(py_arg(4));

    tic_core* core = get_core();
    core->api.rectb((tic_mem*)core, x, y, w, h, color);
    py_newnone(py_retval());
    return true;
}

// reset()
// void (*reset)(tic_mem*)
static bool py_reset(int argc, py_Ref argv)
{
    tic_core* core = get_core();
    core->api.reset((tic_mem*)core);
    py_newnone(py_retval());
    return true;
}

// sfx(id: int, note=-1, duration=-1, channel=0, volume=15, speed=0)
// void (*sfx)(tic_mem*, s32, s32, s32, s32, s32, s32, s32, s32)
static bool py_sfx(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    PY_CHECK_ARG_TYPE(5, tp_int);
    s32 id = py_toint(py_arg(0));
    s32 duration = py_toint(py_arg(2));
    s32 channel = py_toint(py_arg(3));
    s32 volume = py_toint(py_arg(4));
    s32 speed = py_toint(py_arg(5));

    s32 note, octave;
    if (py_isstr(py_arg(1)))
    {
        const char* str_note = py_tostr(py_arg(1));
        if (!parse_note(str_note, &note, &octave))
        {
            return ValueError("invalid note, should be like C#4");
        }
    }
    else
    {
        PY_CHECK_ARG_TYPE(1, tp_int);
        s32 raw_note = py_toint(py_arg(1));
        note = raw_note % NOTES;
        octave = raw_note / NOTES;
    }

    if (channel < 0 || channel >= TIC_SOUND_CHANNELS)
    {
        return ValueError("invalid channel");
    }
    if (id >= SFX_COUNT)
    {
        return ValueError("invalid sfx index");
    }

    tic_core* core = get_core();
    core->api.sfx((tic_mem*)core, id, note, octave, duration, channel, volume & 0xf, volume & 0xf, speed);
    py_newnone(py_retval());
    return true;
}

// sync(mask=0, bank=0, tocart=False)
// void (*sync)(tic_mem*, u32, s32, bool)
static bool py_sync(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_bool);
    u32 mask = py_toint(py_arg(0));
    s32 bank = py_toint(py_arg(1));
    bool tocart = py_tobool(py_arg(2));

    if (bank < 0 || bank >= TIC_BANKS)
    {
        return ValueError("invalid sync bank");
    }

    tic_core* core = get_core();
    core->api.sync((tic_mem*)core, mask, bank, tocart);
    py_newnone(py_retval());
    return true;
}

// ttri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, u1: float, v1: float, u2: float, v2: float, u3: float, v3: float, texsrc=0, chromakey=-1, z1=0.0, z2=0.0, z3=0.0)
// void (*ttri)(tic_mem*, float, float, float, float, float, float, float, float, float, float, float, float, tic_texture_src, u8*, s32, float, float, float, bool)
static bool py_ttri(int argc, py_Ref argv)
{
    float x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3;
    if (!py_castfloat32(py_arg(0), &x1)) return false;
    if (!py_castfloat32(py_arg(1), &y1)) return false;
    if (!py_castfloat32(py_arg(2), &x2)) return false;
    if (!py_castfloat32(py_arg(3), &y2)) return false;
    if (!py_castfloat32(py_arg(4), &x3)) return false;
    if (!py_castfloat32(py_arg(5), &y3)) return false;
    if (!py_castfloat32(py_arg(6), &u1)) return false;
    if (!py_castfloat32(py_arg(7), &v1)) return false;
    if (!py_castfloat32(py_arg(8), &u2)) return false;
    if (!py_castfloat32(py_arg(9), &v2)) return false;
    if (!py_castfloat32(py_arg(10), &u3)) return false;
    if (!py_castfloat32(py_arg(11), &v3)) return false;

    PY_CHECK_ARG_TYPE(12, tp_int);
    tic_texture_src texsrc = py_toint(py_arg(12));

    u8 colors[TIC_PALETTE_SIZE];
    int colors_count = prepare_colorindex(py_arg(13), colors);
    if (colors_count == -1) return false;

    float z1, z2, z3;
    if (!py_castfloat32(py_arg(14), &z1)) return false;
    if (!py_castfloat32(py_arg(15), &z2)) return false;
    if (!py_castfloat32(py_arg(16), &z3)) return false;

    tic_core* core = get_core();
    core->api.ttri((tic_mem*)core,
                   x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3,
                   texsrc,
                   colors, colors_count,
                   z1, z2, z3,
                   z1 != 0 || z2 != 0 || z3 != 0);
    py_newnone(py_retval());
    return true;
}

// time() -> float
// double (*time)(tic_mem*)
static bool py_time(int argc, py_Ref argv)
{
    tic_core* core = get_core();
    double res = core->api.time((tic_mem*)core);
    py_newfloat(py_retval(), res);
    return true;
}

// trace(message, color=15)
// void (*trace)(tic_mem*, const char*, u8)
static bool py_trace(int argc, py_Ref argv)
{
    PY_CHECK_ARG_TYPE(1, tp_int);
    u8 color = py_toint(py_arg(1));

    if (!py_str(py_arg(0))) return false;
    py_Ref arg0 = py_pushtmp();
    py_assign(arg0, py_retval());
    const char* msg = py_tostr(arg0);

    tic_core* core = get_core();
    core->api.trace((tic_mem*)core, msg, color);
    py_pop();
    py_newnone(py_retval());
    return true;
}

// tri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)
// void (*tri)(tic_mem*, float, float, float, float, float, float, u8)
static bool py_tri(int argc, py_Ref argv)
{
    float x1, y1, x2, y2, x3, y3;
    u8 color;
    if (!py_castfloat32(py_arg(0), &x1)) return false;
    if (!py_castfloat32(py_arg(1), &y1)) return false;
    if (!py_castfloat32(py_arg(2), &x2)) return false;
    if (!py_castfloat32(py_arg(3), &y2)) return false;
    if (!py_castfloat32(py_arg(4), &x3)) return false;
    if (!py_castfloat32(py_arg(5), &y3)) return false;
    PY_CHECK_ARG_TYPE(6, tp_int);
    color = py_toint(py_arg(6));

    tic_core* core = get_core();
    core->api.tri((tic_mem*)core, x1, y1, x2, y2, x3, y3, color);
    py_newnone(py_retval());
    return true;
}

// trib(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)
// void (*trib)(tic_mem*, float, float, float, float, float, float, u8)
static bool py_trib(int argc, py_Ref argv)
{
    float x1, y1, x2, y2, x3, y3;
    u8 color;
    if (!py_castfloat32(py_arg(0), &x1)) return false;
    if (!py_castfloat32(py_arg(1), &y1)) return false;
    if (!py_castfloat32(py_arg(2), &x2)) return false;
    if (!py_castfloat32(py_arg(3), &y2)) return false;
    if (!py_castfloat32(py_arg(4), &x3)) return false;
    if (!py_castfloat32(py_arg(5), &y3)) return false;
    PY_CHECK_ARG_TYPE(6, tp_int);
    color = py_toint(py_arg(6));

    tic_core* core = get_core();
    core->api.trib((tic_mem*)core, x1, y1, x2, y2, x3, y3, color);
    py_newnone(py_retval());
    return true;
}

// tstamp() -> int
// s32 (*tstamp)(tic_mem*)
static bool py_tstamp(int argc, py_Ref argv)
{
    tic_core* core = get_core();
    s32 res = core->api.tstamp((tic_mem*)core);
    py_newint(py_retval(), res);
    return true;
}

// vbank(bank: int | None = None) -> int
// s32 (*vbank)(tic_mem*, s32)
static bool py_vbank(int argc, py_Ref argv)
{
    int bank = -1;
    if (!py_isnone(py_arg(0)))
    {
        bank = py_toint(py_arg(0));
    }

    tic_core* core = get_core();

    s32 prev = core->state.vbank.id;
    if (bank >= 0)
    {
        core->api.vbank((tic_mem*)core, bank);
    }
    py_newint(py_retval(), prev);
    return true;
}

/*************TIC-80 MISC BEGIN****************/

static void bind_pkpy_v2()
{
    py_GlobalRef mod = py_getmodule("__main__");
    py_bind(mod, "btn(id: int) -> bool", py_btn);
    py_bind(mod, "btnp(id: int, hold=-1, period=-1) -> bool", py_btnp);
    py_bind(mod, "cls(color=0)", py_cls);
    py_bind(mod, "spr(id: int, x: int, y: int, colorkey=-1, scale=1, flip=0, rotate=0, w=1, h=1)",
            py_spr);
    py_bind(mod, "print(text, x=0, y=0, color=15, fixed=False, scale=1, alt=False)", py_print);
    py_bind(mod, "circ(x: int, y: int, radius: int, color: int)", py_circ);
    py_bind(mod, "circb(x: int, y: int, radius: int, color: int)", py_circb);
    py_bind(mod, "clip(x: int, y: int, width: int, height: int)", py_clip);
    py_bind(mod, "elli(x: int, y: int, a: int, b: int, color: int)", py_elli);
    py_bind(mod, "ellib(x: int, y: int, a: int, b: int, color: int)", py_ellib);
    py_bind(mod, "exit()", py_exit);
    py_bind(mod, "fget(sprite_id: int, flag: int) -> bool", py_fget);
    py_bind(mod, "fset(sprite_id: int, flag: int, b: bool)", py_fset);
    py_bind(mod, "font(text: str, x: int, y: int, chromakey: int, char_width=8, char_height=8, fixed=False, scale=1, alt=False) -> int", py_font);
    py_bind(mod, "key(code=-1) -> bool", py_key);
    py_bind(mod, "keyp(code=-1, hold=-1, period=-17) -> bool", py_keyp);
    py_bind(mod, "line(x0: float, y0: float, x1: float, y1: float, color: int)", py_line);
    py_bind(mod, "map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None)", py_map);
    py_bind(mod, "memcpy(dest: int, source: int, size: int)", py_memcpy);
    py_bind(mod, "memset(dest: int, value: int, size: int)", py_memset);
    py_bind(mod, "mget(x: int, y: int) -> int", py_mget);
    py_bind(mod, "mset(x: int, y: int, tile_id: int)", py_mset);
    py_bind(mod, "mouse() -> tuple[int, int, bool, bool, bool, int, int]", py_mouse);
    py_bind(mod, "music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1)", py_music);
    py_bind(mod, "peek(addr: int, bits=8) -> int", py_peekbits);
    py_bind(mod, "peek1(addr: int) -> int", py_peek1);
    py_bind(mod, "peek2(addr: int) -> int", py_peek2);
    py_bind(mod, "peek4(addr: int) -> int", py_peek4);
    py_bind(mod, "pix(x: int, y: int, color: int | None = None) -> int | None", py_pix);
    py_bind(mod, "pmem(index: int, value: int | None = None) -> int | None", py_pmem);
    py_bind(mod, "poke(addr: int, value: int, bits=8)", py_pokebits);
    py_bind(mod, "poke1(addr: int, value: int)", py_poke1);
    py_bind(mod, "poke2(addr: int, value: int)", py_poke2);
    py_bind(mod, "poke4(addr: int, value: int)", py_poke4);
    py_bind(mod, "rect(x: int, y: int, w: int, h: int, color: int)", py_rect);
    py_bind(mod, "rectb(x: int, y: int, w: int, h: int, color: int)", py_rectb);
    py_bind(mod, "reset()", py_reset);
    py_bind(mod, "sfx(id: int, note=-1, duration=-1, channel=0, volume=15, speed=0)", py_sfx);
    py_bind(mod, "sync(mask=0, bank=0, tocart=False)", py_sync);
    py_bind(mod, "ttri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, u1: float, v1: float, u2: float, v2: float, u3: float, v3: float, texsrc=0, chromakey=-1, z1=0.0, z2=0.0, z3=0.0)", py_ttri);
    py_bind(mod, "time() -> float", py_time);
    py_bind(mod, "trace(message, color=15)", py_trace);
    py_bind(mod, "tri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_tri);
    py_bind(mod, "trib(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_trib);
    py_bind(mod, "tstamp() -> int", py_tstamp);
    py_bind(mod, "vbank(bank: int | None = None) -> int", py_vbank);
}

void close_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (core->currentVM)
    {
        core->currentVM = NULL;
        py_resetvm();
    }
}

static bool init_pkpy_v2(tic_mem* tic, const char* code)
{
    py_initialize();
    tic_core* core = (tic_core*)tic;
    close_pkpy_v2(tic);

    N.TIC = py_name("TIC");
    N.BOOT = py_name("BOOT");
    N.SCN = py_name("SCN");
    N.BDR = py_name("BDR");
    N.MENU = py_name("MENU");

    py_setvmctx(core);
    core->currentVM = (void*)py_retval();
    bind_pkpy_v2();

    py_StackRef p0 = py_peek(0);
    if (!py_exec(code, "main.py", EXEC_MODE, NULL))
    {
        log_and_clearexc(p0);
        return false;
    }
    return true;
}

void tick_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_tick = py_getglobal(N.TIC);
    if (!py_tick) return;

    py_StackRef p0 = py_peek(0);
    py_push(py_tick);
    py_pushnil();
    if (!py_vectorcall(0, 0))
    {
        log_and_clearexc(p0);
    }
}

void boot_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_boot = py_getglobal(N.BOOT);
    if (!py_boot) return;

    py_StackRef p0 = py_peek(0);
    py_push(py_boot);
    py_pushnil();
    if (!py_vectorcall(0, 0))
    {
        log_and_clearexc(p0);
    }
}

void callback_scanline(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_scn = py_getglobal(N.SCN);
    if (!py_scn) return;

    py_StackRef p0 = py_peek(0);
    py_push(py_scn);
    py_pushnil();
    py_Ref py_row = py_retval();
    py_newint(py_row, row);
    py_push(py_row);
    if (!py_vectorcall(1, 0))
    {
        log_and_clearexc(p0);
    }
}

void callback_border(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_bdr = py_getglobal(N.BDR);
    if (!py_bdr) return;

    py_StackRef p0 = py_peek(0);
    py_push(py_bdr);
    py_pushnil();
    py_newint(py_pushtmp(), row);
    if (!py_vectorcall(1, 0))
    {
        log_and_clearexc(p0);
    }
}
void callback_menu(tic_mem* tic, s32 index, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_menu = py_getglobal(N.MENU);
    if (!py_menu) return;

    py_StackRef p0 = py_peek(0);
    py_push(py_menu);
    py_pushnil();
    py_newint(py_pushtmp(), index);
    if (!py_vectorcall(1, 0))
    {
        log_and_clearexc(p0);
    }
}

static const char* const PythonKeywords[] =
    {
        "False", "None", "True", "and", "as", "assert",
        "async", "await", "break", "class", "continue",
        "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is",
        "lambda", "nonlocal", "not", "or", "pass", "raise",
        "return", "try", "while", "with", "yield"};

static const u8 DemoRom[] =
    {
#include "../build/assets/pythondemo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/pythonmark.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(Python) =
    {
        .id = 20,
        .name = "python",
        .fileExtension = ".py",
        .projectComment = "#",
        .init = init_pkpy_v2,
        .close = close_pkpy_v2,
        .tick = tick_pkpy_v2,
        .boot = boot_pkpy_v2,

        .callback =
            {
                .scanline = callback_scanline,
                .border = callback_border,
                .menu = callback_menu,
            },

        .getOutline = NULL,
        .eval = NULL,
        //above is a must need
        .blockCommentStart = NULL,
        .blockCommentEnd = NULL,
        .blockCommentStart2 = NULL,
        .blockCommentEnd2 = NULL,
        .singleComment = "#",
        .blockStringStart = NULL,
        .blockStringEnd = NULL,
        .blockEnd = NULL,
        .stdStringStartEnd = "'\"",

        .keywords = PythonKeywords,
        .keywordsCount = COUNT_OF(PythonKeywords),

        .demo = {DemoRom, sizeof DemoRom},
        .mark = {MarkRom, sizeof MarkRom, "pythonmark.tic"},
};
